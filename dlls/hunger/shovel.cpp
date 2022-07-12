/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "gamerules.h"

#define	SHOVEL_BODYHIT_VOLUME 128
#define	SHOVEL_WALLHIT_VOLUME 512

LINK_ENTITY_TO_CLASS( weapon_th_shovel, CWeaponEinarShovel )

enum shovel_e
{
	SHOVEL_IDLE = 0,
	SHOVEL_DRAW,
	SHOVEL_HOLSTER,
	SHOVEL_ATTACK1HIT,
	SHOVEL_ATTACK1MISS,
	SHOVEL_ATTACK2MISS,
	SHOVEL_ATTACK2HIT,
	SHOVEL_ATTACK3MISS,
	SHOVEL_ATTACK3HIT,
	SHOVEL_IDLE2,
	SHOVEL_IDLE3
};

void CWeaponEinarShovel::Spawn()
{
	Precache();
	m_iId = WEAPON_SHOVEL;
	SET_MODEL( ENT( pev ), "models/w_shovel.mdl" );
	m_iClip = -1;

	FallInit();// get ready to fall down.
}

void CWeaponEinarShovel::Precache()
{
	PRECACHE_MODEL( "models/v_shovel.mdl" );
	PRECACHE_MODEL( "models/w_shovel.mdl" );
	PRECACHE_MODEL( "models/p_shovel.mdl" );

	PRECACHE_SOUND( "weapons/cbar_hit1.wav" );
	PRECACHE_SOUND( "weapons/cbar_hit2.wav" );
	PRECACHE_SOUND( "weapons/cbar_hitbod1.wav" );
	PRECACHE_SOUND( "weapons/cbar_hitbod2.wav" );
	PRECACHE_SOUND( "weapons/cbar_hitbod3.wav" );
	PRECACHE_SOUND( "weapons/cbar_miss1.wav");

	PRECACHE_SOUND( "kelly/cbar_hitkelly1.wav" );
	PRECACHE_SOUND( "kelly/cbar_hitkelly2.wav" );
	PRECACHE_SOUND( "kelly/cbar_hitkelly3.wav" );

}

int CWeaponEinarShovel::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 1;
	p->iId = WEAPON_SHOVEL;
	p->iWeight = SHOVEL_WEIGHT;
	return 1;
}

/*int CWeaponEinarShovel::AddToPlayer(CBasePlayer* pPlayer)
{
	if( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev );
			WRITE_BYTE( m_iId );
		MESSAGE_END();
		return TRUE;
	}
	return FALSE;
}*/

BOOL CWeaponEinarShovel::Deploy()
{
	return DefaultDeploy( "models/v_shovel.mdl", "models/p_shovel.mdl", SHOVEL_DRAW, "crowbar" );
}

void CWeaponEinarShovel::Holster( int skiplocal /* = 0 */ )
{
	KillLaser();

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5f;
	SendWeaponAnim( SHOVEL_HOLSTER );
}

#define SHOVEL_MIN_SWING_SPEED 70
#define SHOVEL_LENGTH 24

void CWeaponEinarShovel::ItemPostFrame()
{
	MakeLaser();
#ifndef CLIENT_DLL
	Vector weaponVelocity = m_pPlayer->GetWeaponVelocity();
	float speed = weaponVelocity.Length();
	if (speed >= SHOVEL_MIN_SWING_SPEED)
	{
		if (!playedWooshSound)
		{
			// prevent w-w-woo-woosh stutters when player waves crowbar around frantically
			if (gpGlobals->time > lastWooshSoundTime + 0.5f)
			{
				EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "weapons/cbar_miss1.wav", 1, ATTN_NORM, 0, 98 + RANDOM_LONG(0, 3));
				lastWooshSoundTime = gpGlobals->time;
			}
			playedWooshSound = true;
		}
		CheckSmack(speed);
	}
	else
	{
		playedWooshSound = false;
		hitCount = 0;
		ClearEntitiesHitThisSwing();
	}
#endif
}

//Uncomment to debug the crowbar
//#define SHOW_SHOVEL_DAMAGE_LINE

void CWeaponEinarShovel::MakeLaser(void)
{

#ifndef CLIENT_DLL

	//This is for debugging the crowbar
#ifdef SHOW_SHOVEL_DAMAGE_LINE
	TraceResult tr;

	// ALERT( at_console, "serverflags %f\n", gpGlobals->serverflags );

	UTIL_MakeVectors(m_pPlayer->GetWeaponViewAngles());
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecEnd = vecSrc + gpGlobals->v_up * SHOVEL_LENGTH;
	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, ENT(pev), &tr);

	float flBeamLength = tr.flFraction;

	if (!g_pLaser || !(g_pLaser->pev)) {
		g_pLaser = CBeam::BeamCreate(g_pModelNameLaser, 3);
	}

	g_pLaser->PointsInit(vecSrc, vecEnd);
	g_pLaser->SetColor(214, 34, 34);
	g_pLaser->SetScrollRate(255);
	g_pLaser->SetBrightness(96);
	g_pLaser->pev->spawnflags |= SF_BEAM_TEMPORARY;	// Flag these to be destroyed on save/restore or level transition
	g_pLaser->pev->owner = m_pPlayer->edict();
#else
	//Normally the crowbar doesn't have a laser sight
	KillLaser();
#endif //SHOW_CROWBAR_DAMAGE_LINE

#endif
}

#ifndef CLIENT_DLL

void CWeaponEinarShovel::CheckSmack(float speed)
{
	UTIL_MakeVectors(m_pPlayer->GetWeaponViewAngles());
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecEnd = vecSrc + gpGlobals->v_up * SHOVEL_LENGTH;

	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, ENT(m_pPlayer->pev), &tr);

	if (!tr.fStartSolid && !tr.fAllSolid && tr.flFraction < 1.0)	// we hit something!
	{
		CBaseEntity* pEntity = CBaseEntity::Instance(tr.pHit);

		if (pEntity && HasNotHitThisEntityThisSwing(pEntity))
		{
			RememberHasHitThisEntityThisSwing(pEntity);

			// play thwack, smack, or dong sound
			float flVol = 1.0;

			hitCount++;

			ClearMultiDamage();
			// hit damage is greater when swing is faster + weaker for every additional hit
			pEntity->TraceAttack(m_pPlayer->pev, gSkillData.plrDmgCrowbar * (speed / SHOVEL_MIN_SWING_SPEED) * (1.f / hitCount), gpGlobals->v_up, &tr, DMG_CLUB);
			ApplyMultiDamage(m_pPlayer->pev, m_pPlayer->pev);

			if (pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE)
			{
				// Skeletons make different hit sounds.
				if (pEntity->Classify() == CLASS_SKELETON)
				{
					// play thwack or smack sound
					switch (RANDOM_LONG(0, 2))
					{
					case 0:
						EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_ITEM, "kelly/cbar_hitkelly1.wav", 1, ATTN_NORM);
						break;
					case 1:
						EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_ITEM, "kelly/cbar_hitkelly2.wav", 1, ATTN_NORM);
						break;
					case 2:
						EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_ITEM, "kelly/cbar_hitkelly3.wav", 1, ATTN_NORM);
						break;
					}
				}
				else
				{
					// play thwack or smack sound
					switch (RANDOM_LONG(0, 2))
					{
					case 0:
						EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/cbar_hitbod1.wav", 1, ATTN_NORM);
						break;
					case 1:
						EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/cbar_hitbod2.wav", 1, ATTN_NORM);
						break;
					case 2:
						EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/cbar_hitbod3.wav", 1, ATTN_NORM);
						break;
					}
				}
			m_pPlayer->m_iWeaponVolume = SHOVEL_BODYHIT_VOLUME;
			if( !pEntity->IsAlive() )
				return;
			else
				flVol = 0.1f;
			}
			else
			{
				float fvolbar = TEXTURETYPE_PlaySound(&tr, vecSrc, vecSrc + (vecEnd - vecSrc) * 2, BULLET_PLAYER_CROWBAR);

				if (g_pGameRules->IsMultiplayer())
				{
					// override the volume here, cause we don't play texture sounds in multiplayer, 
					// and fvolbar is going to be 0 from the above call.

					fvolbar = 1;
				}

				// also play crowbar strike
				switch (RANDOM_LONG(0, 1))
				{
				case 0:
					EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/cbar_hit1.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0, 3));
					break;
				case 1:
					EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/cbar_hit2.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0, 3));
					break;
				}
			}

			//vibrate a bit
			char buffer[256];
			sprintf(buffer, "vibrate 90.0 %i 1.0\n", 1 - (int)CVAR_GET_FLOAT("hand"));
			SERVER_COMMAND(buffer);

			m_pPlayer->m_iWeaponVolume = flVol * SHOVEL_WALLHIT_VOLUME;
			DecalGunshot(&tr, BULLET_PLAYER_CROWBAR);
		}
	}
}

bool CWeaponEinarShovel::HasNotHitThisEntityThisSwing(CBaseEntity* pEntity)
{
	int hitEntitiesCount = sizeof(hitEntities) / sizeof(EHANDLE);
	for (int i = 0; i < hitEntitiesCount; i++)
	{
		if (((CBaseEntity*)hitEntities[i]) == pEntity)
		{
			return false;
		}
	}
	return true;
}

void CWeaponEinarShovel::RememberHasHitThisEntityThisSwing(CBaseEntity* pEntity)
{
	int hitEntitiesCount = sizeof(hitEntities) / sizeof(EHANDLE);
	for (int i = 0; i < hitEntitiesCount; i++)
	{
		if (hitEntities[i])
		{
			continue;
		}
		else
		{
			hitEntities[i] = pEntity;
			return;
		}
	}
}

void CWeaponEinarShovel::ClearEntitiesHitThisSwing()
{
	int hitEntitiesCount = sizeof(hitEntities) / sizeof(EHANDLE);
	for (int i = 0; i < hitEntitiesCount; i++)
	{
		hitEntities[i] = NULL;
	}
}

#endif