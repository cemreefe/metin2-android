#ifdef __ANDROID__
#include "../EterLib/StdAfx.h"
#include "../EterLib/IME.h"

CIME::CIME() { m_max = 0; m_userMax = 0; m_bOnlyNumberMode = FALSE; m_bEnablePaste = false; m_bUseDefaultIME = false; m_hOrgIMC = 0; }
CIME::~CIME() {}

bool CIME::Initialize(HWND hWnd) { ms_hWnd = hWnd; return true; }
void CIME::Uninitialize(void) {}
void CIME::Clear() {}
void CIME::SetMax(int iMax) { m_max = iMax; }
void CIME::SetUserMax(int iMax) { m_userMax = iMax; }
void CIME::SetText(const char*, int) {}
int  CIME::GetText(std::string& rstrText, bool) { rstrText.clear(); return 0; }
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
void CIME::EnableCaptureInput() { ms_bCaptureInput = true; }
void CIME::DisableCaptureInput() { ms_bCaptureInput = false; }
bool CIME::IsCaptureEnabled() { return ms_bCaptureInput; }
void CIME::SetNumberMode() { m_bOnlyNumberMode = TRUE; }
void CIME::SetStringMode() { m_bOnlyNumberMode = FALSE; }
bool CIME::__IsWritable(wchar_t) { return true; }
void CIME::AddExceptKey(wchar_t key) { m_exceptKey.push_back(key); }
void CIME::ClearExceptKey() { m_exceptKey.clear(); }
void CIME::PasteTextFromClipBoard() {}
void CIME::EnablePaste(bool bFlag) { m_bEnablePaste = bFlag; }
void CIME::PasteString(const char*) {}
void CIME::FinalizeString(bool) {}
void CIME::UseDefaultIME() { m_bUseDefaultIME = true; }
int  CIME::GetCurPos() { return ms_curpos; }
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
LRESULT CIME::WMChar(HWND, UINT, WPARAM wParam, LPARAM) { if (ms_pEvent) ms_pEvent->OnWM_CHAR(wParam, 0); return 0; }
void CIME::IncCurPos() { ++ms_curpos; }
void CIME::DecCurPos() { --ms_curpos; }
void CIME::SetCurPos(int offset) { ms_curpos = offset; }
void CIME::DelCurPos() {}
void CIME::CheckInputLocale() {}
void CIME::CheckToggleState() {}
void CIME::SetSupportLevel(DWORD) {}
void CIME::InsertString(wchar_t*, int) {}
void CIME::OnChar(wchar_t) {}
UINT CIME::GetCodePageFromLang(LANGID) { return 949; }
void CIME::ResultProcess(HIMC) {}
void CIME::CompositionProcessBuilding(HIMC) {}
void CIME::CompositionProcess(HIMC) {}
void CIME::AttributeProcess(HIMC) {}
void CIME::CandidateProcess(HIMC) {}
void CIME::ReadingProcess(HIMC) {}
bool CIME::IsMax(const wchar_t*, int) { return false; }
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
