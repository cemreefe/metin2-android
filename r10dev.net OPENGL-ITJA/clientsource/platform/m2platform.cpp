// Shared platform-port pieces: pure path helpers used by every adapter.
#include "m2platform.h"

#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>

namespace M2Plat
{
	void Log(ELogLevel level, const char* tag, const char* fmt, ...)
	{
		va_list args;
		va_start(args, fmt);
		LogV(level, tag, fmt, args);
		va_end(args);
	}

	void NormalizePath(const char* c_szPath, char* szOut, size_t uOutSize)
	{
		if (!uOutSize)
			return;
		if (c_szPath[0] && c_szPath[1] == ':')
			c_szPath += 2;
		while (*c_szPath == '/' || *c_szPath == '\\')
			++c_szPath;
		size_t i = 0;
		for (; c_szPath[i] && i < uOutSize - 1; ++i)
			szOut[i] = c_szPath[i] == '\\' ? '/' : (char)tolower((unsigned char)c_szPath[i]);
		szOut[i] = '\0';
	}

	int FileAccess(const char* c_szPath, int iMode)
	{
		if (access(c_szPath, iMode) == 0)
			return 0;
		char szNormalized[1024];
		NormalizePath(c_szPath, szNormalized, sizeof(szNormalized));
		if (access(szNormalized, iMode) == 0)
			return 0;
		// Materializing also builds the lazy directory skeleton, so retry
		// even when no file was copied (the path may be a folder).
		MaterializeFile(c_szPath);
		return access(szNormalized, iMode) == 0 ? 0 : -1;
	}

	__attribute__((weak)) bool MaterializeFile(const char*)
	{
		return false;
	}

	__attribute__((weak)) void SetPointerVisible(bool)
	{
	}

	__attribute__((weak)) bool IsTouchPrimary()
	{
		return true;
	}

	__attribute__((weak)) void ListLazyDir(const char*, ListDirFn, void*)
	{
	}

	__attribute__((weak)) bool OpenAudioOutput(unsigned, AudioRenderFn, void*)
	{
		return false;
	}
}
