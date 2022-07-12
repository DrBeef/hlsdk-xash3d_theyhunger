/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "entity_types.h"
#include "usercmd.h"
#include "pm_defs.h"
#include "pm_materials.h"

#include "eventscripts.h"
#include "ev_hldm.h"

#include "r_efx.h"
#include "event_api.h"
#include "event_args.h"
#include "in_defs.h"

#include <string.h>

#include "r_studioint.h"
#include "com_model.h"

extern engine_studio_api_t IEngineStudio;

static int g_tracerCount[32];

extern "C" char PM_FindTextureType( char *name );

void V_PunchAxis( int axis, float punch );
void VectorAngles( const float *forward, float *angles );

extern cvar_t *cl_lw;

extern "C"
{
// HLDM
void EV_FireGlock1( struct event_args_s *args );
void EV_FireGlock2( struct event_args_s *args );
void EV_FireShotGunSingle( struct event_args_s *args );
void EV_FireShotGunDouble( struct event_args_s *args );
void EV_FireMP5( struct event_args_s *args );
void EV_FireMP52( struct event_args_s *args );
void EV_FirePython( struct event_args_s *args );
void EV_FireGauss( struct event_args_s *args );
void EV_SpinGauss( struct event_args_s *args );
void EV_Crowbar( struct event_args_s *args );
void EV_FireCrossbow( struct event_args_s *args );
void EV_FireCrossbow2( struct event_args_s *args );
void EV_FireRpg( struct event_args_s *args );
void EV_EgonFire( struct event_args_s *args );
void EV_EgonStop( struct event_args_s *args );
void EV_TripmineFire( struct event_args_s *args );
void EV_SnarkFire( struct event_args_s *args );

void EV_TrainPitchAdjust( struct event_args_s *args );

void EV_FireAP9( struct event_args_s *args );
void EV_FireChaingun1( struct event_args_s *args );
void EV_FireChaingun2( struct event_args_s *args );
void EV_FireSniper( struct event_args_s *args );
void EV_FireSniper2( struct event_args_s *args );
void EV_FireTaurus( struct event_args_s *args );
}

#define VECTOR_CONE_1DEGREES Vector( 0.00873, 0.00873, 0.00873 )
#define VECTOR_CONE_2DEGREES Vector( 0.01745, 0.01745, 0.01745 )
#define VECTOR_CONE_3DEGREES Vector( 0.02618, 0.02618, 0.02618 )
#define VECTOR_CONE_4DEGREES Vector( 0.03490, 0.03490, 0.03490 )
#define VECTOR_CONE_5DEGREES Vector( 0.04362, 0.04362, 0.04362 )
#define VECTOR_CONE_6DEGREES Vector( 0.05234, 0.05234, 0.05234 )
#define VECTOR_CONE_7DEGREES Vector( 0.06105, 0.06105, 0.06105 )	
#define VECTOR_CONE_8DEGREES Vector( 0.06976, 0.06976, 0.06976 )
#define VECTOR_CONE_9DEGREES Vector( 0.07846, 0.07846, 0.07846 )
#define VECTOR_CONE_10DEGREES Vector( 0.08716, 0.08716, 0.08716 )
#define VECTOR_CONE_15DEGREES Vector( 0.13053, 0.13053, 0.13053 )
#define VECTOR_CONE_20DEGREES Vector( 0.17365, 0.17365, 0.17365 )

// play a strike sound based on the texture that was hit by the attack traceline.  VecSrc/VecEnd are the
// original traceline endpoints used by the attacker, iBulletType is the type of bullet that hit the texture.
// returns volume of strike instrument (crowbar) to play
float EV_HLDM_PlayTextureSound( int idx, pmtrace_t *ptr, float *vecSrc, float *vecEnd, int iBulletType )
{
	// hit the world, try to play sound based on texture material type
	char chTextureType = CHAR_TEX_CONCRETE;
	float fvol;
	float fvolbar;
	const char *rgsz[4];
	int cnt;
	float fattn = ATTN_NORM;
	int entity;
	char *pTextureName;
	char texname[64];
	char szbuffer[64];

	entity = gEngfuncs.pEventAPI->EV_IndexFromTrace( ptr );

	// FIXME check if playtexture sounds movevar is set
	//
	chTextureType = 0;

	// Player
	if( entity >= 1 && entity <= gEngfuncs.GetMaxClients() )
	{
		// hit body
		chTextureType = CHAR_TEX_FLESH;
	}
	else if( entity == 0 )
	{
		// get texture from entity or world (world is ent(0))
		pTextureName = (char *)gEngfuncs.pEventAPI->EV_TraceTexture( ptr->ent, vecSrc, vecEnd );

		if ( pTextureName )
		{
			strcpy( texname, pTextureName );
			pTextureName = texname;

			// strip leading '-0' or '+0~' or '{' or '!'
			if( *pTextureName == '-' || *pTextureName == '+' )
			{
				pTextureName += 2;
			}

			if( *pTextureName == '{' || *pTextureName == '!' || *pTextureName == '~' || *pTextureName == ' ' )
			{
				pTextureName++;
			}

			// '}}'
			strcpy( szbuffer, pTextureName );
			szbuffer[CBTEXTURENAMEMAX - 1] = 0;

			// get texture type
			chTextureType = PM_FindTextureType( szbuffer );
		}
	}
	
	switch (chTextureType)
	{
	default:
	case CHAR_TEX_CONCRETE:
		fvol = 0.9;
		fvolbar = 0.6;
		rgsz[0] = "player/pl_step1.wav";
		rgsz[1] = "player/pl_step2.wav";
		cnt = 2;
		break;
	case CHAR_TEX_METAL:
		fvol = 0.9;
		fvolbar = 0.3;
		rgsz[0] = "player/pl_metal1.wav";
		rgsz[1] = "player/pl_metal2.wav";
		cnt = 2;
		break;
	case CHAR_TEX_DIRT:
		fvol = 0.9;
		fvolbar = 0.1;
		rgsz[0] = "player/pl_dirt1.wav";
		rgsz[1] = "player/pl_dirt2.wav";
		rgsz[2] = "player/pl_dirt3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_VENT:
		fvol = 0.5;
		fvolbar = 0.3;
		rgsz[0] = "player/pl_duct1.wav";
		rgsz[1] = "player/pl_duct1.wav";
		cnt = 2;
		break;
	case CHAR_TEX_GRATE:
		fvol = 0.9;
		fvolbar = 0.5;
		rgsz[0] = "player/pl_grate1.wav";
		rgsz[1] = "player/pl_grate4.wav";
		cnt = 2;
		break;
	case CHAR_TEX_TILE:
		fvol = 0.8;
		fvolbar = 0.2;
		rgsz[0] = "player/pl_tile1.wav";
		rgsz[1] = "player/pl_tile3.wav";
		rgsz[2] = "player/pl_tile2.wav";
		rgsz[3] = "player/pl_tile4.wav";
		cnt = 4;
		break;
	case CHAR_TEX_SLOSH:
		fvol = 0.9;
		fvolbar = 0.0;
		rgsz[0] = "player/pl_slosh1.wav";
		rgsz[1] = "player/pl_slosh3.wav";
		rgsz[2] = "player/pl_slosh2.wav";
		rgsz[3] = "player/pl_slosh4.wav";
		cnt = 4;
		break;
	case CHAR_TEX_WOOD:
		fvol = 0.9;
		fvolbar = 0.2;
		rgsz[0] = "debris/wood1.wav";
		rgsz[1] = "debris/wood2.wav";
		rgsz[2] = "debris/wood3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_GLASS:
	case CHAR_TEX_COMPUTER:
		fvol = 0.8;
		fvolbar = 0.2;
		rgsz[0] = "debris/glass1.wav";
		rgsz[1] = "debris/glass2.wav";
		rgsz[2] = "debris/glass3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_FLESH:
		if( iBulletType == BULLET_PLAYER_CROWBAR )
			return 0.0; // crowbar already makes this sound
		fvol = 1.0;
		fvolbar = 0.2;
		rgsz[0] = "weapons/bullet_hit1.wav";
		rgsz[1] = "weapons/bullet_hit2.wav";
		fattn = 1.0;
		cnt = 2;
		break;
	}

	// play material hit sound
	gEngfuncs.pEventAPI->EV_PlaySound( 0, ptr->endpos, CHAN_STATIC, rgsz[gEngfuncs.pfnRandomLong( 0, cnt - 1 )], fvol, fattn, 0, 96 + gEngfuncs.pfnRandomLong( 0, 0xf ) );
	return fvolbar;
}

char *EV_HLDM_DamageDecal( physent_t *pe )
{
	static char decalname[32];
	int idx;

	if( pe->classnumber == 1 )
	{
		idx = gEngfuncs.pfnRandomLong( 0, 2 );
		sprintf( decalname, "{break%i", idx + 1 );
	}
	else if( pe->rendermode != kRenderNormal )
	{
		sprintf( decalname, "{bproof1" );
	}
	else
	{
		idx = gEngfuncs.pfnRandomLong( 0, 4 );
		sprintf( decalname, "{shot%i", idx + 1 );
	}
	return decalname;
}

void EV_HLDM_GunshotDecalTrace( pmtrace_t *pTrace, char *decalName )
{
	int iRand;
	physent_t *pe;

	gEngfuncs.pEfxAPI->R_BulletImpactParticles( pTrace->endpos );

	iRand = gEngfuncs.pfnRandomLong( 0, 0x7FFF );
	if( iRand < ( 0x7fff / 2 ) )// not every bullet makes a sound.
	{
		switch( iRand % 5 )
		{
		case 0:
			gEngfuncs.pEventAPI->EV_PlaySound( -1, pTrace->endpos, 0, "weapons/ric1.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );
			break;
		case 1:
			gEngfuncs.pEventAPI->EV_PlaySound( -1, pTrace->endpos, 0, "weapons/ric2.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );
			break;
		case 2:
			gEngfuncs.pEventAPI->EV_PlaySound( -1, pTrace->endpos, 0, "weapons/ric3.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );
			break;
		case 3:
			gEngfuncs.pEventAPI->EV_PlaySound( -1, pTrace->endpos, 0, "weapons/ric4.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );
			break;
		case 4:
			gEngfuncs.pEventAPI->EV_PlaySound( -1, pTrace->endpos, 0, "weapons/ric5.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );
			break;
		}
	}

	pe = gEngfuncs.pEventAPI->EV_GetPhysent( pTrace->ent );

	// Only decal brush models such as the world etc.
	if(  decalName && decalName[0] && pe && ( pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP ) )
	{
		if( CVAR_GET_FLOAT( "r_decals" ) )
		{
			gEngfuncs.pEfxAPI->R_DecalShoot(
				gEngfuncs.pEfxAPI->Draw_DecalIndex( gEngfuncs.pEfxAPI->Draw_DecalIndexFromName( decalName ) ),
				gEngfuncs.pEventAPI->EV_IndexFromTrace( pTrace ), 0, pTrace->endpos, 0 );
		}
	}
}

void EV_HLDM_DecalGunshot( pmtrace_t *pTrace, int iBulletType )
{
	physent_t *pe;

	pe = gEngfuncs.pEventAPI->EV_GetPhysent( pTrace->ent );

	if( pe && ( pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP ) )
	{
		switch( iBulletType )
		{
		case BULLET_PLAYER_9MM:
		case BULLET_MONSTER_9MM:
		case BULLET_PLAYER_MP5:
		case BULLET_MONSTER_MP5:
		case BULLET_PLAYER_BUCKSHOT:
		case BULLET_PLAYER_357:
		default:
			// smoke and decal
			EV_HLDM_GunshotDecalTrace( pTrace, EV_HLDM_DamageDecal( pe ) );
			break;
		}
	}
}

int EV_HLDM_CheckTracer( int idx, float *vecSrc, float *end, float *forward, float *right, int iBulletType, int iTracerFreq, int *tracerCount )
{
	int tracer = 0;
	int i;
	qboolean player = idx >= 1 && idx <= gEngfuncs.GetMaxClients() ? true : false;

	if( iTracerFreq != 0 && ( (*tracerCount)++ % iTracerFreq ) == 0 )
	{
		vec3_t vecTracerSrc;

		if( player )
		{
			vec3_t offset( 0, 0, -4 );

			// adjust tracer position for player
			for( i = 0; i < 3; i++ )
			{
				vecTracerSrc[i] = vecSrc[i] + offset[i] + right[i] * 2 + forward[i] * 16;
			}
		}
		else
		{
			VectorCopy( vecSrc, vecTracerSrc );
		}

		if( iTracerFreq != 1 )		// guns that always trace also always decal
			tracer = 1;

		switch( iBulletType )
		{
		case BULLET_PLAYER_MP5:
		case BULLET_MONSTER_MP5:
		case BULLET_MONSTER_9MM:
		case BULLET_MONSTER_12MM:
		default:
			EV_CreateTracer( vecTracerSrc, end );
			break;
		}
	}

	return tracer;
}

/*
================
FireBullets

Go to the trouble of combining multiple pellets into a single damage call.
================
*/
void EV_HLDM_FireBullets( int idx, float *forward, float *right, float *up, int cShots, float *vecSrc, float *vecDirShooting, float flDistance, int iBulletType, int iTracerFreq, int *tracerCount, float flSpreadX, float flSpreadY )
{
	int i;
	pmtrace_t tr;
	int iShot;
	int tracer;

	for( iShot = 1; iShot <= cShots; iShot++ )	
	{
		vec3_t vecDir, vecEnd;
		float x, y, z;

		float stabilised = gEngfuncs.pfnGetCvarFloat("vr_weapon_stabilised");

		//We randomize for the Shotgun.
		if( iBulletType == BULLET_PLAYER_BUCKSHOT )
		{
			do{
				x = gEngfuncs.pfnRandomFloat( -0.5, 0.5 ) + gEngfuncs.pfnRandomFloat( -0.5, 0.5 );
				y = gEngfuncs.pfnRandomFloat( -0.5, 0.5 ) + gEngfuncs.pfnRandomFloat( -0.5, 0.5 );
				z = x * x + y * y;
			}while( z > 1 );

			for( i = 0 ; i < 3; i++ )
			{
				vecDir[i] = vecDirShooting[i] + x * flSpreadX * right[i] + y * flSpreadY * up [i];
				vecEnd[i] = vecSrc[i] + flDistance * vecDir[i];
			}
		}//But other guns already have their spread randomized in the synched spread.
		else
		{
			//If stabiliased, reduce the spread
			float spreadX = flSpreadX;
			float spreadY = flSpreadY;
			if (stabilised != 0.0f)
			{
				spreadX *= 0.45f;
				spreadY *= 0.45f;
			}

			for( i = 0 ; i < 3; i++ )
			{
				vecDir[i] = vecDirShooting[i] + spreadX * right[i] + spreadY * up [i];
				vecEnd[i] = vecSrc[i] + flDistance * vecDir[i];
			}
		}

		gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( false, true );

		// Store off the old count
		gEngfuncs.pEventAPI->EV_PushPMStates();

		// Now add in all of the players.
		gEngfuncs.pEventAPI->EV_SetSolidPlayers( idx - 1 );	

		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr );

		tracer = EV_HLDM_CheckTracer( idx, vecSrc, tr.endpos, forward, right, iBulletType, iTracerFreq, tracerCount );

        // haptics
        if (gMobileEngfuncs) {
            int leftHanded = (int) CVAR_GET_FLOAT("hand");
            switch (iBulletType) {
                default:
                case BULLET_PLAYER_9MM:
                    gMobileEngfuncs->pfnVibrate(120, 1 - leftHanded, 0.4);
                    break;
                case BULLET_PLAYER_MP5:
                    gMobileEngfuncs->pfnVibrate(60, 1 - leftHanded, 0.75);
                    if (stabilised != 0.0f)
                        gMobileEngfuncs->pfnVibrate(60, leftHanded, 0.5);
                    break;
                case BULLET_PLAYER_BUCKSHOT:
                    gMobileEngfuncs->pfnVibrate(120, 1 - leftHanded, 0.9);
                    if (stabilised != 0.0f)
                        gMobileEngfuncs->pfnVibrate(120, leftHanded, 0.75);
                    break;
                case BULLET_PLAYER_357:
                    gMobileEngfuncs->pfnVibrate(120, 1 - leftHanded, 0.8);
                    break;
            }
        }


		// do damage, paint decals
		if( tr.fraction != 1.0 )
		{
			switch( iBulletType )
			{
			default:
			case BULLET_PLAYER_9MM:
				EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
				EV_HLDM_DecalGunshot( &tr, iBulletType );
				break;
			case BULLET_PLAYER_MP5:
				if( !tracer )
				{
					EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
					EV_HLDM_DecalGunshot( &tr, iBulletType );
				}
				break;
			case BULLET_PLAYER_BUCKSHOT:
				EV_HLDM_DecalGunshot( &tr, iBulletType );
				break;
			case BULLET_PLAYER_357:
				EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
				EV_HLDM_DecalGunshot( &tr, iBulletType );
				break;
			}
		}

		gEngfuncs.pEventAPI->EV_PopPMStates();
	}
}

//======================
//	    GLOCK START
//======================
// Shared Glock fire implementation for EV_FireGlock1 and EV_FireGlock2.
static void EV_FireGlock_Impl( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;
	int empty;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;

	idx = args->entindex;

	cl_entity_s* viewModel = gEngfuncs.GetViewModel();
	if (viewModel == nullptr)
		return;
	VectorCopy(viewModel->curstate.origin, origin);
	VectorCopy(viewModel->curstate.angles, angles);
	angles.x = -angles.x;
	VectorCopy(viewModel->curstate.velocity, velocity);

	AngleVectors(angles, forward, right, up);

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex( "models/shell.mdl" );// brass shell

	if( EV_IsLocal( idx ) )
	{
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( empty ? GLOCK_SHOOT_EMPTY : GLOCK_SHOOT, 1 );

		V_PunchAxis( 0, -2.0 );
	}
 
	EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 20, -12, 4 );

	EV_EjectBrass( ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHELL );

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, gEngfuncs.pfnRandomLong( 0, 1 ) ? "weapons/pl_gun1.wav" : "weapons/pl_gun2.wav", gEngfuncs.pfnRandomFloat( 0.46, 0.5 ), ATTN_NORM, 0, PITCH_NORM );

	EV_GetGunPosition( args, vecSrc, origin );

	VectorCopy( forward, vecAiming );

	EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_9MM, 0, &g_tracerCount[idx - 1], args->fparam1, args->fparam2 );
}

void EV_FireGlock1( event_args_t *args )
{
	EV_FireGlock_Impl( args );
}

void EV_FireGlock2( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;
	int empty = args->bparam1;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t vecSpread;
	vec3_t up, right, forward;

	idx = args->entindex;

	cl_entity_s* viewModel = gEngfuncs.GetViewModel();
	if (viewModel == nullptr)
		return;
	VectorCopy(viewModel->curstate.origin, origin);
	VectorCopy(viewModel->curstate.angles, angles);
	angles.x = -angles.x;
	VectorCopy(viewModel->curstate.velocity, velocity);

	// Glock uses different body in multiplayer versus single player
	int multiplayer = gEngfuncs.GetMaxClients() == 1 ? 0 : 1;

	AngleVectors(angles, forward, right, up);

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex( "models/shell.mdl" );// brass shell

	if( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( empty ? GLOCK_SHOOT_EMPTY : GLOCK_SHOOT, multiplayer );

		V_PunchAxis( 0, -2.0 );
	}

	EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 20, -12, 4 );

        EV_EjectBrass( ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHELL );

	if( !multiplayer )
	{
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/pl_gun3.wav", gEngfuncs.pfnRandomFloat( 0.92, 1.0 ), ATTN_NORM, 0, 98 + gEngfuncs.pfnRandomLong( 0, 3 ) );
	}
	else
	{
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, gEngfuncs.pfnRandomLong( 0, 1 ) ? "weapons/pl_gun1.wav" : "weapons/pl_gun2.wav", gEngfuncs.pfnRandomFloat( 0.46, 0.5 ), ATTN_NORM, 0, 100 );
	}

	EV_GetGunPosition( args, vecSrc, origin );

	VectorCopy( forward, vecAiming );

	EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_9MM, 0, &g_tracerCount[idx - 1], args->fparam1, args->fparam2 );
}
//======================
//	   GLOCK END
//======================

//======================
//	  SHOTGUN START
//======================
void EV_FireShotGunDouble( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	int j;
	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	//vec3_t vecSpread;
	vec3_t up, right, forward;
	//float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shotgunshell.mdl");// brass shell

	if( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( SHOTGUN_FIRE2, 2 );
		V_PunchAxis( 0, -10.0 );
	}

	for( j = 0; j < 2; j++ )
	{
		EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 32, -12, 6 );

		EV_EjectBrass( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHOTSHELL ); 
	}

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/dbarrel1.wav", gEngfuncs.pfnRandomFloat( 0.98, 1.0 ), ATTN_NORM, 0, 85 + gEngfuncs.pfnRandomLong( 0, 0x1f ) );

	EV_GetGunPosition( args, vecSrc, origin );
	VectorCopy( forward, vecAiming );

	if( gEngfuncs.GetMaxClients() > 1 )
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 8, vecSrc, vecAiming, 2048, BULLET_PLAYER_BUCKSHOT, 0, &g_tracerCount[idx - 1], 0.17365, 0.04362 );
	}
	else
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 12, vecSrc, vecAiming, 2048, BULLET_PLAYER_BUCKSHOT, 0, &g_tracerCount[idx - 1], 0.08716, 0.08716 );
	}
}

void EV_FireShotGunSingle( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	//vec3_t vecSpread;
	vec3_t up, right, forward;
	//float flSpread = 0.01;

	idx = args->entindex;
	cl_entity_s* viewModel = gEngfuncs.GetViewModel();
	if (viewModel == nullptr)
		return;
	VectorCopy(viewModel->curstate.origin, origin);
	VectorCopy(viewModel->curstate.angles, angles);
	angles.x = -angles.x;
	VectorCopy(viewModel->curstate.velocity, velocity);

	AngleVectors( angles, forward, right, up );

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shotgunshell.mdl");// brass shell

	if( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( SHOTGUN_FIRE, 2 );

		V_PunchAxis( 0, -5.0 );
	}

	EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 32, -12, 6 );

	EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHOTSHELL ); 

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/sbarrel1.wav", gEngfuncs.pfnRandomFloat( 0.95, 1.0 ), ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong( 0, 0x1f ) );

	EV_GetGunPosition( args, vecSrc, origin );
	VectorCopy( forward, vecAiming );

	if( gEngfuncs.GetMaxClients() > 1 )
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 4, vecSrc, vecAiming, 2048, BULLET_PLAYER_BUCKSHOT, 0, &g_tracerCount[idx - 1], 0.08716, 0.04362 );
	}
	else
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 6, vecSrc, vecAiming, 2048, BULLET_PLAYER_BUCKSHOT, 0, &g_tracerCount[idx - 1], 0.08716, 0.08716 );
	}
}
//======================
//	   SHOTGUN END
//======================

//======================
//	    MP5 START
//======================
void EV_FireMP5( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	//float flSpread = 0.01;

	idx = args->entindex;

	cl_entity_s* viewModel = gEngfuncs.GetViewModel();
	if (viewModel == nullptr)
		return;
	VectorCopy(viewModel->curstate.origin, origin);
	VectorCopy(viewModel->curstate.angles, angles);
	angles.x = -angles.x;
	VectorCopy(viewModel->curstate.velocity, velocity);

	AngleVectors(angles, forward, right, up);

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shell.mdl");// brass shell

	if( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( MP5_FIRE1 + gEngfuncs.pfnRandomLong( 0, 2 ), 2 );

		V_PunchAxis( 0, gEngfuncs.pfnRandomFloat( -2, 2 ) );
	}

	EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 20, -12, 4 );

	EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHELL ); 

	switch( gEngfuncs.pfnRandomLong( 0, 1 ) )
	{
	case 0:
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/hks1.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong( 0, 0xf ) );
		break;
	case 1:
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/hks2.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong( 0, 0xf ) );
		break;
	}

	EV_GetGunPosition( args, vecSrc, origin );
	VectorCopy( forward, vecAiming );

	EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_MP5, 2, &g_tracerCount[idx - 1], args->fparam1, args->fparam2 );
}

//======================
//	   PHYTON START 
//	     ( .357 )
//======================
void EV_FirePython( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	//float flSpread = 0.01;

	idx = args->entindex;

	cl_entity_s* viewModel = gEngfuncs.GetViewModel();
	if (viewModel == nullptr)
		return;
	VectorCopy(viewModel->curstate.origin, origin);
	VectorCopy(viewModel->curstate.angles, angles);
	angles.x = -angles.x;
	VectorCopy(viewModel->curstate.velocity, velocity);

	AngleVectors( angles, forward, right, up );

	if( EV_IsLocal( idx ) )
	{
		// Python uses different body in multiplayer versus single player
		int multiplayer = gEngfuncs.GetMaxClients() == 1 ? 0 : 1;

		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( PYTHON_FIRE1, multiplayer ? 1 : 0 );

		V_PunchAxis( 0, -10.0 );
	}

	switch( gEngfuncs.pfnRandomLong( 0, 1 ) )
	{
	case 0:
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/357_shot1.wav", gEngfuncs.pfnRandomFloat( 0.8, 0.9 ), ATTN_NORM, 0, PITCH_NORM );
		break;
	case 1:
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/357_shot2.wav", gEngfuncs.pfnRandomFloat( 0.8, 0.9 ), ATTN_NORM, 0, PITCH_NORM );
		break;
	}

	EV_GetGunPosition( args, vecSrc, origin );

	VectorCopy( forward, vecAiming );

	EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_357, 0, 0, args->fparam1, args->fparam2 );
}
//======================
//	    PHYTON END 
//	     ( .357 )
//======================

//======================
//	   GAUSS START 
//======================
#define SND_STOP			(1 << 5)
#define SND_CHANGE_PITCH	(1 << 7)		// duplicated in protocol.h change sound pitch

void EV_SpinGauss( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;
	int iSoundState = 0;

	int pitch;

	idx = args->entindex;
	cl_entity_s* viewModel = gEngfuncs.GetViewModel();
	if (viewModel == nullptr)
		return;
	VectorCopy(viewModel->curstate.origin, origin);
	VectorCopy(viewModel->curstate.angles, angles);
	angles.x = -angles.x;
	VectorCopy(viewModel->curstate.velocity, velocity);

	pitch = args->iparam1;

	iSoundState = args->bparam1 ? SND_CHANGE_PITCH : 0;
	iSoundState = args->bparam2 ? SND_STOP : iSoundState;

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "ambience/pulsemachine.wav", 1.0, ATTN_NORM, iSoundState, pitch );
}

/*
==============================
EV_StopPreviousGauss

==============================
*/
void EV_StopPreviousGauss( int idx )
{
	// Make sure we don't have a gauss spin event in the queue for this guy
	gEngfuncs.pEventAPI->EV_KillEvents( idx, "events/gaussspin.sc" );
	gEngfuncs.pEventAPI->EV_StopSound( idx, CHAN_WEAPON, "ambience/pulsemachine.wav" );
}

extern float g_flApplyVel;

void EV_FireGauss( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;
	float flDamage = args->fparam1;
	//int primaryfire = args->bparam1;

	int m_fPrimaryFire = args->bparam1;
	//int m_iWeaponVolume = GAUSS_PRIMARY_FIRE_VOLUME;
	vec3_t vecSrc;
	vec3_t vecDest;
	//edict_t		*pentIgnore;
	pmtrace_t tr, beam_tr;
	float flMaxFrac = 1.0;
	//int nTotal = 0;
	int fHasPunched = 0;
	int fFirstBeam = 1;
	int nMaxHits = 10;
	physent_t *pEntity;
	int m_iBeam, m_iGlow, m_iBalls;
	vec3_t up, right, forward;

	idx = args->entindex;
	cl_entity_s* viewModel = gEngfuncs.GetViewModel();
	if (viewModel == nullptr)
		return;
	VectorCopy(viewModel->curstate.origin, origin);
	VectorCopy(viewModel->curstate.angles, angles);
	angles.x = -angles.x;
	VectorCopy(viewModel->curstate.velocity, velocity);

	if( args->bparam2 )
	{
		EV_StopPreviousGauss( idx );
		return;
	}

	//Con_Printf( "Firing gauss with %f\n", flDamage );
	EV_GetGunPosition( args, vecSrc, origin );

	m_iBeam = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/smoke.spr" );
	m_iBalls = m_iGlow = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/hotglow.spr" );

	AngleVectors( angles, forward, right, up );

	VectorMA( vecSrc, 8192, forward, vecDest );

	if( EV_IsLocal( idx ) )
	{
		V_PunchAxis( 0.0f, -2.0f );
		gEngfuncs.pEventAPI->EV_WeaponAnimation( GAUSS_FIRE2, 2 );

		if( m_fPrimaryFire == false )
			 g_flApplyVel = flDamage; 
	}

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/gauss2.wav", 0.5f + flDamage * ( 1.0f / 400.0f ), ATTN_NORM, 0, 85 + gEngfuncs.pfnRandomLong( 0, 0x1f ) );

	while( flDamage > 10 && nMaxHits > 0 )
	{
		nMaxHits--;

		gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( false, true );
		
		// Store off the old count
		gEngfuncs.pEventAPI->EV_PushPMStates();
	
		// Now add in all of the players.
		gEngfuncs.pEventAPI->EV_SetSolidPlayers( idx - 1 );	

		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( vecSrc, vecDest, PM_STUDIO_BOX, -1, &tr );

		gEngfuncs.pEventAPI->EV_PopPMStates();

		if( tr.allsolid )
			break;

		if( fFirstBeam )
		{
			if( EV_IsLocal( idx ) )
			{
				// Add muzzle flash to current weapon model
				EV_MuzzleFlash();
			}
			fFirstBeam = 0;

			gEngfuncs.pEfxAPI->R_BeamEntPoint( 
				idx | 0x1000,
				tr.endpos,
				m_iBeam,
				0.1f,
				m_fPrimaryFire ? 1.0f : 2.5f,
				0.0f,
				(m_fPrimaryFire ? 128.0f : flDamage) / 255.0f,
				0,
				0,
				0,
				(m_fPrimaryFire ? 255 : 255) / 255.0f,
				(m_fPrimaryFire ? 128 : 255) / 255.0f,
				(m_fPrimaryFire ? 0 : 255) / 255.0f
			);
		}
		else
		{
			gEngfuncs.pEfxAPI->R_BeamPoints( vecSrc,
				tr.endpos,
				m_iBeam,
				0.1f,
				m_fPrimaryFire ? 1.0f : 2.5f,
				0.0f,
				(m_fPrimaryFire ? 128.0f : flDamage) / 255.0f,
				0,
				0,
				0,
				(m_fPrimaryFire ? 255 : 255) / 255.0f,
				(m_fPrimaryFire ? 128 : 255) / 255.0f,
				(m_fPrimaryFire ? 0 : 255) / 255.0f
			);
		}

		pEntity = gEngfuncs.pEventAPI->EV_GetPhysent( tr.ent );
		if( pEntity == NULL )
			break;

		if( pEntity->solid == SOLID_BSP )
		{
			float n;

			//pentIgnore = NULL;

			n = -DotProduct( tr.plane.normal, forward );

			if( n < 0.5f ) // 60 degrees	
			{
				// ALERT( at_console, "reflect %f\n", n );
				// reflect
				vec3_t r;

				VectorMA( forward, 2.0f * n, tr.plane.normal, r );

				flMaxFrac = flMaxFrac - tr.fraction;

				VectorCopy( r, forward );

				VectorMA( tr.endpos, 8.0, forward, vecSrc );
				VectorMA( vecSrc, 8192.0, forward, vecDest );

				gEngfuncs.pEfxAPI->R_TempSprite( tr.endpos, vec3_origin, 0.2, m_iGlow, kRenderGlow, kRenderFxNoDissipation, flDamage * n / 255.0f, flDamage * n * 0.5f * 0.1f, FTENT_FADEOUT );

				vec3_t fwd;
				VectorAdd( tr.endpos, tr.plane.normal, fwd );

				gEngfuncs.pEfxAPI->R_Sprite_Trail( TE_SPRITETRAIL, tr.endpos, fwd, m_iBalls, 3, 0.1, gEngfuncs.pfnRandomFloat( 10.0f, 20.0f ) / 100.0f, 100,
									255, 100 );

				// lose energy
				if( n == 0.0f )
				{
					n = 0.1f;
				}

				flDamage = flDamage * ( 1 - n );
			}
			else
			{
				// tunnel
				EV_HLDM_DecalGunshot( &tr, BULLET_MONSTER_12MM );

				gEngfuncs.pEfxAPI->R_TempSprite( tr.endpos, vec3_origin, 1.0, m_iGlow, kRenderGlow, kRenderFxNoDissipation, flDamage / 255.0f, 6.0f, FTENT_FADEOUT );

				// limit it to one hole punch
				if( fHasPunched )
				{
					break;
				}
				fHasPunched = 1;
				
				// try punching through wall if secondary attack (primary is incapable of breaking through)
				if( !m_fPrimaryFire )
				{
					vec3_t start;

					VectorMA( tr.endpos, 8.0, forward, start );

					// Store off the old count
					gEngfuncs.pEventAPI->EV_PushPMStates();

					// Now add in all of the players.
					gEngfuncs.pEventAPI->EV_SetSolidPlayers ( idx - 1 );

					gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
					gEngfuncs.pEventAPI->EV_PlayerTrace( start, vecDest, PM_STUDIO_BOX, -1, &beam_tr );

					if( !beam_tr.allsolid )
					{
						vec3_t delta;

						// trace backwards to find exit point
						gEngfuncs.pEventAPI->EV_PlayerTrace( beam_tr.endpos, tr.endpos, PM_STUDIO_BOX, -1, &beam_tr );

						VectorSubtract( beam_tr.endpos, tr.endpos, delta );

						n = Length( delta );

						if( n < flDamage )
						{
							if( n == 0 )
								n = 1;
							flDamage -= n;

							// absorption balls
							{
								vec3_t fwd;
								VectorSubtract( tr.endpos, forward, fwd );
								gEngfuncs.pEfxAPI->R_Sprite_Trail( TE_SPRITETRAIL, tr.endpos, fwd, m_iBalls, 3, 0.1, gEngfuncs.pfnRandomFloat( 10.0f, 20.0f ) / 100.0f, 100,
									255, 100 );
							}

	//////////////////////////////////// WHAT TO DO HERE
							// CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, NORMAL_EXPLOSION_VOLUME, 3.0 );

							EV_HLDM_DecalGunshot( &beam_tr, BULLET_MONSTER_12MM );

							gEngfuncs.pEfxAPI->R_TempSprite( beam_tr.endpos, vec3_origin, 0.1, m_iGlow, kRenderGlow, kRenderFxNoDissipation, flDamage / 255.0f, 6.0f, FTENT_FADEOUT );

							// balls
							{
								vec3_t fwd;
								VectorSubtract( beam_tr.endpos, forward, fwd );
								gEngfuncs.pEfxAPI->R_Sprite_Trail( TE_SPRITETRAIL, beam_tr.endpos, fwd, m_iBalls, (int)( flDamage * 0.3f ), 0.1, gEngfuncs.pfnRandomFloat( 10.0f, 20.0f ) / 100.0f, 200,
									255, 40 );
							}

							VectorAdd( beam_tr.endpos, forward, vecSrc );
						}
					}
					else
					{
						flDamage = 0;
					}

					gEngfuncs.pEventAPI->EV_PopPMStates();
				}
				else
				{
					if( m_fPrimaryFire )
					{
						// slug doesn't punch through ever with primary 
						// fire, so leave a little glowy bit and make some balls
						gEngfuncs.pEfxAPI->R_TempSprite( tr.endpos, vec3_origin, 0.2, m_iGlow, kRenderGlow, kRenderFxNoDissipation, 200.0f / 255.0f, 0.3, FTENT_FADEOUT );
						{
							vec3_t fwd;
							VectorAdd( tr.endpos, tr.plane.normal, fwd );
							gEngfuncs.pEfxAPI->R_Sprite_Trail( TE_SPRITETRAIL, tr.endpos, fwd, m_iBalls, 8, 0.6, gEngfuncs.pfnRandomFloat( 10.0f, 20.0f ) / 100.0f, 100,
								255, 200 );
						}
					}

					flDamage = 0;
				}
			}
		}
		else
		{
			VectorAdd( tr.endpos, forward, vecSrc );
		}
	}
}
//======================
//	   GAUSS END 
//======================

void EV_TrainPitchAdjust( event_args_t *args )
{
	int idx;
	vec3_t origin;

	unsigned short us_params;
	int noise;
	float m_flVolume;
	int pitch;
	int stop;

	const char *pszSound;

	idx = args->entindex;

	VectorCopy( args->origin, origin );

	us_params = (unsigned short)args->iparam1;
	stop = args->bparam1;

	m_flVolume = (float)( us_params & 0x003f ) / 40.0f;
	noise = (int)( ( ( us_params ) >> 12 ) & 0x0007 );
	pitch = (int)( 10.0f * (float)( ( us_params >> 6 ) & 0x003f ) );

	switch( noise )
	{
	case 1:
		pszSound = "plats/ttrain1.wav";
		break;
	case 2:
		pszSound = "plats/ttrain2.wav";
		break;
	case 3:
		pszSound = "plats/ttrain3.wav";
		break; 
	case 4:
		pszSound = "plats/ttrain4.wav";
		break;
	case 5:
		pszSound = "plats/ttrain6.wav";
		break;
	case 6:
		pszSound = "plats/ttrain7.wav";
		break;
	default:
		// no sound
		return;
	}

	if( stop )
	{
		gEngfuncs.pEventAPI->EV_StopSound( idx, CHAN_STATIC, pszSound );
	}
	else
	{
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_STATIC, pszSound, m_flVolume, ATTN_NORM, SND_CHANGE_PITCH, pitch );
	}
}

int EV_TFC_IsAllyTeam( int iTeam1, int iTeam2 )
{
	return 0;
}

//======================
//	    AP9 START
//======================
enum ap9_e
{
	AP9_IDLE = 0,
	AP9_RELOAD,
	AP9_DRAW,
	AP9_SHOOT1,
	AP9_SHOOT2,
	AP9_SHOOT3
};

void EV_FireAP9( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	float flSpread;
	int autoaim = args->bparam1;

	idx = args->entindex;
	cl_entity_s* viewModel = gEngfuncs.GetViewModel();
	if (viewModel == nullptr)
		return;
	VectorCopy(viewModel->curstate.origin, origin);
	VectorCopy(viewModel->curstate.angles, angles);
	angles.x = -angles.x;
	VectorCopy(viewModel->curstate.velocity, velocity);

	AngleVectors( angles, forward, right, up );

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex( "models/shell.mdl" );// brass shell

	if( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( AP9_SHOOT1 + gEngfuncs.pfnRandomLong( 0, 2 ), 1 );

		if( autoaim )
		{
			V_PunchAxis( 0, gEngfuncs.pfnRandomFloat( -3.5f, 2.0f ) );
			V_PunchAxis( 1, gEngfuncs.pfnRandomFloat( -1.5f, 1.5f ) );
		}
		else
		{
			V_PunchAxis( 0, gEngfuncs.pfnRandomFloat( -3.5f, -1.5f ) );
			V_PunchAxis( 1, gEngfuncs.pfnRandomFloat( -1.0f, 1.0f ) );
		}
	}

	EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 27, -10, 11 );

	EV_EjectBrass( ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHELL );

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/ap9_fire.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong( 0, 0xf ) );

	EV_GetGunPosition( args, vecSrc, origin );
	VectorCopy( forward, vecAiming );

	flSpread = autoaim ? 0.07f : 0.035f;

	if( gEngfuncs.GetMaxClients() > 1 )
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_9MM, 0, &g_tracerCount[idx-1], args->fparam1, args->fparam2);
	}
	else
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_MP5, 0, &g_tracerCount[idx-1], args->fparam1, args->fparam2);
	}
}
//======================
//	    AP9 END
//======================

//======================
//	    TAURUS START
//======================
enum taurus_e
{
	TAURUS_IDLE1 = 0,
	TAURUS_IDLE2,
	TAURUS_IDLE3,
	TAURUS_SHOOT,
	TAURUS_SHOOT2,
	TAURUS_SHOOT3,
	TAURUS_SHOOT_EMPTY,
	TAURUS_RELOAD,
	TAURUS_RELOAD2,
	TAURUS_DRAW,
	TAURUS_DRAW2
};

void EV_FireTaurus( event_args_t *args )
{
	int idx, empty;

	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;

	idx = args->entindex;
	empty = args->bparam1;
	cl_entity_s* viewModel = gEngfuncs.GetViewModel();
	if (viewModel == nullptr)
		return;
	VectorCopy(viewModel->curstate.origin, origin);
	VectorCopy(viewModel->curstate.angles, angles);
	angles.x = -angles.x;
	VectorCopy(viewModel->curstate.velocity, velocity);

	AngleVectors( angles, forward, right, up );

	if( EV_IsLocal( idx ) )
	{
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( empty ? TAURUS_SHOOT_EMPTY : ( TAURUS_SHOOT + gEngfuncs.pfnRandomLong( 0, 2 ) ), 1 );

		V_PunchAxis( 0, -3.0 );
	}

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/tau_fire.wav", gEngfuncs.pfnRandomFloat(0.92, 1.0), ATTN_NORM, 0, 98 + gEngfuncs.pfnRandomLong( 0, 3 ) );

	EV_GetGunPosition( args, vecSrc, origin );

	VectorCopy( forward, vecAiming );

	EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_9MM, 0, 0, args->fparam1, args->fparam2 );
}
//======================
//	    TAURUS END
//======================

//======================
//	   SNIPER START
//======================
enum sniper_e
{
	SNIPER_IDLE = 0,
	SNIPER_RELOAD,
	SNIPER_DRAW,
	SNIPER_SHOOT1
};

void EV_FireSniper( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;
	float flSpread;

	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;

	idx = args->entindex;
	cl_entity_s* viewModel = gEngfuncs.GetViewModel();
	if (viewModel == nullptr)
		return;
	VectorCopy(viewModel->curstate.origin, origin);
	VectorCopy(viewModel->curstate.angles, angles);
	angles.x = -angles.x;
	VectorCopy(viewModel->curstate.velocity, velocity);
	int scope = args->bparam1;

	AngleVectors( angles, forward, right, up );

	if( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();

		gEngfuncs.pEventAPI->EV_WeaponAnimation( SNIPER_SHOOT1, 0 );

		// Substract additional pitch from already exisiting one.
		if( scope )
		{
			V_PunchAxis( 0, -2.0f );
		}
		else
		{
			V_PunchAxis( 0, gEngfuncs.pfnRandomFloat( -2.0f, -4.0f ) );
			V_PunchAxis( 1, gEngfuncs.pfnRandomFloat( -1.0f, 1.0f ) );
		}
	}

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/sniper.wav", gEngfuncs.pfnRandomFloat( 0.8, 0.9 ), ATTN_NORM, 0, PITCH_NORM );

	EV_GetGunPosition( args, vecSrc, origin );

	VectorCopy( forward, vecAiming );

	if( gEngfuncs.GetMaxClients() > 1 )
		flSpread = scope ? 0.002 : 0.07846;
	else
		flSpread = scope ? 0.004 : 0.08716;
	EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_357, 0, 0, args->fparam1, args->fparam2);
}
//======================
//	SNIPER END
//======================

//======================
//	TFCSNIPER START
//======================
enum tfcsniper_e
{
	TFCSNIPER_IDLE = 0,
	TFCSNIPER_AIM,
	TFCSNIPER_FIRE,
	TFCSNIPER_DRAW,
	TFCSNIPER_HOLSTER,
	TFCSNIPER_AUTOIDLE,
	TFCSNIPER_AUTOFIRE,
	TFCSNIPER_AUTODRAW,
	TFCSNIPER_AUTOHOLSTER
};

void EV_FireSniper2( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;
	float flSpread;

	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;

	idx = args->entindex;
	cl_entity_s* viewModel = gEngfuncs.GetViewModel();
	if (viewModel == nullptr)
		return;
	VectorCopy(viewModel->curstate.origin, origin);
	VectorCopy(viewModel->curstate.angles, angles);
	angles.x = -angles.x;
	VectorCopy(viewModel->curstate.velocity, velocity);
	int scope = args->bparam1;

	AngleVectors( angles, forward, right, up );

	if( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();

		gEngfuncs.pEventAPI->EV_WeaponAnimation( scope ? TFCSNIPER_FIRE : TFCSNIPER_AUTOFIRE, 0 );

		// Substract additional pitch from already exisiting one.
		if( scope )
		{
			V_PunchAxis( 0, -2.0f );
		}
		else
		{
			V_PunchAxis( 0, gEngfuncs.pfnRandomFloat( -2.0f, -4.0f ) );
			V_PunchAxis( 1, gEngfuncs.pfnRandomFloat( -1.0f, 1.0f ) );
		}
	}

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/sniper.wav", gEngfuncs.pfnRandomFloat( 0.8f, 0.9f ), ATTN_NORM, 0, PITCH_NORM );

	EV_GetGunPosition( args, vecSrc, origin );

	VectorCopy( forward, vecAiming );

	if( gEngfuncs.GetMaxClients() > 1 )
		flSpread = scope ? 0.002f : 0.07846f;
	else
		flSpread = scope ? 0.004f : 0.08716f;
	EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_357, 0, 0, args->fparam1, args->fparam2);
}

//======================
//	    EINAR1 END
//======================

//======================
//	 CHAINGUN START
//======================
enum chaingun_e
{
	CHAINGUN_IDLE = 0,
	CHAINGUN_IDLE2,
	CHAINGUN_SPINUP,
	CHAINGUN_SPINDOWN,
	CHAINGUN_FIRE,
	CHAINGUN_DRAW,
	CHAINGUN_HOLSTER
};

void EV_FireChaingun1( event_args_t *args )
{
	int idx, spinup;
	vec3_t origin;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	spinup = args->bparam1;

	if( spinup )
	{
		if( EV_IsLocal( idx ) )
			gEngfuncs.pEventAPI->EV_WeaponAnimation( CHAINGUN_SPINUP, 0 );
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/asscan1.wav", 1, ATTN_NORM, 0, PITCH_NORM );
	}
	else
	{
		gEngfuncs.pEventAPI->EV_KillEvents( idx, "events/chaingun2.sc" );
		gEngfuncs.pEventAPI->EV_StopSound( idx, CHAN_WEAPON, "weapons/asscan2.wav" );

		if( EV_IsLocal( idx ) )
			gEngfuncs.pEventAPI->EV_WeaponAnimation( CHAINGUN_SPINDOWN, 0 );
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/asscan3.wav", 1, ATTN_NORM, 0, PITCH_NORM );
	}
}

unsigned int g_iEjectBrass;

void EV_FireChaingun2( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell, iClipNotEmpty;
	float flSpread;
	int iBulletsCount;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;

	idx = args->entindex;
	cl_entity_s* viewModel = gEngfuncs.GetViewModel();
	if (viewModel == nullptr)
		return;
	VectorCopy(viewModel->curstate.origin, origin);
	VectorCopy(viewModel->curstate.angles, angles);
	angles.x = -angles.x;
	VectorCopy(viewModel->curstate.velocity, velocity);
	iClipNotEmpty = args->bparam1;         
	AngleVectors( angles, forward, right, up );

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex( "models/shell.mdl" );// brass shell

	if( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( CHAINGUN_FIRE, 0 );

		V_PunchAxis( 0, gEngfuncs.pfnRandomFloat( -0.5, 0.5 ) );
		V_PunchAxis( 1, gEngfuncs.pfnRandomFloat( -0.5, 0.5 ) );
	}

	if( ( g_iEjectBrass++ ) % 2 )
	{
		EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 20, -10, 5 );
		EV_EjectBrass( ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHELL );
	}

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/asscan2.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong( 0, 0xf ) );

	EV_GetGunPosition( args, vecSrc, origin );
	VectorCopy( forward, vecAiming );

	iBulletsCount = iClipNotEmpty ? 4 : 2;
	flSpread = ( gEngfuncs.GetMaxClients() > 1 ) ? 0.15 : 0.1;
	EV_HLDM_FireBullets( idx, forward, right, up, iBulletsCount, vecSrc, vecAiming, 8192, BULLET_PLAYER_MP5, 0, &g_tracerCount[idx - 1], args->fparam1, args->fparam2);
}
//======================
//	 CHAINGUN END
//======================

//======================
//	  CROSSBOW START
//======================
enum crossbow_e
{
	CROSSBOW_IDLE1 = 0,	// full
	CROSSBOW_IDLE2,		// empty
	CROSSBOW_FIDGET1,	// full
	CROSSBOW_FIDGET2,	// empty
	CROSSBOW_FIRE1,		// full
	CROSSBOW_FIRE2,		// reload
	CROSSBOW_FIRE3,		// empty
	CROSSBOW_RELOAD,	// from empty
	CROSSBOW_DRAW1,		// full
	CROSSBOW_DRAW2,		// empty
	CROSSBOW_HOLSTER1,	// full
	CROSSBOW_HOLSTER2	// empty
};

//=====================
// EV_BoltCallback
// This function is used to correct the origin and angles 
// of the bolt, so it looks like it's stuck on the wall.
//=====================
void EV_BoltCallback(struct tempent_s* ent, float frametime, float currenttime)
{
	ent->entity.origin = ent->entity.baseline.vuser1;
	ent->entity.angles = ent->entity.baseline.vuser2;
}

void EV_FireCrossbow2(event_args_t* args)
{
	vec3_t vecSrc, vecEnd;
	vec3_t up, right, forward;
	pmtrace_t tr;

	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	idx = args->entindex;

	cl_entity_s* viewModel = gEngfuncs.GetViewModel();
	if (viewModel == nullptr)
		return;
	VectorCopy(viewModel->curstate.origin, origin);
	VectorCopy(viewModel->curstate.angles, angles);
	angles.x = -angles.x;
	VectorCopy(viewModel->curstate.velocity, velocity);

	AngleVectors(angles, forward, right, up);

	EV_GetGunPosition(args, vecSrc, origin);

	VectorMA(vecSrc, 8192, forward, vecEnd);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/xbow_fire1.wav", 1, ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong(0, 0xF));
	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_ITEM, "weapons/xbow_reload1.wav", gEngfuncs.pfnRandomFloat(0.95, 1.0), ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong(0, 0xF));

	if (EV_IsLocal(idx))
	{
		if (args->iparam1)
			gEngfuncs.pEventAPI->EV_WeaponAnimation(CROSSBOW_FIRE1, 1);
		else if (args->iparam2)
			gEngfuncs.pEventAPI->EV_WeaponAnimation(CROSSBOW_FIRE3, 1);
	}

	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();

	// Now add in all of the players.
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(idx - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr);

	if (gMobileEngfuncs) {
		gMobileEngfuncs->pfnVibrate(50, 1 - (int)CVAR_GET_FLOAT("hand"), 0.6);
	}

	//We hit something
	if (tr.fraction < 1.0)
	{
		physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);

		//Not the world, let's assume we hit something organic ( dog, cat, uncle joe, etc ).
		if (pe->solid != SOLID_BSP)
		{
			switch (gEngfuncs.pfnRandomLong(0, 1))
			{
			case 0:
				gEngfuncs.pEventAPI->EV_PlaySound(idx, tr.endpos, CHAN_BODY, "weapons/xbow_hitbod1.wav", 1, ATTN_NORM, 0, PITCH_NORM);
				break;
			case 1:
				gEngfuncs.pEventAPI->EV_PlaySound(idx, tr.endpos, CHAN_BODY, "weapons/xbow_hitbod2.wav", 1, ATTN_NORM, 0, PITCH_NORM);
				break;
			}
		}
		//Stick to world but don't stick to glass, it might break and leave the bolt floating. It can still stick to other non-transparent breakables though.
		else if (pe->rendermode == kRenderNormal)
		{
			gEngfuncs.pEventAPI->EV_PlaySound(0, tr.endpos, CHAN_BODY, "weapons/xbow_hit1.wav", gEngfuncs.pfnRandomFloat(0.95, 1.0), ATTN_NORM, 0, PITCH_NORM);

			//Not underwater, do some sparks...
			if (gEngfuncs.PM_PointContents(tr.endpos, NULL) != CONTENTS_WATER)
				gEngfuncs.pEfxAPI->R_SparkShower(tr.endpos);

			vec3_t vBoltAngles;
			int iModelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex("models/crossbow_bolt.mdl");

			VectorAngles(forward, vBoltAngles);

			TEMPENTITY* bolt = gEngfuncs.pEfxAPI->R_TempModel(tr.endpos - forward * 10, Vector(0, 0, 0), vBoltAngles, 5, iModelIndex, TE_BOUNCE_NULL);

			if (bolt)
			{
				bolt->flags |= (FTENT_CLIENTCUSTOM); //So it calls the callback function.
				bolt->entity.baseline.vuser1 = tr.endpos - forward * 10; // Pull out a little bit
				bolt->entity.baseline.vuser2 = vBoltAngles; //Look forward!
				bolt->callback = EV_BoltCallback; //So we can set the angles and origin back. (Stick the bolt to the wall)
			}
		}
	}

	gEngfuncs.pEventAPI->EV_PopPMStates();
}

//TODO: Fully predict the fliying bolt.
void EV_FireCrossbow(event_args_t* args)
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	idx = args->entindex;

	cl_entity_s* viewModel = gEngfuncs.GetViewModel();
	if (viewModel == nullptr)
		return;
	VectorCopy(viewModel->curstate.origin, origin);
	VectorCopy(viewModel->curstate.angles, angles);
	angles.x = -angles.x;
	VectorCopy(viewModel->curstate.velocity, velocity);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/xbow_fire1.wav", 1, ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong(0, 0xF));
	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_ITEM, "weapons/xbow_reload1.wav", gEngfuncs.pfnRandomFloat(0.95, 1.0), ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong(0, 0xF));

	if (gMobileEngfuncs) {
		gMobileEngfuncs->pfnVibrate(50, 1 - (int)CVAR_GET_FLOAT("hand"), 0.4);
	}

	//Only play the weapon anims if I shot it. 
	if (EV_IsLocal(idx))
	{
		if (args->iparam1)
			gEngfuncs.pEventAPI->EV_WeaponAnimation(CROSSBOW_FIRE1, 1);
		else if (args->iparam2)
			gEngfuncs.pEventAPI->EV_WeaponAnimation(CROSSBOW_FIRE3, 1);

		V_PunchAxis(0, -2.0);
	}
}
//======================
//	   CROSSBOW END 
//======================

//======================
//	    RPG START 
//======================
enum rpg_e
{
	RPG_IDLE = 0,
	RPG_FIDGET,
	RPG_RELOAD,		// to reload
	RPG_FIRE2,		// to empty
	RPG_HOLSTER1,	// loaded
	RPG_DRAW1,		// loaded
	RPG_HOLSTER2,	// unloaded
	RPG_DRAW_UL,	// unloaded
	RPG_IDLE_UL,	// unloaded idle
	RPG_FIDGET_UL	// unloaded fidget
};

void EV_FireRpg(event_args_t* args)
{
	int idx;
	vec3_t origin;

	idx = args->entindex;

	cl_entity_s* viewModel = gEngfuncs.GetViewModel();
	if (viewModel == nullptr)
		return;
	VectorCopy(viewModel->curstate.origin, origin);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/rocketfire1.wav", 0.9, ATTN_NORM, 0, PITCH_NORM);
	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_ITEM, "weapons/glauncher.wav", 0.7, ATTN_NORM, 0, PITCH_NORM);

	if (gMobileEngfuncs) {
		int leftHanded = (int)CVAR_GET_FLOAT("hand");
		int stabilised = (int)CVAR_GET_FLOAT("vr_weapon_stabilised");
		gMobileEngfuncs->pfnVibrate(200, 1 - leftHanded, 0.9);
		if (stabilised)
			gMobileEngfuncs->pfnVibrate(200, leftHanded, 0.8);
	}

	//Only play the weapon anims if I shot it. 
	if (EV_IsLocal(idx))
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation(RPG_FIRE2, 1);

		V_PunchAxis(0, -5.0);
	}
}
//======================
//	     RPG END 
//======================

//======================
//	    EGON END 
//======================
enum egon_e
{
	EGON_IDLE1 = 0,
	EGON_FIDGET1,
	EGON_ALTFIREON,
	EGON_ALTFIRECYCLE,
	EGON_ALTFIREOFF,
	EGON_FIRE1,
	EGON_FIRE2,
	EGON_FIRE3,
	EGON_FIRE4,
	EGON_DRAW,
	EGON_HOLSTER
};

int g_fireAnims1[] = { EGON_FIRE1, EGON_FIRE2, EGON_FIRE3, EGON_FIRE4 };
int g_fireAnims2[] = { EGON_ALTFIRECYCLE };

enum EGON_FIRESTATE
{
	FIRE_OFF,
	FIRE_CHARGE
};

enum EGON_FIREMODE
{
	FIRE_NARROW,
	FIRE_WIDE
};

#define	EGON_PRIMARY_VOLUME		450
#define EGON_BEAM_SPRITE		"sprites/xbeam1.spr"
#define EGON_FLARE_SPRITE		"sprites/XSpark1.spr"
#define EGON_SOUND_OFF			"weapons/egon_off1.wav"
#define EGON_SOUND_RUN			"weapons/egon_run3.wav"
#define EGON_SOUND_STARTUP		"weapons/egon_windup2.wav"

#define ARRAYSIZE(p)		( sizeof(p) /sizeof(p[0]) )

BEAM* pBeam = NULL;
BEAM* pBeam2 = NULL;
TEMPENTITY* pFlare = NULL;	// Vit_amiN: egon's beam flare

void EV_EgonFlareCallback(struct tempent_s* ent, float frametime, float currenttime)
{
	float delta = currenttime - ent->tentOffset.z;	// time past since the last scale
	if (delta >= ent->tentOffset.y)
	{
		ent->entity.curstate.scale += ent->tentOffset.x * delta;
		ent->tentOffset.z = currenttime;
	}
}

void EV_EgonFire(event_args_t* args)
{
	int idx, /*iFireState,*/ iFireMode;
	vec3_t origin;

	idx = args->entindex;

	cl_entity_s* viewModel = gEngfuncs.GetViewModel();
	if (viewModel == nullptr)
		return;
	VectorCopy(viewModel->curstate.origin, origin);

	//iFireState = args->iparam1;
	iFireMode = args->iparam2;
	int iStartup = args->bparam1;

	if (iStartup)
	{
		if (iFireMode == FIRE_WIDE)
			gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, EGON_SOUND_STARTUP, 0.98, ATTN_NORM, 0, 125);
		else
			gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, EGON_SOUND_STARTUP, 0.9, ATTN_NORM, 0, 100);

		if (gMobileEngfuncs) {
			//Vibrate until we say stop
			gMobileEngfuncs->pfnVibrate(-1, 1 - (int)CVAR_GET_FLOAT("hand"), 0.5);
		}
	}
	else
	{
		// If there is any sound playing already, kill it. - Solokiller
		// This is necessary because multiple sounds can play on the same channel at the same time.
		// In some cases, more than 1 run sound plays when the egon stops firing, in which case only the earliest entry in the list is stopped.
		// This ensures no more than 1 of those is ever active at the same time.
		gEngfuncs.pEventAPI->EV_StopSound(idx, CHAN_STATIC, EGON_SOUND_RUN);

		if (iFireMode == FIRE_WIDE)
			gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_STATIC, EGON_SOUND_RUN, 0.98, ATTN_NORM, 0, 125);
		else
			gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_STATIC, EGON_SOUND_RUN, 0.9, ATTN_NORM, 0, 100);
	}

	//Only play the weapon anims if I shot it.
	if (EV_IsLocal(idx))
		gEngfuncs.pEventAPI->EV_WeaponAnimation(g_fireAnims1[gEngfuncs.pfnRandomLong(0, 3)], 1);

	if (iStartup == 1 && EV_IsLocal(idx) && !(pBeam || pBeam2 || pFlare) && cl_lw->value) //Adrian: Added the cl_lw check for those lital people that hate weapon prediction.
	{
		vec3_t vecSrc, vecEnd, angles, forward, right, up;
		pmtrace_t tr;

		cl_entity_t* pl = gEngfuncs.GetEntityByIndex(idx);

		if (pl)
		{
			VectorCopy(viewModel->curstate.angles, angles);
			angles.x = -angles.x;
			AngleVectors(angles, forward, NULL, NULL);

			EV_GetGunPosition(args, vecSrc, pl->origin);

			VectorMA(vecSrc, 2048, forward, vecEnd);

			gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(false, true);

			// Store off the old count
			gEngfuncs.pEventAPI->EV_PushPMStates();

			// Now add in all of the players.
			gEngfuncs.pEventAPI->EV_SetSolidPlayers(idx - 1);

			gEngfuncs.pEventAPI->EV_SetTraceHull(2);
			gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr);

			gEngfuncs.pEventAPI->EV_PopPMStates();

			/*			int iBeamModelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex( EGON_BEAM_SPRITE );

						float r = 50.0f;
						float g = 50.0f;
						float b = 125.0f;

						if( IEngineStudio.IsHardware() )
						{
							r /= 100.0f;
							g /= 100.0f;
						}

						pBeam = gEngfuncs.pEfxAPI->R_BeamEntPoint( idx | 0x1000, tr.endpos, iBeamModelIndex, 99999, 3.5, 0.2, 0.7, 55, 0, 0, r, g, b );

						if( pBeam )
							 pBeam->flags |= ( FBEAM_SINENOISE );

						pBeam2 = gEngfuncs.pEfxAPI->R_BeamEntPoint( idx | 0x1000, tr.endpos, iBeamModelIndex, 99999, 5.0, 0.08, 0.7, 25, 0, 0, r, g, b );

						// Vit_amiN: egon beam flare
						pFlare = gEngfuncs.pEfxAPI->R_TempSprite( tr.endpos, vec3_origin, 1.0, gEngfuncs.pEventAPI->EV_FindModelIndex( EGON_FLARE_SPRITE ), kRenderGlow, kRenderFxNoDissipation, 1.0, 99999, FTENT_SPRCYCLE | FTENT_PERSIST );
			*/
		}
	}

	if (pFlare)	// Vit_amiN: store the last mode for EV_EgonStop()
	{
		pFlare->tentOffset.x = (iFireMode == FIRE_WIDE) ? 1.0f : 0.0f;
	}
}

void EV_EgonStop(event_args_t* args)
{
	int idx;
	vec3_t origin;

	idx = args->entindex;

	cl_entity_s* viewModel = gEngfuncs.GetViewModel();
	if (viewModel == nullptr)
		return;
	VectorCopy(viewModel->curstate.origin, origin);

	gEngfuncs.pEventAPI->EV_StopSound(idx, CHAN_STATIC, EGON_SOUND_RUN);

	if (args->iparam1)
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, EGON_SOUND_OFF, 0.98, ATTN_NORM, 0, 100);

	if (gMobileEngfuncs) {
		//we say stop
		gMobileEngfuncs->pfnVibrate(0.0f, 1 - (int)CVAR_GET_FLOAT("hand"), 0.0f);
	}

	if (EV_IsLocal(idx))
	{
		if (pBeam)
		{
			pBeam->die = 0.0;
			pBeam = NULL;
		}

		if (pBeam2)
		{
			pBeam2->die = 0.0;
			pBeam2 = NULL;
		}

		if (pFlare)	// Vit_amiN: egon beam flare
		{
			pFlare->die = gEngfuncs.GetClientTime();

			if (gEngfuncs.GetMaxClients() == 1 || !(pFlare->flags & FTENT_NOMODEL))
			{
				if (pFlare->tentOffset.x != 0.0f)	// true for iFireMode == FIRE_WIDE
				{
					pFlare->callback = &EV_EgonFlareCallback;
					pFlare->fadeSpeed = 2.0;			// fade out will take 0.5 sec
					pFlare->tentOffset.x = 10.0;		// scaling speed per second
					pFlare->tentOffset.y = 0.1;			// min time between two scales
					pFlare->tentOffset.z = pFlare->die;	// the last callback run time
					pFlare->flags = FTENT_FADEOUT | FTENT_CLIENTCUSTOM;
				}
			}

			pFlare = NULL;
		}
	}
}
