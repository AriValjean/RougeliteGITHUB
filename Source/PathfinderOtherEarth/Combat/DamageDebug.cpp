// DamageDebug.cpp
// Drop into: Source/PathfinderOtherEarth/Combat/DamageDebug.cpp

#include "Combat/DamageDebug.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY_STATIC(LogDamageDebug, Log, All);

static TAutoConsoleVariable<int32> CVarPathfinderDebugDamage(
	TEXT("Pathfinder.DebugDamage"),
	0,
	TEXT("1 = print a damage breakdown (base, Attack Damage, crit, marks, Defense, final) on screen and in the log for every hit."),
	ECVF_Cheat);

bool PathfinderDamageDebug::IsEnabled()
{
	return CVarPathfinderDebugDamage.GetValueOnGameThread() != 0;
}

void PathfinderDamageDebug::Print(const FString& Message, const FColor& Color)
{
	if (!IsEnabled())
	{
		return;
	}
	UE_LOG(LogDamageDebug, Log, TEXT("%s"), *Message);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 6.f, Color, Message);
	}
}
