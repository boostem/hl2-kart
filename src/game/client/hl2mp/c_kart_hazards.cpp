//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client side of the kart hazards (kart_hazards.cpp on the server).
//
//			C_KartHazardOil draws the Oil Slick as a flat translucent quad
//			lying on the ground, lit by the world light at its spot. It
//			spreads out as it lands and fades out before it is removed; while
//			it flies it is a small blob.
//
//=============================================================================//

#include "cbase.h"
#include "kart_hazards_shared.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/MaterialSystemUtil.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define KART_OIL_FLY_SCALE		0.3f	// size of the blob in the air, of the puddle's

class C_KartHazardOil : public C_BaseEntity
{
	DECLARE_CLASS( C_KartHazardOil, C_BaseEntity );
	DECLARE_CLIENTCLASS();

public:
	C_KartHazardOil();

	virtual bool ShouldDraw( void ) { return !IsDormant() && !IsEffectActive( EF_NODRAW ); }
	virtual bool IsTransparent( void ) { return true; }
	virtual bool IsTwoPass( void ) { return false; }
	virtual void GetRenderBounds( Vector &mins, Vector &maxs );
	virtual int DrawModel( int flags );

private:
	float m_flDieTime;
	float m_flLandTime;
	CMaterialReference m_Material;
};

IMPLEMENT_CLIENTCLASS_DT( C_KartHazardOil, DT_KartHazardOil, CKartHazardOil )
	RecvPropTime( RECVINFO( m_flDieTime ) ),
	RecvPropTime( RECVINFO( m_flLandTime ) ),
END_RECV_TABLE()

C_KartHazardOil::C_KartHazardOil()
{
	m_flDieTime = 0.0f;
	m_flLandTime = 0.0f;
}

void C_KartHazardOil::GetRenderBounds( Vector &mins, Vector &maxs )
{
	mins.Init( -KART_OIL_RADIUS, -KART_OIL_RADIUS, -2.0f );
	maxs.Init( KART_OIL_RADIUS, KART_OIL_RADIUS, 2.0f );
}

int C_KartHazardOil::DrawModel( int flags )
{
	if ( !m_Material.IsValid() )
	{
		m_Material.Init( KART_OIL_MATERIAL, TEXTURE_GROUP_CLIENT_EFFECTS );
	}

	float flTime = gpGlobals->curtime;
	float flScale = KART_OIL_FLY_SCALE;
	if ( m_flLandTime > 0.0f )
	{
		float flGrow = clamp( ( flTime - m_flLandTime ) / KART_OIL_GROW_TIME, 0.0f, 1.0f );
		flScale = Lerp( SimpleSpline( flGrow ), KART_OIL_FLY_SCALE, 1.0f );
	}

	float flAlpha = clamp( ( m_flDieTime - flTime ) / KART_OIL_FADE_TIME, 0.0f, 1.0f );
	if ( flAlpha <= 0.0f )
		return 0;

	// Lit by the world at its spot, so it doesn't glow in the dark.
	Vector vecLight = engine->GetLightForPoint( GetAbsOrigin(), true );
	unsigned char color[4];
	for ( int i = 0; i < 3; i++ )
	{
		color[i] = (unsigned char)( 255.0f * clamp( vecLight[i], 0.0f, 1.0f ) );
	}
	color[3] = (unsigned char)( KART_OIL_MAX_ALPHA * flAlpha );

	Vector vecForward, vecRight, vecUp;
	AngleVectors( GetAbsAngles(), &vecForward, &vecRight, &vecUp );
	vecForward *= KART_OIL_RADIUS * flScale;
	vecRight *= KART_OIL_RADIUS * flScale;
	const Vector &vecCenter = GetAbsOrigin();

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Bind( m_Material );
	IMesh *pMesh = pRenderContext->GetDynamicMesh();

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	const float flCorners[4][2] = { { 1, -1 }, { 1, 1 }, { -1, 1 }, { -1, -1 } };
	const float flTexCoords[4][2] = { { 1, 0 }, { 1, 1 }, { 0, 1 }, { 0, 0 } };
	for ( int i = 0; i < 4; i++ )
	{
		Vector vecPos = vecCenter + vecForward * flCorners[i][0] + vecRight * flCorners[i][1];
		meshBuilder.Position3fv( vecPos.Base() );
		meshBuilder.Color4ubv( color );
		meshBuilder.TexCoord2f( 0, flTexCoords[i][0], flTexCoords[i][1] );
		meshBuilder.Normal3fv( vecUp.Base() );
		meshBuilder.AdvanceVertex();
	}

	meshBuilder.End();
	pMesh->Draw();
	return 1;
}
