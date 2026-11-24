// SkillZone.h
// Reusable ground area: lasts a while and "ticks" on every enemy inside (Rain of Arrows now;
// Wizard fire fields / poison clouds later). The skill that spawned it decides what a tick does
// by listening to On Zone Tick Target.
//
// Make a Blueprint child per look (e.g. BP_Zone_RainOfArrows) and use On Zone Started / On Zone Tick /
// On Zone Ended for the ground ring and falling-arrow FX. The actor lingers ~1.5 s after it ends so
// FX can fade.
//
// Drop into: Source/PathfinderOtherEarth/Skills/SkillZone.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkillZone.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillZoneTarget, AActor*, Target);

UCLASS(Blueprintable)
class PATHFINDEROTHEREARTH_API ASkillZone : public AActor
{
	GENERATED_BODY()

public:
	ASkillZone();

	/** Starts ticking: first tick right away, then every Tick Interval, for Duration seconds. */
	UFUNCTION(BlueprintCallable, Category = "Zone")
	void StartZone(float InRadius, float InDuration, float InTickInterval);

	/** Fired once per enemy inside, every tick. */
	UPROPERTY(BlueprintAssignable, Category = "Zone")
	FOnSkillZoneTarget OnZoneTickTarget;

	UFUNCTION(BlueprintPure, Category = "Zone")
	float GetRadius() const { return Radius; }

	/** Ends the zone early (e.g. the skill was removed). */
	UFUNCTION(BlueprintCallable, Category = "Zone")
	void FinishZone();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Blueprint hooks for FX. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Zone")
	void OnZoneStarted(float ZoneRadius, float ZoneDuration);

	UFUNCTION(BlueprintImplementableEvent, Category = "Zone")
	void OnZoneTick();

	UFUNCTION(BlueprintImplementableEvent, Category = "Zone")
	void OnZoneEnded();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zone")
	TObjectPtr<USceneComponent> Root;

private:
	void DoTick();

	float Radius = 400.f;
	float TickInterval = 0.5f;
	bool bEnded = false;

	FTimerHandle TickTimer;
	FTimerHandle EndTimer;
};
