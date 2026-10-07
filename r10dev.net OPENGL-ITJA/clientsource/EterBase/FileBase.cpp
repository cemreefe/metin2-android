#include "StdAfx.h"
#include "FileBase.h"

#ifdef M2_PORT
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../platform/m2platform.h"
#ifdef __ANDROID__
#include <android/asset_manager.h>
AAssetManager* g_pAssetManager = nullptr;
#endif
#endif

CFileBase::CFileBase() : m_hFile(NULL), m_dwSize(0)
{
#ifdef __ANDROID__
	m_pAsset = NULL;
#endif
}

CFileBase::~CFileBase()
{
	Destroy();
}

char* CFileBase::GetFileName()
{
	return m_filename;
}

void CFileBase::Destroy()
{
	Close();
	m_dwSize = 0;
}

void CFileBase::Close()
{
#ifdef __ANDROID__
	if (m_pAsset)
	{
		AAsset_close((AAsset*)m_pAsset);
		m_pAsset = NULL;
	}
#endif
	if (m_hFile)
	{
#ifdef M2_PORT
		if ((int)(intptr_t)m_hFile != -1)
			close((int)(intptr_t)m_hFile);
#else
		CloseHandle(m_hFile);
#endif
		m_hFile = NULL;
	}
}

BOOL CFileBase::Create(const char* filename, EFileMode mode)
{
	Destroy();

	strncpy(m_filename, filename, MAX_PATH);

#ifdef M2_PORT
#ifdef __ANDROID__
	if (mode == FILEMODE_READ && g_pAssetManager)
	{
		char normalizedPath[MAX_PATH];
		strncpy(normalizedPath, filename, MAX_PATH);
		for(int i=0; normalizedPath[i]; ++i)
			if(normalizedPath[i] == '\\') normalizedPath[i] = '/';
			
		AAsset* asset = AAssetManager_open(g_pAssetManager, normalizedPath, AASSET_MODE_RANDOM);
		if (asset)
		{
			m_pAsset = asset;
			m_dwSize = AAsset_getLength(asset);
			m_mode = mode;
			return true;
		}
	}
#endif

	int flags = (mode == FILEMODE_WRITE) ? (O_RDWR | O_CREAT) : O_RDONLY;
	int fd = open(filename, flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (fd == -1 && mode == FILEMODE_READ)
	{
		char szLower[MAX_PATH];
		M2Plat::NormalizePath(filename, szLower, sizeof(szLower));
		fd = open(szLower, flags);
		if (fd == -1 && M2Plat::MaterializeFile(filename))
			fd = open(szLower, flags);
	}
	m_hFile = (HANDLE)(intptr_t)fd;

	if (fd != -1)
	{
		struct stat st;
		fstat(fd, &st);
		m_dwSize = st.st_size;
		m_mode = mode;
		return true;
	}
#else
	DWORD dwMode, dwShareMode = FILE_SHARE_READ;

	if (mode == FILEMODE_WRITE)
	{
		dwMode = GENERIC_READ | GENERIC_WRITE;
		dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
	}
	else
		dwMode = GENERIC_READ;

	m_hFile = CreateFile(filename,					// name of the file
		dwMode,					// desired access
		dwShareMode,				// share mode
		NULL,						// security attributes
		mode == FILEMODE_READ ? OPEN_EXISTING : OPEN_ALWAYS, // creation disposition
		FILE_ATTRIBUTE_NORMAL,		// flags and attr
		NULL);						// template file

	if (m_hFile != (HANDLE)-1)
	{
		m_dwSize = GetFileSize(m_hFile, NULL);
		m_mode = mode;
		return true;
	}
#endif
	m_hFile = NULL;
	return false;
}

DWORD CFileBase::Size()
{
	return (m_dwSize);
}

void CFileBase::SeekCur(DWORD size)
{
#ifdef M2_PORT
#ifdef __ANDROID__
	if (m_pAsset) {
		AAsset_seek((AAsset*)m_pAsset, size, SEEK_CUR);
		return;
	}
#endif
	lseek((int)(intptr_t)m_hFile, size, SEEK_CUR);
#else
	SetFilePointer(m_hFile, size, NULL, FILE_CURRENT);
#endif
}

void CFileBase::Seek(DWORD offset)
{
	if (offset > m_dwSize)
		offset = m_dwSize;

#ifdef M2_PORT
#ifdef __ANDROID__
	if (m_pAsset) {
		AAsset_seek((AAsset*)m_pAsset, offset, SEEK_SET);
		return;
	}
#endif
	lseek((int)(intptr_t)m_hFile, offset, SEEK_SET);
#else
	SetFilePointer(m_hFile, offset, NULL, FILE_BEGIN);
#endif
}

DWORD CFileBase::GetPosition()
{
#ifdef M2_PORT
#ifdef __ANDROID__
	if (m_pAsset) {
		return AAsset_getLength((AAsset*)m_pAsset) - AAsset_getRemainingLength((AAsset*)m_pAsset);
	}
#endif
	return lseek((int)(intptr_t)m_hFile, 0, SEEK_CUR);
#else
	return SetFilePointer(m_hFile, 0, NULL, FILE_CURRENT);
#endif
}

BOOL CFileBase::Write(const void* src, int bytes)
{
#ifdef M2_PORT
#ifdef __ANDROID__
	if (m_pAsset) return false;
#endif
	ssize_t ret = write((int)(intptr_t)m_hFile, src, bytes);
	if (ret < 0) return false;
	struct stat st;
	fstat((int)(intptr_t)m_hFile, &st);
	m_dwSize = st.st_size;
	return true;
#else
	DWORD dwUseless;
	BOOL ret = WriteFile(m_hFile, src, bytes, &dwUseless, NULL);

	if (!ret)
		return false;

	m_dwSize = GetFileSize(m_hFile, NULL);
	return true;
#endif
}

BOOL CFileBase::Read(void* dest, int bytes)
{
#ifdef M2_PORT
#ifdef __ANDROID__
	if (m_pAsset) {
		int readBytes = AAsset_read((AAsset*)m_pAsset, dest, bytes);
		return readBytes >= 0 ? true : false;
	}
#endif
	ssize_t ret = read((int)(intptr_t)m_hFile, dest, bytes);
	return ret >= 0 ? true : false;
#else
	DWORD dwUseless;
	return ReadFile(m_hFile, dest, bytes, &dwUseless, NULL);
#endif
}

BOOL CFileBase::IsNull()
{
#ifdef M2_PORT
#ifdef __ANDROID__
	if (m_pAsset) return false;
#endif
	return ((int)(intptr_t)m_hFile == -1 || m_hFile == NULL) ? true : false;
#else
	return !m_hFile ? true : false;
#endif
}