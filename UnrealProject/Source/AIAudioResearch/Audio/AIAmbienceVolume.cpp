// Copyright 2025 Igor Szuniewicz. Audio Generation Research Project.

#include "AIAmbienceVolume.h"
#include "Components/BoxComponent.h"
#include "AIAmbienceComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

AAIAmbienceVolume::AAIAmbienceVolume()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f; // Tick every 100ms for checking

	// Create trigger box
	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	RootComponent = Bounds;
	Bounds->SetBoxExtent(FVector(500.0f, 500.0f, 500.0f));
	
	// CRITICAL: Proper collision settings for overlap detection
	Bounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Bounds->SetCollisionObjectType(ECC_WorldDynamic);
	Bounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	Bounds->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Bounds->SetGenerateOverlapEvents(true);

	// Visual settings for editor
	Bounds->SetHiddenInGame(false);
	Bounds->SetVisibility(true);
	Bounds->ShapeColor = FColor(0, 255, 255); // Cyan color for ambience zones
	Bounds->SetLineThickness(2.0f);
}

void AAIAmbienceVolume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	TimeSinceLastCheck += DeltaTime;
	if (TimeSinceLastCheck < TickCheckInterval) return;
	TimeSinceLastCheck = 0.0f;
	
	// Get player pawn if not cached
	if (!CachedPlayer)
	{
		CachedPlayer = UGameplayStatics::GetPlayerPawn(this, 0);
	}
	
	if (!CachedPlayer || !Bounds) return;
	
	// Use ONLY bounds overlap - manual FBox check ignores rotation and causes false positives
	bool bIsInside = Bounds->IsOverlappingActor(CachedPlayer);
	
	// State change detection
	if (bIsInside && !bPlayerInside)
	{
		UE_LOG(LogTemp, Log, TEXT("AIAmbienceVolume '%s': ENTERING ZONE"), *AmbiencePrompt);
		bPlayerInside = true;
		TriggerAmbience(CachedPlayer);
	}
	else if (!bIsInside && bPlayerInside)
	{
		UE_LOG(LogTemp, Log, TEXT("AIAmbienceVolume '%s': EXITING ZONE"), *AmbiencePrompt);
		bPlayerInside = false;
		StopAmbience(CachedPlayer);
	}
}

void AAIAmbienceVolume::TriggerAmbience(AActor* PlayerActor)
{
	if (!PlayerActor || AmbiencePrompt.IsEmpty()) return;
	
	UAIAmbienceComponent* AmbienceComp = PlayerActor->FindComponentByClass<UAIAmbienceComponent>();
	if (AmbienceComp)
	{
		UE_LOG(LogTemp, Log, TEXT("AIAmbienceVolume: Player entered '%s' zone"), *AmbiencePrompt);
		AmbienceComp->SwitchAmbience(AmbiencePrompt);
	}
	
	// Pre-generate footsteps for expected surface in this zone
	if (bPreGenFootsteps)
	{
		UAIFootstepComponent* FootstepComp = PlayerActor->FindComponentByClass<UAIFootstepComponent>();
		if (FootstepComp)
		{
			UE_LOG(LogTemp, Log, TEXT("AIAmbienceVolume: Pre-generating footsteps for surface type"));
			FootstepComp->PreGenerateSurface(ExpectedSurface);
		}
	}
}

void AAIAmbienceVolume::StopAmbience(AActor* PlayerActor)
{
	if (!PlayerActor) return;
	
	UAIAmbienceComponent* AmbienceComp = PlayerActor->FindComponentByClass<UAIAmbienceComponent>();
	if (AmbienceComp)
	{
		// ONLY stop if exiting the CURRENTLY PLAYING zone
		if (AmbienceComp->AmbiencePrompt == AmbiencePrompt)
		{
			UE_LOG(LogTemp, Log, TEXT("AIAmbienceVolume: Player exited '%s' zone"), *AmbiencePrompt);
			AmbienceComp->StopAmbience();
		}
	}
}

void AAIAmbienceVolume::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
	// NOTE: Tick() handles all triggering - overlap events commented out to prevent double-triggers
	// The tick-based detection is more reliable for zone transitions
}

void AAIAmbienceVolume::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);
	// NOTE: Tick() handles all stopping - overlap events commented out to prevent double-triggers
}
