// ProgressionComponent.cpp
// Drop into: Source/PathfinderOtherEarth/Combat/ProgressionComponent.cpp

#include "ProgressionComponent.h"

UProgressionComponent::UProgressionComponent()
{
	// Event-driven: nothing to do every frame.
	PrimaryComponentTick.bCanEverTick = false;
}

void UProgressionComponent::BeginPlay()
{
	Super::BeginPlay();

	// Tuning values are only final after the Blueprint defaults load, so compute here, not in the constructor.
	XPToNextLevel = GetXPRequiredForLevel(Level);
}

float UProgressionComponent::GetXPRequiredForLevel(int32 ForLevel) const
{
	const int32 SafeLevel = FMath::Max(ForLevel, 1);
	return FMath::RoundToFloat(BaseXPToNextLevel * FMath::Pow(XPGrowthPerLevel, static_cast<float>(SafeLevel - 1)));
}

void UProgressionComponent::AddXP(float Amount)
{
	if (Amount <= 0.f)
	{
		return;
	}

	CurrentXP += Amount;

	// Loop so one big reward can give several levels. The guard stops a bad tuning value from hanging the game.
	int32 Guard = 0;
	while (CurrentXP >= XPToNextLevel && XPToNextLevel > 0.f && Guard++ < 100)
	{
		CurrentXP -= XPToNextLevel;
		++Level;
		XPToNextLevel = GetXPRequiredForLevel(Level);
		OnLevelUp.Broadcast(Level);
	}

	// Sends the real threshold (the old Level Up math sent the leftover XP here by mistake).
	OnXPChanged.Broadcast(CurrentXP, XPToNextLevel, Level);
}

void UProgressionComponent::RestoreProgress(int32 NewLevel, float NewCurrentXP)
{
	Level = FMath::Max(NewLevel, 1);
	XPToNextLevel = GetXPRequiredForLevel(Level);
	CurrentXP = FMath::Clamp(NewCurrentXP, 0.f, XPToNextLevel);

	OnXPChanged.Broadcast(CurrentXP, XPToNextLevel, Level);
}

float UProgressionComponent::GetXPPercent() const
{
	return XPToNextLevel > 0.f ? CurrentXP / XPToNextLevel : 0.f;
}
