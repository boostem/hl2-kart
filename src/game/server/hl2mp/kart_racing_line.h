//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The racing line bots follow, authored in Hammer:
//
//			kart_path_node		a point on the line, chained to the following
//								one by its "next" targetname (like path_track).
//
//			The race manager builds a closed Catmull-Rom spline through the
//			chain on map spawn, resamples it at an even spacing and answers
//			queries along it. Distance 0 is the node nearest the kart_finish.
//
//=============================================================================//

#ifndef KART_RACING_LINE_H
#define KART_RACING_LINE_H
#ifdef _WIN32
#pragma once
#endif

// Spacing of the cached samples along the spline, in units.
#define KART_RACING_LINE_SPACING	64.0f

// Fewest nodes a closed line needs.
#define KART_RACING_LINE_MIN_NODES	3

//-----------------------------------------------------------------------------
// kart_path_node: one point on the racing line.
//-----------------------------------------------------------------------------
class CKartPathNode : public CPointEntity
{
	DECLARE_CLASS( CKartPathNode, CPointEntity );
	DECLARE_DATADESC();

public:
	CKartPathNode();

	virtual void Spawn( void );

	string_t GetNextName( void ) const { return m_iszNext; }
	float GetWidth( void ) const { return m_flWidth; }
	float GetSpeedScale( void ) const { return m_flSpeedScale; }
	bool IsDriftHint( void ) const { return m_bDrift; }

private:
	string_t m_iszNext;		// targetname of the following node
	float m_flWidth;		// how far a bot may stray from the line, either side
	float m_flSpeedScale;	// fraction of top speed to aim for here
	bool m_bDrift;			// a corner worth drifting through
};

//-----------------------------------------------------------------------------
// A point on the racing line.
//-----------------------------------------------------------------------------
struct KartRacingLinePoint_t
{
	Vector	pos;			// world position on the spline
	Vector	dir;			// unit tangent, in the driving direction
	float	distance;		// distance along the line from its start, [0, length)
	float	width;			// allowed deviation either side, lerped between nodes
	float	speedScale;		// target speed fraction, lerped between nodes
	bool	drift;			// the segment's start node is a drift hint
	int		node;			// line node the segment starts at (see GetNode)
};

//-----------------------------------------------------------------------------
// The racing line: the node loop and the spline sampled along it.
//-----------------------------------------------------------------------------
class CKartRacingLine
{
public:
	CKartRacingLine();

	// Rebuilds the line from the kart_path_node entities on the map, warning
	// about broken chains. Leaves the line empty when there is no closed loop.
	// vecStart picks the node the line starts at (the one nearest it).
	void Build( const Vector *pVecStart );
	void Clear( void );

	bool IsValid( void ) const { return m_Samples.Count() >= 2; }
	float GetLength( void ) const { return m_flLength; }

	int GetNodeCount( void ) const { return m_Nodes.Count(); }
	CKartPathNode *GetNode( int i ) const;
	// Distance along the line of node i.
	float GetNodeDistance( int i ) const { return m_NodeDistances[i]; }

	int GetSampleCount( void ) const { return m_Samples.Count(); }
	const KartRacingLinePoint_t &GetSample( int i ) const { return m_Samples[i]; }

	// The point flDistance along the line, wrapped to [0, length). False when
	// the line is empty.
	bool GetPoint( float flDistance, KartRacingLinePoint_t &point ) const;

	// The point on the line nearest vecPos. False when the line is empty.
	bool GetNearestPoint( const Vector &vecPos, KartRacingLinePoint_t &point ) const;

	// The point on the line nearest vecPos, looking only within flWindow of
	// flDistance either way along the line, so a kart stays on its own stretch
	// where the track passes close to itself. False when the line is empty.
	bool GetNearestPointNear( const Vector &vecPos, float flDistance, float flWindow, KartRacingLinePoint_t &point ) const;

	// Number of nodes on the map, whether or not they made it onto the line.
	static int CountMapNodes( void );

private:
	// Lerps between samples i and i + 1 (wrapping) at fraction t.
	void LerpSamples( int i, float t, KartRacingLinePoint_t &point ) const;

	// The point on segments iFirst .. iFirst + nCount - 1 (wrapping) nearest vecPos.
	void NearestOnSegments( const Vector &vecPos, int iFirst, int nCount, KartRacingLinePoint_t &point ) const;

	CUtlVector< CHandle< CKartPathNode > > m_Nodes;
	CUtlVector< float > m_NodeDistances;
	CUtlVector< KartRacingLinePoint_t > m_Samples;
	float m_flLength;
};

#endif // KART_RACING_LINE_H
