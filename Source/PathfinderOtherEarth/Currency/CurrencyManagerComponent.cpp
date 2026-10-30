#include "Currency/CurrencyManagerComponent.h"
#include "GameFramework/Actor.h"

UCurrencyManagerComponent::UCurrencyManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCurrencyManagerComponent::AddGold(int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}
	ApplyGold(static_cast<int64>(CurrentGold) + Amount);
}

void UCurrencyManagerComponent::SubtractGold(int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}
	ApplyGold(static_cast<int64>(CurrentGold) - Amount);
}

bool UCurrencyManagerComponent::TrySpendGold(int32 Cost)
{
	if (Cost < 0 || !HasEnoughGold(Cost))
	{
		return false;
	}
	ApplyGold(static_cast<int64>(CurrentGold) - Cost);
	return true;
}

void UCurrencyManagerComponent::SetGold(int32 NewAmount)
{
	ApplyGold(NewAmount);
}

UCurrencyManagerComponent* UCurrencyManagerComponent::FindCurrencyManager(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UCurrencyManagerComponent>() : nullptr;
}

void UCurrencyManagerComponent::ApplyGold(int64 NewAmount)
{
	const int32 Clamped = static_cast<int32>(FMath::Clamp<int64>(NewAmount, 0, MaxGold));
	if (Clamped == CurrentGold)
	{
		return;
	}
	CurrentGold = Clamped;
	OnGoldChanged.Broadcast(CurrentGold);
}
