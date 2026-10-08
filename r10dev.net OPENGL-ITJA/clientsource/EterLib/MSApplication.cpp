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

#ifdef M2_PORT
#include <mutex>
#include <deque>
#include "../EterBase/Timer.h"

struct STouchEvent { bool key; int action, x, y; };
static std::mutex s_touchMutex;
static std::deque<STouchEvent> s_touchQueue;
// A touch has no hover phase, so a press is held back for two rendered frames after
// moving the cursor; the frame's pick pass then sees what is under the finger.
static bool s_bPressHovered = false;
static bool s_bWaitFrame = false;
static int s_iWaitFrames = 0;
static DWORD s_dwWaitStart = 0;

static bool s_bFrameCounted = false;

void CMSApplication::PortFrameDone()
{
	s_bFrameCounted = true;
	if (s_iWaitFrames > 0 && --s_iWaitFrames == 0)
		s_bWaitFrame = false;
}

// Phases without a 3D game frame (login, character select) never reach the pick pass,
// so the end of any rendered frame counts instead.
void CMSApplication::PortEndFrame()
{
	if (!s_bFrameCounted)
		PortFrameDone();
	s_bFrameCounted = false;
}

void CMSApplication::PushTouchEvent(int action, int x, int y)
{
	std::lock_guard<std::mutex> lock(s_touchMutex);
	s_touchQueue.push_back({ false, action, x, y });
}

void CMSApplication::PushKeyEvent(int action, int keyCode, int unicodeChar)
{
	std::lock_guard<std::mutex> lock(s_touchMutex);
	s_touchQueue.push_back({ true, action, keyCode, unicodeChar });
}

bool CMSApplication::IsMessage()
{
	std::lock_guard<std::mutex> lock(s_touchMutex);
	if (s_bWaitFrame && ELTimer_GetMSec() - s_dwWaitStart > 1500)
		s_bWaitFrame = false;
	return !s_bWaitFrame && !s_touchQueue.empty();
}

bool CMSApplication::MessageProcess()
{
	STouchEvent ev;
	{
		std::lock_guard<std::mutex> lock(s_touchMutex);
		if (s_touchQueue.empty())
			return true;
		ev = s_touchQueue.front();
		bool bPress = !ev.key && (ev.action == 0 || ev.action == TOUCH_SYNTHETIC_DOWN);
		if (bPress && !s_bPressHovered)
		{
			s_bPressHovered = true;
			s_bWaitFrame = true;
			s_dwWaitStart = ELTimer_GetMSec();
			s_iWaitFrames = 2;
			ev.action = 2;
		}
		else
		{
			if (bPress)
				s_bPressHovered = false;
			s_touchQueue.pop_front();
		}
	}
	if (ev.key)
		OnPortKeyEvent(ev.action, ev.x, ev.y);
	else
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