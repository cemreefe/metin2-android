#pragma once

#include "MSWindow.h"
#include "../platform/m2platform.h"

class CMSApplication : public CMSWindow
{
public:
	CMSApplication();
	virtual ~CMSApplication();

	void Initialize(HINSTANCE hInstance);

	void MessageLoop();

	bool IsMessage();
	bool MessageProcess();
#ifdef M2_PORT
	// Touch actions 0/1/2 are down/up/move (MotionEvent on Android, pointer events on web);
	// the synthetic ones replay a deferred tap.
	// TOUCH_WHEEL carries a mouse-wheel delta in x, as a pinch gesture maps to camera distance.
	enum { TOUCH_SYNTHETIC_DOWN = 16, TOUCH_SYNTHETIC_UP = 17, TOUCH_WHEEL = 18,
		TOUCH_RIGHT_DOWN = 19, TOUCH_RIGHT_UP = 20 };
	static void PushTouchEvent(int action, int x, int y);
	virtual void OnTouchEvent(int action, int x, int y) {}
	static void PushKeyEvent(int action, int keyCode, int unicodeChar);
	virtual void OnPortKeyEvent(int action, int keyCode, int unicodeChar) {}
	static void PortFrameDone();
	static void PortEndFrame();
#endif

protected:
	void ClearWindowClass();

	LRESULT WindowProcedure(HWND hWnd, UINT uiMsg, WPARAM wParam, LPARAM lParam);
};
