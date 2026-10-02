#ifdef __ANDROID__
#include "../EterLib/StdAfx.h"
#include "../EterLib/IME.h"
#include "../EterLib/TextTag.h"
#include <algorithm>
#include <wctype.h>

extern DWORD GetDefaultCodePage();

bool CIME::Initialize(HWND hWnd)
{
	ms_hWnd = hWnd;
	ms_uInputCodePage = ms_uOutputCodePage = GetDefaultCodePage();
	return true;
}

void CIME::Clear()
{
	ms_lastpos = 0;
	ms_curpos = 0;
	ms_compLen = 0;
	ms_ulbegin = 0;
	ms_ulend = 0;
}

void CIME::SetText(const char* c_szText, int len)
{
	ms_compLen = 0;
	ms_ulbegin = 0;
	ms_ulend = 0;
	ms_lastpos = len > 0 ? MultiByteToWideChar(ms_uInputCodePage, 0, c_szText, len, m_wText, IMESTR_MAXLEN) : 0;
	if (ms_lastpos < 0)
		ms_lastpos = 0;
	ms_curpos = min(ms_curpos, ms_lastpos);
}

int CIME::GetText(std::string& rstrText, bool)
{
	char text[IMESTR_MAXLEN * 4];
	int len = WideCharToMultiByte(ms_uOutputCodePage, 0, m_wText, ms_lastpos, text, sizeof(text), NULL, NULL);
	if (len > 0)
		rstrText.append(text, text + len);
	return rstrText.size();
}

void CIME::EnableCaptureInput()
{
	ms_bCaptureInput = true;
}

void CIME::DisableCaptureInput()
{
	ms_bCaptureInput = false;
}

bool CIME::__IsWritable(wchar_t key)
{
	return m_exceptKey.end() == std::find(m_exceptKey.begin(), m_exceptKey.end(), key);
}

void CIME::PasteString(const char* str)
{
	wchar_t wText[IMESTR_MAXLEN];
	int wLen = MultiByteToWideChar(ms_uInputCodePage, 0, str, strlen(str), wText, IMESTR_MAXLEN);
	if (wLen <= 0)
		return;
	InsertString(wText, wLen);
	if (ms_pEvent)
		ms_pEvent->OnUpdate();
}

int CIME::GetCurPos()
{
	return GetTextTagOutputLen(m_wText, ms_curpos);
}

LRESULT CIME::WMChar(HWND, UINT, WPARAM wParam, LPARAM lParam)
{
	wchar_t c = (wchar_t)wParam;
	if (c == 8)
	{
		if (!ms_bCaptureInput)
			return 0;
		if (ms_curpos > 0)
		{
			DecCurPos();
			DelCurPos();
		}
		if (ms_pEvent)
			ms_pEvent->OnUpdate();
		return 0;
	}

	if (c < 0x80 && ms_pEvent && ms_pEvent->OnWM_CHAR(wParam, lParam))
		return 0;
	if (!ms_bCaptureInput)
		return 0;

	OnChar(c);
	if (c == L'|')
		OnChar(c);
	if (ms_pEvent)
		ms_pEvent->OnUpdate();
	return 0;
}

void CIME::IncCurPos()
{
	if (ms_curpos < ms_lastpos)
	{
		int pos = FindColorTagEndPosition(m_wText + ms_curpos, ms_lastpos - ms_curpos);
		if (pos > 0)
			ms_curpos = min(ms_lastpos, max(0, ms_curpos + (pos + 1)));
		else
			++ms_curpos;
	}
}

void CIME::DecCurPos()
{
	if (ms_curpos > 0)
	{
		int pos = FindColorTagStartPosition(m_wText + ms_curpos - 1, ms_curpos);
		if (pos > 0)
			ms_curpos = min(ms_lastpos, max(0, ms_curpos - (pos + 1)));
		else
			--ms_curpos;
	}
}

void CIME::SetCurPos(int offset)
{
	if (offset < 0 || offset > ms_lastpos)
		ms_curpos = ms_lastpos;
	else
		ms_curpos = min(ms_lastpos, GetTextTagInternalPosFromRenderPos(m_wText, ms_lastpos, offset));
}

void CIME::DelCurPos()
{
	if (ms_curpos < ms_lastpos)
	{
		int eraseCount = FindColorTagEndPosition(m_wText + ms_curpos, ms_lastpos - ms_curpos) + 1;
		memmove(m_wText + ms_curpos, m_wText + ms_curpos + eraseCount, sizeof(wchar_t) * (ms_lastpos - ms_curpos - eraseCount));
		ms_lastpos -= eraseCount;
		ms_curpos = min(ms_lastpos, ms_curpos);
		m_wText[ms_lastpos] = 0;
	}
}

void CIME::InsertString(wchar_t* wString, int iSize)
{
	if (IsMax(wString, iSize))
		return;
	if (ms_curpos < ms_lastpos)
		memmove(m_wText + ms_curpos + iSize, m_wText + ms_curpos, sizeof(wchar_t) * (ms_lastpos - ms_curpos));
	memcpy(m_wText + ms_curpos, wString, sizeof(wchar_t) * iSize);
	ms_curpos += iSize;
	ms_lastpos += iSize;
	m_wText[ms_lastpos] = 0;
}

void CIME::OnChar(wchar_t c)
{
	if (m_bOnlyNumberMode && !iswdigit(c))
		return;
	if ((c >= 0x00 && c <= 0x1f) || c == 0x7f)
		return;
	if (!__IsWritable(c))
		return;
	InsertString(&c, 1);
}

bool CIME::IsMax(const wchar_t* wInput, int len)
{
	if (ms_lastpos + len >= IMESTR_MAXLEN)
		return true;
	int textLen = WideCharToMultiByte(ms_uOutputCodePage, 0, m_wText, ms_lastpos, 0, 0, NULL, NULL);
	int inputLen = WideCharToMultiByte(ms_uOutputCodePage, 0, wInput, len, 0, 0, NULL, NULL);
	if (textLen + inputLen > m_max)
		return true;
	if (m_userMax != 0 && m_max != m_userMax)
	{
		std::wstring str = GetTextTagOutputString(m_wText, ms_lastpos);
		std::wstring input = GetTextTagOutputString(wInput, len);
		textLen = WideCharToMultiByte(ms_uOutputCodePage, 0, str.c_str(), str.length(), 0, 0, NULL, NULL);
		inputLen = WideCharToMultiByte(ms_uOutputCodePage, 0, input.c_str(), input.length(), 0, 0, NULL, NULL);
		return textLen + inputLen > m_userMax;
	}
	return false;
}

CIME::CIME() { m_max = 0; m_userMax = 0; m_bOnlyNumberMode = FALSE; m_bEnablePaste = false; m_bUseDefaultIME = false; m_hOrgIMC = 0; }
CIME::~CIME() {}

void CIME::Uninitialize(void) {}
void CIME::SetMax(int iMax) { m_max = iMax; }
void CIME::SetUserMax(int iMax) { m_userMax = iMax; }
const char* CIME::GetCodePageText() { return ""; }
int  CIME::GetCodePage() { return 949; }
int  CIME::GetCandidateCount() { return 0; }
int  CIME::GetCandidatePageCount() { return 0; }
int  CIME::GetCandidate(DWORD, std::string&) { return 0; }
int  CIME::GetCandidateSelection() { return -1; }
int  CIME::GetReading(std::string&) { return 0; }
int  CIME::GetReadingError() { return 0; }
void CIME::SetInputMode(DWORD) {}
DWORD CIME::GetInputMode() { return 0; }
bool CIME::IsIMEEnabled() { return ms_bImeEnabled; }
void CIME::EnableIME(bool bEnable) { ms_bImeEnabled = bEnable; }
void CIME::DisableIME() { ms_bImeEnabled = false; }
bool CIME::IsCaptureEnabled() { return ms_bCaptureInput; }
void CIME::SetNumberMode() { m_bOnlyNumberMode = TRUE; }
void CIME::SetStringMode() { m_bOnlyNumberMode = FALSE; }
void CIME::AddExceptKey(wchar_t key) { m_exceptKey.push_back(key); }
void CIME::ClearExceptKey() { m_exceptKey.clear(); }
void CIME::PasteTextFromClipBoard() {}
void CIME::EnablePaste(bool bFlag) { m_bEnablePaste = bFlag; }
void CIME::FinalizeString(bool) {}
void CIME::UseDefaultIME() { m_bUseDefaultIME = true; }
int  CIME::GetCompLen() { return ms_compLen; }
int  CIME::GetULBegin() { return ms_ulbegin; }
int  CIME::GetULEnd() { return ms_ulend; }
void CIME::CloseCandidateList() {}
void CIME::CloseReadingInformation() {}
void CIME::ChangeInputLanguage() {}
void CIME::ChangeInputLanguageWorker() {}
LRESULT CIME::WMInputLanguage(HWND, UINT, WPARAM, LPARAM) { return 0; }
LRESULT CIME::WMStartComposition(HWND, UINT, WPARAM, LPARAM) { return 0; }
LRESULT CIME::WMComposition(HWND, UINT, WPARAM, LPARAM) { return 0; }
LRESULT CIME::WMEndComposition(HWND, UINT, WPARAM, LPARAM) { return 0; }
LRESULT CIME::WMNotify(HWND, UINT, WPARAM, LPARAM) { return 0; }
void CIME::CheckInputLocale() {}
void CIME::CheckToggleState() {}
void CIME::SetSupportLevel(DWORD) {}
UINT CIME::GetCodePageFromLang(LANGID) { return 949; }
void CIME::ResultProcess(HIMC) {}
void CIME::CompositionProcessBuilding(HIMC) {}
void CIME::CompositionProcess(HIMC) {}
void CIME::AttributeProcess(HIMC) {}
void CIME::CandidateProcess(HIMC) {}
void CIME::ReadingProcess(HIMC) {}
DWORD CIME::GetImeId(UINT) { return 0; }
bool CIME::GetReadingWindowOrientation() { return false; }
void CIME::SetupImeApi() {}

bool CIME::ms_bInitialized = false;
bool CIME::ms_bDisableIMECompletely = false;
bool CIME::ms_bUILessMode = false;
bool CIME::ms_bImeEnabled = false;
bool CIME::ms_bCaptureInput = false;
bool CIME::ms_bChineseIME = false;
bool CIME::ms_bUseIMMCandidate = false;
HWND CIME::ms_hWnd = NULL;
HKL CIME::ms_hklCurrent = 0;
char CIME::ms_szKeyboardLayout[KL_NAMELENGTH + 1] = {0};
OSVERSIONINFOA CIME::ms_stOSVI = {0};
HINSTANCE CIME::ms_hImm32Dll = NULL;
HINSTANCE CIME::ms_hCurrentImeDll = NULL;
DWORD CIME::ms_dwImeState = 0;
DWORD CIME::ms_adwId[2] = {0, 0};
DWORD CIME::ms_dwIMELevel = 0;
DWORD CIME::ms_dwIMELevelSaved = 0;
bool CIME::ms_bCandidateList = false;
DWORD CIME::ms_dwCandidateCount = 0;
bool CIME::ms_bVerticalCandidate = false;
int  CIME::ms_iCandListIndexBase = 0;
WCHAR CIME::ms_wszCandidate[CIME::MAX_CANDLIST][CIME::MAX_CANDIDATE_LENGTH] = {{0}};
DWORD CIME::ms_dwCandidateSelection = 0;
DWORD CIME::ms_dwCandidatePageSize = 0;
bool CIME::ms_bReadingInformation = false;
int  CIME::ms_iReadingError = 0;
bool CIME::ms_bHorizontalReading = false;
vector<wchar_t> CIME::ms_wstrReading;
wchar_t* CIME::ms_wszCurrentIndicator = NULL;
IIMEEventSink* CIME::ms_pEvent = NULL;
wchar_t CIME::m_wText[IMESTR_MAXLEN] = {0};
int CIME::ms_compLen = 0;
int CIME::ms_curpos = 0;
int CIME::ms_lastpos = 0;
int CIME::ms_ulbegin = 0;
int CIME::ms_ulend = 0;
UINT CIME::ms_uOutputCodePage = 949;
UINT CIME::ms_uInputCodePage = 949;
INPUTCONTEXT* (WINAPI* CIME::_ImmLockIMC)(HIMC) = NULL;
BOOL (WINAPI* CIME::_ImmUnlockIMC)(HIMC) = NULL;
LPVOID (WINAPI* CIME::_ImmLockIMCC)(HIMCC) = NULL;
BOOL (WINAPI* CIME::_ImmUnlockIMCC)(HIMCC) = NULL;
UINT (WINAPI* CIME::_GetReadingString)(HIMC, UINT, LPWSTR, PINT, BOOL*, PUINT) = NULL;
BOOL (WINAPI* CIME::_ShowReadingWindow)(HIMC, BOOL) = NULL;
#endif
