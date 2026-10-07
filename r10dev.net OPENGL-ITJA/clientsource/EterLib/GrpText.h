#pragma once

#include "Resource.h"
#include "Ref.h"
#include "GrpFontTexture.h"

class CGraphicText : public CResource
{
public:
	typedef CRef<CGraphicText> TRef;

public:
	static TType Type();

public:
	CGraphicText(const char* c_szFileName);
	virtual ~CGraphicText();

	// Multiplies the generation size of every .fnt load (display.cfg "font_scale").
	// Applied when the atlas is created; already-loaded fonts pick up a change via
	// CResourceManager::ReloadResourcesOfType + CGraphicTextInstance::RefreshAll.
	static void			SetGlobalFontScale(float fScale);
	static float		GetGlobalFontScale();
	// Surface pixels per UI unit: atlases rasterize at this density so text stays sharp
	// when the logical UI is stretched to a bigger screen.
	static void			SetRasterScale(float fScale);
	static float		GetRasterScale();

protected:
	static float		ms_fFontScale;
	static float		ms_fRasterScale;

public:

	virtual bool			CreateDeviceObjects();
	virtual void			DestroyDeviceObjects();

	CGraphicFontTexture* GetFontTexturePointer();

protected:
	bool		OnLoad(int iSize, const void* c_pvBuf);
	void		OnClear();
	bool		OnIsEmpty() const;
	bool		OnIsType(TType type);

protected:
	CGraphicFontTexture m_fontTexture;
};
