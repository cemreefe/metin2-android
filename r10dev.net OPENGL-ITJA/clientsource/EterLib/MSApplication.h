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
	static void PushTouchEvent(int action, int x, int y);
	virtual void OnTouchEvent(int action, int x, int y) {}
#endif

protected:
	void ClearWindowClass();

	LRESULT WindowProcedure(HWND hWnd, UINT uiMsg, WPARAM wParam, LPARAM lParam);
};
