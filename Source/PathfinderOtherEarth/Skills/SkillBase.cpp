// SkillBase.cpp

#include "Skills/SkillBase.h"
#include "Skills/SkillComponent.h"
#include "GameFramework/Character.h"
#include "Animation/AnimMontage.h"

void USkillBase::InitSkill(USkillComponent* InComponent, FName InSkillRow, ESkillSlot InSlot, const FSkillDefinition& InDefinition)
{
	OwningComponent = InComponent;
	SkillRow = InSkillRow;
	Slot = InSlot;
	Definition = InDefinition;
}

UWorld* USkillBase::GetWorld() const
{
	// The class default object has no world. Returning nullptr there is what lets the
	// Blueprint editor offer world nodes (Delay, timers) in children without errors.
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}
	return OwningComponent ? OwningComponent->GetWorld() : nullptr;
}

bool USkillBase::CanActivate_Implementation() const
{
	return GetOwningCharacter() != nullptr && !bIsActive;
}

void USkillBase::ActivateSkill_Implementation()
{
	// Base skill does nothing. Children override this.
	EndSkill();
}

void USkillBase::EndSkill()
{
	if (!bIsActive)
	{
		return;
	}
	bIsActive = false;
	NativeOnSkillEnded();
	OnSkillEnded();
}

float USkillBase::PlaySkillMontage(float PlayRate)
{
	ACharacter* Character = GetOwningCharacter();
	UAnimMontage* Montage = Definition.Montage.LoadSynchronous();
	if (!Character || !Montage)
	{
		return 0.f;
	}
	return Character->PlayAnimMontage(Montage, PlayRate);
}

ACharacter* USkillBase::GetOwningCharacter() const
{
	return OwningComponent ? Cast<ACharacter>(OwningComponent->GetOwner()) : nullptr;
}
