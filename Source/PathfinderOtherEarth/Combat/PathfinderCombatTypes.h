// PathfinderCombatTypes.h
// Damage data shared by weapons, enemies and the health component.
// Replaces the Blueprint S_DamageInfo struct and E_DamageResponses enum.
//
// Drop into: Source/PathfinderOtherEarth/Combat/PathfinderCombatTypes.h

#pragma once

#include "CoreMinimal.h"
#include "PathfinderCombatTypes.generated.h"

class AActor;

/** How the target should react to a hit. Not acted on yet; kept for future hit reactions. */
UENUM(BlueprintType)
enum class EDamageResponse : uint8
{
	None,
	DefaultReaction UMETA(DisplayName = "Default Reaction"),
	Stagger,
	Stun,
	Knockback
};

/**
 * One hit of damage. Make it, fill it, pass it to UHealthComponent::TakeDamage.
 * Amount is the damage BEFORE the target's Defense; the health component applies Defense.
 */
USTRUCT(BlueprintType)
struct PATHFINDEROTHEREARTH_API FDamageInfo
{
	GENERATED_BODY()

	/** Raw damage before the target's Defense (e.g. the Final Damage from Compute Hit Damage). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float Amount = 0.f;

	/** True when the attacker rolled a crit. Used for the red/orange damage numbers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (DisplayName = "Is Critical"))
	bool bIsCritical = false;

	/** Hit reaction to play on the target (future use). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	EDamageResponse DamageResponse = EDamageResponse::None;

	/** Who dealt the damage (the player character or the enemy). Passed on to On Death as the killer. */
	UPROPERTY(BlueprintReadWrite, Category = "Damage")
	TObjectPtr<AActor> Instigator = nullptr;
};
