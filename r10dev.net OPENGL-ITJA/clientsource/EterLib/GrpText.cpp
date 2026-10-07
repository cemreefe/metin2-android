#include "StdAfx.h"
#include "../EterBase/Utils.h"
#include "GrpText.h"

CGraphicText::CGraphicText(const char* c_szFileName) : CResource(c_szFileName)
{
}

CGraphicText::~CGraphicText()
{
}

bool CGraphicText::CreateDeviceObjects()
{
	return m_fontTexture.CreateDeviceObjects();
}

void CGraphicText::DestroyDeviceObjects()
{
	m_fontTexture.DestroyDeviceObjects();
}

CGraphicFontTexture* CGraphicText::GetFontTexturePointer()
{
	return &m_fontTexture;
}

CGraphicText::TType CGraphicText::Type()
{
	static TType s_type = StringToType("CGraphicText");
	return s_type;
}

float CGraphicText::ms_fFontScale = 1.0f;

void CGraphicText::SetGlobalFontScale(float fScale)
{
	ms_fFontScale = fScale;
}

float CGraphicText::GetGlobalFontScale()
{
	return ms_fFontScale;
}

bool CGraphicText::OnLoad(int /*iSize*/, const void* /*c_pvBuf*/)
{
	static char strName[32];
	int size;
	bool bItalic = false;

	// format
	// ����.fnt		"����" ��Ʈ �⺻ ������ 12 �� �ε�
	// ����:18.fnt  "����" ��Ʈ ������ 18 �� �ε�
	// ����:14i.fnt "����" ��Ʈ ������ 14 & ���Ÿ����� �ε�
	const char* p = strrchr(GetFileName(), ':');

	if (p)
	{
		strncpy(strName, GetFileName(), MIN(31, p - GetFileName()));
		++p;

		static char num[8];

		int i = 0;
		while (*p && isdigit(*p))
		{
			num[i++] = *(p++);
		}

		num[i] = '\0';
		if (*p == 'i')
			bItalic = true;
		size = atoi(num);
	}
	else
	{
		p = strrchr(GetFileName(), '.');

		if (!p)
		{
			assert(!"CGraphicText::OnLoadFromFile there is no extension (ie: .fnt)");
			strName[0] = '\0';
		}
		else
			strncpy(strName, GetFileName(), MIN(31, p - GetFileName()));

		size = 12;
	}

	size = int(size * ms_fFontScale + 0.5f);
	if (size < 1)
		size = 1;

	if (!m_fontTexture.Create(strName, size, bItalic))
		return false;

	return true;
}

void CGraphicText::OnClear()
{
	m_fontTexture.Destroy();
}

bool CGraphicText::OnIsEmpty() const
{
	return m_fontTexture.IsEmpty();
}

bool CGraphicText::OnIsType(TType type)
{
	if (CGraphicText::Type() == type)
		return true;

	return CResource::OnIsType(type);
}