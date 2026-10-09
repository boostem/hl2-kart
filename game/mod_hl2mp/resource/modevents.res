//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//=============================================================================

// No spaces in event names, max length 32
// All strings are case sensitive
//
// valid data key types are:
//   string : a zero terminated string
//   bool   : unsigned int, 1 bit
//   byte   : unsigned int, 8 bit
//   short  : signed int, 16 bit
//   long   : signed int, 32 bit
//   float  : float, 32 bit
//   local  : any data, but not networked to clients
//
// following key names are reserved:
//   local      : if set to 1, event is not networked to clients
//   unreliable : networked, but unreliable
//   suppress   : never fire this event
//   time	: firing server time
//   eventid	: holds the event ID

"modevents"
{
	"player_death"				// a game event, name may be 32 charaters long
	{
		"userid"	"short"   	// user ID who died				
		"attacker"	"short"	 	// user ID who killed
		"weapon"	"string" 	// weapon name killed used 
	}
	
	"teamplay_round_start"			// round restart
	{
		"full_reset"	"bool"		// is this a full reset of the map
	}
	
	"spec_target_updated"
	{
	}
	
	"achievement_earned"
	{
		"player"	"byte"		// entindex of the player
		"achievement"	"short"		// achievement ID
	}

	"kart_checkpoint"			// a kart hit its next checkpoint (index 0: crossed the line to start lap 1)
	{
		"userid"	"short"		// user ID of the player
		"index"		"byte"		// checkpoint index
	}

	"kart_lap"				// a kart completed a lap
	{
		"userid"	"short"		// user ID of the player
		"lap"		"byte"		// the lap just completed, 1..laps
		"laptime"	"float"		// its time, seconds
	}

	"kart_race_finish"			// a kart completed its last lap
	{
		"userid"	"short"		// user ID of the player
		"position"	"byte"		// finishing position, 1 for the winner
		"totaltime"	"float"		// race time, seconds (the laps completed so far when dnf)
		"dnf"		"bool"		// finished for the player when kart_finish_timeout ran out
	}

	"kart_countdown"			// one tick of the countdown before a race
	{
		"seconds"	"byte"		// seconds left until the start: 3, 2, 1
	}

	"kart_race_start"			// the countdown is over: go
	{
		"laps"		"byte"		// laps in this race
	}

	"kart_item_pickup"			// a kart was given an item (the roulette starts)
	{
		"userid"	"short"		// user ID of the player
		"item"		"byte"		// KartItem_t (kart_items.h)
	}

	"kart_item_use"				// a kart used its held item
	{
		"userid"	"short"		// user ID of the player
		"item"		"byte"		// KartItem_t (kart_items.h)
		"backward"	"bool"		// thrown backwards
	}

	"kart_hit"					// an item hit a kart: it spins out or is stunned
	{
		"userid"	"short"		// user ID of the kart hit
		"attacker"	"short"		// user ID of the kart that threw the item, 0 for none
		"type"		"byte"		// KartHitType (kart_shareddefs.h): 1 spin-out, 2 stun
	}
}
