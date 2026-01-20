// Copyright 2025 Igor Szuniewicz. Audio Generation Research Project.

#include "AIAmbienceGenerator.h"
#include "AIAudioManager.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "Engine/Engine.h"

AAIAmbienceGenerator::AAIAmbienceGenerator()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create audio component for ambient playback
	AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AmbienceAudioComponent"));
	RootComponent = AudioComponent;
	
	// Configure for ambient looping
	AudioComponent->bAutoActivate = false;
	AudioComponent->bIsUISound = false;
}

void AAIAmbienceGenerator::BeginPlay()
{
	Super::BeginPlay();

	// Create audio manager
	AudioManager = NewObject<UAudioManager>(this);
	
	// Bind to sound ready event
	AudioManager->OnSoundReady.AddDynamic(this, &AAIAmbienceGenerator::OnAmbienceReady);
	
	// Connect to server
	AudioManager->Initialize(ServerURL);
	
	UE_LOG(LogTemp, Log, TEXT("AIAmbienceGenerator: Initialized, connecting to %s"), *ServerURL);

	// Auto-generate on start if enabled
	if (bGenerateOnStart)
	{
		// Small delay to ensure connection is established
		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &AAIAmbienceGenerator::GenerateAmbience, 1.0f, false);
	}
}

void AAIAmbienceGenerator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clear loop timer
	GetWorld()->GetTimerManager().ClearTimer(LoopTimerHandle);
	
	if (AudioManager)
	{
		AudioManager->Shutdown();
	}

	Super::EndPlay(EndPlayReason);
}

void AAIAmbienceGenerator::GenerateAmbience()
{
	if (!AudioManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("AIAmbienceGenerator: AudioManager not initialized"));
		return;
	}

	if (!AudioManager->IsConnected())
	{
		UE_LOG(LogTemp, Warning, TEXT("AIAmbienceGenerator: Not connected to server yet, retrying in 1s..."));
		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &AAIAmbienceGenerator::GenerateAmbience, 1.0f, false);
		return;
	}

	if (bIsGenerating)
	{
		UE_LOG(LogTemp, Warning, TEXT("AIAmbienceGenerator: Already generating ambience"));
		return;
	}

	bIsGenerating = true;

	UE_LOG(LogTemp, Log, TEXT("AIAmbienceGenerator: Generating ambient sound: '%s' (%.1fs)"), *AmbiencePrompt, AmbienceDuration);
	
	// Show on-screen message
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, 
			FString::Printf(TEXT("🎵 Generating ambience (%.0fs): %s"), AmbienceDuration, *AmbiencePrompt));
	}

	// Request the ambient sound with specified duration
	AudioManager->RequestSound(AmbiencePrompt, TEXT("ambience_main"), AmbienceDuration);
}

void AAIAmbienceGenerator::StopAmbience()
{
	// Stop loop timer
	GetWorld()->GetTimerManager().ClearTimer(LoopTimerHandle);
	
	if (AudioComponent && AudioComponent->IsPlaying())
	{
		AudioComponent->Stop();
		UE_LOG(LogTemp, Log, TEXT("AIAmbienceGenerator: Ambience stopped"));
	}
}

void AAIAmbienceGenerator::OnAmbienceReady(const FString& SoundId, USoundWaveProcedural* Sound)
{
	if (SoundId != TEXT("ambience_main"))
	{
		return; // Not our sound
	}

	bIsGenerating = false;

	if (!Sound)
	{
		UE_LOG(LogTemp, Error, TEXT("AIAmbienceGenerator: Failed to generate ambience"));
		return;
	}

	CachedDuration = Sound->Duration;
	UE_LOG(LogTemp, Log, TEXT("AIAmbienceGenerator: Ambience ready! Duration: %.2f sec"), CachedDuration);

	// Set sound and play
	if (AudioComponent)
	{
		AudioComponent->SetSound(Sound);
		AudioComponent->SetVolumeMultiplier(Volume);
		AudioComponent->Play();

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, 
				FString::Printf(TEXT("✅ Ambience playing! (%.1fs)"), CachedDuration));
		}

		// Setup loop timer if looping is enabled
		if (bLoopAmbience && CachedDuration > 0.1f)
		{
			// Set timer to replay slightly before end to avoid gap
			float LoopDelay = CachedDuration - 0.1f;
			GetWorld()->GetTimerManager().SetTimer(
				LoopTimerHandle, 
				this, 
				&AAIAmbienceGenerator::ReplayAmbience, 
				LoopDelay, 
				true  // Repeating timer
			);
			UE_LOG(LogTemp, Log, TEXT("AIAmbienceGenerator: Loop timer set for %.2fs"), LoopDelay);
		}

		UE_LOG(LogTemp, Log, TEXT("AIAmbienceGenerator: Ambience playing (loop: %s)"), 
			bLoopAmbience ? TEXT("true") : TEXT("false"));
	}
}

void AAIAmbienceGenerator::ReplayAmbience()
{
	if (AudioComponent && AudioManager)
	{
		// Replay from cache (instant, 0ms latency)
		AudioManager->PlaySound(TEXT("ambience_main"), AudioComponent);
		UE_LOG(LogTemp, Log, TEXT("AIAmbienceGenerator: Looping ambience"));
	}
}
