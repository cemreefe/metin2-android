// Web adapter for the M2Net port: each handle is a WebSocket to the ws->tcp
// bridge (tools/ws_bridge.py). Browsers cannot open raw TCP, so every game
// connection is framed as ws://<bridge>/connect?target=<host>:<port> and the
// bridge pipes bytes both ways.
//
// All callbacks run on the browser main thread, the same thread the game loop
// runs on — no locking needed; events between frames just sit in the queue.
#include "../m2net.h"
#include "../m2platform.h"

#include <emscripten/websocket.h>
#include <emscripten/emscripten.h>

#include <deque>
#include <vector>
#include <string>
#include <string.h>
#include <stdlib.h>

namespace
{
	enum EWebSockState { WS_CONNECTING, WS_OPEN, WS_CLOSED, WS_FAILED };

	struct SWebSock
	{
		bool bUsed = false;
		EMSCRIPTEN_WEBSOCKET_T ws = 0;
		EWebSockState eState = WS_CONNECTING;
		int iErrCode = 0;
		std::deque<std::vector<uint8_t>> qIn;
	};

	const int MAX_HANDLES = 64;
	SWebSock s_socks[MAX_HANDLES];

	int AllocHandle(EMSCRIPTEN_WEBSOCKET_T ws)
	{
		for (int i = 0; i < MAX_HANDLES; ++i)
			if (!s_socks[i].bUsed)
			{
				s_socks[i].bUsed = true;
				s_socks[i].ws = ws;
				s_socks[i].eState = WS_CONNECTING;
				s_socks[i].iErrCode = 0;
				s_socks[i].qIn.clear();
				return i;
			}
		return -1;
	}

	SWebSock* GetSock(int h)
	{
		if (h < 0 || h >= MAX_HANDLES || !s_socks[h].bUsed)
			return NULL;
		return &s_socks[h];
	}

	EM_BOOL OnOpen(int, const EmscriptenWebSocketOpenEvent*, void* userData)
	{
		SWebSock* s = GetSock((int)(intptr_t)userData);
		if (s)
			s->eState = WS_OPEN;
		return EM_TRUE;
	}

	EM_BOOL OnMessage(int, const EmscriptenWebSocketMessageEvent* e, void* userData)
	{
		SWebSock* s = GetSock((int)(intptr_t)userData);
		if (s && !e->isText && e->data && e->numBytes)
			s->qIn.emplace_back(e->data, e->data + e->numBytes);
		return EM_TRUE;
	}

	EM_BOOL OnError(int, const EmscriptenWebSocketErrorEvent*, void* userData)
	{
		SWebSock* s = GetSock((int)(intptr_t)userData);
		if (s && s->eState != WS_CLOSED)
		{
			s->eState = WS_FAILED;
			s->iErrCode = 1;
		}
		return EM_TRUE;
	}

	EM_BOOL OnClose(int, const EmscriptenWebSocketCloseEvent* e, void* userData)
	{
		SWebSock* s = GetSock((int)(intptr_t)userData);
		if (s && s->eState != WS_FAILED)
		{
			s->eState = WS_CLOSED;
			s->iErrCode = e->code;
		}
		return EM_TRUE;
	}
}

int M2Net::Connect(const char* host, int port)
{
	// Bridge endpoint: ws(s)://<bridge-host>/connect?target=<host>:<port>
	// M2_WS_BRIDGE overrides; default is same-origin /ws.
	char szUrl[512];
	const char* cBridge = getenv("M2_WS_BRIDGE");
	if (cBridge && *cBridge)
		snprintf(szUrl, sizeof(szUrl), "%s%starget=%s:%d",
			cBridge, strchr(cBridge, '?') ? "&" : "?", host, port);
	else
	{
		char szOrigin[256] = { 0 };
		// worker location is the worker script URL; the page origin lives on
		// the main thread
		MAIN_THREAD_EM_ASM({
			var proto = location.protocol === 'https:' ? 'wss://' : 'ws://';
			stringToUTF8(proto + location.host + '/ws', $0, $1);
		}, szOrigin, sizeof(szOrigin));
		snprintf(szUrl, sizeof(szUrl), "%s?target=%s:%d", szOrigin, host, port);
	}

	EmscriptenWebSocketCreateAttributes attrs;
	emscripten_websocket_init_create_attributes(&attrs);
	attrs.url = szUrl;
	attrs.protocols = NULL;   // ws_bridge speaks plain binary ws
	attrs.createOnMainThread = EM_TRUE;

	EMSCRIPTEN_WEBSOCKET_T ws = emscripten_websocket_new(&attrs);
	if (ws <= 0)
		return -1;

	int h = AllocHandle(ws);
	if (h < 0)
	{
		emscripten_websocket_close(ws, 0, NULL);
		emscripten_websocket_delete(ws);
		return -1;
	}

	emscripten_websocket_set_onopen_callback(ws, (void*)(intptr_t)h, OnOpen);
	emscripten_websocket_set_onmessage_callback(ws, (void*)(intptr_t)h, OnMessage);
	emscripten_websocket_set_onerror_callback(ws, (void*)(intptr_t)h, OnError);
	emscripten_websocket_set_onclose_callback(ws, (void*)(intptr_t)h, OnClose);

	M2Plat::Log(M2Plat::LOG_INFO, "net", "connect %s:%d via %s (h=%d)", host, port, szUrl, h);
	return h;
}

bool M2Net::ConnectDone(int handle, int* errCode)
{
	SWebSock* s = GetSock(handle);
	if (!s)
	{
		if (errCode) *errCode = -1;
		return true;
	}
	if (s->eState == WS_OPEN)
	{
		if (errCode) *errCode = 0;
		return true;
	}
	if (s->eState == WS_FAILED || s->eState == WS_CLOSED)
	{
		if (errCode) *errCode = s->iErrCode ? s->iErrCode : 1;
		return true;
	}
	return false;
}

int M2Net::Poll(int handle)
{
	SWebSock* s = GetSock(handle);
	if (!s)
		return NET_POLL_ERROR;
	int flags = 0;
	if (s->eState == WS_OPEN)
		flags |= NET_POLL_WRITABLE;
	if (!s->qIn.empty() || s->eState == WS_CLOSED || s->eState == WS_FAILED)
		flags |= NET_POLL_READABLE;
	if (s->eState == WS_FAILED)
		flags |= NET_POLL_ERROR;
	return flags;
}

int M2Net::Send(int handle, const void* buf, int len)
{
	SWebSock* s = GetSock(handle);
	if (!s || s->eState != WS_OPEN)
		return NET_ERROR;
	// Browsers buffer ws sends internally; report all bytes queued.
	EMSCRIPTEN_RESULT r = emscripten_websocket_send_binary(s->ws, (void*)buf, len);
	return r == EMSCRIPTEN_RESULT_SUCCESS ? len : NET_ERROR;
}

int M2Net::Recv(int handle, void* buf, int cap)
{
	SWebSock* s = GetSock(handle);
	if (!s)
		return NET_ERROR;
	if (!s->qIn.empty())
	{
		std::vector<uint8_t>& front = s->qIn.front();
		int n = (int)front.size() < cap ? (int)front.size() : cap;
		memcpy(buf, front.data(), n);
		if (n == (int)front.size())
			s->qIn.pop_front();
		else
			front.erase(front.begin(), front.begin() + n);
		return n;
	}
	if (s->eState == WS_CLOSED)
		return NET_CLOSED;
	if (s->eState == WS_FAILED)
		return NET_ERROR;
	return NET_WOULD_BLOCK;
}

void M2Net::Close(int handle)
{
	SWebSock* s = GetSock(handle);
	if (!s)
		return;
	emscripten_websocket_close(s->ws, 1000, NULL);
	emscripten_websocket_delete(s->ws);
	s->bUsed = false;
	s->qIn.clear();
}
