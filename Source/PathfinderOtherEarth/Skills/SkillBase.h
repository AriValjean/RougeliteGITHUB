// SkillBase.h
// Parent of every class skill (dash, basic, ultimate).
// USkillComponent creates one instance per equipped skill and calls CanActivate / ActivateSkill.
// Children: C++ (e.g. USkill_Dash) or Blueprint. A skill must call End Skill when it finishes.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Items/PathfinderTypes.h"
#include "SkillBase.generated.h"

class ACharacter;
class USkillComponent;

UCLASS(Abstract, Blueprintable, BlueprintType)
class PATHFINDEROTHEREARTH_API USkillBase : public UObject
{
	GENERATED_BODY()

public:
	/** Called by USkillComponent right after it creates this skill. */
	void InitSkill(USkillComponent* InComponent, FName InSkillRow, ESkillSlot InSlot, const FSkillDefinition& InDefinition);

	/** Gives Blueprint children a world, so Delay, timers and Spawn Actor work inside skills. */
	virtual UWorld* GetWorld() const override;

	/** Extra checks before the skill can start (cooldown is already checked by the component). */
	UFUNCTION(BlueprintNativeEvent, Category = "Skill")
	bool CanActivate() const;

	/** Runs the skill. The cooldown has already started. Call End Skill when the skill is finished. */
	UFUNCTION(BlueprintNativeEvent, Category = "Skill")
	void ActivateSkill();

	/** Marks the skill as finished. Safe to call more than once. */
	UFUNCTION(BlueprintCallable, Category = "Skill")
	void EndSkill();

	/** Plays the Montage from this skill's DT_SkillsCPP row, if one is set. Returns its length (0 if none). */
	UFUNCTION(BlueprintCallable, Category = "Skill")
	float PlaySkillMontage(float PlayRate = 1.f);

	UFUNCTION(BlueprintPure, Category = "Skill")
	bool IsActive() const { return bIsActive; }

	UFUNCTION(BlueprintPure, Category = "Skill")
	ACharacter* GetOwningCharacter() const;

	UFUNCTION(BlueprintPure, Category = "Skill")
	USkillComponent* GetSkillComponent() const { return OwningComponent; }

	UFUNCTION(BlueprintPure, Category = "Skill")
	ESkillSlot GetSlot() const { return Slot; }

	UFUNCTION(BlueprintPure, Category = "Skill")
	FName GetSkillRow() const { return SkillRow; }

	UFUNCTION(BlueprintPure, Category = "Skill")
	const FSkillDefinition& GetDefinition() const { return Definition; }

	/** Internal: the component calls this right before ActivateSkill. */
	void MarkActive() { bIsActive = true; }

protected:
	/** Blueprint hook: runs whenever the skill ends (finished normally or cut short). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Skill")
	void OnSkillEnded();

	/** C++ hook for children: clean up (timers, tags, movement) when the skill ends. */
	virtual void NativeOnSkillEnded() {}

	UPROPERTY(Transient)
	TObjectPtr<USkillComponent> OwningComponent;

	UPROPERTY(Transient)
	FName SkillRow;

	UPROPERTY(Transient)
	ESkillSlot Slot = ESkillSlot::Dash;

	UPROPERTY(Transient)
	FSkillDefinition Definition;

private:
	bool bIsActive = false;
};
