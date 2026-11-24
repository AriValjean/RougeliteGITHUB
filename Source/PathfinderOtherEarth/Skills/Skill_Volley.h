// Skill_Volley.h
// Ranger Basic: fire a fan of arrows at the crosshair. Each arrow damages the first enemy it hits
// and slows it. Point-blank, several arrows can hit the same enemy.
//
// All numbers come from the skill's DT_SkillsCPP row:
//   Cooldown, Damage (per arrow), Attack Damage Scaling, Can Crit, Projectile Count, Spread Angle,
//   Range, Projectile Speed, Magnitude (slow, 0.2 = 20%), Effect Duration (slow seconds).
// The Blueprint child (BP_Skill_Volley_Ranger) only picks the arrow look and the fire socket.
//
// Drop into: Source/PathfinderOtherEarth/Skills/Skill_Volley.h

#pragma once

#include "CoreMinimal.h"
#include "Skills/SkillBase.h"
#include "Skill_Volley.generated.h"

class ASkillProjectile;

UCLASS(Blueprintable)
class PATHFINDEROTHEREARTH_API USkill_Volley : public USkillBase
{
	GENERATED_BODY()

public:
	virtual void ActivateSkill_Implementation() override;

	/** Slow source name on the enemy's Status Effect component (Volley slows refresh, never stack). */
	static const FName SlowSourceName;

protected:
	/** Blueprint hooks for FX / sound. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Volley")
	void OnVolleyFired(FVector AimDirection);

	UFUNCTION(BlueprintImplementableEvent, Category = "Volley")
	void OnArrowHit(AActor* HitActor, float FinalDamage);

	/** The arrow. Leave empty to use the plain C++ projectile (invisible, still works). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Volley")
	TSubclassOf<ASkillProjectile> ProjectileClass;

	/** Mesh socket the arrows come from. Falls back to above the character if the socket is missing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Volley")
	FName FireSocket = TEXT("hand_l");

private:
	UFUNCTION()
	void HandleArrowHit(AActor* HitActor);

	FVector GetFireLocation() const;
};
