//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Shared kart-mode definitions. See kart_shareddefs.h.
//
//=============================================================================//

#include "cbase.h"
#include "kart_shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar kart_enabled( "kart_enabled", "1", FCVAR_REPLICATED | FCVAR_NOTIFY, "Players spawn as karts instead of HL2DM characters. Takes effect on respawn." );
ConVar kart_model( "kart_model", KART_DEFAULT_MODEL, FCVAR_REPLICATED | FCVAR_NOTIFY, "Kart model, e.g. " KART_SCRAP_MODEL " or " KART_PLACEHOLDER_MODEL ". Takes effect on respawn. A model not precached at map start (set it before the map loads) falls back to " KART_DEFAULT_MODEL "." );

// Race flow. Replicated so the HUD can show "WAITING FOR PLAYERS (n/m)".
ConVar kart_min_players( "kart_min_players", "1", FCVAR_REPLICATED | FCVAR_NOTIFY, "Karts needed before a race starts (unless every kart says mp_ready_signal in chat).", true, 1, true, MAX_PLAYERS );

// Movement tuning (defaults duplicated in cfg/kart_tuning.cfg, which server.cfg and listenserver.cfg exec at map start).
#define KART_TUNING_FLAGS	( FCVAR_REPLICATED | FCVAR_NOTIFY )

// Speed
ConVar kart_max_speed( "kart_max_speed", "650", KART_TUNING_FLAGS, "Kart top speed with the throttle held, in units per second." );
ConVar kart_accel( "kart_accel", "300", KART_TUNING_FLAGS, "Kart acceleration toward top speed above kart_accel_low_threshold, in units per second squared." );
ConVar kart_accel_low( "kart_accel_low", "500", KART_TUNING_FLAGS, "Kart acceleration below kart_accel_low_threshold (launch from a standstill), in units per second squared." );
ConVar kart_accel_low_threshold( "kart_accel_low_threshold", "300", KART_TUNING_FLAGS, "Below this speed (units per second) the kart accelerates at kart_accel_low." );
ConVar kart_brake_decel( "kart_brake_decel", "500", KART_TUNING_FLAGS, "Kart deceleration with the opposite throttle held, in units per second squared." );
ConVar kart_reverse_speed( "kart_reverse_speed", "150", KART_TUNING_FLAGS, "Kart top speed in reverse, in units per second." );
ConVar kart_reverse_accel( "kart_reverse_accel", "250", KART_TUNING_FLAGS, "Kart acceleration in reverse, in units per second squared." );
ConVar kart_reverse_delay( "kart_reverse_delay", "0.25", KART_TUNING_FLAGS, "Seconds the kart waits at a standstill with the brake held before it reverses." );
ConVar kart_coast_decel( "kart_coast_decel", "120", KART_TUNING_FLAGS, "Kart deceleration with no throttle, in units per second squared." );

// Steering
ConVar kart_turn_rate_low( "kart_turn_rate_low", "120", KART_TUNING_FLAGS, "Kart steering rate at low speed, in degrees per second." );
ConVar kart_turn_rate_high( "kart_turn_rate_high", "70", KART_TUNING_FLAGS, "Kart steering rate at kart_max_speed, in degrees per second." );
ConVar kart_turn_min_speed( "kart_turn_min_speed", "20", KART_TUNING_FLAGS, "Below this speed (units per second) the kart cannot turn." );

// Air
ConVar kart_air_turn_scale( "kart_air_turn_scale", "0.5", KART_TUNING_FLAGS, "Steering rate multiplier while the kart is airborne." );
ConVar kart_air_control( "kart_air_control", "2", KART_TUNING_FLAGS, "How fast the airborne kart's velocity turns toward its heading, in fractions per second (0 = none)." );

// Wall bumps
ConVar kart_bump_threshold( "kart_bump_threshold", "100", KART_TUNING_FLAGS, "Speed (units per second) a collision has to take off the kart for a bump." );
ConVar kart_bump_restitution( "kart_bump_restitution", "0.5", KART_TUNING_FLAGS, "Fraction of the speed left along the heading that the kart keeps after a bump." );
ConVar kart_bump_cooldown( "kart_bump_cooldown", "0.3", KART_TUNING_FLAGS, "Minimum seconds between two kart bump sounds." );

// Hop and drift
ConVar kart_hop_velocity( "kart_hop_velocity", "160", KART_TUNING_FLAGS, "Upward speed of the kart's hop (jump tapped on the ground), in units per second." );
ConVar kart_drift_min_speed( "kart_drift_min_speed", "250", KART_TUNING_FLAGS, "Below this forward speed (units per second) the kart cannot drift, and a drift ends." );
ConVar kart_drift_slip_angle( "kart_drift_slip_angle", "25", KART_TUNING_FLAGS, "Degrees the kart's nose points into the turn past its direction of travel while drifting." );
ConVar kart_drift_turn_min( "kart_drift_turn_min", "45", KART_TUNING_FLAGS, "Drift turn rate while steering against the drift (widest line), in degrees per second." );
ConVar kart_drift_turn_max( "kart_drift_turn_max", "105", KART_TUNING_FLAGS, "Drift turn rate while steering into the drift (tightest line), in degrees per second." );
ConVar kart_drift_slip_rate( "kart_drift_slip_rate", "90", KART_TUNING_FLAGS, "How fast the slip angle builds up in a drift and straightens out after it, in degrees per second." );

// Mini-turbo and boost
ConVar kart_turbo_charge_min( "kart_turbo_charge_min", "0.75", KART_TUNING_FLAGS, "Mini-turbo charge per second of drifting while steering against the drift." );
ConVar kart_turbo_charge_max( "kart_turbo_charge_max", "1.25", KART_TUNING_FLAGS, "Mini-turbo charge per second of drifting while steering into the drift (halfway between with no steer)." );
ConVar kart_turbo_tier1_time( "kart_turbo_tier1_time", "0.6", KART_TUNING_FLAGS, "Drift charge for a tier 1 mini-turbo." );
ConVar kart_turbo_tier2_time( "kart_turbo_tier2_time", "1.4", KART_TUNING_FLAGS, "Drift charge for a tier 2 mini-turbo." );
ConVar kart_turbo_tier3_time( "kart_turbo_tier3_time", "2.4", KART_TUNING_FLAGS, "Drift charge for a tier 3 mini-turbo." );
ConVar kart_turbo_tier1_duration( "kart_turbo_tier1_duration", "0.6", KART_TUNING_FLAGS, "Seconds of boost from releasing a drift at tier 1." );
ConVar kart_turbo_tier2_duration( "kart_turbo_tier2_duration", "1.0", KART_TUNING_FLAGS, "Seconds of boost from releasing a drift at tier 2." );
ConVar kart_turbo_tier3_duration( "kart_turbo_tier3_duration", "1.6", KART_TUNING_FLAGS, "Seconds of boost from releasing a drift at tier 3." );
ConVar kart_boost_scale( "kart_boost_scale", "1.3", KART_TUNING_FLAGS, "Speed of a drift mini-turbo boost, as a multiple of kart_max_speed." );
ConVar kart_boost_decay( "kart_boost_decay", "400", KART_TUNING_FLAGS, "How fast a kart above kart_max_speed (after a boost) slows back down to it, in units per second squared." );

// Hit reactions
ConVar kart_spinout_time( "kart_spinout_time", "1.0", KART_TUNING_FLAGS, "Seconds a kart spins out for when an item hits it, with no inputs." );
ConVar kart_spinout_turns( "kart_spinout_turns", "2", KART_TUNING_FLAGS, "Full turns the kart's body spins through during a spin-out (drawn only: the heading does not change)." );
ConVar kart_spinout_speed_scale( "kart_spinout_speed_scale", "0.5", KART_TUNING_FLAGS, "Fraction of its speed a kart keeps when it starts to spin out." );
ConVar kart_spinout_decel( "kart_spinout_decel", "500", KART_TUNING_FLAGS, "How fast a spinning-out kart skids to a stop, in units per second squared." );
ConVar kart_stun_time( "kart_stun_time", "0.5", KART_TUNING_FLAGS, "Seconds a kart is stunned for: it steers and drives, but cannot hop or drift and is held to kart_stun_speed_scale." );
ConVar kart_stun_speed_scale( "kart_stun_speed_scale", "0.5", KART_TUNING_FLAGS, "A stunned kart's speed is cut to this fraction when hit and held under this fraction of kart_max_speed until the stun ends." );
ConVar kart_hit_immunity( "kart_hit_immunity", "1.0", KART_TUNING_FLAGS, "Seconds after a spin-out or stun ends during which the kart cannot be hit again." );

const color32 g_KartColors[KART_COLOR_COUNT] =
{
	{ 230,  60,  60, 255 },	// red
	{  60, 120, 230, 255 },	// blue
	{  70, 200,  90, 255 },	// green
	{ 245, 210,  50, 255 },	// yellow
	{ 240, 140,  40, 255 },	// orange
	{ 170,  80, 220, 255 },	// purple
	{  60, 210, 210, 255 },	// cyan
	{ 240, 120, 190, 255 },	// pink
};
