// Copyright 2025 Igor Szuniewicz. Audio Generation Research Project.

#include "AIAmbienceComponent.h"
#include "AIAudioManager.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

UAIAmbienceComponent::UAIAmbienceComponent()
{
	// Default ID
	if (CurrentSoundId.IsEmpty())
	{
		CurrentSoundId = GetIDForPrompt(AmbiencePrompt);
	}

	// Primary component tick (not needed but good practice)
	PrimaryComponentTick.bCanEverTick = false;
}

void UAIAmbienceComponent::BeginPlay()
{
	Super::BeginPlay();

	// Create AudioComponent if it doesn't exist
	if (!AudioComponent)
	{
		AudioComponent = NewObject<UAudioComponent>(this, TEXT("AudioComponent"));
		if (AudioComponent)
		{
			AudioComponent->RegisterComponent();
			if (GetOwner())
			{
				AudioComponent->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
			}
		}
	}

	// Initialize AudioManager
	AudioManager = NewObject<UAudioManager>(this);
	if (AudioManager)
	{
		AudioManager->Initialize(ServerURL);

		// Bind to sound ready event
		AudioManager->OnSoundReady.AddDynamic(this, &UAIAmbienceComponent::OnAmbienceReady);

		// Set initial volume
		if (AudioComponent)
		{
			AudioComponent->SetVolumeMultiplier(Volume);
		}

		// NOTE: We do NOT auto-generate ambience on spawn.
		// Ambience is controlled by AIAmbienceVolume zones.
		// SwitchAmbience() will be called when player enters a zone.
		UE_LOG(LogTemp, Log, TEXT("AIAmbienceComponent: Ready. Waiting for zone trigger."));
	}
}

void UAIAmbienceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clear all timers
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
	}

	if (AudioManager)
	{
		AudioManager->Shutdown();
	}

	Super::EndPlay(EndPlayReason);
}

FString UAIAmbienceComponent::GetIDForPrompt(const FString& Prompt)
{
	// Sanitize prompt to create safe ID 
	// e.g. "forest ambience" -> "ambience_forest_ambience"
	FString SafePrompt = Prompt;
	// Simple sanitization: remove spaces and special chars if needed, but for now simple prefix is enough
	// as Manager handles strings as keys map.
	// But let's verify if Manager supports spaces. Yes it checks Contains().
	// To be safe and unique per prompt:
	return FString::Printf(TEXT("ambience_%u"), GetTypeHash(Prompt));
}

void UAIAmbienceComponent::SwitchAmbience(FString NewPrompt)
{
	if (NewPrompt.IsEmpty()) return;
	
	// Guard: Don't re-trigger if same ambience is already playing
	if (NewPrompt == AmbiencePrompt && AudioComponent && AudioComponent->IsPlaying())
	{
		UE_LOG(LogTemp, Log, TEXT("AIAmbienceComponent: '%s' already playing, ignoring trigger"), *NewPrompt);
		return;
	}
	
	UE_LOG(LogTemp, Log, TEXT("AIAmbienceComponent: Switching ambience to '%s'"), *NewPrompt);

	// Update prompt
	AmbiencePrompt = NewPrompt;
	FString NewId = GetIDForPrompt(NewPrompt);

	// Clear all timers
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(LoopTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(CrossfadeTimerHandle);
	}
	
	// CROSSFADE: Fade out current audio instead of hard stop
	if (AudioComponent && AudioComponent->IsPlaying())
	{
		AudioComponent->FadeOut(CrossfadeDuration, 0.0f);
	}
	
	// Set new ID
	CurrentSoundId = NewId;
	bIsGenerating = false; // Reset flag to allow new generation

	// Check if already cached OR already generating
	if (AudioManager && AudioManager->IsSoundCached(NewId))
	{
		// CROSSFADE: Delay play using SAFE member function (not lambda!)
		bPendingCachedPlay = true;
		GetWorld()->GetTimerManager().SetTimer(CrossfadeTimerHandle, this, 
			&UAIAmbienceComponent::PlayAfterCrossfade, CrossfadeDuration, false);
	}
	else if (GeneratingSoundIds.Contains(NewId))
	{
		UE_LOG(LogTemp, Log, TEXT("AIAmbienceComponent: '%s' already generating, will play when ready"), *NewId);
		bPendingCachedPlay = true; // Will play when OnAmbienceReady fires
	}
	else
	{
		// Not cached and not generating, start generation (will play after fade-out naturally completes)
		bPendingCachedPlay = false;
		GetWorld()->GetTimerManager().SetTimer(CrossfadeTimerHandle, this, 
			&UAIAmbienceComponent::GenerateAmbience, CrossfadeDuration, false);
	}
}

void UAIAmbienceComponent::PlayAfterCrossfade()
{
	if (!AudioManager || !AudioComponent || CurrentSoundId.IsEmpty()) return;

	UE_LOG(LogTemp, Log, TEXT("AIAmbienceComponent: Playing CACHED sound '%s' after crossfade"), *CurrentSoundId);
	AudioManager->PlaySound(CurrentSoundId, AudioComponent);
	AudioComponent->FadeIn(CrossfadeDuration, Volume);

	// Setup looping
	CachedDuration = AmbienceDuration;
	if (bLoopAmbience && GetWorld())
	{
		float LoopDelay = CachedDuration - 0.1f;
		GetWorld()->GetTimerManager().SetTimer(LoopTimerHandle, this,
			&UAIAmbienceComponent::ReplayAmbience, LoopDelay, true);
	}
}

void UAIAmbienceComponent::PreCacheAmbience(FString Prompt)
{
	if (!AudioManager || Prompt.IsEmpty()) return;

	FString Id = GetIDForPrompt(Prompt);

	// Check if already cached
	if (AudioManager->IsSoundCached(Id))
	{
		UE_LOG(LogTemp, Log, TEXT("AIAmbienceComponent: Pre-cache '%s' (ID: %s) already in cache."), *Prompt, *Id);
		return;
	}

	// Check if already generating
	if (GeneratingSoundIds.Contains(Id))
	{
		UE_LOG(LogTemp, Log, TEXT("AIAmbienceComponent: Pre-cache '%s' (ID: %s) already generating."), *Prompt, *Id);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("AIAmbienceComponent: Pre-caching '%s' (ID: %s)"), *Prompt, *Id);

	// Track this ID as generating
	GeneratingSoundIds.Add(Id);

	// Request generation but do not switch to it yet.
	// The OnAmbienceReady handler ignores IDs != CurrentSoundId, so it won't play automatically.
	AudioManager->RequestSound(Prompt, Id, AmbienceDuration, AIModel);
}

void UAIAmbienceComponent::StopAmbience()
{
	UE_LOG(LogTemp, Log, TEXT("AIAmbienceComponent: Stopping ambience"));

	// Clear loop timer
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(LoopTimerHandle);
	}

	// Stop audio
	if (AudioComponent)
	{
		AudioComponent->Stop();
	}

	// Reset state
	bIsGenerating = false;
	CurrentSoundId = TEXT("");
}

void UAIAmbienceComponent::TryGenerateAmbience()
{
	if (!AudioManager) return;
	
	if (AudioManager->IsConnected())
	{
		GenerateAmbience();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AIAmbienceComponent: Still waiting for connection, retrying..."));
		// Retry after another 1.0s using safe timer
		FTimerHandle RetryHandle;
		GetWorld()->GetTimerManager().SetTimer(RetryHandle, this, &UAIAmbienceComponent::TryGenerateAmbience, 1.0f, false);
	}
}

void UAIAmbienceComponent::GenerateAmbience()
{
	if (!AudioManager || bIsGenerating) return;

	// Ensure ID is set
	if (CurrentSoundId.IsEmpty())
	{
		CurrentSoundId = GetIDForPrompt(AmbiencePrompt);
	}

	// Check cache again just in case
	if (AudioManager->IsSoundCached(CurrentSoundId))
	{
		UE_LOG(LogTemp, Log, TEXT("AIAmbienceComponent: Sound '%s' already cached, playing instead of generating"), *CurrentSoundId);
		AudioManager->PlaySound(CurrentSoundId, AudioComponent);
		return;
	}
	
	// Check if already generating (from PreCache)
	if (GeneratingSoundIds.Contains(CurrentSoundId))
	{
		UE_LOG(LogTemp, Log, TEXT("AIAmbienceComponent: Sound '%s' already generating (from pre-cache), waiting..."), *CurrentSoundId);
		bIsGenerating = true; // Mark so we play when ready
		return;
	}

	bIsGenerating = true;
	GeneratingSoundIds.Add(CurrentSoundId);

	UE_LOG(LogTemp, Log, TEXT("AIAmbienceComponent: Generating ambience '%s' (ID: %s)"),
		*AmbiencePrompt, *CurrentSoundId);

	// Request sound
	AudioManager->RequestSound(AmbiencePrompt, CurrentSoundId, AmbienceDuration, AIModel);
}

void UAIAmbienceComponent::OnAmbienceReady(const FString& SoundId, USoundWaveProcedural* Sound)
{
	// Remove from generating set (it's now cached)
	GeneratingSoundIds.Remove(SoundId);

	// Ignore if this is not the CURRENTLY desired ambience (e.g. from pre-gen of another zone)
	if (SoundId != CurrentSoundId)
	{
		UE_LOG(LogTemp, Log, TEXT("AIAmbienceComponent: Received sound '%s' but current focus is '%s'. Ignoring (it is cached)."), *SoundId, *CurrentSoundId);
		return;
	}

	bIsGenerating = false;

	if (!Sound)
	{
		UE_LOG(LogTemp, Error, TEXT("AIAmbienceComponent: Failed to generate ambience"));
		return;
	}

	CachedDuration = Sound->Duration;
	UE_LOG(LogTemp, Log, TEXT("AIAmbienceComponent: Ambience ready! Duration: %.2f sec"), CachedDuration);

	if (AudioComponent && AudioManager)
	{
		// Use PlaySound to ensure proper sound wave handling
		AudioManager->PlaySound(CurrentSoundId, AudioComponent);

		// Setup looping manually
		if (bLoopAmbience && CachedDuration > 0.5f)
		{
			// Replay EXACTLY at the end to minimize potential gap
			// Or slightly before
			float LoopDelay = CachedDuration - 0.1f;
			GetWorld()->GetTimerManager().SetTimer(
				LoopTimerHandle,
				this,
				&UAIAmbienceComponent::ReplayAmbience,
				LoopDelay,
				true
			);
		}
	}
}

void UAIAmbienceComponent::ReplayAmbience()
{
	if (AudioComponent && AudioManager && !CurrentSoundId.IsEmpty())
	{
		// Use AudioManager to play from cache (creates fresh sound instance)
		// This fixes looping issues with procedural sounds that might have empty buffers
		AudioManager->PlaySound(CurrentSoundId, AudioComponent);
	}
}
