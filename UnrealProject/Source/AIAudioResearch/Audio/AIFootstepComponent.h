// Copyright 2025 Igor Szuniewicz. Audio Generation Research Project.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AIFootstepComponent.generated.h"

class UAudioManager;
class UAudioComponent;
class USoundWaveProcedural;
class UPhysicalMaterial;

/**
 * Surface type enum for footstep sounds
 */
UENUM(BlueprintType)
enum class EFootstepSurface : uint8
{
	Stone    UMETA(DisplayName = "Stone"),
	Grass    UMETA(DisplayName = "Grass"),
	Carpet   UMETA(DisplayName = "Carpet"),
	Wood     UMETA(DisplayName = "Wood"),
	Metal    UMETA(DisplayName = "Metal"),
	Water    UMETA(DisplayName = "Water")
};

/**
 * Component that handles AI-generated footstep sounds
 * Supports multiple surface types with physical material detection
 * Includes pre-generation for upcoming surfaces
 */
UCLASS(ClassGroup=(AIAudio), meta=(BlueprintSpawnableComponent))
class AIAUDIORESEARCH_API UAIFootstepComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAIFootstepComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	/** Play a footstep sound based on current detected surface */
	UFUNCTION(BlueprintCallable, Category = "AI Audio|Footsteps")
	void PlayFootstep();
	
	/** Pre-generate footstep variants for a specific surface */
	UFUNCTION(BlueprintCallable, Category = "AI Audio|Footsteps")
	void PreGenerateSurface(EFootstepSurface Surface);
	
	/** Pre-generate neighbor surfaces for the given surface */
	void PreGenerateNeighbors(EFootstepSurface Surface);
	
	/** Pre-generate footsteps for the current default surface */
	UFUNCTION(BlueprintCallable, Category = "AI Audio|Footsteps")
	void PreGenerateFootsteps();
	
	/** Check if footsteps for current surface are ready */
	UFUNCTION(BlueprintPure, Category = "AI Audio|Footsteps")
	bool AreFootstepsReady() const;
	
	/** Get generation progress for current surface (0.0 - 1.0) */
	UFUNCTION(BlueprintPure, Category = "AI Audio|Footsteps")
	float GetGenerationProgress() const;
	
	/** Get the current detected surface type */
	UFUNCTION(BlueprintPure, Category = "AI Audio|Footsteps")
	EFootstepSurface GetCurrentSurface() const { return CurrentSurface; }

	// ============ SETTINGS ============
	
	/** Server URL for the unified audio server */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Settings")
	FString ServerURL = TEXT("ws://localhost:8770");
	
	/** AI model to use for footstep generation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Settings")
	FString AIModel = TEXT("elevenlabs");
	
	/** Default surface type (used at start) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Settings")
	EFootstepSurface DefaultSurface = EFootstepSurface::Stone;
	
	/** Number of footstep variants to generate per surface */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Settings", meta = (ClampMin = "1", ClampMax = "5"))
	int32 NumVariants = 1;
	
	/** Duration of each footstep sound in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Settings", meta = (ClampMin = "0.3", ClampMax = "2.0"))
	float FootstepDuration = 1.0f;
	
	/** Volume multiplier for footsteps */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Settings", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float VolumeMultiplier = 1.0f;
	
	/** Pitch variation range (random pitch = 1.0 +/- this value) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Settings", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float PitchVariation = 0.1f;

	/** Low Pass Filter variation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Settings", meta = (ClampMin = "0.0", ClampMax = "15000.0"))
	float FilterVariation = 8000.0f;

	/** Show debug messages on screen */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Debug")
	bool bShowDebug = true;

	// ============ SURFACE SETTINGS ============
	
	/** Mapping of surface types to AI prompts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Surfaces")
	TMap<EFootstepSurface, FString> SurfacePrompts;
	
	/** Mapping of Physical Material names to surface types */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Surfaces")
	TMap<FName, EFootstepSurface> PhysMatToSurface;
	
	// ============ VELOCITY PREDICTION ============
	
	/** Enable velocity-based prediction for pre-generation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Prediction")
	bool bEnableVelocityPrediction = true;
	
	/** How far ahead to predict (in cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Prediction", meta = (ClampMin = "100", ClampMax = "1000"))
	float PredictionDistance = 500.0f;
	
	/** Minimum velocity to trigger prediction (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Prediction", meta = (ClampMin = "50", ClampMax = "500"))
	float MinVelocityForPrediction = 100.0f;
	
	/** How often to check for velocity prediction (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Prediction", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float PredictionCheckInterval = 0.3f;

private:
	UFUNCTION()
	void OnSoundReady(const FString& SoundId, USoundWaveProcedural* Sound);
	
	UFUNCTION()
	void OnPreGenerationComplete(float TotalTimeSeconds);
	
	/** Detect current surface from line trace */
	void DetectCurrentSurface();
	
	/** Get surface name as string for ID generation */
	FString GetSurfaceName(EFootstepSurface Surface) const;
	
	/** Initialize default prompt mappings */
	void InitializeDefaultPrompts();
	
	/** Predict upcoming surface based on velocity and pre-generate */
	void PredictUpcomingSurface();
	
	/** Get surface from physical material */
	EFootstepSurface GetSurfaceFromPhysMat(UPhysicalMaterial* PhysMat);
	
	UPROPERTY()
	UAudioManager* AudioManager;
	
	UPROPERTY()
	UAudioComponent* AudioComponent;
	
	/** Cached sounds per surface: Surface -> Array of variant IDs */
	TMap<EFootstepSurface, TArray<FString>> SurfaceSoundIds;
	
	/** Track which surfaces are fully generated */
	TSet<EFootstepSurface> GeneratedSurfaces;
	
	/** Track which surfaces are currently generating */
	TSet<EFootstepSurface> GeneratingSurfaces;
	
	/** Count of generated variants per surface */
	TMap<EFootstepSurface, int32> GeneratedCountPerSurface;
	
	/** Current detected surface */
	EFootstepSurface CurrentSurface = EFootstepSurface::Stone;
	
	/** Index of last played footstep per surface (to avoid repeating) */
	TMap<EFootstepSurface, int32> LastPlayedIndex;
	
	/** Timer for line trace throttling */
	float TimeSinceLastTrace = 0.0f;
	
	/** Timer for velocity prediction */
	float TimeSinceLastPrediction = 0.0f;
	
	/** Last predicted surface to avoid spam */
	EFootstepSurface LastPredictedSurface = EFootstepSurface::Stone;
	
	/** Previous position for velocity calculation */
	FVector PreviousPosition = FVector::ZeroVector;
};
