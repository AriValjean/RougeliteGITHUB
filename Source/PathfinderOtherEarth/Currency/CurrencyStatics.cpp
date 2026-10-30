#include "Currency/CurrencyStatics.h"
#include "Currency/CurrencyTypes.h"
#include "Currency/PathfinderEconomySettings.h"

float UCurrencyStatics::GetStageScale(int32 Stage)
{
	const UPathfinderEconomySettings* Settings = GetDefault<UPathfinderEconomySettings>();
	const int32 StagesCompleted = FMath::Max(Stage - 1, 0);
	return FMath::Pow(Settings->GrowthPerStage, static_cast<float>(StagesCompleted));
}

int32 UCurrencyStatics::RollGoldDrop(const FDataTableRowHandle& RewardRow, int32 Stage)
{
	const FEnemyRewardRow* Row = RewardRow.GetRow<FEnemyRewardRow>(TEXT("RollGoldDrop"));
	if (!Row)
	{
		return 0;
	}

	if (FMath::FRand() >= Row->GoldDropChance)
	{
		return 0;
	}

	const int32 Low = FMath::Min(Row->MinGold, Row->MaxGold);
	const int32 High = FMath::Max(Row->MinGold, Row->MaxGold);
	const int32 BaseAmount = FMath::RandRange(Low, High);

	return FMath::Max(0, FMath::RoundToInt(BaseAmount * GetStageScale(Stage)));
}

int32 UCurrencyStatics::GetScaledCost(int32 BaseCost, int32 Stage)
{
	const UPathfinderEconomySettings* Settings = GetDefault<UPathfinderEconomySettings>();
	const float CostScale = FMath::Pow(GetStageScale(Stage), Settings->CostExponent);
	return FMath::Max(0, FMath::RoundToInt(BaseCost * CostScale));
}
