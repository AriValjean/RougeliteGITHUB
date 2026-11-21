// SkillComponent.cpp
// Drop into: Source/PathfinderOtherEarth/Skills/SkillComponent.cpp

#include "Skills/SkillComponent.h"
#include "Skills/SkillBase.h"
#include "Items/WeaponManagerComponent.h"
#include "Stats/StatsManagerComponent.h"
#include "Accessory/AccessoryManagerComponent.h"
#include "Engine/DataTable.h"
#include "TimerManager.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Dashing, "State.Dashing", "Character is in the middle of a dash.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Invincible, "State.Invincible", "Character ignores incoming damage.");

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
	EndAllSkills();
	if (WeaponManager.IsValid())
	{
		WeaponManager->OnClassChanged.RemoveDynamic(this, &USkillComponent::HandleClassChanged);
	}
	if (AccessoryManager.IsValid())
	{
		AccessoryManager->OnAccessoryChanged.RemoveDynamic(this, &USkillComponent::HandleAccessoryChanged);
	}
	Super::EndPlay(EndPlayReason);
}

// ----------------------------------------------------------------------------
// Using skills
// ----------------------------------------------------------------------------

bool USkillComponent::TryActivateSkill(ESkillSlot Slot)
{
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

	// 1) accessory / extra skill modifiers, 2) Cooldown Reduction stat (never on Dash).
	float Cooldown = GetModifiedSkillValue(Slot, ESkillStat::Cooldown, Equipped->Definition.Cooldown);
	if (Slot != ESkillSlot::Dash)
	{
		Cooldown *= 1.f - FMath::Clamp(GetCooldownReductionPercent(), 0.f, 0.9f);
	}
	Cooldown = FMath::Max(0.f, Cooldown);

	Equipped->LastCooldownDuration = Cooldown;
	Equipped->ReadyAtTime = World->GetTimeSeconds() + Cooldown;

	// Keep a local pointer: the skill may change the kit while running.
	USkillBase* Skill = Equipped->Skill;
	Skill->MarkActive();

	OnSkillActivated.Broadcast(Slot);
	if (Cooldown > 0.f)
	{
		OnCooldownStarted.Broadcast(Slot, Cooldown);
	}

	Skill->ActivateSkill();
	return true;
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
	EndAllSkills();
	EquippedSkills.Empty();

	const FClassKit* BaseKit = FindKitRow(EnumToRowName(GetCurrentClass()));
	const FClassKit* SubKit = (CurrentSubclass != ESubclass::None) ? FindKitRow(EnumToRowName(CurrentSubclass)) : nullptr;

	// Subclass row wins for any slot it fills in; empty slots keep the base class's skill.
	auto Pick = [BaseKit, SubKit](FName FClassKit::* Member) -> FName
	{
		if (SubKit && !(SubKit->*Member).IsNone())
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
	if (SkillClass && !SkillClass->HasAnyClassFlags(CLASS_Abstract))
	{
		NewSkill.Skill = NewObject<USkillBase>(this, SkillClass);
		NewSkill.Skill->InitSkill(this, SkillRow, Slot, *Definition);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SkillComponent: skill '%s' has no usable Skill Class (empty or abstract)"), *SkillRow.ToString());
	}

	EquippedSkills.Add(Slot, NewSkill);
}

const FClassKit* USkillComponent::FindKitRow(FName RowName) const
{
	if (!ClassKitTable || RowName.IsNone())
	{
		return nullptr;
	}
	return ClassKitTable->FindRow<FClassKit>(RowName, TEXT("SkillComponent::FindKitRow"), /*bWarnIfRowMissing*/ false);
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
