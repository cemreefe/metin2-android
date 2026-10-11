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

// Ray-vs-vertical-column test in world space: the crosshair ray runs along the
// camera view direction; a mob counts as picked when the ray passes within its
// body radius + pick margin, with hit height deciding body vs head. This is
// done in world space because the device's projection matrices hold UI state
// at update time and cannot be trusted here.
static DWORD PickCrosshairTarget(float fPickRadius, bool* pbHead)
{
	CPythonCharacterManager& rkChrMgr = CPythonCharacterManager::Instance();

	CInstanceBase* pkInstMain = rkChrMgr.GetMainInstancePtr();
	CCamera* pkCmr = CCameraManager::Instance().GetCurrentCamera();
	if (!pkInstMain || !pkCmr)
		return 0;

	const D3DXVECTOR3& v3Eye = pkCmr->GetEye();
	D3DXVECTOR3 v3Dir = pkCmr->GetTarget() - v3Eye;
	D3DXVec3Normalize(&v3Dir, &v3Dir);

	const float fMaxRange = 8000.0f;
	DWORD dwBestVID = 0;
	float fBestT = fMaxRange;
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

		// Camera space uses the -y flip convention (same as ProjectPosition).
		D3DXVECTOR3 c(c_rPos.x, -c_rPos.y, c_rPos.z);
		D3DXVECTOR3 toC = c - v3Eye;
		float fT = D3DXVec3Dot(&toC, &v3Dir);
		if (fT < 50.0f || fT > fMaxRange)
			continue;

		D3DXVECTOR3 v3Closest = v3Eye + v3Dir * fT;
		float fBodyR = max(v3Max.x - v3Min.x, v3Max.y - v3Min.y) * 0.5f;
		if (fBodyR < 30.0f)
			fBodyR = 30.0f;
		float fColR = fBodyR + fPickRadius * 0.8f;

		// horizontal miss distance from the mob's vertical axis
		float fDX = v3Closest.x - c.x;
		float fDY = v3Closest.y - c.y;
		float fMiss2 = fDX * fDX + fDY * fDY;
		if (fMiss2 > fColR * fColR)
			continue;
		// vertical range check: the ray may pass above/below the body
		float fHitZ = v3Closest.z;
		if (fHitZ < c.z - 20.0f || fHitZ > c.z + fH + 40.0f)
			continue;

		if (fT < fBestT)
		{
			dwBestVID = pkInst->GetVirtualID();
			fBestT = fT;
			bBestHead = (fHitZ - c.z) > fH * SHOOTER_HEAD_TOP_FRAC;
		}
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

	// Recoil: small camera kick per shot, decayed in ShooterUpdate.
	m_fShooterRecoil += m_isShooterAiming ? 0.15f : 0.3f;

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
		m_fShooterRecoil *= 0.82f;
		const float fPitch = ClampShooter(m_fShooterPitch - m_fShooterRecoil, SHOOTER_PITCH_MIN, SHOOTER_PITCH_MAX);
		pkCmrCur->Pitch(fPitch - pkCmrCur->GetPitch());
		pkCmrCur->Roll(m_fShooterYaw - pkCmrCur->GetRoll());
		pkCmrCur->SetDistance(m_isShooterAiming ? SHOOTER_AIM_DISTANCE : SHOOTER_DISTANCE);
		// The shoulder offset itself is applied in __UpdateCamera right after
		// SetPositionCamera rebuilds eye/target — eye/target writes done here
		// would be wiped by that rebuild before the next render.
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
