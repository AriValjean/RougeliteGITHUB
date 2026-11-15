#include "Currency/CurrencyManagerComponent.h"
#include "Meta/PathfinderMetaSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UCurrencyManagerComponent::UCurrencyManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCurrencyManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	// Forward the subsystem's Gold changes as On Gold Changed, so existing listeners (W_Currency) keep working.
	if (UPathfinderMetaSubsystem* Meta = GetMeta())
	{
		Meta->OnCurrencyChanged.AddUniqueDynamic(this, &UCurrencyManagerComponent::HandleCurrencyChanged);
	}
}

void UCurrencyManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The subsystem outlives this component, so stop listening.
	if (UPathfinderMetaSubsystem* Meta = GetMeta())
	{
		Meta->OnCurrencyChanged.RemoveDynamic(this, &UCurrencyManagerComponent::HandleCurrencyChanged);
	}

	Super::EndPlay(EndPlayReason);
}

UPathfinderMetaSubsystem* UCurrencyManagerComponent::GetMeta() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UPathfinderMetaSubsystem>() : nullptr;
}

void UCurrencyManagerComponent::HandleCurrencyChanged(FName CurrencyId, int32 NewAmount, int32 Delta)
{
	if (CurrencyId == GoldCurrencyId)
	{
		OnGoldChanged.Broadcast(NewAmount);
	}
}

void UCurrencyManagerComponent::AddGold(int32 Amount)
{
	UPathfinderMetaSubsystem* Meta = GetMeta();
	if (!Meta || Amount <= 0)
	{
		return;
	}

	// Respect the cap: only add what still fits.
	const int32 Room = FMath::Max(0, MaxGold - Meta->GetCurrency(GoldCurrencyId));
	const int32 ToAdd = FMath::Min(Amount, Room);
	if (ToAdd > 0)
	{
		Meta->AddCurrency(GoldCurrencyId, ToAdd);
	}
}

void UCurrencyManagerComponent::SubtractGold(int32 Amount)
{
	UPathfinderMetaSubsystem* Meta = GetMeta();
	if (!Meta || Amount <= 0)
	{
		return;
	}
	Meta->SetCurrency(GoldCurrencyId, FMath::Max(0, Meta->GetCurrency(GoldCurrencyId) - Amount));
}

bool UCurrencyManagerComponent::TrySpendGold(int32 Cost)
{
	UPathfinderMetaSubsystem* Meta = GetMeta();
	return Meta && Meta->TrySpendCurrency(GoldCurrencyId, Cost);
}

void UCurrencyManagerComponent::SetGold(int32 NewAmount)
{
	if (UPathfinderMetaSubsystem* Meta = GetMeta())
	{
		Meta->SetCurrency(GoldCurrencyId, FMath::Clamp(NewAmount, 0, MaxGold));
	}
}

int32 UCurrencyManagerComponent::GetGoldAmount() const
{
	const UPathfinderMetaSubsystem* Meta = GetMeta();
	return Meta ? Meta->GetCurrency(GoldCurrencyId) : 0;
}

bool UCurrencyManagerComponent::HasEnoughGold(int32 Cost) const
{
	return Cost >= 0 && Cost <= GetGoldAmount();
}

UCurrencyManagerComponent* UCurrencyManagerComponent::FindCurrencyManager(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UCurrencyManagerComponent>() : nullptr;
}
