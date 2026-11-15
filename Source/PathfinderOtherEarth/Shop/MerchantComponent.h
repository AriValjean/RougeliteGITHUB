
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Accessory/AccessoryTypes.h"
#include "MerchantComponent.generated.h"

class UDataTable;

/** One card in the shop. */
USTRUCT(BlueprintType)
struct PATHFINDEROTHEREARTH_API FShopCard
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	EShopCategory Category = EShopCategory::None;

	/** Row name in DT_AccessoriesCPP. None = nothing available for this card (show it as empty). */
	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	FName AccessoryId;

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	EItemRarity Rarity = EItemRarity::Common;

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	int32 Price = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	bool bSold = false;

	/** Each card can be rerolled once per shop. */
	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	bool bRerolled = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnShopChanged);

UCLASS(Blueprintable, ClassGroup = (Pathfinder), meta = (BlueprintSpawnableComponent))
class PATHFINDEROTHEREARTH_API UMerchantComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMerchantComponent();

	/** Finds the merchant in the current level (one per shop room). Use from the shop widget. */
	UFUNCTION(BlueprintPure, Category = "Shop", meta = (WorldContext = "WorldContextObject"))
	static UMerchantComponent* FindMerchant(const UObject* WorldContextObject);

	/** The cards for this shop. Rolls the stock the first time it's called. */
	UFUNCTION(BlueprintCallable, Category = "Shop")
	const TArray<FShopCard>& GetCards();

	/**
	 * Cost of the next reroll (shared by all cards). 0 while the player still has free rerolls
	 * from the Enchantress; after that it starts at the base cost and goes up with each paid reroll.
	 */
	UFUNCTION(BlueprintPure, Category = "Shop")
	int32 GetRerollCost() const;

	/** Free rerolls left in this shop (Enchantress "Free Shop Rerolls" upgrade minus rerolls already used here). */
	UFUNCTION(BlueprintPure, Category = "Shop")
	int32 GetFreeRerollsRemaining() const;

	/** True if the card has an item, isn't sold and hasn't been rerolled yet. (Doesn't check gold.) */
	UFUNCTION(BlueprintPure, Category = "Shop")
	bool CanReroll(int32 CardIndex) const;

	/** True if Buyer has at least Cost gold. Use to tint prices red and disable buttons. */
	UFUNCTION(BlueprintPure, Category = "Shop")
	static bool CanAfford(int32 Cost, const AActor* Buyer);

	/** Buys a card: spends gold and gives the accessory. False if sold, empty, maxed or not affordable. */
	UFUNCTION(BlueprintCallable, Category = "Shop")
	bool TryPurchase(int32 CardIndex, AActor* Buyer);

	/** Rerolls a card into a different item of the same category (rarity rolled again). False if not allowed or not affordable. */
	UFUNCTION(BlueprintCallable, Category = "Shop")
	bool TryReroll(int32 CardIndex, AActor* Buyer);

	/** Fires after the stock is rolled and after every purchase or reroll. The shop widget refreshes on this. */
	UPROPERTY(BlueprintAssignable, Category = "Shop")
	FOnShopChanged OnShopChanged;

protected:
	virtual void BeginPlay() override;

	/** Row struct FAccessoryData (DT_AccessoriesCPP). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop|Tables")
	TObjectPtr<UDataTable> AccessoryTable;

	/** Row struct FRarityWeights (DT_RarityWeightCPP). Needs rows named Rarity Row Prefix + stage number. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop|Tables")
	TObjectPtr<UDataTable> RarityWeightTable;

	/** Rows are looked up as e.g. "Shop_Stage1", "Shop_Stage2". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop|Tables")
	FString RarityRowPrefix = TEXT("Shop_Stage");

	/** Price at stage 1 for each rarity, before stage scaling. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop|Prices")
	TMap<EItemRarity, int32> BasePrices;

	/** First paid reroll in a shop at stage 1, before stage scaling. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop|Prices", meta = (ClampMin = "0"))
	int32 RerollBaseCost = 50;

	/** Each paid reroll in the same shop multiplies the next reroll's cost by this. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop|Prices", meta = (ClampMin = "1.0"))
	float RerollCostMultiplier = 1.5f;

	/** Overall stage at which the 4th (Ability) card appears. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop", meta = (ClampMin = "1"))
	int32 AbilityCardFromStage = 3;

private:
	void EnsureStock();
	void RollStock();

	/** Picks a shop-pool accessory of Category, starting at StartRarity and stepping down. */
	bool PickAccessory(EShopCategory Category, EItemRarity StartRarity, const TSet<FName>& Exclude,
		FName& OutId, EItemRarity& OutRarity);

	EItemRarity RollRarity();
	bool HasShopItems(EShopCategory Category) const;
	bool IsMaxedForPlayer(FName AccessoryId, const FAccessoryData& Row) const;

	int32 GetStage() const;
	float GetCostScale() const;
	int32 GetPrice(EItemRarity Rarity) const;

	/** Total free rerolls per shop from the Enchantress upgrade (0 if not bought). */
	int32 GetFreeRerollsPerShop() const;

	FRandomStream& Rng();

	UPROPERTY(VisibleInstanceOnly, Category = "Shop|Runtime")
	TArray<FShopCard> Cards;

	/** All rerolls used in this shop, free and paid. */
	UPROPERTY(VisibleInstanceOnly, Category = "Shop|Runtime")
	int32 RerollsUsed = 0;

	bool bStockRolled = false;

	/** Used only if there's no Room Flow subsystem (shouldn't happen in game). */
	FRandomStream FallbackStream;
};
