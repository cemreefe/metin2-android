#pragma once

#include "MSWindow.h"

class CMSApplication : public CMSWindow
{
public:
	CMSApplication();
	virtual ~CMSApplication();

	void Initialize(HINSTANCE hInstance);

	void MessageLoop();

	bool IsMessage();
	bool MessageProcess();
#ifdef __ANDROID__
	// Touch actions 0/1/2 are MotionEvent down/up/move; the synthetic ones replay a deferred tap.
	enum { TOUCH_SYNTHETIC_DOWN = 16, TOUCH_SYNTHETIC_UP = 17 };
	static void PushTouchEvent(int action, int x, int y);
	virtual void OnTouchEvent(int action, int x, int y) {}
	static void PushKeyEvent(int action, int keyCode, int unicodeChar);
	virtual void OnAndroidKeyEvent(int action, int keyCode, int unicodeChar) {}
	static void AndroidFrameDone();
	static void AndroidEndFrame();
#endif

protected:
	void ClearWindowClass();

	LRESULT WindowProcedure(HWND hWnd, UINT uiMsg, WPARAM wParam, LPARAM lParam);
};
