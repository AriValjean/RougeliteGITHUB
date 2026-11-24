// DamageDebug.h
// On-screen + log damage breakdown, toggled with the console command:
//     Pathfinder.DebugDamage 1   (on)
//     Pathfinder.DebugDamage 0   (off)
// Prints one line where damage is calculated ([Weapon] / [Skill]: base, Attack Damage, crit)
// and one line where it lands ([Hit]: incoming, damage-taken multiplier e.g. marks, Defense, final).
//
// Drop into: Source/PathfinderOtherEarth/Combat/DamageDebug.h

#pragma once

#include "CoreMinimal.h"

namespace PathfinderDamageDebug
{
	/** True while Pathfinder.DebugDamage is 1. Check this before building the message (keeps it free when off). */
	PATHFINDEROTHEREARTH_API bool IsEnabled();

	/** Prints to the screen (6 s) and the Output Log. Does nothing when disabled. */
	PATHFINDEROTHEREARTH_API void Print(const FString& Message, const FColor& Color = FColor::Cyan);
}
