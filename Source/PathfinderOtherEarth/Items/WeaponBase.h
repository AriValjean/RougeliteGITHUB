// WeaponBase.h
// C++ parent for BP_WeaponBase. Empty for now: it exists so C++ data (FWeaponData::WeaponActorClass)
// and the future C++ weapon manager can refer to "any weapon" by type.
// Shared weapon functions move here when BPC_WeaponManager goes to C++.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponBase.generated.h"

UCLASS(Abstract, Blueprintable)
class PATHFINDEROTHEREARTH_API AWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	AWeaponBase();
};
