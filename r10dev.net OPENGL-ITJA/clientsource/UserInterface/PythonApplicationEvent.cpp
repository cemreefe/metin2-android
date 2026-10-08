#include "StdAfx.h"
#include "PythonApplication.h"
#include "../EterLib/GrpText.h"
#include "../EterLib/GrpTextInstance.h"
#include "../EterLib/ResourceManager.h"
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
#ifdef M2_PORT
void CPythonApplication::OnPortKeyEvent(int action, int keyCode, int unicodeChar)
{
	int iDIK = M2Plat::KeyToDIK(keyCode);
	if (action == 1)
	{
		if (iDIK)
			KeyUp(iDIK);
		return;
	}

	// Control keys come through as VK_* so WMChar and the IME see the same
	// values Windows would send; printable keys arrive in unicodeChar.
	int iVK = M2Plat::KeyToVK(keyCode);
	int iChar = iVK ? iVK : unicodeChar;

	if (unicodeChar && !iVK)
		FeedDevSequence(unicodeChar);

	if (iChar)
		CPythonIME::Instance().WMChar(NULL, WM_CHAR, iChar, 0);

	if (iVK)
		OnIMEKeyDown(iVK);

	if (iDIK)
		KeyDown(iDIK);
}

namespace
{
	const DWORD c_dwTouchHoldMs = 350;

	void CollectTouchBlockers(UI::CWindow* pWindow, std::vector<M2Plat::SRect>& rects)
	{
		for (UI::CWindow* pChild : pWindow->GetChildren())
		{
			if (!pChild->IsShow() || 0 == strcmp(pChild->GetName(), "game"))
				continue;
			if (pChild->IsFlag(UI::CWindow::FLAG_NOT_PICK) || pChild->IsFlag(UI::CWindow::FLAG_IGNORE_SIZE))
				CollectTouchBlockers(pChild, rects);
			else
			{
				const RECT& r = pChild->GetRect();
				rects.push_back({ r.left, r.top, r.right, r.bottom });
			}
		}
	}

	bool IsTouchOnWorld()
	{
		if (!CPythonBackground::Instance().IsMapReady())
			return false;
		UI::CWindow* pWindow = UI::CWindowManager::Instance().GetPointWindow();
		return pWindow && 0 == strcmp(pWindow->GetName(), "game");
	}
}

void CPythonApplication::ReloadPortFonts()
{
	CResourceManager::Instance().ReloadResourcesOfType(CGraphicText::Type());
	CGraphicTextInstance::RefreshAll();
}

// Typing "devops$" anywhere toggles the hidden developer options window.
void CPythonApplication::FeedDevSequence(int iChar)
{
	static const char c_szSequence[] = "devops$";
	static size_t s_nMatched = 0;
	if (iChar == c_szSequence[s_nMatched])
		++s_nMatched;
	else
		s_nMatched = (iChar == c_szSequence[0]) ? 1 : 0;
	if (c_szSequence[s_nMatched])
		return;
	s_nMatched = 0;
	PyRun_SimpleString("import uidevoptions\nuidevoptions.Toggle()\n");
}

// Re-reads display.cfg and resizes the logical UI canvas in place; Python then rebuilds
// the windows that cached the old screen size.
void CPythonApplication::ApplyPortUIScale()
{
	const float fOldFontScale = CGraphicText::GetGlobalFontScale();
	const float fOldRasterScale = CGraphicText::GetRasterScale();
	m_pySystem.FitUIToPortSurface();
	if (CGraphicText::GetGlobalFontScale() != fOldFontScale || CGraphicText::GetRasterScale() != fOldRasterScale)
	{
		CResourceManager::Instance().ReloadResourcesOfType(CGraphicText::Type());
		CGraphicTextInstance::RefreshAll();
	}
	const int iWidth = m_pySystem.GetWidth();
	const int iHeight = m_pySystem.GetHeight();
	m_dwWidth = iWidth;
	m_dwHeight = iHeight;
	AdjustSize(iWidth, iHeight);
	m_grpDevice.FitViewportToSurface(M2Plat::SurfaceWidth(), M2Plat::SurfaceHeight());
	CGraphicBase::SetLogicalScreenSize(iWidth, iHeight);
	UI::CWindowManager& rkWndMgr = UI::CWindowManager::Instance();
	rkWndMgr.SetResolution(iWidth, iHeight);
	rkWndMgr.SetScreenSize(iWidth, iHeight);
}

// Touches on UI windows behave like the left mouse button. On the world, a quick tap is a
// click, holding still is a held click (walk/attack), and dragging rotates the camera like
// the right mouse button does on desktop.
void CPythonApplication::OnTouchEvent(int action, int x, int y)
{
	const int iSurfW = M2Plat::SurfaceWidth();
	const int iSurfH = M2Plat::SurfaceHeight();
	if (action == TOUCH_WHEEL)
	{
		OnMouseWheel(x);
		return;
	}
	if (iSurfW > 0 && iSurfH > 0 && m_dwWidth && m_dwHeight)
	{
		x = x * (int)m_dwWidth / iSurfW;
		y = y * (int)m_dwHeight / iSurfH;
	}
	extern volatile int g_iPortCursorX;
	extern volatile int g_iPortCursorY;
	extern volatile int g_iPortCursorWarpX;
	extern volatile int g_iPortCursorWarpY;
	if (action != TOUCH_RIGHT_DOWN)
	{
		x += g_iPortCursorWarpX;
		y += g_iPortCursorWarpY;
	}
	g_iPortCursorX = x;
	g_iPortCursorY = y;

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
	// Right-button events exist so desktop-web mice keep the camera-drag button.
	if (action == TOUCH_RIGHT_DOWN)
	{
		OnMouseMove(x, y);
		OnMouseRightButtonDown(x, y);
		return;
	}
	if (action == TOUCH_RIGHT_UP)
	{
		OnMouseMove(x, y);
		OnMouseRightButtonUp(x, y);
		g_iPortCursorX -= g_iPortCursorWarpX;
		g_iPortCursorY -= g_iPortCursorWarpY;
		g_iPortCursorWarpX = g_iPortCursorWarpY = 0;
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
		{
			m_eTouchMode = TOUCH_CAMERA;
			m_fTouchCameraSensitivity = fMAX(0.25f, fMIN(2.0f, CPythonSystem::GetPortDisplayConfig("camera_sensitivity", 1.0f)));
		}
		if (m_eTouchMode == TOUCH_CAMERA)
		{
			m_fTouchCameraDX += (x - m_iTouchLastX) * m_fTouchCameraSensitivity;
			m_fTouchCameraDY += (y - m_iTouchLastY) * m_fTouchCameraSensitivity;
			m_iTouchLastX = x;
			m_iTouchLastY = y;
			return;
		}
		if (m_eTouchMode == TOUCH_WORLD_PENDING)
			return;
		// While a mouse camera drag is active, leave the move to the per-frame
		// cursor poll (as on Win32): a second Drag() from the event would apply
		// the delta and the poll would then zero it before the camera updates.
		if (CCamera* pkCmrDrag = CCameraManager::Instance().GetCurrentCamera())
			if (pkCmrDrag->IsDraging())
				return;
		OnMouseMove(x, y);
		return;

	case 1:
	case 3:
	{
		ETouchMode eMode = m_eTouchMode;
		m_eTouchMode = TOUCH_NONE;
		m_fTouchCameraDX = m_fTouchCameraDY = 0.0f;
		if (eMode == TOUCH_WORLD_PENDING && action == 1)
		{
			PushTouchEvent(TOUCH_SYNTHETIC_DOWN, m_iTouchStartX * iSurfW / (int)m_dwWidth, m_iTouchStartY * iSurfH / (int)m_dwHeight);
			PushTouchEvent(TOUCH_SYNTHETIC_UP, m_iTouchStartX * iSurfW / (int)m_dwWidth, m_iTouchStartY * iSurfH / (int)m_dwHeight);
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

void CPythonApplication::OnPortFrame()
{
	// The surface follows the window (browser resize, fullscreen on/off). Once it has
	// settled, re-fit the UI canvas and let Python rebuild the screen-sized windows.
	static int s_iLastSurfW = 0, s_iLastSurfH = 0;
	static DWORD s_dwSurfChangedAt = 0;
	const int iSurfW = M2Plat::SurfaceWidth();
	const int iSurfH = M2Plat::SurfaceHeight();
	if (iSurfW != s_iLastSurfW || iSurfH != s_iLastSurfH)
	{
		if (s_iLastSurfW && s_iLastSurfH)
			s_dwSurfChangedAt = ELTimer_GetMSec() | 1;
		s_iLastSurfW = iSurfW;
		s_iLastSurfH = iSurfH;
		m_grpDevice.FitViewportToSurface(iSurfW, iSurfH);
	}
	else if (s_dwSurfChangedAt && ELTimer_GetMSec() - s_dwSurfChangedAt >= 250)
	{
		s_dwSurfChangedAt = 0;
		PyRun_SimpleString("import uimobilehud\nuimobilehud.OnSurfaceResized()\n");
	}

	// Like the original per-frame cursor drag: the camera turns by exactly this
	// frame's pointer movement, so it stops the frame the pointer stops.
	if (m_eTouchMode == TOUCH_CAMERA)
	{
		if (CCamera* pkCmrCur = CCameraManager::Instance().GetCurrentCamera())
			pkCmrCur->DragBy(m_fTouchCameraDX, m_fTouchCameraDY);
		m_fTouchCameraDX = m_fTouchCameraDY = 0.0f;
	}

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
		M2Plat::SetGameControlsVisible(bVisible);
	}

	std::vector<M2Plat::SRect> blockers;
	UI::CWindowManager& rkWndMgr = UI::CWindowManager::Instance();
	if (rkWndMgr.GetLockWindow())
		blockers.push_back({ 0, 0, (LONG)m_dwWidth, (LONG)m_dwHeight });
	else
		for (UI::CWindow* pLayer : rkWndMgr.GetLayers())
			CollectTouchBlockers(pLayer, blockers);
	M2Plat::SetTouchBlockers(blockers.empty() ? NULL : &blockers[0], (int)blockers.size(), (int)m_dwWidth, (int)m_dwHeight);
}
#endif

