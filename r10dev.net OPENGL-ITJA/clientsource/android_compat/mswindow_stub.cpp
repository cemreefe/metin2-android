#ifdef M2_PORT
#include "../EterLib/MSWindow.h"
#include <set>

CMSWindow::CMSWindow() : m_hWnd(NULL), m_isActive(true), m_isVisible(false)
{
	m_rect.left = m_rect.top = 0;
	m_rect.right = 800; m_rect.bottom = 600;
}
CMSWindow::~CMSWindow() { Destroy(); }

void CMSWindow::Destroy() { m_hWnd = NULL; }
bool CMSWindow::Create(const char*, int, DWORD, DWORD, HICON, int) { m_isActive = true; return true; }

void CMSWindow::Show() { m_isVisible = true; }
void CMSWindow::Hide() { m_isVisible = false; }
void CMSWindow::SetVisibleMode(bool isVisible) { m_isVisible = isVisible; }

void CMSWindow::SetPosition(int x, int y)
{
	int w = m_rect.right - m_rect.left;
	int h = m_rect.bottom - m_rect.top;
	m_rect.left = x; m_rect.top = y;
	m_rect.right = x + w; m_rect.bottom = y + h;
}
void CMSWindow::SetCenterPosition() {}
void CMSWindow::SetText(const char*) {}
void CMSWindow::AdjustSize(int width, int height) { SetSize(width, height); }
void CMSWindow::SetSize(int width, int height)
{
	m_rect.right = m_rect.left + width;
	m_rect.bottom = m_rect.top + height;
}

bool CMSWindow::IsVisible() { return m_isVisible; }
bool CMSWindow::IsActive() { return m_isActive; }

void CMSWindow::GetMousePosition(POINT* ppt) { if (ppt) { ppt->x = 0; ppt->y = 0; } }
void CMSWindow::GetClientRect(RECT* prc) { if (prc) *prc = m_rect; }
void CMSWindow::GetWindowRect(RECT* prc) { if (prc) *prc = m_rect; }

int CMSWindow::GetScreenWidth() { return m_rect.right - m_rect.left; }
int CMSWindow::GetScreenHeight() { return m_rect.bottom - m_rect.top; }

HWND CMSWindow::GetWindowHandle() { return m_hWnd; }
HINSTANCE CMSWindow::GetInstance() { return ms_hInstance; }

LRESULT CMSWindow::WindowProcedure(HWND, UINT, WPARAM, LPARAM) { return 0; }
void CMSWindow::OnSize(WPARAM, LPARAM) {}

const char* CMSWindow::RegisterWindowClass(DWORD, int, WNDPROC, HICON, int) { return "metin2"; }

CMSWindow::TWindowClassSet CMSWindow::ms_stWCSet;
HINSTANCE CMSWindow::ms_hInstance = NULL;
#endif
