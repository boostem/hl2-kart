//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client side of the kart hazards (kart_hazards.cpp on the server).
//
//			C_KartHazardOil draws the Oil Slick's puddle model lying on the
//			ground. It spreads out as it lands and fades out before it is
//			removed; while it flies it is a small blob.
//
//=============================================================================//

#include "cbase.h"
#include "kart_hazards_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define KART_OIL_FLY_SCALE		0.3f	// size of the blob in the air, of the puddle's

class C_KartHazardOil : public C_BaseAnimating
{
	DECLARE_CLASS( C_KartHazardOil, C_BaseAnimating );
	DECLARE_CLIENTCLASS();

public:
	C_KartHazardOil();

	virtual void ApplyBoneMatrixTransform( matrix3x4_t &transform );
	virtual int DrawModel( int flags );

private:
	float GetPuddleScale( void ) const;

	float m_flDieTime;
	float m_flLandTime;
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

float C_KartHazardOil::GetPuddleScale( void ) const
{
	if ( m_flLandTime <= 0.0f )
		return KART_OIL_FLY_SCALE;

	float flGrow = clamp( ( gpGlobals->curtime - m_flLandTime ) / KART_OIL_GROW_TIME, 0.0f, 1.0f );
	return Lerp( SimpleSpline( flGrow ), KART_OIL_FLY_SCALE, 1.0f );
}

// Scales the puddle about its origin, like the model scale does.
void C_KartHazardOil::ApplyBoneMatrixTransform( matrix3x4_t &transform )
{
	BaseClass::ApplyBoneMatrixTransform( transform );

	float flScale = GetPuddleScale();
	Vector vecPos;
	MatrixGetColumn( transform, 3, vecPos );
	vecPos = GetRenderOrigin() + ( vecPos - GetRenderOrigin() ) * flScale;
	MatrixSetColumn( vecPos, 3, transform );
	VectorScale( transform[0], flScale, transform[0] );
	VectorScale( transform[1], flScale, transform[1] );
	VectorScale( transform[2], flScale, transform[2] );
}

int C_KartHazardOil::DrawModel( int flags )
{
	float flAlpha = clamp( ( m_flDieTime - gpGlobals->curtime ) / KART_OIL_FADE_TIME, 0.0f, 1.0f );
	if ( flAlpha <= 0.0f )
		return 0;

	// The material is translucent, so the view has set the blend: fade it.
	render->SetBlend( flAlpha * KART_OIL_MAX_ALPHA / 255.0f );
	return BaseClass::DrawModel( flags );
}
