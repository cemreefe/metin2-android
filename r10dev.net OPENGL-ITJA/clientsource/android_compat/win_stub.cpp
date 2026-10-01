// Win32 API stubs/implementations for Android.
// Anything that must actually work (time, file IO, sockets, critical sections)
// is implemented; pure UI/DCOM APIs are inert stubs.

#include "windows.h"
#include "io.h"
#include <pthread.h>
#include <android/log.h>
#include <mutex>
#include <fnmatch.h>
#include <dirent.h>
#include <vector>
#include <string>
#include <unordered_map>

#define LOG_TAG "Metin2WinStub"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

extern "C" {

DWORD GetTickCount(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (DWORD)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}


void Sleep(DWORD dwMilliseconds) { usleep(dwMilliseconds * 1000); }

static DWORD s_lastError = 0;
DWORD GetLastError(void) { return s_lastError; }
void SetLastError(DWORD dwErrCode) { s_lastError = dwErrCode; }

void OutputDebugStringA(LPCSTR lpOutputString) {
    if (lpOutputString) LOGI("%s", lpOutputString);
}

int MessageBoxA(HWND, LPCSTR lpText, LPCSTR lpCaption, UINT) {
    LOGW("MessageBox: %s: %s", lpCaption ? lpCaption : "", lpText ? lpText : "");
    return IDOK;
}

BOOL CloseHandle(HANDLE hObject) { return hObject ? TRUE : FALSE; }

DWORD GetCurrentDirectoryA(DWORD nBufferLength, LPSTR lpBuffer) {
    if (!getcwd(lpBuffer, nBufferLength)) return 0;
    return (DWORD)strlen(lpBuffer);
}

BOOL SetCurrentDirectoryA(LPCSTR lpPathName) {
    return chdir(lpPathName) == 0 ? TRUE : FALSE;
}

DWORD GetFullPathNameA(LPCSTR lpFileName, DWORD nBufferLength, LPSTR lpBuffer, LPSTR* lpFilePart) {
    if (lpFileName[0] == '/' ) {
        strncpy(lpBuffer, lpFileName, nBufferLength);
    } else {
        char cwd[1024];
        if (!getcwd(cwd, sizeof(cwd))) cwd[0] = 0;
        snprintf(lpBuffer, nBufferLength, "%s/%s", cwd, lpFileName);
    }
    if (lpFilePart) {
        char* s = strrchr(lpBuffer, '/');
        *lpFilePart = s ? s + 1 : lpBuffer;
    }
    return (DWORD)strlen(lpBuffer);
}

/* ---- FindFirstFile/FindNextFile over glob ---- */
struct FindHandle {
    std::vector<std::string> paths;
    size_t pos;
};

static void FillFindData(const char* path, LPWIN32_FIND_DATAA fd) {
    memset(fd, 0, sizeof(*fd));
    const char* base = strrchr(path, '/');
    base = base ? base + 1 : path;
    strncpy(fd->cFileName, base, sizeof(fd->cFileName) - 1);
    struct stat st;
    if (stat(path, &st) == 0) {
        fd->nFileSizeLow = (DWORD)st.st_size;
        fd->nFileSizeHigh = (DWORD)(st.st_size >> 32);
        if (S_ISDIR(st.st_mode))
            fd->dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
        if (!(st.st_mode & S_IWUSR))
            fd->dwFileAttributes |= FILE_ATTRIBUTE_READONLY;
    }
}

HANDLE FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData) {
    FindHandle* h = new FindHandle();
    char pattern[1024];
    strncpy(pattern, lpFileName, sizeof(pattern) - 1);
    pattern[sizeof(pattern) - 1] = 0;
    for (char* c = pattern; *c; ++c) if (*c == '\\') *c = '/';
    char* slash = strrchr(pattern, '/');
    const char* dir = ".";
    const char* mask = pattern;
    char dirbuf[1024];
    if (slash) {
        size_t dl = (size_t)(slash - pattern);
        if (dl == 0) dl = 1;
        memcpy(dirbuf, pattern, dl);
        dirbuf[dl] = 0;
        dir = dirbuf;
        mask = slash + 1;
    }
    DIR* dp = opendir(dir);
    if (dp) {
        struct dirent* ent;
        while ((ent = readdir(dp)) != NULL) {
            if (fnmatch(mask, ent->d_name, 0) == 0) {
                char full[1100];
                snprintf(full, sizeof(full), "%s/%s", dir, ent->d_name);
                h->paths.push_back(full);
            }
        }
        closedir(dp);
    } else if (strchr(mask, '*') == NULL && strchr(mask, '?') == NULL) {
        struct stat st;
        if (stat(pattern, &st) == 0) h->paths.push_back(pattern);
    }
    if (h->paths.empty()) {
        delete h;
        return INVALID_HANDLE_VALUE;
    }
    h->pos = 0;
    FillFindData(h->paths[0].c_str(), lpFindFileData);
    h->pos = 1;
    return (HANDLE)h;
}

BOOL FindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData) {
    FindHandle* h = (FindHandle*)hFindFile;
    if (!h || h == (FindHandle*)INVALID_HANDLE_VALUE) return FALSE;
    if (h->pos >= h->paths.size()) return FALSE;
    FillFindData(h->paths[h->pos++].c_str(), lpFindFileData);
    return TRUE;
}

BOOL FindClose(HANDLE hFindFile) {
    FindHandle* h = (FindHandle*)hFindFile;
    if (!h || h == (FindHandle*)INVALID_HANDLE_VALUE) return FALSE;
    delete h;
    return TRUE;
}

/* ---- file API: HANDLE is just an fd ---- */
HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD, LPSECURITY_ATTRIBUTES,
                   DWORD dwCreationDisposition, DWORD, HANDLE) {
    int flags = 0;
    if ((dwDesiredAccess & (GENERIC_READ | GENERIC_WRITE)) == (GENERIC_READ | GENERIC_WRITE))
        flags = O_RDWR;
    else if (dwDesiredAccess & GENERIC_WRITE)
        flags = O_WRONLY;
    else
        flags = O_RDONLY;

    switch (dwCreationDisposition) {
        case CREATE_NEW:     flags |= O_CREAT | O_EXCL; break;
        case CREATE_ALWAYS:  flags |= O_CREAT | O_TRUNC; break;
        case OPEN_ALWAYS:    flags |= O_CREAT; break;
        case TRUNCATE_EXISTING: flags |= O_TRUNC; break;
        case OPEN_EXISTING:
        default: break;
    }
    int fd = open(lpFileName, flags, 0666);
    if (fd < 0) { s_lastError = errno; return INVALID_HANDLE_VALUE; }
    return (HANDLE)(intptr_t)fd;
}

BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
              LPDWORD lpNumberOfBytesRead, LPOVERLAPPED) {
    ssize_t r = read((int)(intptr_t)hFile, lpBuffer, nNumberOfBytesToRead);
    if (r < 0) { s_lastError = errno; return FALSE; }
    if (lpNumberOfBytesRead) *lpNumberOfBytesRead = (DWORD)r;
    return TRUE;
}

BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
               LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED) {
    ssize_t w = write((int)(intptr_t)hFile, lpBuffer, nNumberOfBytesToWrite);
    if (w < 0) { s_lastError = errno; return FALSE; }
    if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = (DWORD)w;
    return TRUE;
}

DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod) {
    int whence = (dwMoveMethod == FILE_BEGIN) ? SEEK_SET : (dwMoveMethod == FILE_END) ? SEEK_END : SEEK_CUR;
    off_t off = lDistanceToMove;
    if (lpDistanceToMoveHigh) off |= ((off_t)*lpDistanceToMoveHigh) << 32;
    off_t r = lseek((int)(intptr_t)hFile, off, whence);
    if (r < 0) { s_lastError = errno; return (DWORD)-1; }
    if (lpDistanceToMoveHigh) *lpDistanceToMoveHigh = (LONG)(r >> 32);
    return (DWORD)r;
}

DWORD GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh) {
    struct stat st;
    if (fstat((int)(intptr_t)hFile, &st) != 0) return (DWORD)-1;
    if (lpFileSizeHigh) *lpFileSizeHigh = (DWORD)(st.st_size >> 32);
    return (DWORD)st.st_size;
}

BOOL GetFileSizeEx(HANDLE hFile, PLARGE_INTEGER lpFileSize) {
    struct stat st;
    if (fstat((int)(intptr_t)hFile, &st) != 0) return FALSE;
    lpFileSize->QuadPart = st.st_size;
    return TRUE;
}

BOOL DeleteFileA(LPCSTR lpFileName) { return unlink(lpFileName) == 0 ? TRUE : FALSE; }

BOOL CreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES) {
    return mkdir(lpPathName, 0777) == 0 ? TRUE : FALSE;
}

BOOL RemoveDirectoryA(LPCSTR lpPathName) { return rmdir(lpPathName) == 0 ? TRUE : FALSE; }
BOOL MoveFileA(LPCSTR a, LPCSTR b) { return rename(a, b) == 0 ? TRUE : FALSE; }

BOOL CopyFileA(LPCSTR a, LPCSTR b, BOOL bFailIfExists) {
    if (bFailIfExists && access(b, F_OK) == 0) return FALSE;
    int in = open(a, O_RDONLY);
    if (in < 0) return FALSE;
    int out = open(b, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (out < 0) { close(in); return FALSE; }
    char buf[65536];
    ssize_t r;
    while ((r = read(in, buf, sizeof(buf))) > 0)
        if (write(out, buf, r) != r) { close(in); close(out); return FALSE; }
    close(in); close(out);
    return r == 0 ? TRUE : FALSE;
}

DWORD GetFileAttributesA(LPCSTR lpFileName) {
    struct stat st;
    if (stat(lpFileName, &st) != 0) return (DWORD)-1;
    DWORD attr = FILE_ATTRIBUTE_NORMAL;
    if (S_ISDIR(st.st_mode)) attr |= FILE_ATTRIBUTE_DIRECTORY;
    if (!(st.st_mode & S_IWUSR)) attr |= FILE_ATTRIBUTE_READONLY;
    return attr;
}

static void tmToSystemTime(const struct tm* t, LPSYSTEMTIME st) {
    st->wYear = (WORD)(t->tm_year + 1900);
    st->wMonth = (WORD)(t->tm_mon + 1);
    st->wDayOfWeek = (WORD)t->tm_wday;
    st->wDay = (WORD)t->tm_mday;
    st->wHour = (WORD)t->tm_hour;
    st->wMinute = (WORD)t->tm_min;
    st->wSecond = (WORD)t->tm_sec;
    st->wMilliseconds = 0;
}

void GetSystemTime(LPSYSTEMTIME lpSystemTime) {
    time_t t = time(NULL);
    struct tm tmv;
    gmtime_r(&t, &tmv);
    tmToSystemTime(&tmv, lpSystemTime);
}

void GetLocalTime(LPSYSTEMTIME lpSystemTime) {
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    tmToSystemTime(&tmv, lpSystemTime);
}

BOOL SystemTimeToFileTime(const SYSTEMTIME*, LPFILETIME lpFileTime) {
    lpFileTime->dwLowDateTime = 0;
    lpFileTime->dwHighDateTime = 0;
    return TRUE;
}

UINT GetSystemDirectoryA(LPSTR lpBuffer, UINT uSize) {
    const char* p = "/system";
    strncpy(lpBuffer, p, uSize);
    return (UINT)strlen(p);
}
UINT GetWindowsDirectoryA(LPSTR lpBuffer, UINT uSize) { return GetSystemDirectoryA(lpBuffer, uSize); }

DWORD GetModuleFileNameA(HMODULE, LPSTR lpFilename, DWORD nSize) {
    const char* p = "metin2_mobile";
    strncpy(lpFilename, p, nSize);
    return (DWORD)strlen(p);
}

HMODULE LoadLibraryA(LPCSTR) { return NULL; }
BOOL FreeLibrary(HMODULE) { return TRUE; }
FARPROC GetProcAddress(HMODULE, LPCSTR) { return NULL; }
HMODULE GetModuleHandleA(LPCSTR) { return NULL; }

void* VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD, DWORD) {
    void* p = malloc(dwSize);
    if (!lpAddress) memset(p, 0, dwSize);
    return p;
}
BOOL VirtualFree(LPVOID lpAddress, SIZE_T, DWORD) { free(lpAddress); return TRUE; }

HGLOBAL GlobalAlloc(UINT uFlags, SIZE_T dwBytes) {
    void* p = malloc(dwBytes);
    if (p) memset(p, 0, dwBytes);
    return (HGLOBAL)p;
}
HGLOBAL GlobalFree(HGLOBAL hMem) { free(hMem); return NULL; }
LPVOID GlobalLock(HGLOBAL hMem) { return (LPVOID)hMem; }
BOOL GlobalUnlock(HGLOBAL) { return TRUE; }
SIZE_T GlobalSize(HGLOBAL) { return 0; }
HLOCAL LocalAlloc(UINT uFlags, SIZE_T uBytes) { return (HLOCAL)GlobalAlloc(uFlags, uBytes); }
HLOCAL LocalFree(HLOCAL hMem) { return (HLOCAL)GlobalFree((HGLOBAL)hMem); }

/* ---- sync objects ---- */
struct MutexBox { pthread_mutex_t m; };
struct EventBox { pthread_mutex_t m; pthread_cond_t c; int signaled; };

HANDLE CreateMutexA(LPSECURITY_ATTRIBUTES, BOOL, LPCSTR) {
    MutexBox* b = new MutexBox();
    pthread_mutex_init(&b->m, NULL);
    return (HANDLE)b;
}
BOOL ReleaseMutex(HANDLE hMutex) {
    if (!hMutex) return FALSE;
    pthread_mutex_unlock(&((MutexBox*)hMutex)->m);
    return TRUE;
}
HANDLE CreateEventA(LPSECURITY_ATTRIBUTES, BOOL bManualReset, BOOL bInitialState, LPCSTR) {
    EventBox* b = new EventBox();
    pthread_mutex_init(&b->m, NULL);
    pthread_cond_init(&b->c, NULL);
    b->signaled = bInitialState ? 1 : 0;
    return (HANDLE)b;
}
BOOL SetEvent(HANDLE hEvent) {
    EventBox* b = (EventBox*)hEvent;
    pthread_mutex_lock(&b->m);
    b->signaled = 1;
    pthread_cond_broadcast(&b->c);
    pthread_mutex_unlock(&b->m);
    return TRUE;
}
BOOL ResetEvent(HANDLE hEvent) {
    EventBox* b = (EventBox*)hEvent;
    pthread_mutex_lock(&b->m);
    b->signaled = 0;
    pthread_mutex_unlock(&b->m);
    return TRUE;
}
DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds) {
    EventBox* b = (EventBox*)hHandle;
    if (!b) return WAIT_TIMEOUT;
    pthread_mutex_lock(&b->m);
    if (b->signaled) { pthread_mutex_unlock(&b->m); return WAIT_OBJECT_0; }
    if (dwMilliseconds == INFINITE) {
        while (!b->signaled) pthread_cond_wait(&b->c, &b->m);
        pthread_mutex_unlock(&b->m);
        return WAIT_OBJECT_0;
    }
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += dwMilliseconds / 1000;
    ts.tv_nsec += (dwMilliseconds % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
    int r = 0;
    while (!b->signaled && r == 0)
        r = pthread_cond_timedwait(&b->c, &b->m, &ts);
    pthread_mutex_unlock(&b->m);
    return (b->signaled) ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
}
DWORD WaitForMultipleObjects(DWORD nCount, const HANDLE* lpHandles, BOOL bWaitAll, DWORD dwMilliseconds) {
    if (!bWaitAll) {
        for (DWORD i = 0; i < nCount; ++i)
            if (WaitForSingleObject(lpHandles[i], 0) == WAIT_OBJECT_0) return WAIT_OBJECT_0 + i;
        if (dwMilliseconds == INFINITE) {
            for (;;) {
                for (DWORD i = 0; i < nCount; ++i)
                    if (WaitForSingleObject(lpHandles[i], 0) == WAIT_OBJECT_0) return WAIT_OBJECT_0 + i;
                usleep(1000);
            }
        }
        return WAIT_TIMEOUT;
    }
    for (DWORD i = 0; i < nCount; ++i) WaitForSingleObject(lpHandles[i], dwMilliseconds);
    return WAIT_OBJECT_0;
}
BOOL TerminateThread(HANDLE, DWORD) { return TRUE; }
DWORD GetCurrentThreadId(void) { return (DWORD)(uintptr_t)pthread_self(); }
DWORD GetCurrentProcessId(void) { return (DWORD)getpid(); }

void InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection) {
    pthread_mutex_t* m = new pthread_mutex_t();
    pthread_mutex_init(m, NULL);
    lpCriticalSection->Opaque = m;
}
void DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection) {
    if (lpCriticalSection->Opaque) {
        pthread_mutex_destroy((pthread_mutex_t*)lpCriticalSection->Opaque);
        delete (pthread_mutex_t*)lpCriticalSection->Opaque;
        lpCriticalSection->Opaque = NULL;
    }
}
void EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection) {
    pthread_mutex_lock((pthread_mutex_t*)lpCriticalSection->Opaque);
}
void LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection) {
    pthread_mutex_unlock((pthread_mutex_t*)lpCriticalSection->Opaque);
}
BOOL TryEnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection) {
    return pthread_mutex_trylock((pthread_mutex_t*)lpCriticalSection->Opaque) == 0 ? TRUE : FALSE;
}

BOOL CreateProcessA(LPCSTR, LPSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES,
                    BOOL, DWORD, LPVOID, LPCSTR, LPSTARTUPINFOA, LPPROCESS_INFORMATION) {
    return FALSE;
}
BOOL SetEnvironmentVariableA(LPCSTR lpName, LPCSTR lpValue) {
    return setenv(lpName, lpValue ? lpValue : "", 1) == 0 ? TRUE : FALSE;
}
DWORD GetEnvironmentVariableA(LPCSTR lpName, LPSTR lpBuffer, DWORD nSize) {
    const char* v = getenv(lpName);
    if (!v) return 0;
    strncpy(lpBuffer, v, nSize);
    return (DWORD)strlen(v);
}

/* ---- UI stubs ---- */
BOOL    DestroyWindow(HWND) { return TRUE; }
BOOL    ShowWindow(HWND, int) { return TRUE; }
BOOL    SetForegroundWindow(HWND) { return TRUE; }
HWND    GetForegroundWindow(void) { return NULL; }
HWND    GetDesktopWindow(void) { return NULL; }
BOOL    GetClientRect(HWND, LPRECT lpRect) { if (lpRect) { lpRect->left=0; lpRect->top=0; lpRect->right=800; lpRect->bottom=600; } return TRUE; }
BOOL    GetWindowRect(HWND, LPRECT lpRect) { return GetClientRect(NULL, lpRect); }
BOOL    ScreenToClient(HWND, LPPOINT) { return TRUE; }
BOOL    ClientToScreen(HWND, LPPOINT) { return TRUE; }
int     GetSystemMetrics(int) { return 0; }
HDC     GetDC(HWND) { return NULL; }
int     ReleaseDC(HWND, HDC) { return 1; }
int     MulDiv(int nNumber, int nNumerator, int nDenominator) {
    return (int)(((int64_t)nNumber * nNumerator) / (nDenominator ? nDenominator : 1));
}
BOOL    IsWindow(HWND h) { return h != NULL; }
BOOL    IsBadReadPtr(const void*, UINT_PTR) { return FALSE; }
BOOL    IsBadWritePtr(LPVOID, UINT_PTR) { return FALSE; }
void    DebugBreak(void) {}
BOOL    PostMessageA(HWND, UINT, WPARAM, LPARAM) { return TRUE; }
LRESULT SendMessageA(HWND, UINT, WPARAM, LPARAM) { return 0; }
BOOL    SetWindowTextA(HWND, LPCSTR) { return TRUE; }
int     GetWindowTextA(HWND, LPSTR lpString, int nMaxCount) { if (lpString && nMaxCount) lpString[0]=0; return 0; }
BOOL    InvalidateRect(HWND, const RECT*, BOOL) { return TRUE; }
BOOL    UpdateWindow(HWND) { return TRUE; }
BOOL    AdjustWindowRect(LPRECT, DWORD, BOOL) { return TRUE; }
int     GetDeviceCaps(HDC, int) { return 0; }
BOOL    DeleteObject(HGDIOBJ) { return TRUE; }
HGDIOBJ GetStockObject(int) { return NULL; }
HFONT   CreateFontA(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCSTR) { return NULL; }
int     SetMapMode(HDC, int) { return 0; }
SHORT   GetAsyncKeyState(int) { return 0; }
SHORT   GetKeyState(int) { return 0; }
BOOL    SetTimer(HWND, UINT_PTR, UINT, void*) { return TRUE; }
BOOL    KillTimer(HWND, UINT_PTR) { return TRUE; }
HCURSOR LoadCursorA(HINSTANCE, LPCSTR) { return NULL; }
HCURSOR SetCursor(HCURSOR hCursor) { return hCursor; }
int     ShowCursor(BOOL) { return 0; }
HICON   LoadIconA(HINSTANCE, LPCSTR) { return NULL; }
BOOL    DestroyIcon(HICON) { return TRUE; }
BOOL    DestroyCursor(HCURSOR) { return TRUE; }
int     DialogBoxParamA(HINSTANCE, LPCSTR, HWND, void*, LPARAM) { return 0; }
BOOL    EndDialog(HWND, intptr_t) { return TRUE; }
UINT    RegisterClassA(const void*) { return 1; }
HWND    CreateWindowExA(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID) { return NULL; }
LRESULT DefWindowProcA(HWND, UINT, WPARAM, LPARAM) { return 0; }

/* ---- atomics ---- */
int  InterlockedIncrement(volatile LONG* lpAddend) { return __sync_add_and_fetch(lpAddend, 1); }
int  InterlockedDecrement(volatile LONG* lpAddend) { return __sync_sub_and_fetch(lpAddend, 1); }
LONG InterlockedExchange(volatile LONG* Target, LONG Value) { return __sync_lock_test_and_set(Target, Value); }
PVOID InterlockedExchangePointer(PVOID volatile* Target, PVOID Value) { return __sync_lock_test_and_set(Target, Value); }
LONG InterlockedCompareExchange(volatile LONG* Destination, LONG Exchange, LONG Comparand) {
    return __sync_val_compare_and_swap(Destination, Comparand, Exchange);
}

BOOL QueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    lpPerformanceCount->QuadPart = ts.tv_sec * 1000000000LL + ts.tv_nsec;
    return TRUE;
}
BOOL QueryPerformanceFrequency(LARGE_INTEGER* lpFrequency) {
    lpFrequency->QuadPart = 1000000000LL;
    return TRUE;
}

/* ---- WSA stubs (sockets themselves are POSIX) ---- */
int WSAStartup(WORD, LPWSADATA) { return 0; }
int WSACleanup(void) { return 0; }
int WSAAsyncSelect(SOCKET, HWND, unsigned int, long) { return 0; }

/* ---- unicode conversion: metin2 uses euc-kr/cp949 mostly; UTF-8 passthrough ---- */
int WideCharToMultiByte(UINT CodePage, DWORD, LPCWSTR lpWideCharStr, int cchWideChar,
                        LPSTR lpMultiByteStr, int cbMultiByte, LPCSTR, LPBOOL) {
    if (CodePage == CP_UTF8 || CodePage == CP_ACP) {
        int n = (int)wcstombs(NULL, lpWideCharStr, 0);
        if (n < 0) n = 0;
        if (cchWideChar != -1) {
            // count chars needed
            n = 0;
            for (int i = 0; lpWideCharStr[i] && (cchWideChar == -1 || i < cchWideChar); ++i) {
                char tmp[MB_LEN_MAX];
                n += (int)wcstombs(tmp, lpWideCharStr + i, 1);
            }
        }
        if (!lpMultiByteStr || cbMultiByte == 0) return n;
        int written = (int)wcstombs(lpMultiByteStr, lpWideCharStr, cbMultiByte - 1);
        if (written < 0) written = 0;
        if (written < cbMultiByte) lpMultiByteStr[written] = 0;
        return written;
    }
    return 0;
}
int MultiByteToWideChar(UINT CodePage, DWORD, LPCSTR lpMultiByteStr, int cbMultiByte,
                        LPWSTR lpWideCharStr, int cchWideChar) {
    if (CodePage == CP_UTF8 || CodePage == CP_ACP) {
        if (cbMultiByte == -1) cbMultiByte = (int)strlen(lpMultiByteStr) + 1;
        if (!lpWideCharStr || cchWideChar == 0)
            return (int)mbstowcs(NULL, lpMultiByteStr, 0) >= 0 ? (int)mbstowcs(NULL, lpMultiByteStr, 0) : 0;
        int n = (int)mbstowcs(lpWideCharStr, lpMultiByteStr, cchWideChar - 1);
        if (n < 0) n = 0;
        if (n < cchWideChar) lpWideCharStr[n] = 0;
        return n;
    }
    return 0;
}

LCID GetSystemDefaultLCID(void) { return 0x0409; }
BOOL GetVersionExA(void*) { return TRUE; }

HANDLE CreateFileMappingA(HANDLE, LPSECURITY_ATTRIBUTES, DWORD, DWORD, DWORD, LPCSTR) { return NULL; }
LPVOID MapViewOfFile(HANDLE, DWORD, DWORD, DWORD, SIZE_T) { return NULL; }
BOOL UnmapViewOfFile(LPCVOID) { return TRUE; }
DWORD FormatMessageA(DWORD, LPCVOID, DWORD, DWORD, LPSTR lpBuffer, DWORD nSize, va_list*) {
    if (lpBuffer && nSize) lpBuffer[0] = 0;
    return 0;
}

/* ---- CRT helpers ---- */
char* _strlwr(char* s) { for (char* p = s; *p; ++p) *p = (char)tolower((unsigned char)*p); return s; }
char* _strupr(char* s) { for (char* p = s; *p; ++p) *p = (char)toupper((unsigned char)*p); return s; }
char* _itoa(int value, char* str, int base) {
    if (base == 16) sprintf(str, "%x", value);
    else if (base == 8) sprintf(str, "%o", value);
    else sprintf(str, "%d", value);
    return str;
}
int _atoi64(const char* s) { return (int)strtoll(s, NULL, 10); }

struct ThreadArgs { void* (*fn)(void*); void* arg; };
static void* thread_trampoline(void* p) {
    ThreadArgs* a = (ThreadArgs*)p;
    void* (*fn)(void*) = a->fn;
    void* arg = a->arg;
    delete a;
    return fn(arg);
}
intptr_t _beginthreadex(void*, unsigned, unsigned (__stdcall* start_address)(void*), void* arglist, unsigned, unsigned* thrdaddr) {
    ThreadArgs* a = new ThreadArgs{ (void*(*)(void*))start_address, arglist };
    pthread_t t;
    if (pthread_create(&t, NULL, thread_trampoline, a) != 0) { delete a; return 0; }
    if (thrdaddr) *thrdaddr = (unsigned)(uintptr_t)t;
    return (intptr_t)t;
}

/* _findfirst family */
intptr_t _findfirst(const char* filespec, _finddata_t* fileinfo) {
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(filespec, &fd);
    if (h == INVALID_HANDLE_VALUE) return -1;
    fileinfo->attrib = fd.dwFileAttributes;
    fileinfo->time_create = fileinfo->time_access = fileinfo->time_write = 0;
    fileinfo->size = (long)fd.nFileSizeLow;
    strncpy(fileinfo->name, fd.cFileName, sizeof(fileinfo->name) - 1);
    fileinfo->name[sizeof(fileinfo->name) - 1] = 0;
    return (intptr_t)h;
}
int _findnext(intptr_t handle, _finddata_t* fileinfo) {
    WIN32_FIND_DATAA fd;
    if (!FindNextFileA((HANDLE)handle, &fd)) return -1;
    fileinfo->attrib = fd.dwFileAttributes;
    fileinfo->time_create = fileinfo->time_access = fileinfo->time_write = 0;
    fileinfo->size = (long)fd.nFileSizeLow;
    strncpy(fileinfo->name, fd.cFileName, sizeof(fileinfo->name) - 1);
    fileinfo->name[sizeof(fileinfo->name) - 1] = 0;
    return 0;
}
int _findclose(intptr_t handle) {
    return FindClose((HANDLE)handle) ? 0 : -1;
}
intptr_t _findfirst64(const char* filespec, _finddata64_t* fileinfo) {
    _finddata_t f;
    intptr_t h = _findfirst(filespec, &f);
    if (h == -1) return -1;
    fileinfo->attrib = f.attrib;
    fileinfo->time_create = f.time_create;
    fileinfo->time_access = f.time_access;
    fileinfo->time_write = f.time_write;
    fileinfo->size = f.size;
    strncpy(fileinfo->name, f.name, sizeof(fileinfo->name) - 1);
    return h;
}
int _findnext64(intptr_t handle, _finddata64_t* fileinfo) {
    _finddata_t f;
    if (_findnext(handle, &f) != 0) return -1;
    fileinfo->attrib = f.attrib;
    fileinfo->time_create = f.time_create;
    fileinfo->time_access = f.time_access;
    fileinfo->time_write = f.time_write;
    fileinfo->size = f.size;
    strncpy(fileinfo->name, f.name, sizeof(fileinfo->name) - 1);
    return 0;
}

int lstrlen(const char* s) { return s ? (int)strlen(s) : 0; }
int lstrcpy(char* d, const char* s) { strcpy(d, s); return lstrlen(d); }
int lstrcat(char* d, const char* s) { strcat(d, s); return lstrlen(d); }
int lstrcmp(const char* a, const char* b) { return strcmp(a, b); }

unsigned RegisterClipboardFormatA(const char*) { return 0xC000; }
BOOL AllocConsole() { return TRUE; }
void* GetCurrentProcess() { return (void*)-1; }
void* GetCurrentThread() { return (void*)-2; }
int LoadString(void*, unsigned, char*, int) { return 0; }
unsigned GetPrivateProfileStringA(const char*, const char*, const char* lpDefault, char* out, unsigned nSize, const char*) {
    if (!out || !nSize) return 0;
    if (lpDefault) { strncpy(out, lpDefault, nSize - 1); out[nSize - 1] = 0; }
    else out[0] = 0;
    return (unsigned)strlen(out);
}
HRESULT CoInitialize(void*) { return S_OK; }
HRESULT CoCreateInstance(const GUID&, void*, unsigned long, const GUID&, void** ppv) { if (ppv) *ppv = NULL; return E_NOINTERFACE; }
void    CoUninitialize() {}
int     timeBeginPeriod(unsigned) { return 0; }
int     timeEndPeriod(unsigned) { return 0; }
BOOL    ReleaseSemaphore(void*, long, long*) { return TRUE; }


void* FindWindowA(const char*, const char*) { return NULL; }
LONG  InterlockedExchangeAdd(LONG volatile* a, LONG v) { return __sync_fetch_and_add(a, v); }

} // extern "C"

BOOL PeekMessageA(MSG*, void*, unsigned, unsigned, unsigned) { return FALSE; }
BOOL GetMessageA(MSG*, void*, unsigned, unsigned) { return FALSE; }
BOOL TranslateMessage(const MSG*) { return FALSE; }
long DispatchMessageA(const MSG*) { return 0; }
void PostQuitMessage(int) {}


char* _ecvt(double value, int count, int* dec, int* sign) {
    static char buf[64];
    if (sign) *sign = (value < 0) ? 1 : 0;
    snprintf(buf, sizeof(buf), "%.*g", count, value);
    if (dec) *dec = 0;
    return buf;
}

BOOL SystemParametersInfoA(unsigned, unsigned, void*, unsigned) { return TRUE; }
BOOL GetCursorPos(POINT* p) { if (p) { p->x = 0; p->y = 0; } return TRUE; }
void* CreateSemaphoreA(void*, long, long, const char*) { return (void*)1; }
unsigned timeGetDevCaps(TIMECAPS* p, unsigned) {
    if (p) { p->wPeriodMin = 1; p->wPeriodMax = 1000; }
    return TIMERR_NOERROR;
}
unsigned _controlfp(unsigned, unsigned) { return 0; }
unsigned _control87(unsigned, unsigned) { return 0; }
int EnumFontFamiliesExA(void*, const void*, FONTENUMPROCA, long, unsigned long) { return 0; }

BOOL SetThreadPriority(void*, int) { return TRUE; }

BOOL SHGetSpecialFolderPathA(void*, char* p, int, BOOL) { if (p) { strcpy(p, "/sdcard"); } return p != NULL; }
LPTOP_LEVEL_EXCEPTION_FILTER SetUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER) { return NULL; }

BOOL SetCursorPos(int, int) { return TRUE; }
BOOL SetFileAttributesA(LPCSTR, DWORD) { return TRUE; }
int  SetBkMode(HDC, int m) { return m; }
unsigned SetBkColor(HDC, unsigned c) { return c; }
LPSTR CharNextExA(unsigned short, LPCSTR p, DWORD) { return (LPSTR)(p && *p ? p + 1 : p); }
LPSTR CharPrevExA(unsigned short, LPCSTR st, LPCSTR p, DWORD) { return (LPSTR)(p > st ? p - 1 : p); }
LPSTR CharNextA(LPCSTR p) { return (LPSTR)(p && *p ? p + 1 : p); }
LPSTR CharPrevA(LPCSTR s, LPCSTR p) { return (LPSTR)(p > s ? p - 1 : p); }

LONG ChangeDisplaySettingsA(DEVMODE*, DWORD) { return DISP_CHANGE_SUCCESSFUL; }
HWND SetCapture(HWND h) { return h; }
BOOL ReleaseCapture() { return TRUE; }
HWND GetCapture() { return NULL; }
HANDLE LoadImageA(HINSTANCE, LPCSTR, UINT, int, int, UINT) { return (HANDLE)1; }

int SetDIBitsToDevice(HDC, int, int, DWORD, DWORD, int, int, UINT, UINT, const void*, const BITMAPINFO*, UINT) { return 0; }

/* ---- misc win api stubs ---- */
int _fltused;
BOOL SetWindowPos(void* h, void* after, int X, int Y, int cx, int cy, unsigned f) { return TRUE; }
BOOL OpenClipboard(void*) { return TRUE; }
BOOL CloseClipboard() { return TRUE; }
void* GetClipboardData(unsigned fmt) { return NULL; }
void* SetClipboardData(unsigned fmt, void* hMem) { return hMem; }

extern "C" {
int WebBrowser_Startup(void* hInstance) { return 0; }
void WebBrowser_Cleanup() {}
void WebBrowser_Destroy() {}
int WebBrowser_Show(void* parent, const char* addr, const void* rc) { return 0; }
void WebBrowser_Hide() {}
void WebBrowser_Move(const void* rc) {}
int WebBrowser_IsVisible() { return 0; }
}
