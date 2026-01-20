// Copyright 2025 Igor Szuniewicz. Audio Generation Research Project.

#include "AILookPreGenComponent.h"
#include "AIDoorComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"

UAILookPreGenComponent::UAILookPreGenComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.0f;  // We handle timing ourselves
}

void UAILookPreGenComponent::BeginPlay()
{
	Super::BeginPlay();
	
	UE_LOG(LogTemp, Log, TEXT("AILookPreGenComponent: Initialized on '%s' (TraceDistance: %.0f cm)"), 
		*GetOwner()->GetName(), TraceDistance);
}

void UAILookPreGenComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	PreCachedObjects.Empty();
	Super::EndPlay(EndPlayReason);
}

void UAILookPreGenComponent::TickComponent(float DeltaTime, ELevelTick TickType, 
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TimeSinceLastCheck += DeltaTime;
	
	if (TimeSinceLastCheck >= CheckInterval)
	{
		TimeSinceLastCheck = 0.0f;
		DoLineTrace();
	}
}

void UAILookPreGenComponent::DoLineTrace()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn) return;
	
	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC) return;
	
	// Get camera location and direction
	FVector CameraLocation;
	FRotator CameraRotation;
	PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
	
	FVector TraceEnd = CameraLocation + CameraRotation.Vector() * TraceDistance;
	
	// Perform line trace
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());
	
	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		CameraLocation,
		TraceEnd,
		TraceChannel,
		QueryParams
	);
	
	// Debug visualization
	if (bShowDebug)
	{
		DrawDebugLine(GetWorld(), CameraLocation, TraceEnd, 
			bHit ? FColor::Green : FColor::Red, false, CheckInterval, 0, 1.0f);
		
		if (bHit)
		{
			DrawDebugSphere(GetWorld(), HitResult.ImpactPoint, 10.0f, 8, 
				FColor::Yellow, false, CheckInterval);
		}
	}
	
	if (!bHit)
	{
		CurrentTarget = nullptr;
		return;
	}
	
	AActor* HitActor = HitResult.GetActor();
	if (!HitActor)
	{
		CurrentTarget = nullptr;
		return;
	}
	
	CurrentTarget = HitActor;
	
	// Check if hit actor has AIDoorComponent
	UAIDoorComponent* DoorComp = HitActor->FindComponentByClass<UAIDoorComponent>();
	if (DoorComp)
	{
		// Use actor name as key since door has multiple sounds
		FString DoorKey = HitActor->GetName();

		// Already pre-cached this door this session?
		if (!PreCachedObjects.Contains(DoorKey))
		{
			UE_LOG(LogTemp, Log, TEXT("AILookPreGenComponent: Looking at door '%s' - triggering pre-cache"),
				*HitActor->GetName());

			PreCachedObjects.Add(DoorKey);
			DoorComp->PreCacheSounds();  // Pre-cache BOTH open and close sounds
		}
	}

	// Future: Add more component types here (chests, switches, etc.)
}
