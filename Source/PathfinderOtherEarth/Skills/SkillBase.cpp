// SkillBase.cpp
// Drop into: Source/PathfinderOtherEarth/Skills/SkillBase.cpp

#include "Skills/SkillBase.h"
#include "Skills/SkillComponent.h"
#include "Skills/SkillTargetIndicator.h"
#include "TimerManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimMontage.h"
#include "Stats/StatsManagerComponent.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageDebug.h"
#include "Engine/World.h"

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

	// Lasting skills (Cooldown Starts On End) start their cooldown now.
	if (OwningComponent)
	{
		OwningComponent->NotifySkillEnded(this);
	}
}

void USkillBase::NotifyRemoved()
{
	EndTargeting(false);
	EndSkill();
	NativeOnRemoved();
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

float USkillBase::GetModifiedValue(ESkillStat Stat, float BaseValue) const
{
	return OwningComponent ? OwningComponent->GetModifiedSkillValue(Slot, Stat, BaseValue) : BaseValue;
}

// ----------------------------------------------------------------------------
// Damage
// ----------------------------------------------------------------------------

float USkillBase::ComputeSkillDamage(float BaseDamage, float AttackDamageScaling, bool bCanCrit, bool& bIsCritical)
{
	bIsCritical = false;

	float AttackDamage = 1.f;
	float CritRate = 0.f;
	float CritDamage = 1.5f;

	const ACharacter* Character = GetOwningCharacter();
	if (const UStatsManagerComponent* Stats = Character ? Character->FindComponentByClass<UStatsManagerComponent>() : nullptr)
	{
		AttackDamage = Stats->GetTotal(ECharacterStatType::AttackDamage);
		CritRate = Stats->GetTotal(ECharacterStatType::CriticalRate);
		CritDamage = Stats->GetTotal(ECharacterStatType::CriticalDamage);
	}

	// Attack Damage is a multiplier (base 1.0). Scaling 0.25 = only a quarter of any bonus counts.
	const float AttackFactor = FMath::Max(0.f, 1.f + (AttackDamage - 1.f) * AttackDamageScaling);
	const float Scaled = BaseDamage * AttackFactor;

	// Same crit rules as AWeaponBase::ComputeHitDamage: chance clamped 0..1, rolled once,
	// multiplier = Critical Damage stat (base 1.5). Skills have no slot crit bonus.
	// Game rule: abilities never crit on their own. They can only crit while the player has the
	// Skill.CanCrit tag (from an accessory) AND the skill is allowed to (Can Crit in its row).
	const bool bCritUnlocked = OwningComponent && OwningComponent->HasStateTag(TAG_Skill_CanCrit);
	const bool bCritAllowed = bCanCrit && bCritUnlocked;

	float Final = Scaled;
	if (bCritAllowed && FMath::FRand() < FMath::Clamp(CritRate, 0.f, 1.f))
	{
		bIsCritical = true;
		Final = Scaled * CritDamage;
	}

	if (PathfinderDamageDebug::IsEnabled())
	{
		const FString SkillName = Definition.DisplayName.IsEmpty() ? SkillRow.ToString() : Definition.DisplayName.ToString();
		FString CritPart = !bCanCrit ? FString(TEXT(" (this skill can't crit)"))
			: (!bCritUnlocked ? FString(TEXT(" (crit locked: needs Skill.CanCrit)")) : FString(TEXT(" (no crit)")));
		if (bIsCritical)
		{
			CritPart = FString::Printf(TEXT(", CRIT x%.2f = %.1f"), CritDamage, Final);
		}
		PathfinderDamageDebug::Print(FString::Printf(
			TEXT("[Skill] %s: base %.1f x attack %.2f (stat %.2f, scaling %.2f) = %.1f%s"),
			*SkillName, BaseDamage, AttackFactor, AttackDamage, AttackDamageScaling, Scaled, *CritPart));
	}

	return FMath::Max(0.f, Final);
}

bool USkillBase::ApplySkillDamage(AActor* Target, float BaseDamage, float AttackDamageScaling, bool bCanCrit,
	EDamageResponse Response, float& FinalDamage, bool& bIsCritical)
{
	FinalDamage = 0.f;
	bIsCritical = false;

	UHealthComponent* Health = UHealthComponent::FindHealthComponent(Target);
	if (!Health)
	{
		return false;
	}

	FDamageInfo Info;
	Info.Amount = ComputeSkillDamage(BaseDamage, AttackDamageScaling, bCanCrit, bIsCritical);
	Info.bIsCritical = bIsCritical;
	Info.DamageResponse = Response;
	Info.Instigator = GetOwningCharacter();

	return Health->TakeDamage(Info, FinalDamage);
}

// ----------------------------------------------------------------------------
// Aiming
// ----------------------------------------------------------------------------

FVector USkillBase::GetCrosshairAimPoint(float Range) const
{
	const ACharacter* Character = GetOwningCharacter();
	const UWorld* World = GetWorld();
	if (!Character || !World)
	{
		return FVector::ZeroVector;
	}

	// Start at the camera, like the weapons' crosshair trace.
	FVector ViewLocation = Character->GetActorLocation();
	FRotator ViewRotation = Character->GetControlRotation();
	if (const APlayerController* PC = Cast<APlayerController>(Character->GetController()))
	{
		PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}

	const FVector End = ViewLocation + ViewRotation.Vector() * Range;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SkillCrosshairAim), false);
	IgnoreOwnerAndGear(Params);
	if (World->LineTraceSingleByChannel(Hit, ViewLocation, End, ECC_Visibility, Params))
	{
		return Hit.ImpactPoint;
	}
	return End;
}

FVector USkillBase::GetGroundTargetLocation(float Range) const
{
	const ACharacter* Character = GetOwningCharacter();
	const UWorld* World = GetWorld();
	if (!Character || !World)
	{
		return FVector::ZeroVector;
	}

	// Where the crosshair points (far trace), flattened and clamped to Range around the player.
	const FVector CharacterLocation = Character->GetActorLocation();
	const FVector AimPoint = GetCrosshairAimPoint(FMath::Max(Range, 1.f) * 4.f);
	FVector Flat = AimPoint - CharacterLocation;
	Flat.Z = 0.f;
	if (Flat.Size() > Range)
	{
		Flat = Flat.GetSafeNormal() * Range;
	}
	const FVector Probe = CharacterLocation + Flat;

	// Drop onto the ground (level geometry only, so enemies don't lift the circle).
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SkillGroundTarget), false);
	FCollisionResponseParams Responses;
	const ECollisionChannel Channel = GetWorldTraceSettings(Params, Responses);
	const FVector Start = Probe + FVector(0.f, 0.f, 500.f);
	const FVector End = Probe - FVector(0.f, 0.f, 3000.f);
	if (World->LineTraceSingleByChannel(Hit, Start, End, Channel, Params, Responses))
	{
		return Hit.ImpactPoint;
	}

	// No ground found: use the player's feet height.
	return FVector(Probe.X, Probe.Y, CharacterLocation.Z - Character->GetSimpleCollisionHalfHeight());
}

FVector USkillBase::ComputeAimLocation() const
{
	return IndicatorShape == EAimIndicatorShape::Line
		? GetCrosshairAimPoint(Definition.Range)
		: GetGroundTargetLocation(Definition.Range);
}

float USkillBase::ComputeLineLength(const FVector& FlatDirection) const
{
	const ACharacter* Character = GetOwningCharacter();
	const UWorld* World = GetWorld();
	const float Range = Definition.Range;
	if (!Character || !World)
	{
		return Range;
	}

	// Stop the rectangle at the first wall (level geometry only, enemies don't shorten it).
	const FVector Start = Character->GetActorLocation();
	const FVector End = Start + FlatDirection * Range;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SkillLineIndicator), false);
	FCollisionResponseParams Responses;
	const ECollisionChannel Channel = GetWorldTraceSettings(Params, Responses);
	if (World->LineTraceSingleByChannel(Hit, Start, End, Channel, Params, Responses))
	{
		return Hit.Distance;
	}
	return Range;
}

void USkillBase::IgnoreOwnerAndGear(FCollisionQueryParams& Params) const
{
	ACharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		return;
	}
	Params.AddIgnoredActor(Character);

	// Weapons attached to the character (in a socket or floating at an offset).
	TArray<AActor*> Attached;
	Character->GetAttachedActors(Attached, true, true);
	Params.AddIgnoredActors(Attached);

	// Anything spawned with the character as its Owner, attached or not.
	for (AActor* Child : Character->Children)
	{
		if (Child)
		{
			Params.AddIgnoredActor(Child);
		}
	}
}

ECollisionChannel USkillBase::GetWorldTraceSettings(FCollisionQueryParams& Params, FCollisionResponseParams& Responses) const
{
	IgnoreOwnerAndGear(Params);

	// Trace like the player's capsule moves: same channel, same responses. Walls and floors block it
	// whether they are Static or Dynamic meshes; triggers / pickups (overlap only) never do.
	ECollisionChannel Channel = ECC_Pawn;
	const ACharacter* Character = GetOwningCharacter();
	if (const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr)
	{
		Channel = Capsule->GetCollisionObjectType();
		Responses.CollisionResponse = Capsule->GetCollisionResponseToChannels();
	}

	// Enemies (and other characters) never count as ground or walls for skills.
	Responses.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
	return Channel;
}

void USkillBase::BeginTargeting()
{
	UWorld* World = GetWorld();
	if (bIsTargeting || !World)
	{
		return;
	}
	bIsTargeting = true;
	bHasAimedLocation = false;

	if (TargetIndicatorClass)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		TargetIndicator = World->SpawnActor<ASkillTargetIndicator>(TargetIndicatorClass, ComputeAimLocation(),
			FRotator::ZeroRotator, Params);
		if (TargetIndicator && IndicatorShape == EAimIndicatorShape::Circle)
		{
			TargetIndicator->SetIndicatorRadius(GetModifiedValue(ESkillStat::Radius, Definition.Radius));
		}
	}

	SetAimMovementMode(true);

	UpdateTargeting();
	World->GetTimerManager().SetTimer(TargetingTimer, this, &USkillBase::UpdateTargeting, 1.f / 60.f, true);

	OnTargetingStarted();
}

void USkillBase::UpdateTargeting()
{
	AimedLocation = ComputeAimLocation();
	if (!TargetIndicator)
	{
		return;
	}

	if (IndicatorShape == EAimIndicatorShape::Circle)
	{
		TargetIndicator->SetActorLocation(AimedLocation + FVector(0.f, 0.f, 2.f));
		return;
	}

	// Line: starts at the player's feet and points (flat) toward the aimed point.
	const ACharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		return;
	}
	FVector FlatDirection = AimedLocation - Character->GetActorLocation();
	FlatDirection.Z = 0.f;
	FlatDirection = FlatDirection.GetSafeNormal();
	if (FlatDirection.IsNearlyZero())
	{
		FlatDirection = Character->GetActorForwardVector().GetSafeNormal2D();
	}

	const FVector Feet = Character->GetActorLocation() - FVector(0.f, 0.f, Character->GetSimpleCollisionHalfHeight());
	TargetIndicator->SetActorLocationAndRotation(Feet + FVector(0.f, 0.f, 2.f), FlatDirection.Rotation());
	TargetIndicator->SetIndicatorLine(ComputeLineLength(FlatDirection),
		GetModifiedValue(ESkillStat::Radius, Definition.Radius) * 2.f);
}

void USkillBase::EndTargeting(bool bKeepAimedLocation)
{
	if (!bIsTargeting)
	{
		return;
	}
	if (bKeepAimedLocation)
	{
		UpdateTargeting(); // final position at the moment of release
	}
	bIsTargeting = false;
	bHasAimedLocation = bKeepAimedLocation;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TargetingTimer);
	}
	if (TargetIndicator)
	{
		TargetIndicator->Destroy();
		TargetIndicator = nullptr;
	}

	SetAimMovementMode(false);

	OnTargetingEnded(bKeepAimedLocation);
}

void USkillBase::SetAimMovementMode(bool bEnable)
{
	ACharacter* Character = GetOwningCharacter();
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;

	if (bEnable)
	{
		if (bAimModeApplied || !Character)
		{
			return;
		}
		bAimModeApplied = true;

		// Rooted: ignore WASD. The camera (mouse) still works because only MOVE input is ignored.
		if (bLockMovementWhileAiming)
		{
			if (AController* Controller = Character->GetController())
			{
				Controller->SetIgnoreMoveInput(true);
				LockedController = Controller;
			}
			if (Movement && !Movement->IsFalling())
			{
				Movement->StopMovementImmediately();
			}
		}

		// Face the camera: the standard "aim mode" swap (turn with the controller, not with movement).
		if (bFaceAimWhileAiming && Movement)
		{
			bSavedOrientRotationToMovement = Movement->bOrientRotationToMovement;
			bSavedUseControllerDesiredRotation = Movement->bUseControllerDesiredRotation;
			Movement->bOrientRotationToMovement = false;
			Movement->bUseControllerDesiredRotation = true;
		}
		return;
	}

	if (!bAimModeApplied)
	{
		return;
	}
	bAimModeApplied = false;

	if (LockedController.IsValid())
	{
		LockedController->SetIgnoreMoveInput(false);
	}
	LockedController.Reset();

	if (bFaceAimWhileAiming && Movement)
	{
		Movement->bOrientRotationToMovement = bSavedOrientRotationToMovement;
		Movement->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
	}
}

bool USkillBase::ConsumeAimedLocation(FVector& OutLocation)
{
	if (!bHasAimedLocation)
	{
		return false;
	}
	OutLocation = AimedLocation;
	bHasAimedLocation = false;
	return true;
}
