// SkillComponent.cpp
// Drop into: Source/PathfinderOtherEarth/Skills/SkillComponent.cpp

#include "Skills/SkillComponent.h"
#include "Skills/SkillBase.h"
#include "Items/WeaponManagerComponent.h"
#include "Stats/StatsManagerComponent.h"
#include "Accessory/AccessoryManagerComponent.h"
#include "Combat/HealthComponent.h"
#include "Engine/DataTable.h"
#include "TimerManager.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Dashing, "State.Dashing", "Character is in the middle of a dash.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Invincible, "State.Invincible", "Character ignores incoming damage.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Skill_CanCrit, "Skill.CanCrit", "Abilities can crit (granted by an accessory). Without it, abilities never crit.");

namespace
{
	template <typename TEnum>
	FName EnumToRowName(TEnum Value)
	{
		// Internal enum name, e.g. ECharacterClass::Rogue -> "Rogue", ESubclass::DarkKnight -> "DarkKnight".
		return FName(*StaticEnum<TEnum>()->GetNameStringByValue(static_cast<int64>(Value)));
	}
}

USkillComponent::USkillComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USkillComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		WeaponManager = Owner->FindComponentByClass<UWeaponManagerComponent>();
		StatsManager = Owner->FindComponentByClass<UStatsManagerComponent>();
		AccessoryManager = Owner->FindComponentByClass<UAccessoryManagerComponent>();
		OwnerHealth = Owner->FindComponentByClass<UHealthComponent>();
	}
	if (OwnerHealth.IsValid())
	{
		OwnerHealth->OnDeath.AddDynamic(this, &USkillComponent::HandleOwnerDeath);
	}
	if (WeaponManager.IsValid())
	{
		WeaponManager->OnClassChanged.AddDynamic(this, &USkillComponent::HandleClassChanged);
	}
	if (AccessoryManager.IsValid())
	{
		AccessoryManager->OnAccessoryChanged.AddDynamic(this, &USkillComponent::HandleAccessoryChanged);
	}

	// Wait one frame so the weapon/accessory managers have restored their saved state first.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &USkillComponent::InitializeAfterLoad));
	}
}

void USkillComponent::InitializeAfterLoad()
{
	RecalculateAccessorySkillModifiers();
	RebuildKit();
}

void USkillComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TapWindowTimer);
	}
	RemoveAllSkills();
	if (WeaponManager.IsValid())
	{
		WeaponManager->OnClassChanged.RemoveDynamic(this, &USkillComponent::HandleClassChanged);
	}
	if (AccessoryManager.IsValid())
	{
		AccessoryManager->OnAccessoryChanged.RemoveDynamic(this, &USkillComponent::HandleAccessoryChanged);
	}
	if (OwnerHealth.IsValid())
	{
		OwnerHealth->OnDeath.RemoveDynamic(this, &USkillComponent::HandleOwnerDeath);
	}
	Super::EndPlay(EndPlayReason);
}

// ----------------------------------------------------------------------------
// Using skills
// ----------------------------------------------------------------------------

bool USkillComponent::TryActivateSkill(ESkillSlot Slot)
{
	// Using a different skill (e.g. dash) while aiming cancels the aim.
	if (bIsTargetingSkill && Slot != TargetingSlot)
	{
		CancelTargeting();
	}

	FEquippedSkill* Equipped = EquippedSkills.Find(Slot);
	UWorld* World = GetWorld();
	if (!Equipped || !Equipped->Skill || !World)
	{
		return false;
	}
	if (IsOnCooldown(Slot) || Equipped->Skill->IsActive() || !Equipped->Skill->CanActivate())
	{
		return false;
	}

	// Keep a local pointer: the skill may change the kit while running.
	USkillBase* Skill = Equipped->Skill;

	// Normal skills start the cooldown on cast; lasting ones (summons) when they end.
	if (!Skill->StartsCooldownOnEnd())
	{
		StartCooldown(Slot);
	}

	Skill->MarkActive();
	OnSkillActivated.Broadcast(Slot);

	Skill->ActivateSkill();
	return true;
}

float USkillComponent::ComputeCooldown(ESkillSlot Slot, const FEquippedSkill& Equipped) const
{
	// 1) accessory / extra skill modifiers, 2) Cooldown Reduction stat (never on Dash).
	float Cooldown = GetModifiedSkillValue(Slot, ESkillStat::Cooldown, Equipped.Definition.Cooldown);
	if (Slot != ESkillSlot::Dash)
	{
		Cooldown *= 1.f - FMath::Clamp(GetCooldownReductionPercent(), 0.f, 0.9f);
	}
	return FMath::Max(0.f, Cooldown);
}

void USkillComponent::StartCooldown(ESkillSlot Slot)
{
	FEquippedSkill* Equipped = EquippedSkills.Find(Slot);
	const UWorld* World = GetWorld();
	if (!Equipped || !World)
	{
		return;
	}

	const float Cooldown = ComputeCooldown(Slot, *Equipped);
	Equipped->LastCooldownDuration = Cooldown;
	Equipped->ReadyAtTime = World->GetTimeSeconds() + Cooldown;
	if (Cooldown > 0.f)
	{
		OnCooldownStarted.Broadcast(Slot, Cooldown);
	}
}

void USkillComponent::NotifySkillEnded(USkillBase* Skill)
{
	if (!Skill || !Skill->StartsCooldownOnEnd())
	{
		return;
	}
	for (const TPair<ESkillSlot, FEquippedSkill>& Pair : EquippedSkills)
	{
		if (Pair.Value.Skill == Skill)
		{
			StartCooldown(Pair.Key);
			return;
		}
	}
}

void USkillComponent::BeginSkillInput(ESkillSlot Slot)
{
	FEquippedSkill* Equipped = EquippedSkills.Find(Slot);
	UWorld* World = GetWorld();
	if (!Equipped || !Equipped->Skill || !World)
	{
		return;
	}

	// Normal skills activate on press.
	if (!Equipped->Definition.bHoldToAim)
	{
		TryActivateSkill(Slot);
		return;
	}

	// Hold To Aim: only start aiming if it could actually be cast.
	if (IsOnCooldown(Slot) || Equipped->Skill->IsActive() || !Equipped->Skill->CanActivate())
	{
		return;
	}
	if (bIsTargetingSkill)
	{
		CancelTargeting();
	}

	bIsTargetingSkill = true;
	TargetingSlot = Slot;

	// Tap-or-hold skills wait out the tap window before showing the indicator, so taps never flash it.
	const float TapThreshold = Equipped->Skill->GetTapThreshold();
	if (TapThreshold > 0.f)
	{
		World->GetTimerManager().SetTimer(TapWindowTimer, this, &USkillComponent::StartHeldTargeting, TapThreshold, false);
	}
	else
	{
		Equipped->Skill->BeginTargeting();
	}
}

void USkillComponent::StartHeldTargeting()
{
	if (!bIsTargetingSkill)
	{
		return;
	}
	if (USkillBase* Skill = GetSkill(TargetingSlot))
	{
		Skill->BeginTargeting();
	}
}

void USkillComponent::ReleaseSkillInput(ESkillSlot Slot)
{
	if (!bIsTargetingSkill || Slot != TargetingSlot)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TapWindowTimer);
	}
	bIsTargetingSkill = false;

	USkillBase* Skill = GetSkill(Slot);
	if (!Skill)
	{
		return;
	}

	// Released inside the tap window: quick cast (the skill gets no aimed location).
	if (!Skill->IsTargeting())
	{
		TryActivateSkill(Slot);
		return;
	}

	Skill->EndTargeting(/*bKeepAimedLocation*/ true);
	if (!TryActivateSkill(Slot))
	{
		// Couldn't cast after all: throw the aimed spot away.
		FVector Unused;
		Skill->ConsumeAimedLocation(Unused);
	}
}

void USkillComponent::CancelTargeting()
{
	if (!bIsTargetingSkill)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TapWindowTimer);
	}
	bIsTargetingSkill = false;
	if (USkillBase* Skill = GetSkill(TargetingSlot))
	{
		Skill->EndTargeting(/*bKeepAimedLocation*/ false);
	}
}

void USkillComponent::HandleOwnerDeath(AActor* Killer)
{
	CancelTargeting();

	// Ends lasting skills too (e.g. the Arcane Familiar disappears).
	EndAllSkills();
}

float USkillComponent::GetCooldownReductionPercent() const
{
	return StatsManager.IsValid() ? StatsManager->GetTotal(ECharacterStatType::CooldownReduction) : 0.f;
}

// ----------------------------------------------------------------------------
// Class / subclass
// ----------------------------------------------------------------------------

void USkillComponent::SetSubclass(ESubclass NewSubclass)
{
	if (CurrentSubclass == NewSubclass)
	{
		return;
	}
	CurrentSubclass = NewSubclass;
	OnSubclassChanged.Broadcast(CurrentSubclass);
	RebuildKit();
}

ECharacterClass USkillComponent::GetCurrentClass() const
{
	return WeaponManager.IsValid() ? WeaponManager->GetCurrentClass() : ECharacterClass::Rogue;
}

void USkillComponent::HandleClassChanged(ECharacterClass NewClass)
{
	// A new class starts without a subclass.
	CurrentSubclass = ESubclass::None;
	OnSubclassChanged.Broadcast(CurrentSubclass);
	RebuildKit();
}

void USkillComponent::RebuildKit()
{
	RemoveAllSkills();

	const FClassKit* BaseKit = FindKitRow(EnumToRowName(GetCurrentClass()));
	const FClassKit* SubKit = (CurrentSubclass != ESubclass::None) ? FindKitRow(EnumToRowName(CurrentSubclass)) : nullptr;

	// Subclass row wins for any slot where it names a USABLE skill (row exists in DT_SkillsCPP and has a
	// Skill Class). Empty or not-built-yet subclass skills fall back to the base class's skill.
	auto Pick = [this, BaseKit, SubKit](FName FClassKit::* Member) -> FName
	{
		if (SubKit && IsUsableSkillRow(SubKit->*Member))
		{
			return SubKit->*Member;
		}
		return BaseKit ? BaseKit->*Member : NAME_None;
	};

	BuildSlot(ESkillSlot::Dash, Pick(&FClassKit::DashSkill));
	BuildSlot(ESkillSlot::Basic, Pick(&FClassKit::BasicSkill));
	BuildSlot(ESkillSlot::Ultimate, Pick(&FClassKit::UltimateSkill));

	OnKitChanged.Broadcast();
}

void USkillComponent::BuildSlot(ESkillSlot Slot, FName SkillRow)
{
	if (SkillRow.IsNone() || !SkillTable)
	{
		return;
	}

	const FSkillDefinition* Definition = SkillTable->FindRow<FSkillDefinition>(SkillRow, TEXT("SkillComponent::BuildSlot"));
	if (!Definition)
	{
		UE_LOG(LogTemp, Warning, TEXT("SkillComponent: skill row '%s' not found in %s"), *SkillRow.ToString(), *GetNameSafe(SkillTable));
		return;
	}

	FEquippedSkill NewSkill;
	NewSkill.SkillRow = SkillRow;
	NewSkill.Definition = *Definition;

	UClass* SkillClass = Definition->SkillClass.Get();
	if (!SkillClass || SkillClass->HasAnyClassFlags(CLASS_Abstract))
	{
		// Placeholder row (not built yet): leave the slot empty so the skill bar hides it.
		UE_LOG(LogTemp, Log, TEXT("SkillComponent: skill '%s' has no Skill Class yet, slot left empty."), *SkillRow.ToString());
		return;
	}

	NewSkill.Skill = NewObject<USkillBase>(this, SkillClass);
	NewSkill.Skill->InitSkill(this, SkillRow, Slot, *Definition);
	EquippedSkills.Add(Slot, NewSkill);
}

bool USkillComponent::IsUsableSkillRow(FName SkillRow) const
{
	if (SkillRow.IsNone() || !SkillTable)
	{
		return false;
	}
	const FSkillDefinition* Definition = SkillTable->FindRow<FSkillDefinition>(SkillRow, TEXT("SkillComponent::IsUsableSkillRow"), false);
	const UClass* SkillClass = Definition ? Definition->SkillClass.Get() : nullptr;
	return SkillClass && !SkillClass->HasAnyClassFlags(CLASS_Abstract);
}

const FClassKit* USkillComponent::FindKitRow(FName RowName) const
{
	if (!ClassKitTable || RowName.IsNone())
	{
		return nullptr;
	}
	return ClassKitTable->FindRow<FClassKit>(RowName, TEXT("SkillComponent::FindKitRow"), /*bWarnIfRowMissing*/ false);
}

void USkillComponent::RemoveAllSkills()
{
	CancelTargeting();

	// Ends running skills and lets them clean up lasting effects (e.g. a mark on an enemy).
	for (TPair<ESkillSlot, FEquippedSkill>& Pair : EquippedSkills)
	{
		if (Pair.Value.Skill)
		{
			Pair.Value.Skill->NotifyRemoved();
		}
	}
	EquippedSkills.Empty();
}

void USkillComponent::EndAllSkills()
{
	for (TPair<ESkillSlot, FEquippedSkill>& Pair : EquippedSkills)
	{
		if (Pair.Value.Skill && Pair.Value.Skill->IsActive())
		{
			Pair.Value.Skill->EndSkill();
		}
	}
}

// ----------------------------------------------------------------------------
// Queries
// ----------------------------------------------------------------------------

bool USkillComponent::HasSkill(ESkillSlot Slot) const
{
	const FEquippedSkill* Equipped = EquippedSkills.Find(Slot);
	return Equipped && Equipped->Skill;
}

USkillBase* USkillComponent::GetSkill(ESkillSlot Slot) const
{
	const FEquippedSkill* Equipped = EquippedSkills.Find(Slot);
	return Equipped ? Equipped->Skill.Get() : nullptr;
}

bool USkillComponent::GetSkillDefinition(ESkillSlot Slot, FSkillDefinition& OutDefinition) const
{
	if (const FEquippedSkill* Equipped = EquippedSkills.Find(Slot))
	{
		OutDefinition = Equipped->Definition;
		return true;
	}
	return false;
}

float USkillComponent::GetCooldownRemaining(ESkillSlot Slot) const
{
	const FEquippedSkill* Equipped = EquippedSkills.Find(Slot);
	const UWorld* World = GetWorld();
	if (!Equipped || !World)
	{
		return 0.f;
	}
	return FMath::Max(0.f, static_cast<float>(Equipped->ReadyAtTime - World->GetTimeSeconds()));
}

void USkillComponent::ResetCooldown(ESkillSlot Slot)
{
	// Get Cooldown Remaining returns 0 from now on.
	if (FEquippedSkill* Equipped = EquippedSkills.Find(Slot))
	{
		Equipped->ReadyAtTime = 0.0;
	}
}

float USkillComponent::GetCooldownDuration(ESkillSlot Slot) const
{
	const FEquippedSkill* Equipped = EquippedSkills.Find(Slot);
	return Equipped ? Equipped->LastCooldownDuration : 0.f;
}

bool USkillComponent::IsAnySkillActive() const
{
	for (const TPair<ESkillSlot, FEquippedSkill>& Pair : EquippedSkills)
	{
		if (Pair.Value.Skill && Pair.Value.Skill->IsActive())
		{
			return true;
		}
	}
	return false;
}

// ----------------------------------------------------------------------------
// Skill modifiers
// ----------------------------------------------------------------------------

int32 USkillComponent::MakeModKey(ESkillSlot Slot, ESkillStat Stat)
{
	return (static_cast<int32>(Slot) << 8) | static_cast<int32>(Stat);
}

void USkillComponent::AddToBucket(TMap<int32, FSkillModTotals>& Bucket, ESkillSlot Slot, ESkillStat Stat, EModifierMode Mode, float Amount)
{
	FSkillModTotals& Totals = Bucket.FindOrAdd(MakeModKey(Slot, Stat));
	if (Mode == EModifierMode::Flat)
	{
		Totals.Flat += Amount;
	}
	else
	{
		Totals.Percent += Amount;
	}
}

float USkillComponent::GetModifiedSkillValue(ESkillSlot Slot, ESkillStat Stat, float BaseValue) const
{
	const int32 Key = MakeModKey(Slot, Stat);
	float Flat = 0.f;
	float Percent = 0.f;

	if (const FSkillModTotals* FromAccessories = AccessoryModifiers.Find(Key))
	{
		Flat += FromAccessories->Flat;
		Percent += FromAccessories->Percent;
	}
	if (const FSkillModTotals* FromExtra = ExtraModifiers.Find(Key))
	{
		Flat += FromExtra->Flat;
		Percent += FromExtra->Percent;
	}

	return (BaseValue + Flat) * (1.f + Percent);
}

void USkillComponent::AddSkillModifier(ESkillSlot Slot, ESkillStat Stat, EModifierMode Mode, float Amount)
{
	AddToBucket(ExtraModifiers, Slot, Stat, Mode, Amount);
	OnSkillModifiersChanged.Broadcast();
}

void USkillComponent::ClearExtraSkillModifiers()
{
	ExtraModifiers.Reset();
	OnSkillModifiersChanged.Broadcast();
}

void USkillComponent::HandleAccessoryChanged(FName AccessoryID, int32 NewCount)
{
	RecalculateAccessorySkillModifiers();
}

void USkillComponent::RecalculateAccessorySkillModifiers()
{
	// Rebuilt from scratch every time, so stacks and level loads can never double-count.
	AccessoryModifiers.Reset();

	if (AccessoryManager.IsValid())
	{
		for (const TPair<FName, int32>& Held : AccessoryManager->GetAccessoryCounts())
		{
			if (Held.Value <= 0)
			{
				continue;
			}

			FAccessoryData Data;
			if (!AccessoryManager->GetAccessoryData(Held.Key, Data))
			{
				continue;
			}

			for (const FSkillModifier& Modifier : Data.SkillModifiers)
			{
				// First copy = BaseValue, each extra copy = PerStackValue (same rule as stat modifiers).
				const float Amount = Modifier.BaseValue + Modifier.PerStackValue * (Held.Value - 1);
				AddToBucket(AccessoryModifiers, Modifier.Slot, Modifier.Stat, Modifier.Mode, Amount);
			}
		}
	}

	OnSkillModifiersChanged.Broadcast();
}

// ----------------------------------------------------------------------------
// State tags
// ----------------------------------------------------------------------------

void USkillComponent::AddStateTag(FGameplayTag Tag)
{
	if (Tag.IsValid())
	{
		StateTags.AddTag(Tag);
	}
}

void USkillComponent::RemoveStateTag(FGameplayTag Tag)
{
	StateTags.RemoveTag(Tag);
}

bool USkillComponent::HasStateTag(FGameplayTag Tag) const
{
	return StateTags.HasTag(Tag);
}

bool USkillComponent::IsInvincible() const
{
	return StateTags.HasTag(TAG_State_Invincible);
}
