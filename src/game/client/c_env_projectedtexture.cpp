//====== Copyright � 1996-2003, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "c_env_projectedtexture.h"
#include "shareddefs.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterial.h"
#include "view.h"
#include "iviewrender.h"
#include "view_shared.h"
#include "texture_group_names.h"
#include "tier0/icommandline.h"

// shoud really remove and put in cbase.h, like in ASW
#include "vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar r_flashlightdepthres;

static ConVar r_projtex_filtersize( "r_projtex_filtersize", "0.5", 0 );

/*static ConVar r_projtex_r( "r_projtex_r", "10", 0 );
static ConVar r_projtex_g( "r_projtex_g", "10", 0 );
static ConVar r_projtex_b( "r_projtex_b", "2", 0 );*/

static ConVar r_projtex_quadratic( "r_projtex_quadratic", "0", 0 );
static ConVar r_projtex_linear( "r_projtex_linear", "100", 0 );
static ConVar r_projtex_constant( "r_projtex_constant", "0", 0 );

/*static ConVar r_projtex_slopescale( "r_projtex_slopescale", "3.0", 0 );
static ConVar r_projtex_depthbias( "r_projtex_depthbias", "0.00001", 0 );*/

// maybe rename to r_shadowmap_quality or r_shadowmap_sharpness?
static ConVar r_projtex_quality( "r_projtex_quality", "-1", FCVAR_ARCHIVE,
	"\n-1 to use custom settings \n0 - 512 with filter size of 2.0 \n1 - 1024 with filter size of 1.0 \n2 - 2048 with filter size of 0.5 \n3 - 4096 with filter size of 0.25 (overkill) \n4 - 8192 with filter size of 0.125 (RIP PC) \nMap needs to be reloaded to completely take effect atm." );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
IMPLEMENT_CLIENTCLASS_DT( C_EnvProjectedTexture, DT_EnvProjectedTexture, CEnvProjectedTexture )
	RecvPropEHandle( RECVINFO( m_hTargetEntity )	),
	RecvPropBool(	 RECVINFO( m_bState )			),
	RecvPropBool(	 RECVINFO( m_bAlwaysUpdate )	),
	RecvPropFloat(	 RECVINFO( m_flLightFOV )		),
	RecvPropBool(	 RECVINFO( m_bEnableShadows )	),
	RecvPropBool(	 RECVINFO( m_bLightOnlyTarget ) ),
	RecvPropBool(	 RECVINFO( m_bLightWorld )		),
	RecvPropBool(	 RECVINFO( m_bCameraSpace )		),

	RecvPropVector(	 RECVINFO( m_LinearFloatLightColor )		),
	RecvPropInt(	 RECVINFO( m_nLinear )	),
	RecvPropInt(	 RECVINFO( m_nQuadratic )	),
	RecvPropInt(	 RECVINFO( m_nConstant )	),

	RecvPropFloat(	 RECVINFO( m_flAmbient )		),
	RecvPropString(  RECVINFO( m_SpotlightTextureName ) ),
	RecvPropInt(	 RECVINFO( m_nSpotlightTextureFrame ) ),
	RecvPropFloat(	 RECVINFO( m_flNearZ )	),
	RecvPropFloat(	 RECVINFO( m_flFarZ )	),
	RecvPropInt(	 RECVINFO( m_nShadowQuality )	),
END_RECV_TABLE()

C_EnvProjectedTexture::C_EnvProjectedTexture( void )
{
	m_LightHandle = CLIENTSHADOW_INVALID_HANDLE;
}

C_EnvProjectedTexture::~C_EnvProjectedTexture( void )
{
	ShutDownLightHandle();
}

void C_EnvProjectedTexture::ShutDownLightHandle( void )
{
	// Clear out the light
	if( m_LightHandle != CLIENTSHADOW_INVALID_HANDLE )
	{
		g_pClientShadowMgr->DestroyFlashlight( m_LightHandle );
		m_LightHandle = CLIENTSHADOW_INVALID_HANDLE;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : updateType - 
//-----------------------------------------------------------------------------
void C_EnvProjectedTexture::OnDataChanged( DataUpdateType_t updateType )
{
	UpdateLight();
	BaseClass::OnDataChanged( updateType );
}

void C_EnvProjectedTexture::UpdateLight( void )
{
	if ( m_bAlwaysUpdate )
	{
		m_bForceUpdate = true;
	}

	/*if ( !m_bForceUpdate )
	{
		bVisible = IsBBoxVisible();		
	}*/

	if ( m_bState == false )
	{
		if ( m_LightHandle != CLIENTSHADOW_INVALID_HANDLE )
		{
			ShutDownLightHandle();
		}

		return;
	}
	
	Vector vForward, vRight, vUp, vPos = GetAbsOrigin();
	FlashlightState_t state;

	if ( m_hTargetEntity != NULL )
	{
		if ( m_bCameraSpace )
		{
			const QAngle &angles = GetLocalAngles();

			C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
			if( pPlayer )
			{
				const QAngle playerAngles = pPlayer->GetAbsAngles();
				
				Vector vPlayerForward, vPlayerRight, vPlayerUp;
				AngleVectors( playerAngles, &vPlayerForward, &vPlayerRight, &vPlayerUp );

            	matrix3x4_t	mRotMatrix;
				AngleMatrix( angles, mRotMatrix );

				VectorITransform( vPlayerForward, mRotMatrix, vForward );
				VectorITransform( vPlayerRight, mRotMatrix, vRight );
				VectorITransform( vPlayerUp, mRotMatrix, vUp );

				float dist = (m_hTargetEntity->GetAbsOrigin() - GetAbsOrigin()).Length();
				vPos = m_hTargetEntity->GetAbsOrigin() - vForward*dist;

				VectorNormalize( vForward );
				VectorNormalize( vRight );
				VectorNormalize( vUp );
			}
		}
		else
		{
			vForward = m_hTargetEntity->GetAbsOrigin() - GetAbsOrigin();
			VectorNormalize( vForward );

			// JasonM - unimplemented
			Assert (0);

			//Quaternion q = DirectionToOrientation( dir );


			//
			// JasonM - set up vRight, vUp
			//

//			VectorNormalize( vRight );
//			VectorNormalize( vUp );
		}
	}
	else
	{
		AngleVectors( GetAbsAngles(), &vForward, &vRight, &vUp );
	}

	state.m_fHorizontalFOVDegrees = m_flLightFOV;
	state.m_fVerticalFOVDegrees = m_flLightFOV;

	state.m_vecLightOrigin = vPos;
	BasisToQuaternion( vForward, vRight, vUp, state.m_quatOrientation );

	/*state.m_fQuadraticAtten = r_projtex_quadratic.GetInt(); //0
	state.m_fLinearAtten = r_projtex_linear.GetInt(); //100
	state.m_fConstantAtten = r_projtex_constant.GetInt(); //0*/

	state.m_fQuadraticAtten = m_nLinear; //0
	state.m_fLinearAtten = m_nQuadratic; //100
	state.m_fConstantAtten = m_nConstant;

	state.m_Color[0] = m_LinearFloatLightColor.x;
	state.m_Color[1] = m_LinearFloatLightColor.y;
	state.m_Color[2] = m_LinearFloatLightColor.z;
	state.m_Color[3] = 0.0f; // fixme: need to make ambient work m_flAmbient;
	state.m_NearZ = m_flNearZ;
	state.m_FarZ = m_flFarZ;
	//state.m_flShadowSlopeScaleDepthBias = mat_slopescaledepthbias_shadowmap.GetFloat();
	state.m_flShadowSlopeScaleDepthBias = g_pMaterialSystemHardwareConfig->GetShadowSlopeScaleDepthBias();
	//state.m_flShadowDepthBias = mat_depthbias_shadowmap.GetFloat();
	state.m_flShadowDepthBias = g_pMaterialSystemHardwareConfig->GetShadowDepthBias();
	state.m_bEnableShadows = m_bEnableShadows;
	state.m_pSpotlightTexture = materials->FindTexture( m_SpotlightTextureName, TEXTURE_GROUP_OTHER, false );
	state.m_nSpotlightTextureFrame = m_nSpotlightTextureFrame;

	state.m_nShadowQuality = m_nShadowQuality; // Allow entity to affect shadow quality

	// NOTE: need to reload map when changing filtersize for this entity apparently, not for the flashlight though
	// also the filtersize change only takes effect on map reload aaaa
	// need to check to see if one of the values are changed, then set this to -1
	// also this is all setup for DoShadowNvidiaPCF5x5Gaussian lol
	/*if ( r_projtex_quality.GetInt() == 0 )
	{
		r_flashlightdepthres.SetValue( 512.0f );
		r_projtex_filtersize.SetValue( 2.0f );
	}
	else if ( r_projtex_quality.GetInt() == 1 )
	{
		r_flashlightdepthres.SetValue( 1024.0f );
		r_projtex_filtersize.SetValue( 1.0f );
	}
	else if ( r_projtex_quality.GetInt() == 2 )
	{
		r_flashlightdepthres.SetValue( 2048.0f );
		r_projtex_filtersize.SetValue( 0.5f );
	}
	else if ( r_projtex_quality.GetInt() == 3 )
	{
		r_flashlightdepthres.SetValue( 4096.0f );
		r_projtex_filtersize.SetValue( 0.25f );
	}
	else if ( r_projtex_quality.GetInt() == 4 )
	{
		r_flashlightdepthres.SetValue( 8192.0f );
		r_projtex_filtersize.SetValue( 0.125f );
	}*/
	
	state.m_flShadowFilterSize = r_projtex_filtersize.GetFloat();

	if( m_LightHandle == CLIENTSHADOW_INVALID_HANDLE )
	{
		m_LightHandle = g_pClientShadowMgr->CreateFlashlight( state );

		if ( m_LightHandle != CLIENTSHADOW_INVALID_HANDLE )
		{
			m_bForceUpdate = false;
		}
	}
	else
	{
		if ( m_hTargetEntity != NULL || m_bForceUpdate == true )
		{
			g_pClientShadowMgr->UpdateFlashlightState( m_LightHandle, state );
		}

		//g_pClientShadowMgr->UpdateFlashlightState( m_LightHandle, state );
		m_bForceUpdate = false;
	}

	if( m_bLightOnlyTarget )
	{
		g_pClientShadowMgr->SetFlashlightTarget( m_LightHandle, m_hTargetEntity );
	}
	else
	{
		g_pClientShadowMgr->SetFlashlightTarget( m_LightHandle, NULL );
	}

	g_pClientShadowMgr->SetFlashlightLightWorld( m_LightHandle, m_bLightWorld );

	if ( !m_bForceUpdate )
	{
		g_pClientShadowMgr->UpdateProjectedTexture( m_LightHandle, true );
	}
}

void C_EnvProjectedTexture::Simulate( void )
{
	UpdateLight();

	BaseClass::Simulate();
	//return true;
}

