#pragma once

#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
  #error "This windows.h is only for platform-port (Android/web) builds!"
#endif

#include "../platform/m2platform.h"

// Win32 compatibility layer for platform-port builds.
// Types, macros, constants and function prototypes the engine needs.
// Implementations of the stubbed functions live in win_stub.cpp.

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <wchar.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>

#define WIN32_LEAN_AND_MEAN
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

/* ---- calling conventions / declspec ---- */
#ifndef WINAPI
#define WINAPI
#endif
#ifndef CALLBACK
#define CALLBACK
#endif
#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef STDMETHODCALLTYPE
#define STDMETHODCALLTYPE
#endif
#define __stdcall
#define __cdecl
#define __fastcall
#define __thiscall
#ifndef __forceinline
#define __forceinline inline __attribute__((always_inline))
#endif
#ifndef __declspec
#define __declspec(x)
#endif
#ifndef _declspec
#define _declspec(x)
#endif

/* ---- basic scalar types ---- */
typedef int BOOL;
typedef int32_t LONG;
typedef uint32_t ULONG;
typedef int16_t SHORT;
typedef uint16_t USHORT;
typedef int32_t INT;
typedef uint32_t UINT;
typedef float FLOAT;
typedef uint32_t DWORD;
typedef uint16_t WORD;
typedef uint8_t BYTE;
typedef char CHAR;
typedef unsigned char UCHAR;
typedef wchar_t WCHAR;
typedef wchar_t TCHAR;
typedef int64_t LONGLONG;
typedef uint64_t ULONGLONG;
typedef uint64_t DWORDLONG;
typedef int64_t LONG64;
typedef uint64_t ULONG64;
typedef int64_t __int64_t_compat;
#define __int64 long long
typedef size_t SIZE_T;
typedef intptr_t INT_PTR;
typedef uintptr_t UINT_PTR;
typedef intptr_t LONG_PTR;
typedef uintptr_t ULONG_PTR;
typedef uintptr_t DWORD_PTR;
typedef intptr_t SSIZE_T;

typedef void VOID;
typedef void* PVOID;
typedef void* LPVOID;
typedef const void* LPCVOID;
typedef char* LPSTR;
typedef const char* LPCSTR;
typedef char* PSTR;
typedef const char* PCSTR;
typedef char* PCHAR;
typedef const char* PCCHAR;
typedef unsigned char* PUCHAR;
typedef wchar_t* LPWSTR;
typedef const wchar_t* LPCWSTR;
typedef TCHAR* LPTSTR;
typedef const TCHAR* LPCTSTR;
typedef BYTE* PBYTE;
typedef BYTE* LPBYTE;
typedef WORD* PWORD;
typedef WORD* LPWORD;
typedef DWORD* PDWORD;
typedef DWORD* LPDWORD;
typedef UINT* PUINT;
typedef UINT* LPUINT;
typedef INT* PINT;
typedef INT* LPINT;
typedef LONG* PLONG;
typedef LONG* LPLONG;
typedef ULONG* PULONG;
typedef BOOL* PBOOL;
typedef BOOL* LPBOOL;
typedef CHAR* LPCH;
typedef SHORT* PSHORT;
typedef USHORT* PUSHORT;

/* ---- handles ---- */
typedef void* HANDLE;
typedef void* HMODULE;
typedef void* HINSTANCE;
typedef void* HWND;
typedef void* HDC;
typedef void* HGLRC;
typedef void* HICON;
typedef void* HCURSOR;
typedef void* HMENU;
typedef void* HFONT;
typedef void* HBITMAP;
typedef void* HBRUSH;
typedef void* HPEN;
typedef void* HGDIOBJ;
typedef void* HKEY;
typedef void* HGLOBAL;
typedef void* HLOCAL;
typedef void* HRSRC;
typedef void* HHOOK;
typedef void* HMONITOR;
typedef void* HWAVEOUT;
typedef void* HDRVR;
typedef void* HTHEME;
typedef void* HIMC;
typedef void* HIMCC;
typedef void* HKL;
typedef void (*FARPROC)();
typedef uint32_t ATOM;
typedef uint32_t LCID;
typedef uint32_t LANGID;
typedef uint32_t COLORREF;
typedef long LRESULT;
typedef long HRESULT;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;

#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define NULL_HANDLE NULL

/* ---- IME/GDI misc ---- */
struct INPUTCONTEXT;
struct COMPOSITIONFORM;
struct CANDIDATEFORM;
#define KL_NAMELENGTH 9

typedef struct tagLOGFONTA {
    LONG lfHeight, lfWidth, lfEscapement, lfOrientation, lfWeight;
    BYTE lfItalic, lfUnderline, lfStrikeOut, lfCharSet, lfOutPrecision;
    BYTE lfClipPrecision, lfQuality, lfPitchAndFamily;
    CHAR lfFaceName[32];
} LOGFONTA, *LPLOGFONTA;

typedef struct tagLOGFONTW {
    LONG lfHeight, lfWidth, lfEscapement, lfOrientation, lfWeight;
    BYTE lfItalic, lfUnderline, lfStrikeOut, lfCharSet, lfOutPrecision;
    BYTE lfClipPrecision, lfQuality, lfPitchAndFamily;
    WCHAR lfFaceName[32];
} LOGFONTW, *LPLOGFONTW;

typedef struct tagTEXTMETRICA {
    LONG tmHeight, tmAscent, tmDescent, tmInternalLeading, tmExternalLeading;
    LONG tmAveCharWidth, tmMaxCharWidth, tmWeight, tmOverhang, tmDigitizedAspectX, tmDigitizedAspectY;
    CHAR tmFirstChar, tmLastChar, tmDefaultChar, tmBreakChar;
    BYTE tmItalic, tmUnderlined, tmStruckOut, tmPitchAndFamily, tmCharSet;
} TEXTMETRICA, *LPTEXTMETRICA, *PTEXTMETRICA;
typedef TEXTMETRICA TEXTMETRIC;
typedef LOGFONTA LOGFONT;

typedef struct tagOSVERSIONINFOA {
    DWORD dwOSVersionInfoSize;
    DWORD dwMajorVersion, dwMinorVersion, dwBuildNumber, dwPlatformId;
    CHAR szCSDVersion[128];
} OSVERSIONINFOA, *LPOSVERSIONINFOA;
typedef OSVERSIONINFOA OSVERSIONINFO;

/* ---- COM-ish ---- */
typedef struct _GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];
} GUID, *LPGUID, *REFGUID;
typedef GUID IID;
typedef const IID* REFIID;
typedef const GUID* REFCLSID;
typedef GUID CLSID;

typedef union _LARGE_INTEGER {
    struct {
        DWORD LowPart;
        LONG  HighPart;
    };
    struct {
        DWORD LowPart;
        LONG  HighPart;
    } u;
    int64_t QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef union _ULARGE_INTEGER {
    struct {
        DWORD LowPart;
        DWORD HighPart;
    };
    uint64_t QuadPart;
} ULARGE_INTEGER;

/* ---- HRESULT values ---- */
#ifndef S_OK
#define S_OK        ((HRESULT)0L)
#endif
#ifndef S_FALSE
#define S_FALSE     ((HRESULT)1L)
#endif
#define E_FAIL          ((HRESULT)-1)
#define E_NOTIMPL       ((HRESULT)-2)
#define E_OUTOFMEMORY   ((HRESULT)-3)
#define E_INVALIDARG    ((HRESULT)-4)
#define E_POINTER       ((HRESULT)-5)
#define E_HANDLE        ((HRESULT)-6)
#define E_ABORT         ((HRESULT)-7)
#define E_ACCESSDENIED  ((HRESULT)-8)
#define E_NOINTERFACE   ((HRESULT)-10)
#define E_PENDING       ((HRESULT)-9)
#ifndef FAILED
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#endif
#ifndef SUCCEEDED
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#ifndef CONST
#define CONST const
#endif
#ifndef IN
#define IN
#define OUT
#define OPTIONAL
#endif

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

/* ---- core structs ---- */
#ifndef _RECT_DEFINED
#define _RECT_DEFINED
typedef struct tagRECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT, *PRECT, *LPRECT;
#endif

#ifndef _POINT_DEFINED
#define _POINT_DEFINED
typedef struct tagPOINT {
    LONG x;
    LONG y;
} POINT, *PPOINT, *LPPOINT;
#endif

#ifndef _SIZE_DEFINED
#define _SIZE_DEFINED
typedef struct tagSIZE {
    LONG cx;
    LONG cy;
} SIZE, *PSIZE, *LPSIZE;
#endif

#ifndef _PALETTEENTRY_DEFINED
#define _PALETTEENTRY_DEFINED
typedef struct tagPALETTEENTRY {
    BYTE peRed;
    BYTE peGreen;
    BYTE peBlue;
    BYTE peFlags;
} PALETTEENTRY;
#endif

typedef struct tagRGBQUAD {
    BYTE rgbBlue;
    BYTE rgbGreen;
    BYTE rgbRed;
    BYTE rgbReserved;
} RGBQUAD;

typedef struct tagBITMAPINFOHEADER {
    DWORD biSize;
    LONG  biWidth;
    LONG  biHeight;
    WORD  biPlanes;
    WORD  biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG  biXPelsPerMeter;
    LONG  biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
} BITMAPINFOHEADER, *PBITMAPINFOHEADER;

typedef struct tagBITMAPINFO {
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD          bmiColors[1];
} BITMAPINFO, *PBITMAPINFO;

typedef struct tagBITMAPFILEHEADER {
    WORD  bfType;
    DWORD bfSize;
    WORD  bfReserved1;
    WORD  bfReserved2;
    DWORD bfOffBits;
} BITMAPFILEHEADER;

typedef struct _FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

typedef struct _SYSTEMTIME {
    WORD wYear;
    WORD wMonth;
    WORD wDayOfWeek;
    WORD wDay;
    WORD wHour;
    WORD wMinute;
    WORD wSecond;
    WORD wMilliseconds;
} SYSTEMTIME, *PSYSTEMTIME, *LPSYSTEMTIME;

typedef struct _WIN32_FIND_DATAA {
    DWORD    dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD    nFileSizeHigh;
    DWORD    nFileSizeLow;
    DWORD    dwReserved0;
    DWORD    dwReserved1;
    CHAR     cFileName[MAX_PATH];
    CHAR     cAlternateFileName[14];
} WIN32_FIND_DATAA, *PWIN32_FIND_DATAA, *LPWIN32_FIND_DATAA;
typedef WIN32_FIND_DATAA WIN32_FIND_DATA;

typedef struct _SECURITY_ATTRIBUTES {
    DWORD  nLength;
    LPVOID lpSecurityDescriptor;
    BOOL   bInheritHandle;
} SECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

typedef struct _OVERLAPPED {
    ULONG_PTR Internal;
    ULONG_PTR InternalHigh;
    DWORD     Offset;
    DWORD     OffsetHigh;
    HANDLE    hEvent;
} OVERLAPPED, *LPOVERLAPPED;

typedef struct _STARTUPINFOA {
    DWORD  cb;
    LPSTR  lpReserved;
    LPSTR  lpDesktop;
    LPSTR  lpTitle;
    DWORD  dwX;
    DWORD  dwY;
    DWORD  dwXSize;
    DWORD  dwYSize;
    DWORD  dwXCountChars;
    DWORD  dwYCountChars;
    DWORD  dwFillAttribute;
    DWORD  dwFlags;
    WORD   wShowWindow;
    WORD   cbReserved2;
    LPBYTE lpReserved2;
    HANDLE hStdInput;
    HANDLE hStdOutput;
    HANDLE hStdError;
} STARTUPINFOA, *LPSTARTUPINFOA;
typedef STARTUPINFOA STARTUPINFO;

typedef struct _PROCESS_INFORMATION {
    HANDLE hProcess;
    HANDLE hThread;
    DWORD  dwProcessId;
    DWORD  dwThreadId;
} PROCESS_INFORMATION, *LPPROCESS_INFORMATION;

/* ---- critical sections ---- */
typedef struct _CRITICAL_SECTION {
    int   LockCount;
    void* Opaque;
} CRITICAL_SECTION, *LPCRITICAL_SECTION;

/* ---- message / window constants ---- */
#define WM_NULL             0x0000
#define WM_CREATE           0x0001
#define WM_DESTROY          0x0002
#define WM_MOVE             0x0003
#define WM_SIZE             0x0005
#define WM_ACTIVATE         0x0006
#define WM_SETFOCUS         0x0007
#define WM_KILLFOCUS        0x0008
#define WM_PAINT            0x000F
#define WM_CLOSE            0x0010
#define WM_QUIT             0x0012
#define WM_SETCURSOR        0x0020
#define WM_KEYDOWN          0x0100
#define WM_KEYUP            0x0101
#define WM_CHAR             0x0102
#define WM_SYSKEYDOWN       0x0104
#define WM_SYSKEYUP         0x0105
#define WM_SYSCHAR          0x0106
#define WM_MOUSEMOVE        0x0200
#define WM_LBUTTONDOWN      0x0201
#define WM_LBUTTONUP        0x0202
#define WM_RBUTTONDOWN      0x0204
#define WM_RBUTTONUP        0x0205
#define WM_MBUTTONDOWN      0x0207
#define WM_MBUTTONUP        0x0208
#define WM_MOUSEWHEEL       0x020A

#define VK_BACK             0x08
#define VK_TAB              0x09
#define VK_RETURN           0x0D
#define VK_SHIFT            0x10
#define VK_CONTROL          0x11
#define VK_ESCAPE           0x1B
#define VK_SPACE            0x20
#define VK_PRIOR            0x21
#define VK_NEXT             0x22
#define VK_END              0x23
#define VK_HOME             0x24
#define VK_LEFT             0x25
#define VK_UP               0x26
#define VK_RIGHT            0x27
#define VK_DOWN             0x28
#define VK_DELETE           0x2E
#define VK_INSERT           0x2D
#define VK_F1               0x70
#define VK_F2               0x71
#define VK_F3               0x72
#define VK_F4               0x73

#define MB_OK               0x0000
#define MB_OKCANCEL         0x0001
#define MB_YESNO            0x0004
#define MB_ICONERROR        0x0010
#define MB_ICONWARNING      0x0030
#define MB_ICONINFORMATION  0x0040
#define MB_ICONQUESTION     0x0020
#define MB_TOPMOST          0x00040000L
#define MB_SYSTEMMODAL      0x00001000L
#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3) \
    ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) | ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24))
#endif
#define IDOK                1
#define IDCANCEL            2
#define IDYES               6
#define IDNO                7

#define SW_HIDE             0
#define SW_SHOW             5
#define SW_SHOWNORMAL       1
#define SW_RESTORE          9
#define SW_MINIMIZE         6
#define SW_MAXIMIZE         3

#define GENERIC_READ        0x80000000
#define GENERIC_WRITE       0x40000000
#define GENERIC_ALL         0x10000000
#define FILE_SHARE_READ     0x00000001
#define FILE_SHARE_WRITE    0x00000002
#define CREATE_NEW          1
#define CREATE_ALWAYS       2
#define OPEN_EXISTING       3
#define OPEN_ALWAYS         4
#define TRUNCATE_EXISTING   5
#define FILE_ATTRIBUTE_NORMAL      0x00000080
#define FILE_ATTRIBUTE_DIRECTORY   0x00000010
#define FILE_ATTRIBUTE_READONLY    0x00000001
#define FILE_ATTRIBUTE_ARCHIVE     0x00000020
#define FILE_FLAG_SEQUENTIAL_SCAN  0x08000000
#define FILE_BEGIN          0
#define FILE_CURRENT        1
#define FILE_END            2
#define PAGE_READWRITE      0x04
#define FILE_MAP_READ       0x0004
#define FILE_MAP_WRITE      0x0002
#define FILE_MAP_ALL_ACCESS 0x0006
#define MEM_COMMIT          0x1000
#define MEM_RESERVE         0x2000
#define MEM_RELEASE         0x8000
#define MEM_DECOMMIT        0x4000
#define PAGE_NOACCESS       0x01
#define PAGE_READONLY       0x02
#define ERROR_SUCCESS       0
#define ERROR_FILE_NOT_FOUND 2
#define ERROR_PATH_NOT_FOUND 3
#define ERROR_ACCESS_DENIED 5
#define ERROR_INVALID_PARAMETER 87
#define WAIT_OBJECT_0       0
#define WAIT_TIMEOUT        0x102
#define INFINITE            0xFFFFFFFF
#define STILL_ACTIVE        0x103

#define CP_ACP              0
#define CP_OEMCP            1
#define CP_UTF8             65001
#define CP_THREAD_ACP       3

/* ---- helper macros ---- */
#ifndef MAKEWORD
#define MAKEWORD(a, b) ((WORD)(((BYTE)((a) & 0xff)) | ((WORD)((BYTE)((b) & 0xff))) << 8))
#endif
#ifndef MAKELONG
#define MAKELONG(a, b) ((LONG)(((WORD)((a) & 0xffff)) | ((DWORD)((WORD)((b) & 0xffff))) << 16))
#endif
#ifndef LOWORD
#define LOWORD(l) ((WORD)((l) & 0xffff))
#endif
#ifndef HIWORD
#define HIWORD(l) ((WORD)(((l) >> 16) & 0xffff))
#endif
#ifndef LOBYTE
#define LOBYTE(w) ((BYTE)((w) & 0xff))
#endif
#ifndef HIBYTE
#define HIBYTE(w) ((BYTE)(((w) >> 8) & 0xff))
#endif
#ifndef RGB
#define RGB(r,g,b) ((COLORREF)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))
#endif
#ifndef GetRValue
#define GetRValue(rgb) ((BYTE)(rgb))
#define GetGValue(rgb) ((BYTE)(((WORD)(rgb)) >> 8))
#define GetBValue(rgb) ((BYTE)((rgb)>>16))
#endif
#ifndef _T
#define _T(x) x
#define TEXT(x) x
#define _TEXT(x) x
#endif
#ifndef ZeroMemory
#define ZeroMemory(p, s) memset((p), 0, (s))
#endif
#ifndef CopyMemory
#define CopyMemory(d, s, n) memcpy((d), (s), (n))
#define MoveMemory(d, s, n) memmove((d), (s), (n))
#define FillMemory(d, n, v) memset((d), (v), (n))
#endif
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAKELPARAM
#define MAKELPARAM(l, h) ((LPARAM)MAKELONG(l, h))
#define MAKEWPARAM(l, h) ((WPARAM)MAKELONG(l, h))
#endif

#include "../platform/m2platform.h"

/* ---- C runtime aliases ---- */
#define stricmp     strcasecmp
#define _stricmp    strcasecmp
#define strnicmp    strncasecmp
#define _strnicmp   strncasecmp
#define strcmpi     strcasecmp
#define _strcmpi    strcasecmp
#define wcsicmp     wcscasecmp
#define _wcsicmp    wcscasecmp
#define _vsnprintf  vsnprintf
#define _snprintf   snprintf
#define _vsnwprintf vswprintf
#define _snwprintf  swprintf
#define _tzset      tzset
#define _chmod      chmod
#define _access(p, m) M2Plat::FileAccess((p), (m))
#define _mkdir(p)   mkdir((p), 0777)
#define _rmdir      rmdir
#define _unlink     unlink
#define _getcwd     getcwd
#define _chdir      chdir
#define _fileno     fileno
#define _isatty     isatty

/* nanomite protection stubs */
#ifndef NANOBEGIN
#define NANOBEGIN
#define NANOEND
#endif

/* ---- sockets ---- */
typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;
typedef struct sockaddr* PSOCKADDR;
typedef struct hostent HOSTENT;
typedef struct hostent* PHOSTENT;
typedef struct hostent* LPHOSTENT;
typedef struct in_addr IN_ADDR;
typedef struct timeval TIMEVAL;
typedef fd_set FD_SET;
typedef FD_SET* LPFD_SET;
#define INVALID_SOCKET ((SOCKET)(~0))
#define SOCKET_ERROR   (-1)
#define closesocket   close
#define ioctlsocket   ioctl
#define WSAGetLastError()  (errno)
#define WSASetLastError(e) (errno = (e))
#define WSAEWOULDBLOCK  EWOULDBLOCK
#define WSAEINPROGRESS  EINPROGRESS
#define WSAETIMEDOUT    ETIMEDOUT
#define WSAECONNRESET   ECONNRESET
#define WSAECONNABORTED ECONNABORTED
#define WSAENOTCONN     ENOTCONN
#define WSAEISCONN      EISCONN
#define WSAECONNREFUSED ECONNREFUSED
#define WSAEINTR        EINTR
#define WSAEINVAL       EINVAL
#define WSAEADDRINUSE   EADDRINUSE
#define WSA_IO_PENDING  EINPROGRESS
#define FD_READ         0x01
#define FD_WRITE        0x02
#define FD_CLOSE        0x20
#define FD_CONNECT      0x10
#define FD_ACCEPT       0x08
#define WSAGETSELECTEVENT(l) ((int)((l) & 0xFFFF))
#define WSAGETSELECTERROR(l) ((int)(((l) >> 16) & 0xFFFF))
#define SD_RECEIVE 0
#define SD_SEND    1
#define SD_BOTH    2
typedef struct _WSAData {
    WORD wVersion;
    WORD wHighVersion;
    char szDescription[257];
    char szSystemStatus[129];
    unsigned short iMaxSockets;
    unsigned short iMaxUdpDg;
    char* lpVendorInfo;
} WSADATA, *LPWSADATA;

/* ---- function prototypes implemented in win_stub.cpp ---- */
#ifdef __cplusplus
extern "C" {
#endif

DWORD   GetTickCount(void);
void    Sleep(DWORD dwMilliseconds);
DWORD   GetLastError(void);
void    SetLastError(DWORD dwErrCode);
void    OutputDebugStringA(LPCSTR lpOutputString);
#define OutputDebugString OutputDebugStringA
int     MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType);
#define MessageBox MessageBoxA
BOOL    CloseHandle(HANDLE hObject);
DWORD   GetCurrentDirectoryA(DWORD nBufferLength, LPSTR lpBuffer);
#define GetCurrentDirectory GetCurrentDirectoryA
BOOL    SetCurrentDirectoryA(LPCSTR lpPathName);
#define SetCurrentDirectory SetCurrentDirectoryA
DWORD   GetFullPathNameA(LPCSTR lpFileName, DWORD nBufferLength, LPSTR lpBuffer, LPSTR* lpFilePart);
#define GetFullPathName GetFullPathNameA
HANDLE  FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData);
#define FindFirstFile FindFirstFileA
BOOL    FindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData);
#define FindNextFile FindNextFileA
BOOL    FindClose(HANDLE hFindFile);
HANDLE  CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                    LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
                    DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
#define CreateFile CreateFileA
BOOL    ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
                 LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);
BOOL    WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
                  LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped);
DWORD   SetFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod);
DWORD   GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh);
BOOL    GetFileSizeEx(HANDLE hFile, PLARGE_INTEGER lpFileSize);
BOOL    DeleteFileA(LPCSTR lpFileName);
#define DeleteFile DeleteFileA
BOOL    CreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);
#define CreateDirectory CreateDirectoryA
BOOL    RemoveDirectoryA(LPCSTR lpPathName);
#define RemoveDirectory RemoveDirectoryA
BOOL    MoveFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName);
#define MoveFile MoveFileA
BOOL    CopyFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, BOOL bFailIfExists);
#define CopyFile CopyFileA
DWORD   GetFileAttributesA(LPCSTR lpFileName);
#define GetFileAttributes GetFileAttributesA
void    GetSystemTime(LPSYSTEMTIME lpSystemTime);
void    GetLocalTime(LPSYSTEMTIME lpSystemTime);
BOOL    SystemTimeToFileTime(const SYSTEMTIME* lpSystemTime, LPFILETIME lpFileTime);
UINT    GetSystemDirectoryA(LPSTR lpBuffer, UINT uSize);
#define GetSystemDirectory GetSystemDirectoryA
UINT    GetWindowsDirectoryA(LPSTR lpBuffer, UINT uSize);
#define GetWindowsDirectory GetWindowsDirectoryA
DWORD   GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize);
#define GetModuleFileName GetModuleFileNameA
HMODULE LoadLibraryA(LPCSTR lpLibFileName);
#define LoadLibrary LoadLibraryA
BOOL    FreeLibrary(HMODULE hLibModule);
FARPROC GetProcAddress(HMODULE hModule, LPCSTR lpProcName);
HMODULE GetModuleHandleA(LPCSTR lpModuleName);
#define GetModuleHandle GetModuleHandleA
void*   VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect);
BOOL    VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType);
HGLOBAL GlobalAlloc(UINT uFlags, SIZE_T dwBytes);
HGLOBAL GlobalFree(HGLOBAL hMem);
LPVOID  GlobalLock(HGLOBAL hMem);
BOOL    GlobalUnlock(HGLOBAL hMem);
SIZE_T  GlobalSize(HGLOBAL hMem);
HLOCAL  LocalAlloc(UINT uFlags, SIZE_T uBytes);
HLOCAL  LocalFree(HLOCAL hMem);
HANDLE  CreateMutexA(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCSTR lpName);
#define CreateMutex CreateMutexA
BOOL    ReleaseMutex(HANDLE hMutex);
HANDLE  CreateEventA(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCSTR lpName);
#define CreateEvent CreateEventA
BOOL    SetEvent(HANDLE hEvent);
BOOL    ResetEvent(HANDLE hEvent);
DWORD   WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
DWORD   WaitForMultipleObjects(DWORD nCount, const HANDLE* lpHandles, BOOL bWaitAll, DWORD dwMilliseconds);
BOOL    TerminateThread(HANDLE hThread, DWORD dwExitCode);
DWORD   GetCurrentThreadId(void);
DWORD   GetCurrentProcessId(void);
void    InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
void    DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
void    EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
void    LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
BOOL    TryEnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
BOOL    CreateProcessA(LPCSTR lpApplicationName, LPSTR lpCommandLine,
                       LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes,
                       BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment,
                       LPCSTR lpCurrentDirectory, LPSTARTUPINFOA lpStartupInfo,
                       LPPROCESS_INFORMATION lpProcessInformation);
#define CreateProcess CreateProcessA
BOOL    SetEnvironmentVariableA(LPCSTR lpName, LPCSTR lpValue);
#define SetEnvironmentVariable SetEnvironmentVariableA
DWORD   GetEnvironmentVariableA(LPCSTR lpName, LPSTR lpBuffer, DWORD nSize);
#define GetEnvironmentVariable GetEnvironmentVariableA
BOOL    DestroyWindow(HWND hWnd);
BOOL    ShowWindow(HWND hWnd, int nCmdShow);
BOOL    SetForegroundWindow(HWND hWnd);
HWND    GetForegroundWindow(void);
HWND    GetDesktopWindow(void);
BOOL    GetClientRect(HWND hWnd, LPRECT lpRect);
BOOL    GetWindowRect(HWND hWnd, LPRECT lpRect);
BOOL    ScreenToClient(HWND hWnd, LPPOINT lpPoint);
BOOL    ClientToScreen(HWND hWnd, LPPOINT lpPoint);
int     GetSystemMetrics(int nIndex);
HDC     GetDC(HWND hWnd);
int     ReleaseDC(HWND hWnd, HDC hDC);
int     MulDiv(int nNumber, int nNumerator, int nDenominator);
DWORD   timeGetTime(void);
int     WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar,
                            LPSTR lpMultiByteStr, int cbMultiByte, LPCSTR lpDefaultChar, LPBOOL lpUsedDefaultChar);
int     MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte,
                            LPWSTR lpWideCharStr, int cchWideChar);
LCID    GetSystemDefaultLCID(void);
BOOL    GetVersionExA(void* lpVersionInformation);
#define GetVersionEx GetVersionExA
HANDLE  CreateFileMappingA(HANDLE hFile, LPSECURITY_ATTRIBUTES lpFileMappingAttributes, DWORD flProtect,
                           DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCSTR lpName);
#define CreateFileMapping CreateFileMappingA
LPVOID  MapViewOfFile(HANDLE hFileMappingObject, DWORD dwDesiredAccess, DWORD dwFileOffsetHigh,
                      DWORD dwFileOffsetLow, SIZE_T dwNumberOfBytesToMap);
BOOL    UnmapViewOfFile(LPCVOID lpBaseAddress);
DWORD   FormatMessageA(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId,
                       LPSTR lpBuffer, DWORD nSize, va_list* Arguments);
#define FormatMessage FormatMessageA
BOOL    IsBadReadPtr(const void* lp, UINT_PTR ucb);
BOOL    IsBadWritePtr(LPVOID lp, UINT_PTR ucb);
void    DebugBreak(void);
BOOL    PostMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
#define PostMessage PostMessageA
LRESULT SendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
#define SendMessage SendMessageA
void    PostQuitMessage(int nExitCode);
BOOL    SetWindowTextA(HWND hWnd, LPCSTR lpString);
#define SetWindowText SetWindowTextA
int     GetWindowTextA(HWND hWnd, LPSTR lpString, int nMaxCount);
#define GetWindowText GetWindowTextA
int     WSAStartup(WORD wVersionRequested, LPWSADATA lpWSAData);
int     WSACleanup(void);
int     WSAAsyncSelect(SOCKET s, HWND hWnd, unsigned int wMsg, long lEvent);
HOSTENT* gethostbyname(const char* name);
SHORT   GetAsyncKeyState(int vKey);
SHORT   GetKeyState(int nVirtKey);
BOOL    SetTimer(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, void* lpTimerFunc);
BOOL    KillTimer(HWND hWnd, UINT_PTR uIDEvent);
HCURSOR LoadCursorA(HINSTANCE hInstance, LPCSTR lpCursorName);
#define LoadCursor LoadCursorA
HCURSOR SetCursor(HCURSOR hCursor);
int     ShowCursor(BOOL bShow);
HICON   LoadIconA(HINSTANCE hInstance, LPCSTR lpIconName);
#define LoadIcon LoadIconA
BOOL    DestroyIcon(HICON hIcon);
BOOL    DestroyCursor(HCURSOR hCursor);
int     DialogBoxParamA(HINSTANCE hInstance, LPCSTR lpTemplateName, HWND hWndParent, void* lpDialogFunc, LPARAM dwInitParam);
#define DialogBoxParam DialogBoxParamA
BOOL    EndDialog(HWND hDlg, intptr_t nResult);
UINT    RegisterClassA(const void* lpWndClass);
#define RegisterClass RegisterClassA
HWND    CreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle,
                        int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu,
                        HINSTANCE hInstance, LPVOID lpParam);
#define CreateWindowEx CreateWindowExA
LRESULT DefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
#define DefWindowProc DefWindowProcA
BOOL    AdjustWindowRect(LPRECT lpRect, DWORD dwStyle, BOOL bMenu);
BOOL    InvalidateRect(HWND hWnd, const RECT* lpRect, BOOL bErase);
BOOL    UpdateWindow(HWND hWnd);
BOOL    IsWindow(HWND hWnd);
int     GetDeviceCaps(HDC hdc, int nIndex);
BOOL    DeleteObject(HGDIOBJ hObject);
HGDIOBJ GetStockObject(int i);
HFONT   CreateFontA(int cHeight, int cWidth, int cEscapement, int cOrientation, int cWeight,
                    DWORD bItalic, DWORD bUnderline, DWORD bStrikeOut, DWORD iCharSet,
                    DWORD iOutPrecision, DWORD iClipPrecision, DWORD iQuality,
                    DWORD iPitchAndFamily, LPCSTR pszFaceName);
#define CreateFont CreateFontA
int     SetMapMode(HDC hdc, int iMode);
int     InterlockedIncrement(volatile LONG* lpAddend);
int     InterlockedDecrement(volatile LONG* lpAddend);
LONG    InterlockedExchange(volatile LONG* Target, LONG Value);
PVOID   InterlockedExchangePointer(PVOID volatile* Target, PVOID Value);
LONG    InterlockedCompareExchange(volatile LONG* Destination, LONG Exchange, LONG Comparand);
BOOL    QueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount);
BOOL    QueryPerformanceFrequency(LARGE_INTEGER* lpFrequency);


/* io.h equivalents (also provided via android_compat/io.h) */
// _findfirst/_findnext/_findclose live in <io.h>

/* helpers */
char*    _strlwr(char* s);
char*    _strupr(char* s);
char*    _itoa(int value, char* str, int base);
int      _atoi64(const char* s);
intptr_t _beginthreadex(void* security, unsigned stack_size, unsigned (__stdcall* start_address)(void*), void* arglist, unsigned initflag, unsigned* thrdaddr);
#define _beginthread(func, stack, arg) _beginthreadex(NULL, (stack), (unsigned (__stdcall*)(void*))(func), (arg), 0, NULL)

/* MSVC CRT compat */
#ifndef _TRUNCATE
#define _TRUNCATE ((size_t)-1)
#endif
#ifndef _countof
#define _countof(x) (sizeof(x)/sizeof((x)[0]))
#endif
#ifndef strncpy_s
#define strncpy_s(d,ds,s,c) ( (c)==_TRUNCATE ? strncpy((d),(s),(ds)-1) : strncpy((d),(s),(c)) , (d)[(ds)-1]='\0' )
#endif
#ifndef strcpy_s
#define strcpy_s(d,ds,s) strncpy_s((d),(ds),(s),_TRUNCATE)
#endif
#ifndef strcat_s
#define strcat_s(d,ds,s) ( strncat((d),(s),(ds)-strlen(d)-1), (d)[(ds)-1]='\0' )
#endif
#ifndef sprintf_s
#define sprintf_s snprintf
#endif
#ifndef vsprintf_s
#define vsprintf_s vsnprintf
#endif
#ifndef _stricmp
#define _stricmp strcasecmp
#define stricmp strcasecmp
#define _strnicmp strncasecmp
#endif
#ifndef _snprintf
#define _snprintf snprintf
#define _vsnprintf vsnprintf
#endif
#ifndef _fileno
#define _fileno fileno
#endif
#ifndef LF_FACESIZE
#define LF_FACESIZE 32
#endif

/* window-style misc used in dead UI code paths */
typedef long (*WNDPROC)(void* hWnd, unsigned msg, unsigned w, long l);
#define WS_OVERLAPPEDWINDOW 0x00CF0000L
#define WS_OVERLAPPED       0x00000000L
#define WS_CAPTION          0x00C00000L
#define WS_SYSMENU          0x00080000L
#define WS_THICKFRAME       0x00040000L
#define WS_MINIMIZEBOX      0x00020000L
#define WS_MAXIMIZEBOX      0x00010000L
#define BLACK_BRUSH         4
#define NULL_BRUSH          5
#define WHITE_BRUSH         0
#define OPAQUE              2
#define TRANSPARENT         1
BOOL     SetForegroundWindow(void* hWnd);
BOOL     OpenClipboard(void* hWnd);
BOOL     CloseClipboard();
BOOL     EmptyClipboard();
void*    GetClipboardData(unsigned uFormat);
void*    SetClipboardData(unsigned uFormat, void* hMem);
BOOL     IsClipboardFormatAvailable(unsigned format);
BOOL     SetWindowPos(void* hWnd, void* hWndInsertAfter, int X, int Y, int cx, int cy, unsigned uFlags);
#define HWND_TOP        ((void*)0)
#define HWND_TOPMOST    ((void*)-1)
#define HWND_NOTOPMOST  ((void*)-2)
#define HWND_BOTTOM     ((void*)1)
#define SWP_NOSIZE      0x0001
#define SWP_NOMOVE      0x0002
#define SWP_NOZORDER    0x0004
#define SWP_SHOWWINDOW  0x0040
#define SWP_HIDEWINDOW  0x0080
#define SWP_NOREDRAW    0x0008
#define CF_TEXT         1
#define GMEM_MOVEABLE   0x0002
void*    GlobalAlloc(unsigned uFlags, size_t dwBytes);
void*    GlobalLock(void* hMem);
BOOL     GlobalUnlock(void* hMem);
void*    GlobalFree(void* hMem);
size_t   GlobalSize(void* hMem);

void     Tracenf(const char* c_szFormat, ...);
void     Tracen(const char* c_szMsg);
#ifndef _MAX_PATH
#define _MAX_PATH 260
#define MAX_PATH 260
#endif
typedef unsigned char BOOLEAN;
#define MB_ICONSTOP 0x0010
#define MB_ICONERROR 0x0010
#define MB_ICONQUESTION 0x0020
#define MB_ICONEXCLAMATION 0x0030
#define MB_ICONINFORMATION 0x0040
#define MB_OK 0x0000
#define MB_OKCANCEL 0x0001
#define MB_YESNO 0x0004
#define IDOK 1
#define IDCANCEL 2
#define IDYES 6
#define IDNO 7
#define IDABORT 3
#define IDRETRY 4
#define IDIGNORE 5
#define GMEM_ZEROINIT 0x0040
#define _alloca alloca
#include <alloca.h>
int      lstrlen(const char* s);
int      lstrcpy(char* d, const char* s);
int      lstrcat(char* d, const char* s);
int      lstrcmp(const char* a, const char* b);
unsigned RegisterClipboardFormatA(const char* name);
BOOL     AllocConsole();
void*    GetCurrentProcess();
void*    GetCurrentThread();
int      LoadString(void* hInstance, unsigned uID, char* lpBuffer, int nBufferMax);
unsigned GetPrivateProfileStringA(const char* lpAppName, const char* lpKeyName, const char* lpDefault, char* lpReturnedString, unsigned nSize, const char* lpFileName);
#define  GetPrivateProfileString GetPrivateProfileStringA
HRESULT  CoInitialize(void* pvReserved);
#ifdef __cplusplus
HRESULT  CoCreateInstance(const GUID& rclsid, void* pUnkOuter, unsigned long dwClsContext, const GUID& riid, void** ppv);
#endif
void     CoUninitialize();
#define  CLSCTX_INPROC_SERVER 0x1
#define  CLSCTX_ALL           0x17
int      timeBeginPeriod(unsigned uPeriod);
int      timeEndPeriod(unsigned uPeriod);
BOOL     ReleaseSemaphore(void* hSemaphore, long lReleaseCount, long* lpPreviousCount);
char**   CommandLineToArgvA(char* lpCmdLine, int* pNumArgs);
#define  CommandLineToArgv CommandLineToArgvA
BOOL     SetEnvironmentVariableA(const char* lpName, const char* lpValue);
#define  SetEnvironmentVariable SetEnvironmentVariableA
unsigned GetEnvironmentVariableA(const char* lpName, char* lpBuffer, unsigned nSize);
#define  GetEnvironmentVariable GetEnvironmentVariableA
int      GetSystemMetrics(int nIndex);
#define  SM_CXSCREEN 0
#define  SM_CYSCREEN 1
void*    GetDC(void* hWnd);
int      ReleaseDC(void* hWnd, void* hDC);
BOOL     DestroyWindow(void* hWnd);
void*    FindWindowA(const char* lpClassName, const char* lpWindowName);
#define  FindWindow FindWindowA
LONG     InterlockedExchangeAdd(LONG volatile* Addend, LONG Value);
LONG     InterlockedCompareExchange(LONG volatile* Destination, LONG Exchange, LONG Comparand);
void*    InterlockedCompareExchangePointer(void* volatile* Destination, void* Exchange, void* Comparand);

#define SHIFTJIS_CHARSET    128
#define HANGUL_CHARSET      129
#define GB2312_CHARSET      134
#define CHINESEBIG5_CHARSET 136
#define GREEK_CHARSET       161
#define TURKISH_CHARSET     162
#define HEBREW_CHARSET      177
#define ARABIC_CHARSET      178
#define BALTIC_CHARSET      186
#define VIETNAMESE_CHARSET  163
#define THAI_CHARSET        222
#define EASTEUROPE_CHARSET  238
#define RUSSIAN_CHARSET     204
#define DEFAULT_CHARSET     1
#define ANSI_CHARSET        0
#define OEM_CHARSET         255
#define JOHAB_CHARSET       130

typedef struct tagMSG {
    void* hwnd;
    unsigned message;
    unsigned long wParam;
    long lParam;
    unsigned long time;
    long pt_x, pt_y;
} MSG, *LPMSG;
#define PM_NOREMOVE 0
#define PM_REMOVE   1
#define PM_NOYIELD  2
BOOL     PeekMessageA(MSG* lpMsg, void* hWnd, unsigned wMsgFilterMin, unsigned wMsgFilterMax, unsigned wRemoveMsg);
#define  PeekMessage PeekMessageA
BOOL     GetMessageA(MSG* lpMsg, void* hWnd, unsigned wMsgFilterMin, unsigned wMsgFilterMax);
#define  GetMessage GetMessageA
BOOL     TranslateMessage(const MSG* lpMsg);
long     DispatchMessageA(const MSG* lpMsg);
#define  DispatchMessage DispatchMessageA
void     PostQuitMessage(int nExitCode);

#define  GMEM_FIXED          0x0000
#define  GMEM_MOVEABLE       0x0002
#define  GMEM_ZEROINIT       0x0040
#define  GPTR                (GMEM_FIXED | GMEM_ZEROINIT)
#define  GHND                (GMEM_MOVEABLE | GMEM_ZEROINIT)
void*    GlobalAlloc(unsigned uFlags, size_t dwBytes);
void*    GlobalLock(void* hMem);
BOOL     GlobalUnlock(void* hMem);
void*    GlobalFree(void* hMem);
size_t   GlobalSize(void* hMem);
#define  _S_IREAD            0x0100
#define  _S_IWRITE           0x0080
#define  _S_IEXEC            0x0040
char*    _ecvt(double value, int count, int* dec, int* sign);
#define  __min(a,b) ((a)<(b)?(a):(b))
#define  __max(a,b) ((a)>(b)?(a):(b))

#define  WS_POPUP            0x80000000L
#define  SPI_GETSTICKYKEYS   0x003A
#define  SPI_SETSTICKYKEYS   0x003B
#define  SKF_AVAILABLE       0x00000002
#define  SKF_HOTKEYACTIVE    0x00000004
#define  SKF_STICKYKEYSON    0x00000001
typedef struct tagSTICKYKEYS {
    unsigned cbSize;
    unsigned dwFlags;
} STICKYKEYS, *LPSTICKYKEYS;
BOOL     SystemParametersInfoA(unsigned uiAction, unsigned uiParam, void* pvParam, unsigned fWinIni);
#define  SystemParametersInfo SystemParametersInfoA
BOOL     GetCursorPos(POINT* lpPoint);
#define  THREAD_PRIORITY_NORMAL      0
#define  THREAD_PRIORITY_BELOW_NORMAL (-1)
#define  THREAD_PRIORITY_ABOVE_NORMAL 1
#define  THREAD_PRIORITY_LOWEST      (-2)
#define  THREAD_PRIORITY_HIGHEST     2
#define  THREAD_PRIORITY_IDLE        (-15)
#define  THREAD_PRIORITY_TIME_CRITICAL 15
void*    CreateSemaphoreA(void* lpSemaphoreAttributes, long lInitialCount, long lMaximumCount, const char* lpName);
#define  CreateSemaphore CreateSemaphoreA
#define  TIMERR_NOERROR   0
#define  TIMERR_NOCANDO   97
typedef struct timecaps_tag {
    unsigned wPeriodMin;
    unsigned wPeriodMax;
} TIMECAPS, *LPTIMECAPS;
unsigned timeGetDevCaps(TIMECAPS* ptc, unsigned cbtc);
unsigned timeGetTime();
#define  _PC_24  0x0000
#define  _PC_53  0x0001
#define  _PC_64  0x0002
#define  _MCW_PC 0x00030000
#define  _MCW_RC 0x00000200
#define  _MCW_DN 0x00000080
#define  _MCW_EM 0x0000001F
#define  _MCW_IC 0x00001000
#define  _RC_NEAR 0x0000
#define  _DN_FLUSH 0x0040
#define  _EM_ZERODIVIDE 0x0004
#define  _EM_INVALID 0x0010
#define  _EM_DENORMAL 0x0002
#define  _EM_OVERFLOW 0x0008
#define  _EM_UNDERFLOW 0x0010
#define  _EM_INEXACT 0x0020
#define  _IC_AFFINE 0x40000
unsigned _controlfp(unsigned unNew, unsigned unMask);
unsigned _control87(unsigned unNew, unsigned unMask);

#define  DIK_ESCAPE 0x01
#define  DIK_1 0x02
#define  DIK_2 0x03
#define  DIK_3 0x04
#define  DIK_4 0x05
#define  DIK_5 0x06
#define  DIK_6 0x07
#define  DIK_7 0x08
#define  DIK_8 0x09
#define  DIK_9 0x0A
#define  DIK_0 0x0B
#define  DIK_MINUS 0x0C
#define  DIK_EQUALS 0x0D
#define  DIK_BACK 0x0E
#define  DIK_TAB 0x0F
#define  DIK_Q 0x10
#define  DIK_W 0x11
#define  DIK_E 0x12
#define  DIK_R 0x13
#define  DIK_T 0x14
#define  DIK_Y 0x15
#define  DIK_U 0x16
#define  DIK_I 0x17
#define  DIK_O 0x18
#define  DIK_P 0x19
#define  DIK_LBRACKET 0x1A
#define  DIK_RBRACKET 0x1B
#define  DIK_RETURN 0x1C
#define  DIK_LCONTROL 0x1D
#define  DIK_A 0x1E
#define  DIK_S 0x1F
#define  DIK_D 0x20
#define  DIK_F 0x21
#define  DIK_G 0x22
#define  DIK_H 0x23
#define  DIK_J 0x24
#define  DIK_K 0x25
#define  DIK_L 0x26
#define  DIK_SEMICOLON 0x27
#define  DIK_APOSTROPHE 0x28
#define  DIK_GRAVE 0x29
#define  DIK_LSHIFT 0x2A
#define  DIK_BACKSLASH 0x2B
#define  DIK_Z 0x2C
#define  DIK_X 0x2D
#define  DIK_C 0x2E
#define  DIK_V 0x2F
#define  DIK_B 0x30
#define  DIK_N 0x31
#define  DIK_M 0x32
#define  DIK_COMMA 0x33
#define  DIK_PERIOD 0x34
#define  DIK_SLASH 0x35
#define  DIK_RSHIFT 0x36
#define  DIK_MULTIPLY 0x37
#define  DIK_LMENU 0x38
#define  DIK_SPACE 0x39
#define  DIK_CAPITAL 0x3A
#define  DIK_F1 0x3B
#define  DIK_F2 0x3C
#define  DIK_F3 0x3D
#define  DIK_F4 0x3E
#define  DIK_F5 0x3F
#define  DIK_F6 0x40
#define  DIK_F7 0x41
#define  DIK_F8 0x42
#define  DIK_F9 0x43
#define  DIK_F10 0x44
#define  DIK_NUMLOCK 0x45
#define  DIK_SCROLL 0x46
#define  DIK_NUMPAD7 0x47
#define  DIK_NUMPAD8 0x48
#define  DIK_NUMPAD9 0x49
#define  DIK_SUBTRACT 0x4A
#define  DIK_NUMPAD4 0x4B
#define  DIK_NUMPAD5 0x4C
#define  DIK_NUMPAD6 0x4D
#define  DIK_ADD 0x4E
#define  DIK_NUMPAD1 0x4F
#define  DIK_NUMPAD2 0x50
#define  DIK_NUMPAD3 0x51
#define  DIK_NUMPAD0 0x52
#define  DIK_DECIMAL 0x53
#define  DIK_F11 0x57
#define  DIK_F12 0x58
#define  DIK_NUMPADENTER 0x9C
#define  DIK_RCONTROL 0x9D
#define  DIK_DIVIDE 0xB5
#define  DIK_SYSRQ 0xB7
#define  DIK_RMENU 0xB8
#define  DIK_HOME 0xC7
#define  DIK_UP 0xC8
#define  DIK_PRIOR 0xC9
#define  DIK_LEFT 0xCB
#define  DIK_RIGHT 0xCD
#define  DIK_END 0xCF
#define  DIK_DOWN 0xD0
#define  DIK_NEXT 0xD1
#define  DIK_INSERT 0xD2
#define  DIK_DELETE 0xD3
#define  DIK_LWIN 0xDB
#define  DIK_RWIN 0xDC
#define  DIK_APPS 0xDD

#define  DIK_PAUSE 0xC5
#define  DIK_NUMPADCOMMA 0xB3
#define  DIK_WEBHOME 0xEA
#define  DIK_VOLUMEUP 0xB0
#define  DIK_VOLUMEDOWN 0xAE
#define  DIK_MUTE 0xA0
#define  DIK_PLAYPAUSE 0xA2
#define  DIK_MEDIASTOP 0xA4
#define  DIK_NEXTTRACK 0x9E
#define  DIK_PREVTRACK 0x90
#define  DIK_CALCULATOR 0xA1

typedef int (*FONTENUMPROCA)(const void*, const void*, unsigned long, long);
#define  FONTENUMPROC FONTENUMPROCA
int      EnumFontFamiliesExA(void* hdc, const void* lpLogfont, FONTENUMPROCA lpEnumFontFamExProc, long lParam, unsigned long dwFlags);
#define  EnumFontFamiliesEx EnumFontFamiliesExA

#define DDSD_CAPS 0x1
#define DDSD_HEIGHT 0x2
#define DDSD_WIDTH 0x4
#define DDSD_PITCH 0x8
#define DDSD_BACKBUFFERCOUNT 0x20
#define DDSD_ZBUFFERBITDEPTH 0x40
#define DDSD_ALPHABITDEPTH 0x80
#define DDSD_LPSURFACE 0x800
#define DDSD_PIXELFORMAT 0x1000
#define DDSD_CKDESTOVERLAY 0x2000
#define DDSD_CKDESTBLT 0x4000
#define DDSD_CKSRCOVERLAY 0x8000
#define DDSD_CKSRCBLT 0x10000
#define DDSD_MIPMAPCOUNT 0x20000
#define DDSD_REFRESHRATE 0x40000
#define DDSD_LINEARSIZE 0x80000
#define DDSD_TEXTURESTAGE 0x100000
#define DDSD_FVF 0x200000
#define DDSD_SRCVBHANDLE 0x400000
#define DDSD_DEPTH 0x800000
#define DDPF_ALPHAPIXELS 0x1
#define DDPF_ALPHA 0x2
#define DDPF_FOURCC 0x4
#define DDPF_PALETTEINDEXED4 0x8
#define DDPF_PALETTEINDEXEDTO8 0x10
#define DDPF_PALETTEINDEXED8 0x20
#define DDPF_RGB 0x40
#define DDPF_COMPRESSED 0x80
#define DDPF_RGBTOYUV 0x100
#define DDPF_YUV 0x200
#define DDPF_ZBUFFER 0x400
#define DDPF_PALETTEINDEXED1 0x800
#define DDPF_PALETTEINDEXED2 0x1000
#define DDPF_ZPIXELS 0x2000
#define DDPF_STENCILBUFFER 0x4000
#define DDPF_ALPHAPREMULT 0x8000
#define DDPF_LUMINANCE 0x20000
#define DDPF_BUMPLUMINANCE 0x40000
#define DDPF_BUMPDUDV 0x80000
typedef struct _DDPIXELFORMAT {
    DWORD dwSize, dwFlags, dwFourCC;
    union { DWORD dwRGBBitCount; DWORD dwYUVBitCount; DWORD dwZBufferBitDepth; DWORD dwAlphaBitDepth; };
    union { DWORD dwRBitMask; DWORD dwYBitMask; DWORD dwStencilBitDepth; DWORD dwLuminanceBitCount; DWORD dwBumpBitCount; };
    union { DWORD dwGBitMask; DWORD dwUBitMask; DWORD dwZBitMask; DWORD dwBumpDvBitMask; };
    union { DWORD dwBBitMask; DWORD dwVBitMask; DWORD dwStencilBitMask; DWORD dwBumpLuminanceBitMask; DWORD dwBumpDuBitMask; };
    union { DWORD dwRGBAlphaBitMask; DWORD dwYUVAlphaBitMask; DWORD dwLuminanceAlphaBitMask; DWORD dwRGBZBitMask; };
} DDPIXELFORMAT;
typedef struct _DDSURFACEDESC2 {
    DWORD dwSize, dwFlags, dwHeight, dwWidth;
    union { LONG lPitch; DWORD dwLinearSize; };
    DWORD dwBackBufferCount;
    union { DWORD dwMipMapCount; DWORD dwZBufferBitDepth; DWORD dwRefreshRate; };
    DWORD dwAlphaBitDepth;
    DWORD dwReserved;
    DWORD lpSurface; /* 32-bit in the on-disk DDS header */
    DWORD ddckColorKeys[8];
    DDPIXELFORMAT ddpfPixelFormat;
    DWORD ddsCaps[4];
    DWORD dwTextureStage;
} DDSURFACEDESC2;
#ifdef __cplusplus
static_assert(sizeof(DDSURFACEDESC2) == 124, "DDSURFACEDESC2 must match the DDS file header");
#endif


/* ---- more consts/APIs for extended source set ---- */
#define VK_SCROLL       0x91
#define VK_OEM_3        0xC0
#define VK_OEM_1        0xBA
#define VK_OEM_2        0xBF
#define VK_OEM_4        0xDB
#define VK_OEM_5        0xDC
#define VK_OEM_6        0xDD
#define VK_OEM_7        0xDE
#define VK_OEM_PLUS     0xBB
#define VK_OEM_COMMA    0xBC
#define VK_OEM_MINUS    0xBD
#define VK_OEM_PERIOD   0xBE
#define IMAGE_CURSOR    2
#define LR_VGACOLOR     0x0080
#define LR_DEFAULTCOLOR 0x0000
#define LR_SHARED       0x8000
#define MAKEINTRESOURCEA(i) ((LPCSTR)((ULONG_PTR)((WORD)(i))))
#define MAKEINTRESOURCE(i)  MAKEINTRESOURCEA(i)
HCURSOR LoadCursorA(void* hInstance, LPCSTR lpCursorName);
#define  LoadCursor LoadCursorA
#define  IDC_ARROW MAKEINTRESOURCEA(32512)
BOOL     SetCursorPos(int X, int Y);
BOOL     SetFileAttributesA(LPCSTR lpFileName, DWORD dwFileAttributes);
#define  SetFileAttributes SetFileAttributesA
#define  FILE_ATTRIBUTE_HIDDEN 0x0002
#define  FILE_ATTRIBUTE_SYSTEM 0x0004
/* GDI-lite stubs */
#define  FW_DONTCARE 0
#define  FW_NORMAL   400
#define  FW_BOLD     700
#define  OUT_DEFAULT_PRECIS 0
#define  CLIP_DEFAULT_PRECIS 0
#define  DEFAULT_QUALITY 0
#define  ANTIALIASED_QUALITY 4
#define  NONANTIALIASED_QUALITY 3
#define  DEFAULT_PITCH 0
#define  FF_DONTCARE 0
#define  VARIABLE_PITCH 2
#define  TRANSPARENT 1
#define  OPAQUE 2
#define  BI_RGB 0
#define  DIB_RGB_COLORS 0
typedef struct tagABC { int abcA; unsigned abcB; int abcC; } ABC, *LPABC;
typedef struct _ABCFLOAT { float abcfA; float abcfB; float abcfC; } ABCFLOAT, *LPABCFLOAT;
HDC      CreateCompatibleDC(HDC hdc);
BOOL     DeleteDC(HDC hdc);
HGDIOBJ  SelectObject(HDC hdc, HGDIOBJ h);
unsigned SetTextColor(HDC hdc, unsigned color);
int      SetBkMode(HDC hdc, int mode);
unsigned SetBkColor(HDC hdc, unsigned color);
BOOL     TextOutA(HDC hdc, int x, int y, LPCSTR lpString, int c);
#define  TextOut TextOutA
BOOL     TextOutW(HDC hdc, int x, int y, const wchar_t* lpString, int c);
HFONT    CreateFontIndirectA(const LOGFONTA* lplf);
// Port only: switch the TTF every later CreateFontIndirect uses ("" = platform default).
BOOL     GdiSetFontFile(const char* path);
#define  CreateFontIndirect CreateFontIndirectA
BOOL     GetTextExtentPoint32A(HDC hdc, LPCSTR lpString, int c, LPSIZE lpSize);
#define  GetTextExtentPoint32 GetTextExtentPoint32A
BOOL     GetTextExtentPoint32W(HDC hdc, const wchar_t* lpString, int c, LPSIZE lpSize);
BOOL     GetCharABCWidthsFloatA(HDC hdc, unsigned first, unsigned last, LPABCFLOAT abc);
BOOL     GetCharABCWidthsFloatW(HDC hdc, unsigned short first, unsigned short last, LPABCFLOAT abc);
#define  GetCharABCWidthsFloat GetCharABCWidthsFloatA
LPSTR    CharNextExA(unsigned short CodePage, LPCSTR lpCurrentChar, DWORD dwFlags);
LPSTR    CharPrevExA(unsigned short CodePage, LPCSTR lpStart, LPCSTR lpCurrentChar, DWORD dwFlags);
#define  CharNext CharNextA
LPSTR    CharNextA(LPCSTR lpsz);
LPSTR    CharPrevA(LPCSTR lpszStart, LPCSTR lpsz);


/* ---- display/cursor/misc consts ---- */
#define WM_ACTIVATEAPP        0x001C
#define WM_INPUTLANGCHANGE    0x0051
#define WM_IME_STARTCOMPOSITION 0x010D
#define WM_IME_COMPOSITION      0x010E
#define WM_IME_ENDCOMPOSITION   0x010F
#define WM_IME_SETCONTEXT       0x0281
#define WM_IME_NOTIFY           0x0282
#define WM_IME_CONTROL          0x0283
#define WM_IME_COMPOSITIONFULL  0x0284
#define WM_IME_SELECT           0x0285
#define WM_IME_CHAR             0x0286
#define WM_IME_REQUEST          0x0288
#define WM_IME_KEYDOWN          0x0290
#define WM_IME_KEYUP            0x0291
#define WA_INACTIVE       0
#define WA_ACTIVE         1
#define WA_CLICKACTIVE    2
#define DM_BITSPERPEL     0x00040000
#define DM_PELSWIDTH      0x00080000
#define DM_PELSHEIGHT     0x00100000
#define DM_DISPLAYFREQUENCY 0x00400000
#define CDS_FULLSCREEN    0x00000004
#define CDS_RESET         0x40000000
#define DISP_CHANGE_SUCCESSFUL 0
#define DISP_CHANGE_RESTART    1
#define DISP_CHANGE_FAILED    -1
typedef struct _devicemodeA {
    char dmDeviceName[32]; unsigned short dmSpecVersion; unsigned short dmDriverVersion;
    unsigned short dmSize; unsigned short dmDriverExtra; DWORD dmFields;
    struct { short dmOrientation; short dmPaperSize; short dmPaperLength; short dmPaperWidth; short dmScale; short dmCopies; short dmDefaultSource; short dmPrintQuality; } DUMMYSTRUCTNAME;
    short dmColor; short dmDuplex; short dmYResolution; short dmTTOption; short dmCollate;
    char dmFormName[32]; unsigned short dmLogPixels; DWORD dmBitsPerPel; DWORD dmPelsWidth; DWORD dmPelsHeight;
    DWORD dmDisplayFlags; DWORD dmDisplayFrequency; DWORD dmICMMethod; DWORD dmICMIntent;
    DWORD dmMediaType; DWORD dmDitherType; DWORD dmReserved1; DWORD dmReserved2; DWORD dmPanningWidth; DWORD dmPanningHeight;
} DEVMODE, *LPDEVMODE;
LONG     ChangeDisplaySettingsA(DEVMODE* lpDevMode, DWORD dwFlags);
#define  ChangeDisplaySettings ChangeDisplaySettingsA
HWND     SetCapture(HWND hWnd);
BOOL     ReleaseCapture();
HWND     GetCapture();
HANDLE   LoadImageA(HINSTANCE hInst, LPCSTR name, UINT type, int cx, int cy, UINT fuLoad);
#define  LoadImage LoadImageA


#define ISC_SHOWUICOMPOSITIONWINDOW       0x80000000
#define ISC_SHOWUIGUIDELINE               0x40000000
#define ISC_SHOWUIALLCANDIDATEWINDOW      0x0000000F
#define ISC_SHOWUICANDIDATEWINDOW         0x00000001
#define GET_X_LPARAM(lp)   ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp)   ((int)(short)HIWORD(lp))
#define GET_WHEEL_DELTA_WPARAM(wp) ((short)HIWORD(wp))
#ifndef LOWORD
#define LOWORD(l) ((unsigned short)(((unsigned long)(l)) & 0xffff))
#define HIWORD(l) ((unsigned short)((((unsigned long)(l)) >> 16) & 0xffff))
#endif
#define SIZE_RESTORED   0
#define SIZE_MINIMIZED  1
#define SIZE_MAXIMIZED  2
#define WM_EXITSIZEMOVE 0x0232
#define VK_F10          0x79
#define VK_F11          0x7A
#define VK_F12          0x7B
HBITMAP  CreateDIBSection(HDC hdc, const BITMAPINFO* pbmi, UINT iUsage, void** ppvBits, void* hSection, DWORD dwOffset);
int      SetDIBitsToDevice(HDC hdc, int xDest, int yDest, DWORD dwWidth, DWORD dwHeight, int xSrc, int ySrc, UINT uStartScan, UINT cScanLines, const void* lpvBits, const BITMAPINFO* lpbmi, UINT fuColorUse);

typedef struct _EXCEPTION_POINTERS {
    void* ExceptionRecord;
    void* ContextRecord;
} EXCEPTION_POINTERS, *PEXCEPTION_POINTERS;

#define  CSIDL_PERSONAL   0x0005
#define  CSIDL_DESKTOP    0x0000
#define  CSIDL_LOCAL_APPDATA 0x001C
BOOL     SHGetSpecialFolderPathA(void* hwndOwner, char* lpszPath, int nFolder, BOOL fCreate);
#define  SHGetSpecialFolderPath SHGetSpecialFolderPathA
typedef LONG (*LPTOP_LEVEL_EXCEPTION_FILTER)(EXCEPTION_POINTERS* ExceptionInfo);
LPTOP_LEVEL_EXCEPTION_FILTER SetUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter);

#define EXCEPTION_EXECUTE_HANDLER 1
#define EXCEPTION_CONTINUE_SEARCH 0
#define EXCEPTION_CONTINUE_EXECUTION (-1)
BOOL SetThreadPriority(void* hThread, int nPriority);
DWORD GetCurrentThreadId();

#ifdef __cplusplus
}
#endif


