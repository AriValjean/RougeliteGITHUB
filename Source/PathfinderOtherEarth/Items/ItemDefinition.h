// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ItemDefinition.generated.h"

class UInventoryItemFragment;
UENUM(BlueprintType)

enum class EItemRarity : uint8
{
	None,
	Common,
	Uncommon,
	Rare,
	Unique,
	Legendary
	
};
/**
 * 
 */
UCLASS(Blueprintable,BlueprintType, Abstract,Const)
class PATHFINDEROTHEREARTH_API UItemDefinition : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly, Category = "Display")
	FText ItemName;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly, Category = "Display")
	FText ItemDescription;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly, Category = "Display")
	TObjectPtr<UTexture2D> ItemIcon;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly, Category = "Display")
	EItemRarity Rarity = EItemRarity::None;
	

	
	

//item defintion holds array of fragments
	//instanced here makes sure each asset has a unqique/instanced static mesh assocaited
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Instanced, Category = "Fragments Array")

	TArray<TObjectPtr<UInventoryItemFragment>> Fragments;
	
	//function takes input item defintion class and fragment class world represnrtion
	//determinesoutput type 
UFUNCTION(BlueprintCallable,BlueprintPure, meta=(DeterminesOutputType="FragmentClass"))
	static const UInventoryItemFragment* FindFragmentByClass( const TSubclassOf<UItemDefinition> ItemDefinition, const
		TSubclassOf<UInventoryItemFragment> FragmentClass);
	
};
