#include "stb_image.h"
#include "StdAfx.h"

#include <assert.h>

#include "../EterBase/MappedFile.h"
#include "TGAImage.h"

CTGAImage::CTGAImage() : m_dwFlag(0)
{
}

CTGAImage::~CTGAImage()
{
}

CTGAImage::CTGAImage(CImage& image) : m_dwFlag(0)
{
	int w = image.GetWidth();
	int h = image.GetHeight();

	Create(w, h);

	DWORD* pdwDest = GetBasePointer();
	memcpy(pdwDest, image.GetBasePointer(), w * h * sizeof(DWORD));
	FlipTopToBottom();
}

void CTGAImage::Create(int width, int height)
{
	memset(&m_Header, 0, sizeof(m_Header));

	m_Header.imgType = 2;
	m_Header.width = (short)width;
	m_Header.height = (short)height;
	m_Header.colorBits = 32;
	m_Header.desc = 0x08;	// alpha channel ����

	CImage::Create(width, height);
}

bool CTGAImage::LoadFromMemory(int iSize, const BYTE* c_pbMem)
{
	int x, y, comp;
	unsigned char *data = stbi_load_from_memory((const stbi_uc*)c_pbMem, iSize, &x, &y, &comp, 4);
	if (!data)
		return false;

	memset(&m_Header, 0, sizeof(m_Header));
	m_Header.imgType = 2;
	m_Header.width = (short)x;
	m_Header.height = (short)y;
	m_Header.colorBits = 32;
	m_Header.desc = 0x08 | IMAGEDESC_TOPLEFT;

	CImage::Create(x, y);

	DWORD* pdwDest = GetBasePointer();
	UINT hxw = x * y;
	
	for (UINT i = 0; i < hxw; ++i)
	{
		BYTE r = data[i * 4 + 0];
		BYTE g = data[i * 4 + 1];
		BYTE b = data[i * 4 + 2];
		BYTE a = data[i * 4 + 3];
		pdwDest[i] = (a << 24) | (r << 16) | (g << 8) | b;
	}

	stbi_image_free(data);

	if (!(m_Header.desc & IMAGEDESC_TOPLEFT))
		FlipTopToBottom();

	return true;
}

bool CTGAImage::LoadFromDiskFile(const char* c_szFileName)
{
	CMappedFile file;

	const BYTE* c_pbMap;

	if (!file.Create(c_szFileName, (const void**)&c_pbMap, 0, 0))
		return false;

	return LoadFromMemory(file.Size(), c_pbMap);
}

int CTGAImage::GetRLEPixelCount(const DWORD* data)
{
	int r = 0;
	DWORD pixel;

	r = 1;

	if (data >= m_pdwEndPtr)
		return 0;

	pixel = *data;

	while ((r < 127) && (data < m_pdwEndPtr))
	{
		if (pixel != *(++data))
			return r;

		r++;
	}

	return r;
}

int CTGAImage::GetRawPixelCount(const DWORD* data)
{
	int i = 0;

	if (data >= m_pdwEndPtr)
		return 0;

	while ((data < m_pdwEndPtr) && (i < 127))
	{
		int rle = GetRLEPixelCount(data);

		if (rle >= 4)
			break;

		data++;
		i++;
	}

	return i;
}

void CTGAImage::SetCompressed(bool isCompress)
{
	if (isCompress)
		m_Header.imgType = 10;
	else
		m_Header.imgType = 2;
}

void CTGAImage::SetAlphaChannel(bool isExist)
{
	if (isExist)
		m_Header.desc |= 0x08;
	else
		m_Header.desc &= ~0x08;
}

bool CTGAImage::SaveToDiskFile(const char* c_szFileName)
{
	FILE* fp = fopen(c_szFileName, "wb");

	if (!fp)
		return false;

	fwrite(&m_Header, 18, 1, fp);

	if (m_Header.imgType == 10)	// RLE �������� ����
	{
		DWORD* data = GetBasePointer();
		m_pdwEndPtr = data + (GetWidth() * GetHeight());

		while (data < m_pdwEndPtr)
		{
			int rle = GetRLEPixelCount(data);

			if (rle < 4)
			{
				int raw = GetRawPixelCount(data);

				if (raw == 0)
					break;

				fputc(raw - 1, fp);

				while (raw)
				{
					fwrite(data, sizeof(DWORD), 1, fp);
					data++;
					raw--;
				}
			}
			else
			{
				fputc((rle - 1) | 0x80, fp);
				fwrite(data, sizeof(DWORD), 1, fp);
				data += rle;
			}
		}
	}
	else
	{
		int size = GetWidth();
		size *= GetHeight() * 4;
		fwrite(GetBasePointer(), size, 1, fp);
	}

	fclose(fp);
	return true;
}

TGA_HEADER& CTGAImage::GetHeader()
{
	return m_Header;
}