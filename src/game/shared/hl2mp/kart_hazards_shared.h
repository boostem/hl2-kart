//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Kart hazard definitions shared by the server's hazard entities
//			(kart_hazards.cpp) and the client that draws them
//			(c_kart_hazards.cpp).
//
//=============================================================================//

#ifndef KART_HAZARDS_SHARED_H
#define KART_HAZARDS_SHARED_H
#ifdef _WIN32
#pragma once
#endif

// Oil Slick. The model (assets_src/items/oil_slick/) is a puddle on a
// 2 * KART_OIL_RADIUS square.
#define KART_OIL_RADIUS				40.0f	// half the puddle's square and of the trigger's
#define KART_OIL_TRIGGER_HEIGHT		16.0f	// trigger height above the ground; a hop clears it
#define KART_OIL_GROW_TIME			0.25f	// the puddle spreads from nothing as it lands
#define KART_OIL_FADE_TIME			0.75f	// it fades out over its last moments
#define KART_OIL_MAX_ALPHA			230

#endif // KART_HAZARDS_SHARED_H
