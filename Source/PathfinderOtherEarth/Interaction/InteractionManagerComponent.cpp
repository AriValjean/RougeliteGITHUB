// InteractionManagerComponent.cpp

#include "InteractionManagerComponent.h"
#include "InteractableInterface.h"
#include "InteractionPromptWidget.h"

#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"

UInteractionManagerComponent::UInteractionManagerComponent()
{
	// Tick only runs while a hold is in progress (turned on in BeginInteract).
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

// ---------------------------------------------------------------- Setup

void UInteractionManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (const ACharacter* Character = Cast<ACharacter>(Owner))
	{
		DetectionComponent = Character->GetCapsuleComponent();
	}
	else if (Owner)
	{
		DetectionComponent = Cast<UPrimitiveComponent>(Owner->GetRootComponent());
	}

	if (!DetectionComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("InteractionManager on %s: no capsule/primitive root to detect overlaps."), *GetNameSafe(Owner));
		return;
	}

	DetectionComponent->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleBeginOverlap);
	DetectionComponent->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleEndOverlap);

	CreatePromptWidget();

	// Components run BeginPlay BEFORE their owner's Blueprint BeginPlay, which is usually where the
	// Input Mapping Context gets added. Wait one tick so the prompt can read the bound key.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::ScanInitialOverlaps);
	}
}

void UInteractionManagerComponent::ScanInitialOverlaps()
{
	if (!DetectionComponent)
	{
		return;
	}

	// Pick up anything we spawned on top of (no begin-overlap event fires for those).
	TArray<AActor*> AlreadyOverlapping;
	DetectionComponent->GetOverlappingActors(AlreadyOverlapping);
	for (AActor* Actor : AlreadyOverlapping)
	{
		AddCandidate(Actor);
	}
}

void UInteractionManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FocusRefreshTimer);
	}
	if (DetectionComponent)
	{
		DetectionComponent->OnComponentBeginOverlap.RemoveDynamic(this, &ThisClass::HandleBeginOverlap);
		DetectionComponent->OnComponentEndOverlap.RemoveDynamic(this, &ThisClass::HandleEndOverlap);
	}
	Super::EndPlay(EndPlayReason);
}

void UInteractionManagerComponent::CreatePromptWidget()
{
	AActor* Owner = GetOwner();
	if (!PromptWidgetClass || !Owner)
	{
		UE_LOG(LogTemp, Warning, TEXT("InteractionManager: PromptWidgetClass not set - no interaction prompt will show."));
		return;
	}

	// One widget for the whole game, created once and reused (the old BP made a new widget every change).
	PromptWidgetComponent = NewObject<UWidgetComponent>(Owner, TEXT("InteractionPromptWidget"));
	PromptWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	PromptWidgetComponent->SetDrawAtDesiredSize(true);
	PromptWidgetComponent->SetWidgetClass(PromptWidgetClass);
	PromptWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PromptWidgetComponent->SetupAttachment(Owner->GetRootComponent());
	// Absolute location: attached to the player, but does NOT follow them - it sits on the interactable.
	PromptWidgetComponent->SetUsingAbsoluteLocation(true);
	PromptWidgetComponent->RegisterComponent();
	PromptWidgetComponent->InitWidget();
	PromptWidgetComponent->SetVisibility(false);
}

// ---------------------------------------------------------------- Overlap / candidates

void UInteractionManagerComponent::HandleBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	AddCandidate(OtherActor);
}

void UInteractionManagerComponent::HandleEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32)
{
	// An actor can overlap with several components (e.g. NPC capsule + range sphere).
	// Only drop it when the LAST one stops overlapping.
	if (DetectionComponent && DetectionComponent->IsOverlappingActor(OtherActor))
	{
		return;
	}
	RemoveCandidate(OtherActor);
}

bool UInteractionManagerComponent::IsInteractable(const AActor* Actor)
{
	// Works for both C++ and Blueprint implementers.
	return Actor && Actor->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass());
}

void UInteractionManagerComponent::AddCandidate(AActor* Actor)
{
	if (!IsInteractable(Actor) || Actor == GetOwner())
	{
		return;
	}

	Candidates.AddUnique(Actor);

	UWorld* World = GetWorld();
	if (World && !World->GetTimerManager().IsTimerActive(FocusRefreshTimer))
	{
		World->GetTimerManager().SetTimer(FocusRefreshTimer, this, &ThisClass::RefreshFocus, FocusRefreshInterval, true);
	}

	RefreshFocus();
}

void UInteractionManagerComponent::RemoveCandidate(AActor* Actor)
{
	Candidates.Remove(Actor);
	RefreshFocus();
}

// ---------------------------------------------------------------- Focus

bool UInteractionManagerComponent::GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (const AController* Controller = Pawn ? Pawn->GetController() : nullptr)
	{
		Controller->GetPlayerViewPoint(OutLocation, OutRotation);
		return true;
	}
	return false;
}

FVector UInteractionManagerComponent::GetInteractionLocationFor(AActor* Actor) const
{
	const FVector Location = IInteractableInterface::Execute_GetInteractionLocation(Actor);
	return Location.IsNearlyZero() ? Actor->GetActorLocation() : Location;
}

void UInteractionManagerComponent::RefreshFocus()
{
	// Drop anything destroyed (picked-up items etc.).
	Candidates.RemoveAll([](const TWeakObjectPtr<AActor>& Weak) { return !Weak.IsValid(); });

	if (Candidates.IsEmpty())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(FocusRefreshTimer);
		}
		SetFocus(nullptr);
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	const bool bHasView = GetViewPoint(ViewLocation, ViewRotation);
	const FVector OwnerLocation = GetOwner()->GetActorLocation();

	// Pick the available interactable closest to where the camera is aiming.
	AActor* Best = nullptr;
	float BestScore = -MAX_flt;

	for (const TWeakObjectPtr<AActor>& Weak : Candidates)
	{
		AActor* Actor = Weak.Get();
		const FInteractionOption Option = IInteractableInterface::Execute_GetInteractionOption(Actor, GetOwner());
		if (!Option.bIsAvailable)
		{
			continue;
		}

		const FVector Location = GetInteractionLocationFor(Actor);
		const float Score = bHasView
			? FVector::DotProduct(ViewRotation.Vector(), (Location - ViewLocation).GetSafeNormal())
			: -FVector::DistSquared(OwnerLocation, Location);

		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Actor;
		}
	}

	SetFocus(Best);
	UpdatePrompt(false); // option text can change while focused (e.g. gold cost)
}

void UInteractionManagerComponent::SetFocus(AActor* NewFocus)
{
	if (FocusedActor.Get() == NewFocus)
	{
		return;
	}

	CancelHold(); // looking at something else resets hold progress
	FocusedActor = NewFocus;
	UpdatePrompt(true);
	OnFocusChanged.Broadcast(NewFocus);
}

void UInteractionManagerComponent::UpdatePrompt(bool bForce)
{
	if (!PromptWidgetComponent)
	{
		return;
	}

	AActor* Focus = FocusedActor.Get();
	if (!Focus)
	{
		PromptWidgetComponent->SetVisibility(false);
		return;
	}

	const FInteractionOption NewOption = IInteractableInterface::Execute_GetInteractionOption(Focus, GetOwner());
	const bool bChanged = bForce || !NewOption.IsSameAs(FocusedOption);
	FocusedOption = NewOption;

	PromptWidgetComponent->SetWorldLocation(GetInteractionLocationFor(Focus));

	if (bChanged)
	{
		if (UInteractionPromptWidget* Widget = Cast<UInteractionPromptWidget>(PromptWidgetComponent->GetUserWidgetObject()))
		{
			Widget->SetPrompt(FocusedOption);
		}
	}

	PromptWidgetComponent->SetVisibility(true);
}

// ---------------------------------------------------------------- Interact / hold

void UInteractionManagerComponent::BeginInteract()
{
	AActor* Focus = FocusedActor.Get();
	if (!Focus || bIsHolding)
	{
		return;
	}

	FocusedOption = IInteractableInterface::Execute_GetInteractionOption(Focus, GetOwner());
	if (!FocusedOption.bIsAvailable)
	{
		return;
	}

	if (FocusedOption.HoldDuration <= 0.f)
	{
		PerformInteract();
		return;
	}

	bIsHolding = true;
	HoldElapsed = 0.f;
	SetComponentTickEnabled(true);
}

void UInteractionManagerComponent::EndInteract()
{
	CancelHold();
}

void UInteractionManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsHolding || !FocusedActor.IsValid())
	{
		CancelHold();
		SetComponentTickEnabled(false);
		return;
	}

	HoldElapsed += DeltaTime;
	SetHoldProgress(FMath::Clamp(HoldElapsed / FocusedOption.HoldDuration, 0.f, 1.f));

	if (HoldElapsed >= FocusedOption.HoldDuration)
	{
		bIsHolding = false;
		HoldElapsed = 0.f;
		SetComponentTickEnabled(false);
		SetHoldProgress(0.f);
		PerformInteract();
	}
}

void UInteractionManagerComponent::CancelHold()
{
	if (!bIsHolding)
	{
		return;
	}
	bIsHolding = false;
	HoldElapsed = 0.f;
	SetComponentTickEnabled(false);
	SetHoldProgress(0.f);
}

void UInteractionManagerComponent::SetHoldProgress(float Percent)
{
	OnHoldProgress.Broadcast(Percent);
	if (PromptWidgetComponent)
	{
		if (UInteractionPromptWidget* Widget = Cast<UInteractionPromptWidget>(PromptWidgetComponent->GetUserWidgetObject()))
		{
			Widget->SetHoldProgress(Percent);
		}
	}
}

void UInteractionManagerComponent::PerformInteract()
{
	AActor* Target = FocusedActor.Get();
	if (!Target)
	{
		return;
	}

	IInteractableInterface::Execute_Interact(Target, GetOwner());
	OnInteracted.Broadcast(Target);

	// The target may now be gone (pickup) or unavailable (opened chest).
	RefreshFocus();
}
