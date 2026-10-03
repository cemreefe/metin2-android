#include "StdAfx.h"
#include "PythonApplication.h"
#include "../EterLib/Camera.h"
#include "../EterPythonLib/PythonWindow.h"

void CPythonApplication::OnCameraUpdate()
{
	if (m_pyBackground.IsMapReady())
	{
		CCamera* pkCameraMgr = CCameraManager::Instance().GetCurrentCamera();
		if (pkCameraMgr)
			pkCameraMgr->Update();
	}
}

void CPythonApplication::OnUIUpdate()
{
	UI::CWindowManager& rkUIMgr = UI::CWindowManager::Instance();
	rkUIMgr.Update();
}

void CPythonApplication::OnUIRender()
{
	UI::CWindowManager& rkUIMgr = UI::CWindowManager::Instance();
	rkUIMgr.Render();
}

void CPythonApplication::OnSizeChange(int width, int height)
{
}

void CPythonApplication::OnMouseMiddleButtonDown(int x, int y)
{
	CCameraManager& rkCmrMgr = CCameraManager::Instance();
	CCamera* pkCmrCur = rkCmrMgr.GetCurrentCamera();
	if (pkCmrCur)
		pkCmrCur->BeginDrag(x, y);

	if (!m_pyBackground.IsMapReady())
		return;

	SetCursorNum(CAMERA_ROTATE);
	if (CURSOR_MODE_HARDWARE == GetCursorMode())
		SetCursorVisible(FALSE, true);
}

void CPythonApplication::OnMouseMiddleButtonUp(int x, int y)
{
	CCameraManager& rkCmrMgr = CCameraManager::Instance();
	CCamera* pkCmrCur = rkCmrMgr.GetCurrentCamera();
	if (pkCmrCur)
		pkCmrCur->EndDrag();

	if (!m_pyBackground.IsMapReady())
		return;

	SetCursorNum(NORMAL);
	if (CURSOR_MODE_HARDWARE == GetCursorMode())
		SetCursorVisible(TRUE);
}

void CPythonApplication::OnMouseWheel(int nLen)
{
	CCameraManager& rkCmrMgr = CCameraManager::Instance();
	CCamera* pkCmrCur = rkCmrMgr.GetCurrentCamera();
	if (pkCmrCur)
		pkCmrCur->Wheel(nLen);
}

void CPythonApplication::OnMouseMove(int x, int y)
{
	CCameraManager& rkCmrMgr = CCameraManager::Instance();
	CCamera* pkCmrCur = rkCmrMgr.GetCurrentCamera();

	POINT Point;
	if (pkCmrCur)
	{
		if (CPythonBackground::Instance().IsMapReady() && pkCmrCur->Drag(x, y, &Point))
		{
			x = Point.x;
			y = Point.y;
			ClientToScreen(m_hWnd, &Point);

			// 2004.07.26.myevan.��ö��HackShield�� �浹
			SetCursorPos(Point.x, Point.y);
		}
	}

	RECT rcWnd;
	GetClientRect(&rcWnd);

	UI::CWindowManager& rkWndMgr = UI::CWindowManager::Instance();
	rkWndMgr.SetResolution(rcWnd.right - rcWnd.left, rcWnd.bottom - rcWnd.top);

	rkWndMgr.RunMouseMove(x, y);
}

void CPythonApplication::OnMouseLeftButtonDown(int x, int y)
{
	UI::CWindowManager& rkWndMgr = UI::CWindowManager::Instance();
	rkWndMgr.RunMouseLeftButtonDown(x, y);
}

void CPythonApplication::OnMouseLeftButtonUp(int x, int y)
{
	UI::CWindowManager& rkWndMgr = UI::CWindowManager::Instance();
	rkWndMgr.RunMouseLeftButtonUp(x, y);
}

void CPythonApplication::OnMouseLeftButtonDoubleClick(int x, int y)
{
	UI::CWindowManager& rkWndMgr = UI::CWindowManager::Instance();
	rkWndMgr.RunMouseLeftButtonDown(x, y);
	rkWndMgr.RunMouseLeftButtonDoubleClick(x, y);
}

void CPythonApplication::OnMouseRightButtonDown(int x, int y)
{
	UI::CWindowManager& rkWndMgr = UI::CWindowManager::Instance();
	rkWndMgr.RunMouseRightButtonDown(x, y);
}

void CPythonApplication::OnMouseRightButtonUp(int x, int y)
{
	UI::CWindowManager& rkWndMgr = UI::CWindowManager::Instance();
	rkWndMgr.RunMouseRightButtonUp(x, y);
}

#ifdef ENABLE_MOUSEWHEEL_EVENT
bool CPythonApplication::OnMouseWheelScroll(long x, long y, short wDelta)
{
	UI::CWindowManager& rkWndMgr = UI::CWindowManager::Instance();
	return rkWndMgr.RunMouseWheelScroll(x, y, wDelta);
}
#endif

void CPythonApplication::OnKeyDown(int iIndex)
{
	UI::CWindowManager& rkWndMgr = UI::CWindowManager::Instance();

	if (DIK_ESCAPE == iIndex)
	{
		rkWndMgr.RunPressEscapeKey();
	}

	rkWndMgr.RunKeyDown(iIndex);
}

void CPythonApplication::OnKeyUp(int iIndex)
{
	UI::CWindowManager& rkWndMgr = UI::CWindowManager::Instance();
	rkWndMgr.RunKeyUp(iIndex);
}

void CPythonApplication::RunIMEUpdate()
{
	UI::CWindowManager& rkWndMgr = UI::CWindowManager::Instance();
	rkWndMgr.RunIMEUpdate();
}
void CPythonApplication::RunIMETabEvent()
{
	UI::CWindowManager& rkWndMgr = UI::CWindowManager::Instance();
	rkWndMgr.RunIMETabEvent();
}
void CPythonApplication::RunIMEReturnEvent()
{
	UI::CWindowManager& rkWndMgr = UI::CWindowManager::Instance();
	rkWndMgr.RunIMEReturnEvent();
}
void CPythonApplication::OnIMEKeyDown(int iIndex)
{
	UI::CWindowManager& rkWndMgr = UI::CWindowManager::Instance();
	rkWndMgr.RunIMEKeyDown(iIndex);
}
/////////////////////////////

void CPythonApplication::RunIMEChangeCodePage()
{
	UI::CWindowManager& rkWndMgr = UI::CWindowManager::Instance();
	rkWndMgr.RunChangeCodePage();
}
void CPythonApplication::RunIMEOpenCandidateListEvent()
{
	UI::CWindowManager& rkWndMgr = UI::CWindowManager::Instance();
	rkWndMgr.RunOpenCandidate();
}
void CPythonApplication::RunIMECloseCandidateListEvent()
{
	UI::CWindowManager& rkWndMgr = UI::CWindowManager::Instance();
	rkWndMgr.RunCloseCandidate();
}
void CPythonApplication::RunIMEOpenReadingWndEvent()
{
	UI::CWindowManager& rkWndMgr = UI::CWindowManager::Instance();
	rkWndMgr.RunOpenReading();
}
void CPythonApplication::RunIMECloseReadingWndEvent()
{
	UI::CWindowManager& rkWndMgr = UI::CWindowManager::Instance();
	rkWndMgr.RunCloseReading();
}

/////////////////////////////
void CPythonApplication::RunPressExitKey()
{
	UI::CWindowManager& rkWndMgr = UI::CWindowManager::Instance();
	rkWndMgr.RunPressExitKey();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void CPythonApplication::OnMouseUpdate()
{
#ifdef _DEBUG
	if (!m_poMouseHandler)
	{
		//assert(!" CPythonApplication::OnMouseUpdate - Mouse handler has not set!");
		return;
	}
#endif _DEBUG

	UI::CWindowManager& rkWndMgr = UI::CWindowManager::Instance();
	long lx, ly;
	rkWndMgr.GetMousePosition(lx, ly);
	PyCallClassMemberFunc(m_poMouseHandler, "Update", Py_BuildValue("(ii)", lx, ly));
}

void CPythonApplication::OnMouseRender()
{
#ifdef _DEBUG
	if (!m_poMouseHandler)
	{
		//assert(!" CPythonApplication::OnMouseRender - Mouse handler has not set!");
		return;
	}
#endif _DEBUG

	PyCallClassMemberFunc(m_poMouseHandler, "Render", Py_BuildValue("()"));
}
#ifdef __ANDROID__
static int AndroidKeyCodeToDIK(int keyCode)
{
	static const unsigned char c_aLetterDIK[26] = {
		DIK_A, DIK_B, DIK_C, DIK_D, DIK_E, DIK_F, DIK_G, DIK_H, DIK_I, DIK_J, DIK_K, DIK_L, DIK_M,
		DIK_N, DIK_O, DIK_P, DIK_Q, DIK_R, DIK_S, DIK_T, DIK_U, DIK_V, DIK_W, DIK_X, DIK_Y, DIK_Z };
	static const unsigned char c_aDigitDIK[10] = {
		DIK_0, DIK_1, DIK_2, DIK_3, DIK_4, DIK_5, DIK_6, DIK_7, DIK_8, DIK_9 };

	if (keyCode >= 29 && keyCode <= 54)
		return c_aLetterDIK[keyCode - 29];
	if (keyCode >= 7 && keyCode <= 16)
		return c_aDigitDIK[keyCode - 7];
	if (keyCode >= 131 && keyCode <= 140)
		return DIK_F1 + (keyCode - 131);

	switch (keyCode)
	{
	case 4: case 111: return DIK_ESCAPE;
	case 19: return DIK_UP;
	case 20: return DIK_DOWN;
	case 21: return DIK_LEFT;
	case 22: return DIK_RIGHT;
	case 61: return DIK_TAB;
	case 62: return DIK_SPACE;
	case 66: case 160: return DIK_RETURN;
	case 67: return DIK_BACK;
	case 112: return DIK_DELETE;
	case 59: return DIK_LSHIFT;
	case 60: return DIK_RSHIFT;
	case 113: return DIK_LCONTROL;
	case 114: return DIK_RCONTROL;
	case 57: return DIK_LMENU;
	case 122: return DIK_HOME;
	case 123: return DIK_END;
	}
	return 0;
}

static int AndroidKeyCodeToVK(int keyCode)
{
	switch (keyCode)
	{
	case 19: return VK_UP;
	case 20: return VK_DOWN;
	case 21: return VK_LEFT;
	case 22: return VK_RIGHT;
	case 112: return VK_DELETE;
	case 122: return VK_HOME;
	case 123: return VK_END;
	}
	return 0;
}

void CPythonApplication::OnAndroidKeyEvent(int action, int keyCode, int unicodeChar)
{
	int iDIK = AndroidKeyCodeToDIK(keyCode);

	if (action == 1)
	{
		if (iDIK)
			KeyUp(iDIK);
		return;
	}

	int iChar = unicodeChar;
	if (keyCode == 66 || keyCode == 160)
		iChar = VK_RETURN;
	else if (keyCode == 67)
		iChar = VK_BACK;
	else if (keyCode == 61)
		iChar = VK_TAB;
	else if (keyCode == 4 || keyCode == 111)
		iChar = VK_ESCAPE;

	if (iChar)
		CPythonIME::Instance().WMChar(NULL, WM_CHAR, iChar, 0);

	int iVK = AndroidKeyCodeToVK(keyCode);
	if (iVK)
		OnIMEKeyDown(iVK);

	if (iDIK)
		KeyDown(iDIK);
}

void AndroidSetGameControlsVisible(bool bVisible);

namespace
{
	const DWORD c_dwTouchHoldMs = 350;

	bool IsTouchOnWorld()
	{
		if (!CPythonBackground::Instance().IsMapReady())
			return false;
		UI::CWindow* pWindow = UI::CWindowManager::Instance().GetPointWindow();
		return pWindow && 0 == strcmp(pWindow->GetName(), "game");
	}
}

// Touches on UI windows behave like the left mouse button. On the world, a quick tap is a
// click, holding still is a held click (walk/attack), and dragging rotates the camera like
// the right mouse button does on desktop.
void CPythonApplication::OnTouchEvent(int action, int x, int y)
{
	extern int g_iAndroidSurfaceWidth;
	extern int g_iAndroidSurfaceHeight;
	if (g_iAndroidSurfaceWidth > 0 && g_iAndroidSurfaceHeight > 0 && m_dwWidth && m_dwHeight)
	{
		x = x * (int)m_dwWidth / g_iAndroidSurfaceWidth;
		y = y * (int)m_dwHeight / g_iAndroidSurfaceHeight;
	}
	extern volatile int g_iAndroidCursorX;
	extern volatile int g_iAndroidCursorY;
	g_iAndroidCursorX = x;
	g_iAndroidCursorY = y;

	if (action == TOUCH_SYNTHETIC_DOWN)
	{
		OnMouseMove(x, y);
		OnMouseLeftButtonDown(x, y);
		return;
	}
	if (action == TOUCH_SYNTHETIC_UP)
	{
		OnMouseMove(x, y);
		OnMouseLeftButtonUp(x, y);
		return;
	}

	const int iSlop = (int)m_dwHeight / 40 + 1;
	switch (action)
	{
	case 0:
		OnMouseMove(x, y);
		m_iTouchStartX = m_iTouchLastX = x;
		m_iTouchStartY = m_iTouchLastY = y;
		m_dwTouchStartTime = ELTimer_GetMSec();
		if (IsTouchOnWorld())
		{
			m_eTouchMode = TOUCH_WORLD_PENDING;
			return;
		}
		m_eTouchMode = TOUCH_UI;
		OnMouseLeftButtonDown(x, y);
		return;

	case 2:
		if (m_eTouchMode == TOUCH_WORLD_PENDING && (abs(x - m_iTouchStartX) > iSlop || abs(y - m_iTouchStartY) > iSlop))
			m_eTouchMode = TOUCH_CAMERA;
		if (m_eTouchMode == TOUCH_CAMERA)
		{
			CCamera* pkCmrCur = CCameraManager::Instance().GetCurrentCamera();
			if (pkCmrCur)
				pkCmrCur->DragBy(x - m_iTouchLastX, y - m_iTouchLastY);
			m_iTouchLastX = x;
			m_iTouchLastY = y;
			return;
		}
		if (m_eTouchMode == TOUCH_WORLD_PENDING)
			return;
		OnMouseMove(x, y);
		return;

	case 1:
	case 3:
	{
		ETouchMode eMode = m_eTouchMode;
		m_eTouchMode = TOUCH_NONE;
		if (eMode == TOUCH_WORLD_PENDING && action == 1)
		{
			PushTouchEvent(TOUCH_SYNTHETIC_DOWN, m_iTouchStartX * g_iAndroidSurfaceWidth / (int)m_dwWidth, m_iTouchStartY * g_iAndroidSurfaceHeight / (int)m_dwHeight);
			PushTouchEvent(TOUCH_SYNTHETIC_UP, m_iTouchStartX * g_iAndroidSurfaceWidth / (int)m_dwWidth, m_iTouchStartY * g_iAndroidSurfaceHeight / (int)m_dwHeight);
			return;
		}
		if (eMode == TOUCH_CAMERA || eMode == TOUCH_WORLD_PENDING)
			return;
		OnMouseMove(x, y);
		OnMouseLeftButtonUp(x, y);
		return;
	}
	}
}

void CPythonApplication::OnAndroidFrame()
{
	if (m_eTouchMode == TOUCH_WORLD_PENDING && ELTimer_GetMSec() - m_dwTouchStartTime >= c_dwTouchHoldMs)
	{
		m_eTouchMode = TOUCH_WORLD_HOLD;
		OnMouseMove(m_iTouchStartX, m_iTouchStartY);
		OnMouseLeftButtonDown(m_iTouchStartX, m_iTouchStartY);
	}

	bool bVisible = CPythonBackground::Instance().IsMapReady() && CPythonPlayer::Instance().GetMainCharacterIndex() != 0;
	if (bVisible != m_bGameControlsVisible)
	{
		m_bGameControlsVisible = bVisible;
		AndroidSetGameControlsVisible(bVisible);
	}
}
#endif

