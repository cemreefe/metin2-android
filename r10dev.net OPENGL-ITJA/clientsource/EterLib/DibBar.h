#pragma once

#include "GrpDIB.h"

class CBlockTexture;

class CDibBar
{
public:
	CDibBar();
	virtual ~CDibBar();

	bool Create(HDC hdc, DWORD dwWidth, DWORD dwHeight);
	void Invalidate();
	void SetClipRect(const RECT& c_rRect);
	void ClearBar();
	void Render(int ix, int iy);

protected:
	DWORD __NearTextureSize(DWORD dwSize);
	void __DivideTextureSize(DWORD dwSize, DWORD dwMax, DWORD* pdwxStep, DWORD* pdwxCount, DWORD* pdwxRest);
	CBlockTexture* __BuildTextureBlock(LONG dwxPos, LONG dwyPos, LONG dwImageWidth, LONG dwImageHeight, DWORD dwTextureWidth, DWORD dwTextureHeight);
	void __BuildTextureBlockList(DWORD dwWidth, DWORD dwHeight, DWORD dwMax = 256);

	virtual void OnCreate() {}

protected:
	CGraphicDib m_dib;
	std::vector<CBlockTexture*> m_kVec_pkBlockTexture;

	LONG m_dwWidth;
	LONG m_dwHeight;
};
