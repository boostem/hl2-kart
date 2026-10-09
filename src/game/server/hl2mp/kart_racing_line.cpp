//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The bots' racing line. See kart_racing_line.h.
//
//=============================================================================//

#include "cbase.h"
#include "kart_racing_line.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Steps per spline segment when measuring its length before resampling.
#define KART_RACING_LINE_SUBDIV		32

// Defaults and limits of the node keyvalues.
#define KART_PATH_NODE_WIDTH		128.0f
#define KART_PATH_NODE_SPEED_MIN	0.1f
#define KART_PATH_NODE_SPEED_MAX	1.0f

// ##################################################################################
//	>> kart_path_node
// ##################################################################################
LINK_ENTITY_TO_CLASS( kart_path_node, CKartPathNode );

BEGIN_DATADESC( CKartPathNode )
	DEFINE_KEYFIELD( m_iszNext, FIELD_STRING, "next" ),
	DEFINE_KEYFIELD( m_flWidth, FIELD_FLOAT, "width" ),
	DEFINE_KEYFIELD( m_flSpeedScale, FIELD_FLOAT, "speed_scale" ),
	DEFINE_KEYFIELD( m_bDrift, FIELD_BOOLEAN, "drift" ),
END_DATADESC()

CKartPathNode::CKartPathNode()
{
	m_iszNext = NULL_STRING;
	m_flWidth = KART_PATH_NODE_WIDTH;
	m_flSpeedScale = 1.0f;
	m_bDrift = false;
}

void CKartPathNode::Spawn( void )
{
	BaseClass::Spawn();

	if ( m_flWidth < 0.0f )
	{
		Warning( "[kart] kart_path_node '%s' width %.0f is negative, using 0.\n", GetDebugName(), m_flWidth );
		m_flWidth = 0.0f;
	}

	if ( m_flSpeedScale < KART_PATH_NODE_SPEED_MIN || m_flSpeedScale > KART_PATH_NODE_SPEED_MAX )
	{
		float flClamped = clamp( m_flSpeedScale, KART_PATH_NODE_SPEED_MIN, KART_PATH_NODE_SPEED_MAX );
		Warning( "[kart] kart_path_node '%s' speed_scale %.2f is outside %.1f-%.1f, using %.2f.\n", GetDebugName(), m_flSpeedScale,
			KART_PATH_NODE_SPEED_MIN, KART_PATH_NODE_SPEED_MAX, flClamped );
		m_flSpeedScale = flClamped;
	}
}

// ##################################################################################
//	>> CKartRacingLine
// ##################################################################################
CKartRacingLine::CKartRacingLine()
{
	m_flLength = 0.0f;
}

void CKartRacingLine::Clear( void )
{
	m_Nodes.RemoveAll();
	m_NodeDistances.RemoveAll();
	m_Samples.RemoveAll();
	m_flLength = 0.0f;
}

CKartPathNode *CKartRacingLine::GetNode( int i ) const
{
	if ( i < 0 || i >= m_Nodes.Count() )
		return NULL;
	return m_Nodes[i].Get();
}

int CKartRacingLine::CountMapNodes( void )
{
	int nCount = 0;
	CBaseEntity *pEnt = NULL;
	while ( ( pEnt = gEntList.FindEntityByClassname( pEnt, "kart_path_node" ) ) != NULL )
	{
		nCount++;
	}
	return nCount;
}

// The node 'pNode' chains to, or NULL (with a warning) when the chain breaks there.
static CKartPathNode *NextPathNode( CKartPathNode *pNode )
{
	string_t iszNext = pNode->GetNextName();
	if ( iszNext == NULL_STRING || !STRING( iszNext )[0] )
	{
		Warning( "[kart] Racing line breaks at kart_path_node '%s': it has no next node.\n", pNode->GetDebugName() );
		return NULL;
	}

	CBaseEntity *pTarget = gEntList.FindEntityByName( NULL, iszNext );
	if ( !pTarget )
	{
		Warning( "[kart] Racing line breaks at kart_path_node '%s': its next node '%s' doesn't exist.\n", pNode->GetDebugName(), STRING( iszNext ) );
		return NULL;
	}

	if ( !FClassnameIs( pTarget, "kart_path_node" ) )
	{
		Warning( "[kart] Racing line breaks at kart_path_node '%s': its next '%s' is a %s, not a kart_path_node.\n", pNode->GetDebugName(), STRING( iszNext ), pTarget->GetClassname() );
		return NULL;
	}

	if ( gEntList.FindEntityByName( pTarget, iszNext ) )
	{
		Warning( "[kart] Several entities are named '%s' (next of kart_path_node '%s'); the line follows the first one.\n", STRING( iszNext ), pNode->GetDebugName() );
	}

	return static_cast< CKartPathNode * >( pTarget );
}

void CKartRacingLine::Build( const Vector *pVecStart )
{
	Clear();

	CUtlVector< CKartPathNode * > all;
	CBaseEntity *pEnt = NULL;
	while ( ( pEnt = gEntList.FindEntityByClassname( pEnt, "kart_path_node" ) ) != NULL )
	{
		all.AddToTail( static_cast< CKartPathNode * >( pEnt ) );
	}

	// No nodes: a map without a racing line, nothing to say.
	if ( !all.Count() )
		return;

	// Walk the chain from the node nearest the start until it comes back to a
	// node it has seen; the loop runs from that node on.
	CKartPathNode *pFirst = all[0];
	if ( pVecStart )
	{
		float flBest = FLT_MAX;
		for ( int i = 0; i < all.Count(); i++ )
		{
			float flDist = ( all[i]->GetAbsOrigin() - *pVecStart ).LengthSqr();
			if ( flDist < flBest )
			{
				flBest = flDist;
				pFirst = all[i];
			}
		}
	}

	CUtlVector< CKartPathNode * > walk;
	int iLoopStart = -1;
	for ( CKartPathNode *pNode = pFirst; pNode; pNode = NextPathNode( pNode ) )
	{
		iLoopStart = walk.Find( pNode );
		if ( iLoopStart != walk.InvalidIndex() )
			break;

		walk.AddToTail( pNode );
	}

	if ( iLoopStart < 0 )
	{
		Warning( "[kart] The kart_path_node chain from '%s' isn't a closed loop: bots have no racing line.\n", pFirst->GetDebugName() );
		return;
	}

	if ( iLoopStart > 0 )
	{
		Warning( "[kart] kart_path_node '%s' leads into the racing line loop at '%s' but isn't on it.\n", walk[0]->GetDebugName(), walk[iLoopStart]->GetDebugName() );
	}

	int nLoop = walk.Count() - iLoopStart;
	if ( nLoop < KART_RACING_LINE_MIN_NODES )
	{
		Warning( "[kart] The racing line loop has %d kart_path_node; it needs at least %d.\n", nLoop, KART_RACING_LINE_MIN_NODES );
		return;
	}

	if ( nLoop < all.Count() )
	{
		for ( int i = 0; i < all.Count(); i++ )
		{
			if ( walk.Find( all[i] ) < iLoopStart )
			{
				Warning( "[kart] %d of %d kart_path_node are not on the racing line loop (one is '%s').\n", all.Count() - nLoop, all.Count(), all[i]->GetDebugName() );
				break;
			}
		}
	}

	// The loop, starting at its node nearest the start.
	int iRotate = 0;
	if ( pVecStart )
	{
		float flBest = FLT_MAX;
		for ( int i = 0; i < nLoop; i++ )
		{
			float flDist = ( walk[iLoopStart + i]->GetAbsOrigin() - *pVecStart ).LengthSqr();
			if ( flDist < flBest )
			{
				flBest = flDist;
				iRotate = i;
			}
		}
	}

	CUtlVector< Vector > points;
	for ( int i = 0; i < nLoop; i++ )
	{
		CKartPathNode *pNode = walk[iLoopStart + ( iRotate + i ) % nLoop];
		m_Nodes.AddToTail( pNode );
		points.AddToTail( pNode->GetAbsOrigin() );
	}

	// Sample the closed Catmull-Rom spline densely and measure it. Dense point
	// k lies on segment k / SUBDIV, which runs from node k / SUBDIV to the next.
	int nDense = nLoop * KART_RACING_LINE_SUBDIV;
	CUtlVector< Vector > dense;
	CUtlVector< float > denseDist;
	dense.SetCount( nDense + 1 );
	denseDist.SetCount( nDense + 1 );

	float flLength = 0.0f;
	for ( int k = 0; k <= nDense; k++ )
	{
		int iSeg = ( k / KART_RACING_LINE_SUBDIV ) % nLoop;
		float t = (float)( k % KART_RACING_LINE_SUBDIV ) / KART_RACING_LINE_SUBDIV;

		Catmull_Rom_Spline( points[( iSeg + nLoop - 1 ) % nLoop], points[iSeg], points[( iSeg + 1 ) % nLoop], points[( iSeg + 2 ) % nLoop], t, dense[k] );

		if ( k > 0 )
		{
			flLength += ( dense[k] - dense[k - 1] ).Length();
		}
		denseDist[k] = flLength;

		if ( k < nDense && k % KART_RACING_LINE_SUBDIV == 0 )
		{
			m_NodeDistances.AddToTail( flLength );
		}
	}

	if ( flLength < KART_RACING_LINE_SPACING * 2.0f )
	{
		Warning( "[kart] The racing line is only %.0f units long; its kart_path_node are too close together.\n", flLength );
		Clear();
		return;
	}

	// Resample it evenly: the spacing is the nearest to SPACING that fits the
	// loop a whole number of times, so the last sample joins the first cleanly.
	int nSamples = Max( 2, RoundFloatToInt( flLength / KART_RACING_LINE_SPACING ) );
	float flSpacing = flLength / nSamples;

	m_Samples.SetCount( nSamples );
	int k = 0;
	for ( int i = 0; i < nSamples; i++ )
	{
		float flDist = i * flSpacing;
		while ( k < nDense - 1 && denseDist[k + 1] <= flDist )
		{
			k++;
		}

		float flSeg = denseDist[k + 1] - denseDist[k];
		float f = flSeg > 0.0f ? ( flDist - denseDist[k] ) / flSeg : 0.0f;

		int iSeg = ( k / KART_RACING_LINE_SUBDIV ) % nLoop;
		float t = ( ( k % KART_RACING_LINE_SUBDIV ) + f ) / KART_RACING_LINE_SUBDIV;
		CKartPathNode *pFrom = m_Nodes[iSeg];
		CKartPathNode *pTo = m_Nodes[( iSeg + 1 ) % nLoop];

		KartRacingLinePoint_t &sample = m_Samples[i];
		VectorLerp( dense[k], dense[k + 1], f, sample.pos );
		sample.dir = dense[k + 1] - dense[k];
		if ( VectorNormalize( sample.dir ) == 0.0f )
		{
			sample.dir = points[( iSeg + 1 ) % nLoop] - points[iSeg];
			VectorNormalize( sample.dir );
		}
		sample.distance = flDist;
		sample.width = Lerp( t, pFrom->GetWidth(), pTo->GetWidth() );
		sample.speedScale = Lerp( t, pFrom->GetSpeedScale(), pTo->GetSpeedScale() );
		sample.drift = pFrom->IsDriftHint();
		sample.node = iSeg;
	}

	m_flLength = flLength;
}

void CKartRacingLine::LerpSamples( int i, float t, KartRacingLinePoint_t &point ) const
{
	const KartRacingLinePoint_t &a = m_Samples[i];
	const KartRacingLinePoint_t &b = m_Samples[( i + 1 ) % m_Samples.Count()];
	float flSpacing = m_flLength / m_Samples.Count();

	VectorLerp( a.pos, b.pos, t, point.pos );
	VectorLerp( a.dir, b.dir, t, point.dir );
	if ( VectorNormalize( point.dir ) == 0.0f )
	{
		point.dir = a.dir;
	}
	point.distance = a.distance + t * flSpacing;
	point.width = Lerp( t, a.width, b.width );
	point.speedScale = Lerp( t, a.speedScale, b.speedScale );
	point.drift = a.drift;
	point.node = a.node;
}

bool CKartRacingLine::GetPoint( float flDistance, KartRacingLinePoint_t &point ) const
{
	if ( !IsValid() )
		return false;

	flDistance = fmodf( flDistance, m_flLength );
	if ( flDistance < 0.0f )
	{
		flDistance += m_flLength;
	}

	float flSamples = flDistance / ( m_flLength / m_Samples.Count() );
	int i = clamp( (int)flSamples, 0, m_Samples.Count() - 1 );
	LerpSamples( i, clamp( flSamples - i, 0.0f, 1.0f ), point );
	return true;
}

bool CKartRacingLine::GetNearestPoint( const Vector &vecPos, KartRacingLinePoint_t &point ) const
{
	if ( !IsValid() )
		return false;

	int iBest = 0;
	float tBest = 0.0f;
	float flBest = FLT_MAX;
	for ( int i = 0; i < m_Samples.Count(); i++ )
	{
		const Vector &a = m_Samples[i].pos;
		const Vector &b = m_Samples[( i + 1 ) % m_Samples.Count()].pos;

		float t;
		Vector vecClosest;
		CalcClosestPointOnLineSegment( vecPos, a, b, vecClosest, &t );

		float flDist = ( vecClosest - vecPos ).LengthSqr();
		if ( flDist < flBest )
		{
			flBest = flDist;
			iBest = i;
			tBest = t;
		}
	}

	LerpSamples( iBest, tBest, point );
	return true;
}
