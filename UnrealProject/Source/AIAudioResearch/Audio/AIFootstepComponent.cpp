// Copyright 2025 Igor Szuniewicz. Audio Generation Research Project.

#include "AIFootstepComponent.h"
#include "AIAudioManager.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

UAIFootstepComponent::UAIFootstepComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UAIFootstepComponent::InitializeDefaultPrompts()
{
	// Default surface prompts (can be overridden in editor)
	if (SurfacePrompts.Num() == 0)
	{
		SurfacePrompts.Add(EFootstepSurface::Stone, TEXT("footstep on hard stone floor, single step, clear impact"));
		SurfacePrompts.Add(EFootstepSurface::Grass, TEXT("footstep crushing soft grass, outdoor, natural"));
		SurfacePrompts.Add(EFootstepSurface::Carpet, TEXT("muffled footstep on thick soft carpet, quiet indoor"));
		SurfacePrompts.Add(EFootstepSurface::Wood, TEXT("footstep on wooden floorboard, creaky wood"));
		SurfacePrompts.Add(EFootstepSurface::Metal, TEXT("footstep on metal grate, industrial clang"));
		SurfacePrompts.Add(EFootstepSurface::Water, TEXT("footstep splashing through shallow water puddle"));
	}
	
	// Default physical material mappings (can be overridden in editor)
	if (PhysMatToSurface.Num() == 0)
	{
		PhysMatToSurface.Add(FName("PM_Stone"), EFootstepSurface::Stone);
		PhysMatToSurface.Add(FName("PM_Grass"), EFootstepSurface::Grass);
		PhysMatToSurface.Add(FName("PM_Carpet"), EFootstepSurface::Carpet);
		PhysMatToSurface.Add(FName("PM_Wood"), EFootstepSurface::Wood);
		PhysMatToSurface.Add(FName("PM_Metal"), EFootstepSurface::Metal);
		PhysMatToSurface.Add(FName("PM_Water"), EFootstepSurface::Water);
	}
}

FString UAIFootstepComponent::GetSurfaceName(EFootstepSurface Surface) const
{
	switch (Surface)
	{
		case EFootstepSurface::Stone:  return TEXT("stone");
		case EFootstepSurface::Grass:  return TEXT("grass");
		case EFootstepSurface::Carpet: return TEXT("carpet");
		case EFootstepSurface::Wood:   return TEXT("wood");
		case EFootstepSurface::Metal:  return TEXT("metal");
		case EFootstepSurface::Water:  return TEXT("water");
		default: return TEXT("unknown");
	}
}

void UAIFootstepComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	// NOTE: Velocity prediction disabled - using simpler neighbor-based pre-generation
	// When surface changes, we automatically pre-generate likely neighbor surfaces

	// Display generation status on screen (top-left)
	if (bShowDebug && GEngine)
	{
		FString StatusMsg;
		FColor StatusColor;

		bool bCurrentReady = GeneratedSurfaces.Contains(CurrentSurface);

		if (bCurrentReady)
		{
			StatusMsg = FString::Printf(TEXT("[%s]"), *GetSurfaceName(CurrentSurface).ToLower());
			StatusColor = FColor(0, 255, 0); // Green
		}
		else if (GeneratingSurfaces.Contains(CurrentSurface))
		{
			int32 Count = GeneratedCountPerSurface.Contains(CurrentSurface) ? GeneratedCountPerSurface[CurrentSurface] : 0;
			int32 Percent = NumVariants > 0 ? (Count * 100 / NumVariants) : 0;
			StatusMsg = FString::Printf(TEXT("[%s %d%%]"), *GetSurfaceName(CurrentSurface).ToLower(), Percent);
			StatusColor = FColor(255, 255, 0); // Yellow
		}
		else
		{
			StatusMsg = FString::Printf(TEXT("[loading %s...]"), *GetSurfaceName(CurrentSurface).ToLower());
			StatusColor = FColor(255, 255, 255); // White
		}

		GEngine->AddOnScreenDebugMessage(100, 0.0f, StatusColor, StatusMsg);
	}
}

void UAIFootstepComponent::BeginPlay()
{
	Super::BeginPlay();
	
	// Initialize default prompts
	InitializeDefaultPrompts();
	
	// Set initial surface
	CurrentSurface = DefaultSurface;
	
	// Create audio manager
	AudioManager = NewObject<UAudioManager>(this);
	if (AudioManager)
	{
		AudioManager->OnSoundReady.AddDynamic(this, &UAIFootstepComponent::OnSoundReady);
		AudioManager->OnPreGenerationComplete.AddDynamic(this, &UAIFootstepComponent::OnPreGenerationComplete);
		AudioManager->Initialize(ServerURL);
		
		// Wait for connection before generating
		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
		{
			if (AudioManager && AudioManager->IsConnected())
			{
				// Pre-generate default surface AND neighbors
				PreGenerateSurface(DefaultSurface);
				PreGenerateNeighbors(DefaultSurface);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("AIFootstepComponent: Still waiting for connection, retrying..."));
				FTimerHandle RetryHandle;
				GetWorld()->GetTimerManager().SetTimer(RetryHandle, [this]()
				{
					PreGenerateSurface(DefaultSurface);
					PreGenerateNeighbors(DefaultSurface);
				}, 0.5f, false);
			}
		}, 0.5f, false);
	}
	
	// Create audio component for playback
	AudioComponent = NewObject<UAudioComponent>(GetOwner());
	if (AudioComponent)
	{
		AudioComponent->bAutoActivate = false;
		AudioComponent->bAutoDestroy = false;
		AudioComponent->SetVolumeMultiplier(VolumeMultiplier);
		AudioComponent->RegisterComponent();
		AudioComponent->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	}
	
	UE_LOG(LogTemp, Log, TEXT("AIFootstepComponent: Initialized with %d surface types"), SurfacePrompts.Num());
}

void UAIFootstepComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AudioManager)
	{
		AudioManager->Shutdown();
	}
	
	Super::EndPlay(EndPlayReason);
}

void UAIFootstepComponent::DetectCurrentSurface()
{
	if (!GetOwner()) return;
	
	// Line trace down from character
	FHitResult Hit;
	FVector Start = GetOwner()->GetActorLocation();
	FVector End = Start - FVector(0, 0, 200);
	
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetOwner());
	Params.bReturnPhysicalMaterial = true;
	
	// Use WorldStatic channel for better floor detection
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
	{
		UPhysicalMaterial* PhysMat = Hit.PhysMaterial.Get();
		FString HitActorName = Hit.GetActor() ? Hit.GetActor()->GetName() : TEXT("None");
		FString MatNameStr = PhysMat ? PhysMat->GetFName().ToString() : TEXT("NULL");
		
		// ALWAYS log what we hit
		UE_LOG(LogTemp, Log, TEXT("AIFootstepComponent: HIT '%s' | PhysMat: '%s'"), *HitActorName, *MatNameStr);
		
		
		if (PhysMat)
		{
			FName MatName = PhysMat->GetFName();
			
			// Check if we have a mapping for this physical material
			if (PhysMatToSurface.Contains(MatName))
			{
				EFootstepSurface NewSurface = PhysMatToSurface[MatName];
				if (NewSurface != CurrentSurface)
				{
					UE_LOG(LogTemp, Log, TEXT("AIFootstepComponent: Surface changed to %s (from PhysMat '%s')"), 
						*GetSurfaceName(NewSurface), *MatName.ToString());
					CurrentSurface = NewSurface;
					
					// Pre-generate current surface if not cached
					if (!GeneratedSurfaces.Contains(CurrentSurface) && !GeneratingSurfaces.Contains(CurrentSurface))
					{
						PreGenerateSurface(CurrentSurface);
					}
					
					// Pre-generate neighbors automatically
					PreGenerateNeighbors(CurrentSurface);
				}
			}
			else
			{
				// Material not mapped - log warning
				UE_LOG(LogTemp, Warning, TEXT("AIFootstepComponent: PhysMat '%s' NOT in mapping!"), *MatName.ToString());
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("AIFootstepComponent: Hit actor '%s' has NO PhysMat!"), *HitActorName);
		}
	}
	else
	{
		// No hit at all
		UE_LOG(LogTemp, Warning, TEXT("AIFootstepComponent: NO FLOOR HIT!"));
	}
}

void UAIFootstepComponent::PreGenerateSurface(EFootstepSurface Surface)
{
	if (!AudioManager) return;
	
	// CRITICAL: Check if connected before attempting to generate
	if (!AudioManager->IsConnected())
	{
		UE_LOG(LogTemp, Warning, TEXT("AIFootstepComponent: Cannot pre-generate %s - not connected yet, will retry"), *GetSurfaceName(Surface));
		
		// Retry after connection (0.5s delay)
		FTimerHandle RetryHandle;
		GetWorld()->GetTimerManager().SetTimer(RetryHandle, [this, Surface]()
		{
			PreGenerateSurface(Surface);
		}, 0.5f, false);
		return;
	}
	
	// Skip if already generated or generating
	if (GeneratedSurfaces.Contains(Surface))
	{
		UE_LOG(LogTemp, Log, TEXT("AIFootstepComponent: Surface %s already cached"), *GetSurfaceName(Surface));
		return;
	}
	
	if (GeneratingSurfaces.Contains(Surface))
	{
		UE_LOG(LogTemp, Log, TEXT("AIFootstepComponent: Surface %s already generating"), *GetSurfaceName(Surface));
		return;
	}
	
	// Get prompt for this surface
	if (!SurfacePrompts.Contains(Surface))
	{
		UE_LOG(LogTemp, Warning, TEXT("AIFootstepComponent: No prompt for surface %s"), *GetSurfaceName(Surface));
		return;
	}
	
	FString Prompt = SurfacePrompts[Surface];
	GeneratingSurfaces.Add(Surface);
	GeneratedCountPerSurface.Add(Surface, 0);
	
	// Initialize sound ID array for this surface
	TArray<FString>& SoundIds = SurfaceSoundIds.FindOrAdd(Surface);
	SoundIds.Empty();

	UE_LOG(LogTemp, Log, TEXT("AIFootstepComponent: Pre-generating %d variants for '%s' surface"), NumVariants, *GetSurfaceName(Surface));

	// Generate variant IDs and request sounds
	for (int32 i = 0; i < NumVariants; i++)
	{
		FString SoundId = FString::Printf(TEXT("footstep_%s_%d"), *GetSurfaceName(Surface), i);
		SoundIds.Add(SoundId);
		AudioManager->RequestSound(Prompt, SoundId, FootstepDuration, AIModel);
	}
}

void UAIFootstepComponent::PreGenerateNeighbors(EFootstepSurface Surface)
{
	EFootstepSurface NeighborSurface = EFootstepSurface::Stone; // default fallback
	bool bHasNeighbor = false;
	
	switch (Surface)
	{
		case EFootstepSurface::Stone: 
			NeighborSurface = EFootstepSurface::Grass; 
			bHasNeighbor = true; 
			break;
		case EFootstepSurface::Grass: 
			NeighborSurface = EFootstepSurface::Stone; 
			bHasNeighbor = true;
			break;
		case EFootstepSurface::Carpet: 
			NeighborSurface = EFootstepSurface::Wood; 
			bHasNeighbor = true;
			break;
		case EFootstepSurface::Wood: 
			NeighborSurface = EFootstepSurface::Stone; // Wood likely connects to stone
			bHasNeighbor = true;
			break;
		case EFootstepSurface::Metal: 
			NeighborSurface = EFootstepSurface::Stone; 
			bHasNeighbor = true;
			break;
		case EFootstepSurface::Water: 
			NeighborSurface = EFootstepSurface::Grass; // Water often near grass
			bHasNeighbor = true;
			break;
		default: break;
	}
	
	if (bHasNeighbor)
	{
		if (!GeneratedSurfaces.Contains(NeighborSurface) && !GeneratingSurfaces.Contains(NeighborSurface))
		{
			UE_LOG(LogTemp, Log, TEXT("AIFootstepComponent: Neighbor Pre-gen: '%s' triggers '%s'"), 
				*GetSurfaceName(Surface), *GetSurfaceName(NeighborSurface));
				
			PreGenerateSurface(NeighborSurface);
		}
	}
}

void UAIFootstepComponent::PreGenerateFootsteps()
{
	PreGenerateSurface(DefaultSurface);
}

void UAIFootstepComponent::OnSoundReady(const FString& SoundId, USoundWaveProcedural* Sound)
{
	// Parse surface type from sound ID (format: footstep_SURFACE_INDEX)
	for (auto& Pair : SurfaceSoundIds)
	{
		EFootstepSurface Surface = Pair.Key;
		TArray<FString>& Ids = Pair.Value;
		
		if (Ids.Contains(SoundId))
		{
			// Increment count for this surface
			int32& Count = GeneratedCountPerSurface.FindOrAdd(Surface);
			Count++;
			
			UE_LOG(LogTemp, Log, TEXT("AIFootstepComponent: %s variant ready (%d/%d)"), 
				*GetSurfaceName(Surface), Count, NumVariants);
			
			// Check if all variants for this surface are ready
			if (Count >= NumVariants)
			{
				GeneratingSurfaces.Remove(Surface);
				GeneratedSurfaces.Add(Surface);

				UE_LOG(LogTemp, Log, TEXT("AIFootstepComponent: All %s footsteps ready!"), *GetSurfaceName(Surface));
			}
			break;
		}
	}
}

void UAIFootstepComponent::OnPreGenerationComplete(float TotalTimeSeconds)
{
	// This is called for batch pre-gen, not used in per-surface model
}

bool UAIFootstepComponent::AreFootstepsReady() const
{
	return GeneratedSurfaces.Contains(CurrentSurface);
}

float UAIFootstepComponent::GetGenerationProgress() const
{
	if (GeneratedSurfaces.Contains(CurrentSurface)) return 1.0f;
	
	if (GeneratedCountPerSurface.Contains(CurrentSurface))
	{
		return (float)GeneratedCountPerSurface[CurrentSurface] / (float)NumVariants;
	}
	
	return 0.0f;
}

void UAIFootstepComponent::PlayFootstep()
{
	// Detect current surface from ground
	DetectCurrentSurface();
	
	// Check if we have sounds for this surface
	if (!SurfaceSoundIds.Contains(CurrentSurface))
	{
		UE_LOG(LogTemp, Warning, TEXT("AIFootstepComponent: No sounds for surface %s"), *GetSurfaceName(CurrentSurface));
		PreGenerateSurface(CurrentSurface);
		return;
	}
	
	TArray<FString>& Ids = SurfaceSoundIds[CurrentSurface];
	if (Ids.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("AIFootstepComponent: Empty sound array for %s"), *GetSurfaceName(CurrentSurface));
		return;
	}
	
	// Check if sounds are cached
	if (!GeneratedSurfaces.Contains(CurrentSurface))
	{
		UE_LOG(LogTemp, Warning, TEXT("AIFootstepComponent: %s footsteps not ready yet"), *GetSurfaceName(CurrentSurface));
		return;
	}
	
	// Pick a random variant (avoid repeating last one)
	int32 Index = 0;
	if (Ids.Num() > 1)
	{
		int32 LastIdx = LastPlayedIndex.Contains(CurrentSurface) ? LastPlayedIndex[CurrentSurface] : -1;
		do {
			Index = FMath::RandRange(0, Ids.Num() - 1);
		} while (Index == LastIdx && Ids.Num() > 1);
	}
	LastPlayedIndex.FindOrAdd(CurrentSurface) = Index;
	
	// Apply pitch and volume variation
	if (AudioComponent)
	{
		float RandomPitch = 1.0f + FMath::RandRange(-PitchVariation, PitchVariation);
		AudioComponent->SetPitchMultiplier(RandomPitch);
		AudioComponent->SetVolumeMultiplier(VolumeMultiplier);
		
		// Low pass filter variation
		float RandomCutoff = 20000.0f - FMath::RandRange(0.0f, FilterVariation);
		AudioComponent->SetLowPassFilterFrequency(RandomCutoff);
		AudioComponent->SetLowPassFilterEnabled(true);
	}
	
	// Play the sound
	if (AudioManager && AudioComponent)
	{
		AudioManager->PlaySound(Ids[Index], AudioComponent);
	}
}

EFootstepSurface UAIFootstepComponent::GetSurfaceFromPhysMat(UPhysicalMaterial* PhysMat)
{
	if (!PhysMat) return DefaultSurface;
	
	FName MatName = PhysMat->GetFName();
	
	if (PhysMatToSurface.Contains(MatName))
	{
		return PhysMatToSurface[MatName];
	}
	
	return DefaultSurface;
}

void UAIFootstepComponent::PredictUpcomingSurface()
{
	if (!GetOwner()) return;
	
	AActor* Owner = GetOwner();
	FVector CurrentPosition = Owner->GetActorLocation();
	
	// Calculate velocity from position change
	FVector Velocity = (CurrentPosition - PreviousPosition) / PredictionCheckInterval;
	PreviousPosition = CurrentPosition;
	
	float Speed = Velocity.Size2D(); // Only horizontal velocity
	
	// Skip if moving too slow
	if (Speed < MinVelocityForPrediction) return;
	
	// Normalize velocity direction (horizontal only)
	FVector Direction = Velocity.GetSafeNormal2D();
	
	// Calculate prediction point ahead of player
	FVector PredictionPoint = CurrentPosition + Direction * PredictionDistance;
	
	// Raycast down from prediction point to find surface
	FHitResult Hit;
	FVector TraceStart = PredictionPoint + FVector(0, 0, 100); // Start above ground
	FVector TraceEnd = PredictionPoint - FVector(0, 0, 300);   // Trace down
	
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Owner);
	Params.bReturnPhysicalMaterial = true;
	
	if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		UPhysicalMaterial* PhysMat = Hit.PhysMaterial.Get();
		if (PhysMat)
		{
			EFootstepSurface PredictedSurface = GetSurfaceFromPhysMat(PhysMat);
			
			// Only pre-generate if different from current and not already processed
			if (PredictedSurface != CurrentSurface && PredictedSurface != LastPredictedSurface)
			{
				// Check if not already cached or generating
				if (!GeneratedSurfaces.Contains(PredictedSurface) && !GeneratingSurfaces.Contains(PredictedSurface))
				{
					UE_LOG(LogTemp, Log, TEXT("AIFootstepComponent: Velocity prediction -> Pre-generating '%s' footsteps"),
						*GetSurfaceName(PredictedSurface));

					PreGenerateSurface(PredictedSurface);
				}
				
				LastPredictedSurface = PredictedSurface;
			}
		}
	}
}

