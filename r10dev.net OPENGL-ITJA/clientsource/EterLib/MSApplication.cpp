#include "StdAfx.h"
#include "MSApplication.h"

CMSApplication::CMSApplication()
{
}

CMSApplication::~CMSApplication()
{
	//	for (TWindowClassSet::iterator i=ms_stWCSet.begin(); i!=ms_stWCSet.end(); ++i)
	//		UnregisterClass(*i, ms_hInstance);
}

void CMSApplication::Initialize(HINSTANCE hInstance)
{
	ms_hInstance = hInstance;
}

void CMSApplication::MessageLoop()
{
	while (MessageProcess());
}

#ifdef __ANDROID__
#include <mutex>
#include <deque>

struct STouchEvent { int action, x, y; };
static std::mutex s_touchMutex;
static std::deque<STouchEvent> s_touchQueue;

void CMSApplication::PushTouchEvent(int action, int x, int y)
{
	std::lock_guard<std::mutex> lock(s_touchMutex);
	s_touchQueue.push_back({ action, x, y });
}

bool CMSApplication::IsMessage()
{
	std::lock_guard<std::mutex> lock(s_touchMutex);
	return !s_touchQueue.empty();
}

bool CMSApplication::MessageProcess()
{
	STouchEvent ev;
	{
		std::lock_guard<std::mutex> lock(s_touchMutex);
		if (s_touchQueue.empty())
			return true;
		ev = s_touchQueue.front();
		s_touchQueue.pop_front();
	}
	OnTouchEvent(ev.action, ev.x, ev.y);
	return true;
}
#else
bool CMSApplication::IsMessage()
{
	MSG msg;

	if (!PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
		return false;

	return true;
}

bool CMSApplication::MessageProcess()
{
	MSG msg;

	if (!GetMessage(&msg, NULL, 0, 0))
		return false;

	TranslateMessage(&msg);
	DispatchMessage(&msg);
	return true;
}
#endif

LRESULT CMSApplication::WindowProcedure(HWND hWnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uiMsg)
	{
	case WM_CLOSE:
		PostQuitMessage(0);
		break;
	}

	return CMSWindow::WindowProcedure(hWnd, uiMsg, wParam, lParam);
}