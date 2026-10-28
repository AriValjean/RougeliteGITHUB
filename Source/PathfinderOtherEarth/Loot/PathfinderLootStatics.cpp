// PathfinderLootStatics.cpp
// Drop into: Source/PathfinderOtherEarth/Loot/PathfinderLootStatics.cpp

#include "PathfinderLootStatics.h"
#include "Accessory/AccessoryTypes.h"
#include "Engine/DataTable.h"
int32 UPathfinderLootStatics::WeightedPick(const TArray<float>& Weights)
{
	float Total = 0.f;
	for (const float Weight : Weights)
	{
		Total += FMath::Max(Weight, 0.f);
	}

	if (Total <= 0.f)
	{
		return -1;
	}

	const float Roll = FMath::FRandRange(0.f, Total);
	float Running = 0.f;

	for (int32 Index = 0; Index < Weights.Num(); ++Index)
	{
		Running += FMath::Max(Weights[Index], 0.f);
		if (Roll < Running)
		{
			return Index;
		}
	}

	// Floating point edge case (Roll landed exactly on Total) -- return the last valid entry.
	return Weights.Num() - 1;
}

EItemRarity UPathfinderLootStatics::RollRarity(EChestType ChestType, int32 Stage, const UDataTable* RarityWeightTable)
{
	if (!RarityWeightTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("RollRarity: no RarityWeightTable supplied, defaulting to Common."));
		return EItemRarity::Common;
	}

	const UEnum* ChestTypeEnum = StaticEnum<EChestType>();
	const FName RowName = FName(*ChestTypeEnum->GetNameStringByValue(static_cast<int64>(ChestType)));

	const FRarityWeights* Row = RarityWeightTable->FindRow<FRarityWeights>(RowName, TEXT("RollRarity"), /*bWarnIfRowMissing=*/true);
	if (!Row)
	{
		UE_LOG(LogTemp, Warning, TEXT("RollRarity: row '%s' not found in '%s', defaulting to Common."),
			*RowName.ToString(), *RarityWeightTable->GetName());
		return EItemRarity::Common;
	}

	const TArray<float> Weights = { Row->Common, Row->Uncommon, Row->Rare, Row->Unique, Row->Legendary };
	const int32 Index = WeightedPick(Weights);

	switch (Index)
	{
	case 0: return EItemRarity::Common;
	case 1: return EItemRarity::Uncommon;
	case 2: return EItemRarity::Rare;
	case 3: return EItemRarity::Unique;
	case 4: return EItemRarity::Legendary;
	default:
		UE_LOG(LogTemp, Warning, TEXT("RollRarity: WeightedPick returned -1 for row '%s' (all weights <= 0), defaulting to Common."), *RowName.ToString());
		return EItemRarity::Common;
	}
	
}

FName UPathfinderLootStatics::RollAccessory(EItemRarity Rarity, const UDataTable* AccessoryTable)
{
	if (!AccessoryTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("RollAccessory: no AccessoryTable given."));
		return NAME_None;
	}
 
	// Start at the rolled tier and step down toward Common until a tier has something to pick.
	for (int32 Tier = static_cast<int32>(Rarity); Tier >= static_cast<int32>(EItemRarity::Common); --Tier)
	{
		TArray<FName> Candidates;
		TArray<float> Weights;
 
		AccessoryTable->ForeachRow<FAccessoryData>(TEXT("RollAccessory"),
			[&](const FName& RowName, const FAccessoryData& Row)
			{
				if (static_cast<int32>(Row.Rarity) == Tier && Row.DropWeight > 0.f)
				{
					Candidates.Add(RowName);
					Weights.Add(Row.DropWeight);
				}
			});
 
		const int32 Index = WeightedPick(Weights);
		if (Candidates.IsValidIndex(Index))
		{
			return Candidates[Index];
		}
	}
 
	UE_LOG(LogTemp, Warning, TEXT("RollAccessory: no accessory found at or below the rolled rarity."));
	return NAME_None;
}
