// Copyright 2025 Igor Szuniewicz. Audio Generation Research Project.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AudioTestActor.generated.h"

class UAudioManager;
class ULatencyLogger;
class UAudioComponent;
class USoundWaveProcedural;

/**
 * Test actor for triggering AI audio generation during gameplay
 * Place in level and use keyboard keys to test different sounds
 * Features visual feedback for presentations
 */
UCLASS(Blueprintable)
class AIAUDIORESEARCH_API AAudioTestActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AAudioTestActor();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent);

	/** Server URL for WebSocket connection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Config")
	FString ServerURL = TEXT("ws://localhost:8765");
	
	/** Model name for logging purposes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Config")
	FString CurrentModelName = TEXT("Procedural");
	
	/** Test prompts for each key */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Config")
	FString TestPrompt1 = TEXT("footstep_wood");
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Config")
	FString TestPrompt2 = TEXT("footstep_concrete");
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Config")
	FString TestPrompt3 = TEXT("gunshot");
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Config")
	FString TestPrompt4 = TEXT("impact_metal");
	
	/** Path to export CSV logs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Config")
	FString CSVExportPath = TEXT("");
	
	/** Show on-screen debug info */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Config")
	bool bShowDebugInfo = true;

protected:
	UFUNCTION()
	void OnSoundReady(const FString& SoundId, USoundWaveProcedural* Sound);
	
	UFUNCTION()
	void OnLatencyMeasured(const FString& SoundId, float LatencyMs);
	
	void RequestTestSound(const FString& Prompt);
	void ExportLogs();
	void DrawVisualUI();
	FColor GetLatencyColor(float LatencyMs);

private:
	UPROPERTY()
	UAudioManager* AudioManager;
	
	UPROPERTY()
	ULatencyLogger* LatencyLogger;
	
	UPROPERTY()
	UAudioComponent* AudioComponent;
	
	float LastLatency;
	FString LastSoundId;
	int32 TotalRequests;
	
	// Visual feedback
	float PlaybackPulseAlpha;
	TArray<float> LatencyHistory;
	float TimeSinceLastSound;
	bool bIsPlaying;
};
