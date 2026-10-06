// Network port: a non-blocking stream socket the engine core uses for all
// TCP traffic (auth, channel, mark/HTTP services). On POSIX it is a thin
// wrapper over BSD sockets; on web it is backed by WebSocket bridging.
//
// Handles are small non-negative ints. -1 / negative results mean failure.
// Poll() returns a bitmask of NET_POLL_* flags.
#pragma once

namespace M2Net
{
	enum
	{
		NET_POLL_READABLE = 1,
		NET_POLL_WRITABLE = 2,
		NET_POLL_ERROR = 4,
	};

	enum
	{
		NET_WOULD_BLOCK = -1,	// send/recv: nothing to do right now
		NET_ERROR = -2,			// send/recv: fatal, close the handle
		NET_CLOSED = -3,		// recv: peer performed orderly shutdown
	};

	// Begin a non-blocking connect. host may be a dotted quad or a DNS name
	// (the adapter resolves it). Returns a handle >= 0 or < 0 on failure.
	int Connect(const char* host, int port);

	// True once the connect attempt has completed; *errCode is 0 on success.
	// Still false while in progress.
	bool ConnectDone(int handle, int* errCode);

	// Immediate readiness snapshot (no waiting).
	int Poll(int handle);

	// Non-blocking send. Returns bytes queued (>= 0), NET_WOULD_BLOCK, NET_ERROR.
	int Send(int handle, const void* buf, int len);

	// Non-blocking recv. Returns bytes read (>= 0), NET_WOULD_BLOCK (0 bytes
	// available is also fine), NET_CLOSED on orderly shutdown, NET_ERROR.
	int Recv(int handle, void* buf, int cap);

	void Close(int handle);

	// recv()/send()-shaped wrappers so call sites written for POSIX sockets keep
	// working unchanged: -1 + errno on failure, recv returns 0 on orderly close.
	inline int StreamRecv(int handle, void* buf, int len)
	{
		int r = Recv(handle, buf, len);
		if (r == NET_WOULD_BLOCK) { errno = EWOULDBLOCK; return -1; }
		if (r == NET_CLOSED) return 0;
		if (r < 0) { errno = ECONNRESET; return -1; }
		return r;
	}
	inline int StreamSend(int handle, const void* buf, int len)
	{
		int r = Send(handle, buf, len);
		if (r == NET_WOULD_BLOCK) { errno = EWOULDBLOCK; return -1; }
		if (r < 0) { errno = ECONNRESET; return -1; }
		return r;
	}
}
