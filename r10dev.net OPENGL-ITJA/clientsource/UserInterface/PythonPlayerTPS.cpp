#include "StdAfx.h"
#include "PythonPlayer.h"

#include "PythonApplication.h"
#include "PythonCharacterManager.h"
#include "../EterPythonLib/PythonGraphic.h"
#include "PythonNetworkStream.h"
#include "InstanceBase.h"
#include "../EterLib/Camera.h"
#include "../GameLib/FlyingObjectManager.h"
#include "../platform/m2platform.h"

// Third-person shooter mode. Toggle with F6 (or the TPS HUD button).
// The camera is driven to an absolute yaw/pitch over the shoulder, LMB fires at
// whatever sits under the crosshair, RMB aims down sights (zoom + tighter pick
// radius). A head pick sends a different shoot type so the server can give it a
// higher crit chance.
//
// Server shoot types (TPacketCGShoot.bType):
//   60 = body shot, 61 = head shot (higher crit chance)

namespace
{
	const float SHOOTER_LOOK_SENS_X = 0.18f;
	const float SHOOTER_LOOK_SENS_Y = 0.14f;
	const float SHOOTER_PITCH_MIN   = -25.0f;
	const float SHOOTER_PITCH_MAX   = 70.0f;
	const float SHOOTER_DISTANCE    = 260.0f;
	const float SHOOTER_AIM_DISTANCE = 110.0f;
	const DWORD SHOOTER_SHOT_MS     = 300;
	const DWORD SHOOTER_AIM_SHOT_MS = 420;
	const float SHOOTER_PICK_RADIUS  = 90.0f;
	const float SHOOTER_AIM_PICK_RADIUS = 45.0f;
	const float SHOOTER_HEAD_TOP_FRAC  = 0.85f; // top of the bounding box counts as the head zone
	const char* SHOOTER_BULLET_FLYDATA = "d:/ymir work/pc/tps/bullet.msf";

	float ClampShooter(float v, float lo, float hi)
	{
		return v < lo ? lo : (v > hi ? hi : v);
	}
}

void CPythonPlayer::ShooterModeToggle()
{
	m_isShooterMode = !m_isShooterMode;

	if (m_isShooterMode)
	{
		if (CCamera* pkCmrCur = CCameraManager::Instance().GetCurrentCamera())
		{
			m_fShooterYaw = pkCmrCur->GetRoll();
			m_fShooterPitch = pkCmrCur->GetPitch();
			m_fShooterRestDistance = pkCmrCur->GetDistance();
		}
		m_isShooterAiming = false;
		m_isShooterFiring = false;
		m_dwShooterLastShotTime = 0;
		M2Plat::SetPointerVisible(false);
		M2Plat::SetPointerLocked(true);
	}
	else
	{
		m_isShooterAiming = false;
		m_isShooterFiring = false;
		if (CCamera* pkCmrCur = CCameraManager::Instance().GetCurrentCamera())
			pkCmrCur->SetDistance(m_fShooterRestDistance);
		M2Plat::SetPointerLocked(false);
		M2Plat::SetPointerVisible(true);
	}
}

void CPythonPlayer::ShooterOnLook(int iDeltaX, int iDeltaY)
{
	if (!m_isShooterMode)
		return;

	float fSens = m_isShooterAiming ? 0.5f : 1.0f;
	m_fShooterYaw   += iDeltaX * SHOOTER_LOOK_SENS_X * fSens;
	m_fShooterPitch += iDeltaY * SHOOTER_LOOK_SENS_Y * fSens;
	m_fShooterPitch = ClampShooter(m_fShooterPitch, SHOOTER_PITCH_MIN, SHOOTER_PITCH_MAX);
}

void CPythonPlayer::ShooterSetAim(bool isAim)
{
	m_isShooterAiming = m_isShooterMode && isAim;
}

void CPythonPlayer::ShooterSetFire(bool isFire)
{
	m_isShooterFiring = m_isShooterMode && isFire;
}

// Projects an attackable character to screen space and keeps it if it is the
// closest one to the crosshair. Returns the VID or 0 when nothing is aimed at.
static DWORD PickCrosshairTarget(float fPickRadius, bool* pbHead)
{
	CPythonCharacterManager& rkChrMgr = CPythonCharacterManager::Instance();
	CPythonGraphic& rkGraphic = CPythonGraphic::Instance();

	CInstanceBase* pkInstMain = rkChrMgr.GetMainInstancePtr();
	if (!pkInstMain)
		return 0;

	const int iSurfW = M2Plat::SurfaceWidth();
	const int iSurfH = M2Plat::SurfaceHeight();
	const float fCX = iSurfW * 0.5f;
	const float fCY = iSurfH * 0.5f;

	DWORD dwBestVID = 0;
	float fBestDist = fPickRadius;
	bool  bBestHead = false;

	for (CPythonCharacterManager::CharacterIterator itor = rkChrMgr.CharacterInstanceBegin();
		itor != rkChrMgr.CharacterInstanceEnd(); ++itor)
	{
		CInstanceBase* pkInst = *itor;
		if (!pkInst || pkInst == pkInstMain)
			continue;
		if (pkInst->IsDead())
			continue;
		// Mobs are the shoot targets: skip friendly players and town NPCs.
		if (pkInst->IsNPC())
			continue;
		if (pkInst->IsPC() && !pkInst->IsEnemy())
			continue;

		TPixelPosition c_rPos;
		pkInst->NEW_GetPixelPosition(&c_rPos);

		D3DXVECTOR3 v3Min, v3Max;
		pkInst->GetBoundBox(&v3Min, &v3Max);
		float fH = v3Max.z - v3Min.z;
		if (fH < 50.0f)
			fH = 180.0f;

		float fSX, fSY;
		rkGraphic.ProjectPosition(c_rPos.x, -c_rPos.y, c_rPos.z + fH * 0.5f, &fSX, &fSY);

		float fDist = sqrtf((fSX - fCX) * (fSX - fCX) + (fSY - fCY) * (fSY - fCY));
		if (fDist > fBestDist)
			continue;

		float fHX, fHY;
		rkGraphic.ProjectPosition(c_rPos.x, -c_rPos.y, c_rPos.z + fH * SHOOTER_HEAD_TOP_FRAC, &fHX, &fHY);
		bool bHead = (fCY < fHY) && fabsf(fSX - fCX) < fPickRadius;

		dwBestVID = pkInst->GetVirtualID();
		fBestDist = fDist;
		bBestHead = bHead;
	}

	if (pbHead)
		*pbHead = bBestHead;
	return dwBestVID;
}

static void SpawnShooterTracer(CInstanceBase* pkInstMain, DWORD dwFlyIndex)
{
	if (!dwFlyIndex)
		return;

	CPythonGraphic& rkGraphic = CPythonGraphic::Instance();
	const int iSurfW = M2Plat::SurfaceWidth();
	const int iSurfH = M2Plat::SurfaceHeight();

	// far end of the view ray through the crosshair
	float fEX, fEY, fEZ;
	rkGraphic.UnprojectPosition(iSurfW * 0.5f, iSurfH * 0.5f, 0.99f, &fEX, &fEY, &fEZ);

	TPixelPosition c_rPos;
	pkInstMain->NEW_GetPixelPosition(&c_rPos);
	D3DXVECTOR3 v3Start(c_rPos.x, -c_rPos.y, c_rPos.z + 145.0f);
	D3DXVECTOR3 v3End(fEX, fEY, fEZ);

	CFlyTarget kFlyTarget(v3End);
	CFlyingManager::Instance().CreateFlyingInstanceFlyTarget(dwFlyIndex, v3Start, kFlyTarget, false);
}

void CPythonPlayer::ShooterFireOnce()
{
	CInstanceBase* pkInstMain = NEW_GetMainActorPtr();
	if (!pkInstMain)
		return;

	// Mimed shot: the bow-attack motion reads as shouldering the gun. Its fly
	// event needs m_kFlyTarget, which stays cleared, so no arrow spawns and no
	// second shot packet goes out — the tracer below is the only projectile.
	{
		CActorInstance& rkAct = pkInstMain->GetGraphicThingInstanceRef();
		rkAct.ClearFlyTarget();
		rkAct.InterceptOnceMotion(rkAct.GetNormalAttackIndex(), 0.1f);
	}

	bool bHead = false;
	DWORD dwVID = PickCrosshairTarget(m_isShooterAiming ? SHOOTER_AIM_PICK_RADIUS : SHOOTER_PICK_RADIUS, &bHead);

	if (!m_dwShooterFlyIndex)
	{
		DWORD dwCRC = 0;
		if (CFlyingManager::Instance().RegisterFlyingData(SHOOTER_BULLET_FLYDATA, dwCRC))
			m_dwShooterFlyIndex = dwCRC;
	}

	SpawnShooterTracer(pkInstMain, m_dwShooterFlyIndex);

	if (dwVID)
	{
		TPixelPosition c_rTargetPos;
		CPythonCharacterManager::Instance()
			.GetInstancePtr(dwVID)->NEW_GetPixelPosition(&c_rTargetPos);

		CPythonNetworkStream& rkStream = CPythonNetworkStream::Instance();
		rkStream.SendFlyTargetingPacket(dwVID, c_rTargetPos);
		rkStream.SendShootPacket(bHead ? 61 : 60);
	}
}

void CPythonPlayer::ShooterUpdate()
{
	if (!m_isShooterMode)
		return;

	if (CCamera* pkCmrCur = CCameraManager::Instance().GetCurrentCamera())
	{
		pkCmrCur->Pitch(m_fShooterPitch - pkCmrCur->GetPitch());
		pkCmrCur->Roll(m_fShooterYaw - pkCmrCur->GetRoll());
		pkCmrCur->SetDistance(m_isShooterAiming ? SHOOTER_AIM_DISTANCE : SHOOTER_DISTANCE);
	}

	if (m_isShooterFiring)
	{
		DWORD dwNow = ELTimer_GetMSec();
		if (dwNow - m_dwShooterLastShotTime >= (m_isShooterAiming ? SHOOTER_AIM_SHOT_MS : SHOOTER_SHOT_MS))
		{
			m_dwShooterLastShotTime = dwNow;
			ShooterFireOnce();
		}
	}
}

void CPythonPlayer::ShooterRenderCrosshair()
{
	if (!m_isShooterMode)
		return;

	const int iSurfW = M2Plat::SurfaceWidth();
	const int iSurfH = M2Plat::SurfaceHeight();
	const float fCX = iSurfW * 0.5f;
	const float fCY = iSurfH * 0.5f;
	const float fGap = m_isShooterAiming ? 3.0f : 6.0f;
	const float fLen = m_isShooterAiming ? 10.0f : 14.0f;

	CPythonGraphic& rkGraphic = CPythonGraphic::Instance();
	rkGraphic.SetDiffuseColor(0.1f, 1.0f, 0.4f, 0.9f);
	rkGraphic.RenderLine2d(fCX - fGap - fLen, fCY, fCX - fGap, fCY);
	rkGraphic.RenderLine2d(fCX + fGap, fCY, fCX + fGap + fLen, fCY);
	rkGraphic.RenderLine2d(fCX, fCY - fGap - fLen, fCX, fCY - fGap);
	rkGraphic.RenderLine2d(fCX, fCY + fGap, fCX, fCY + fGap + fLen);
}
