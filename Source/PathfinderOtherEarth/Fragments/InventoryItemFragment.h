// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "InventoryItemFragment.generated.h"

/**
 * 
 */
// default to instance - instances are duplicated upon construction 
UCLASS(Blueprintable,BlueprintType,Abstract,DefaultToInstanced,EditInlineNew)
class PATHFINDEROTHEREARTH_API UInventoryItemFragment : public UObject
{
	GENERATED_BODY()
};
