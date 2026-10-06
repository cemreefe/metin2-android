#ifdef M2_PORT
#include "windows.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace
{
	enum EGdiType { GDI_DC = 0x44430001, GDI_BITMAP = 0x424d0002, GDI_FONT = 0x464e0003 };

	struct SGdiBitmap { unsigned type; int width, height; unsigned* bits; };
	struct SGdiFont { unsigned type; float scale; int ascent, descent, lineGap; };
	struct SGdiDC { unsigned type; SGdiBitmap* bitmap; SGdiFont* font; };

	std::vector<unsigned char> s_fontData;
	stbtt_fontinfo s_fontInfo;
	bool s_fontLoaded = false;

	bool LoadSystemFont()
	{
		if (s_fontLoaded)
			return true;

		const char* c_szPaths[] = { M2Plat::FontFilePath() };
		for (const char* c_szPath : c_szPaths)
		{
			if (!c_szPath)
				continue;
			FILE* fp = fopen(c_szPath, "rb");
			if (!fp)
				continue;

			fseek(fp, 0, SEEK_END);
			long size = ftell(fp);
			fseek(fp, 0, SEEK_SET);
			s_fontData.resize(size);
			size_t read = fread(s_fontData.data(), 1, size, fp);
			fclose(fp);

			if (read == (size_t)size && stbtt_InitFont(&s_fontInfo, s_fontData.data(), stbtt_GetFontOffsetForIndex(s_fontData.data(), 0)))
			{
				s_fontLoaded = true;
				return true;
			}
		}
		return false;
	}

	SGdiDC* AsDC(HDC h)
	{
		SGdiDC* dc = (SGdiDC*)h;
		return (dc && dc->type == GDI_DC) ? dc : NULL;
	}

	int LineHeight(const SGdiFont* font)
	{
		return (int)ceilf((font->ascent - font->descent) * font->scale);
	}

	void GetGlyphABC(const SGdiFont* font, int codepoint, float* a, float* b, float* c)
	{
		int advance, lsb, x0, y0, x1, y1;
		stbtt_GetCodepointHMetrics(&s_fontInfo, codepoint, &advance, &lsb);
		stbtt_GetCodepointBitmapBox(&s_fontInfo, codepoint, font->scale, font->scale, &x0, &y0, &x1, &y1);
		*a = (float)x0;
		*b = (float)(x1 - x0);
		*c = advance * font->scale - *a - *b;
	}

	void DrawGlyph(SGdiDC* dc, int x, int y, int codepoint)
	{
		SGdiFont* font = dc->font;
		SGdiBitmap* bmp = dc->bitmap;
		int x0, y0, x1, y1;
		stbtt_GetCodepointBitmapBox(&s_fontInfo, codepoint, font->scale, font->scale, &x0, &y0, &x1, &y1);

		int glyphWidth = x1 - x0;
		int glyphHeight = y1 - y0;
		if (glyphWidth <= 0 || glyphHeight <= 0)
			return;

		std::vector<unsigned char> glyph(glyphWidth * glyphHeight);
		stbtt_MakeCodepointBitmap(&s_fontInfo, glyph.data(), glyphWidth, glyphHeight, glyphWidth, font->scale, font->scale, codepoint);

		int baseline = (int)ceilf(font->ascent * font->scale);
		int dstX = x + x0;
		int dstY = y + baseline + y0;

		for (int gy = 0; gy < glyphHeight; ++gy)
		{
			int py = dstY + gy;
			if (py < 0 || py >= bmp->height)
				continue;

			for (int gx = 0; gx < glyphWidth; ++gx)
			{
				int px = dstX + gx;
				if (px < 0 || px >= bmp->width)
					continue;

				unsigned v = glyph[gy * glyphWidth + gx];
				if (v)
					bmp->bits[py * bmp->width + px] = (v << 16) | (v << 8) | v;
			}
		}
	}
}

HDC CreateCompatibleDC(HDC)
{
	SGdiDC* dc = new SGdiDC;
	dc->type = GDI_DC;
	dc->bitmap = NULL;
	dc->font = NULL;
	return dc;
}

BOOL DeleteDC(HDC h)
{
	delete AsDC(h);
	return TRUE;
}

HBITMAP CreateDIBSection(HDC, const BITMAPINFO* pbmi, UINT, void** ppvBits, void*, DWORD)
{
	if (!pbmi)
		return NULL;

	SGdiBitmap* bmp = new SGdiBitmap;
	bmp->type = GDI_BITMAP;
	bmp->width = abs((int)pbmi->bmiHeader.biWidth);
	bmp->height = abs((int)pbmi->bmiHeader.biHeight);
	bmp->bits = (unsigned*)calloc((size_t)bmp->width * bmp->height, sizeof(unsigned));

	if (ppvBits)
		*ppvBits = bmp->bits;
	return bmp;
}

HFONT CreateFontIndirectA(const LOGFONTA* plf)
{
	if (!plf || !LoadSystemFont())
		return NULL;

	SGdiFont* font = new SGdiFont;
	font->type = GDI_FONT;
	stbtt_GetFontVMetrics(&s_fontInfo, &font->ascent, &font->descent, &font->lineGap);

	int height = plf->lfHeight ? (int)plf->lfHeight : 12;
	font->scale = height > 0 ? stbtt_ScaleForPixelHeight(&s_fontInfo, (float)height) : stbtt_ScaleForMappingEmToPixels(&s_fontInfo, (float)-height);
	return font;
}

HGDIOBJ SelectObject(HDC h, HGDIOBJ obj)
{
	SGdiDC* dc = AsDC(h);
	if (!dc || !obj)
		return NULL;

	unsigned type = *(unsigned*)obj;
	if (type == GDI_BITMAP)
	{
		HGDIOBJ old = dc->bitmap;
		dc->bitmap = (SGdiBitmap*)obj;
		return old;
	}
	if (type == GDI_FONT)
	{
		HGDIOBJ old = dc->font;
		dc->font = (SGdiFont*)obj;
		return old;
	}
	return NULL;
}

unsigned SetTextColor(HDC, unsigned c)
{
	return c;
}

BOOL GetTextExtentPoint32W(HDC h, const wchar_t* s, int c, LPSIZE sz)
{
	SGdiDC* dc = AsDC(h);
	if (!dc || !dc->font || !sz)
		return FALSE;

	float width = 0.0f;
	for (int i = 0; i < c; ++i)
	{
		int advance, lsb;
		stbtt_GetCodepointHMetrics(&s_fontInfo, s[i], &advance, &lsb);
		width += advance * dc->font->scale;
	}
	sz->cx = (LONG)ceilf(width);
	sz->cy = LineHeight(dc->font);
	return TRUE;
}

BOOL GetTextExtentPoint32A(HDC h, LPCSTR s, int c, LPSIZE sz)
{
	std::vector<wchar_t> ws(c > 0 ? c : 0);
	for (int i = 0; i < c; ++i)
		ws[i] = (unsigned char)s[i];
	return GetTextExtentPoint32W(h, ws.data(), c, sz);
}

BOOL GetCharABCWidthsFloatW(HDC h, unsigned short first, unsigned short last, LPABCFLOAT abc)
{
	SGdiDC* dc = AsDC(h);
	if (!dc || !dc->font || !abc)
		return FALSE;

	for (unsigned ch = first; ch <= last; ++ch, ++abc)
		GetGlyphABC(dc->font, ch, &abc->abcfA, &abc->abcfB, &abc->abcfC);
	return TRUE;
}

BOOL GetCharABCWidthsFloatA(HDC h, unsigned first, unsigned last, LPABCFLOAT abc)
{
	return GetCharABCWidthsFloatW(h, (unsigned short)first, (unsigned short)last, abc);
}

BOOL TextOutW(HDC h, int x, int y, const wchar_t* s, int c)
{
	SGdiDC* dc = AsDC(h);
	if (!dc || !dc->font || !dc->bitmap || !s)
		return FALSE;

	float penX = (float)x;
	for (int i = 0; i < c; ++i)
	{
		DrawGlyph(dc, (int)penX, y, s[i]);
		int advance, lsb;
		stbtt_GetCodepointHMetrics(&s_fontInfo, s[i], &advance, &lsb);
		penX += advance * dc->font->scale;
	}
	return TRUE;
}

BOOL TextOutA(HDC h, int x, int y, LPCSTR s, int c)
{
	if (!s)
		return FALSE;
	std::vector<wchar_t> ws(c > 0 ? c : 0);
	for (int i = 0; i < c; ++i)
		ws[i] = (unsigned char)s[i];
	return TextOutW(h, x, y, ws.data(), c);
}
#endif
