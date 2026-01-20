// Copyright 2025 Igor Szuniewicz. Audio Generation Research Project.

#include "AIDoorComponent.h"
#include "AIAudioManager.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

UAIDoorComponent::UAIDoorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAIDoorComponent::BeginPlay()
{
	Super::BeginPlay();
	
	// Create audio manager
	AudioManager = NewObject<UAudioManager>(this);
	
	if (AudioManager)
	{
		AudioManager->OnSoundReady.AddDynamic(this, &UAIDoorComponent::OnSoundReady);
		AudioManager->Initialize(ServerURL);
	}
	
	// Create audio component for playback
	AudioComponent = NewObject<UAudioComponent>(GetOwner());
	if (AudioComponent)
	{
		AudioComponent->bAutoActivate = false;
		AudioComponent->bAutoDestroy = false;
		AudioComponent->SetVolumeMultiplier(Volume);
		AudioComponent->RegisterComponent();
		AudioComponent->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	}
	
	UE_LOG(LogTemp, Log, TEXT("AIDoorComponent: Ready on '%s' (Open: '%s', Close: '%s')"), 
		*GetOwner()->GetName(), *OpenPrompt, *ClosePrompt);
}

void UAIDoorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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

FString UAIDoorComponent::GetOpenSoundId() const
{
	// Fixed ID based on owner name - always the same for open
	return FString::Printf(TEXT("door_%s_open"), *GetOwner()->GetName());
}

FString UAIDoorComponent::GetCloseSoundId() const
{
	// Fixed ID based on owner name - always the same for close
	return FString::Printf(TEXT("door_%s_close"), *GetOwner()->GetName());
}

bool UAIDoorComponent::IsOpenSoundCached() const
{
	FString Id = GetOpenSoundId();
	return CachedSounds.Contains(Id) || (AudioManager && AudioManager->IsSoundCached(Id));
}

bool UAIDoorComponent::IsCloseSoundCached() const
{
	FString Id = GetCloseSoundId();
	return CachedSounds.Contains(Id) || (AudioManager && AudioManager->IsSoundCached(Id));
}

void UAIDoorComponent::PreCacheSounds()
{
	// Pre-cache BOTH open and close sounds
	PreCacheSoundInternal(OpenPrompt, GetOpenSoundId());
	PreCacheSoundInternal(ClosePrompt, GetCloseSoundId());
}

void UAIDoorComponent::PreCacheSoundInternal(const FString& Prompt, const FString& SoundId)
{
	if (!AudioManager) return;

	// Already cached or generating?
	if (CachedSounds.Contains(SoundId) || GeneratingSounds.Contains(SoundId))
	{
		return;
	}

	// Also check AudioManager cache
	if (AudioManager->IsSoundCached(SoundId))
	{
		CachedSounds.Add(SoundId);
		return;
	}

	// Check connection
	if (!AudioManager->IsConnected())
	{
		UE_LOG(LogTemp, Warning, TEXT("AIDoorComponent: Not connected, will retry pre-cache"));
		return;
	}

	GeneratingSounds.Add(SoundId);

	UE_LOG(LogTemp, Log, TEXT("AIDoorComponent: Pre-caching '%s' (ID: %s)"), *Prompt, *SoundId);

	AudioManager->RequestSound(Prompt, SoundId, SoundDuration, AIModel);
}

void UAIDoorComponent::PlayOpenSound()
{
	PlaySoundInternal(OpenPrompt, GetOpenSoundId());
}

void UAIDoorComponent::PlayCloseSound()
{
	PlaySoundInternal(ClosePrompt, GetCloseSoundId());
}

void UAIDoorComponent::PlaySoundInternal(const FString& Prompt, const FString& SoundId)
{
	if (!AudioManager || !AudioComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("AIDoorComponent: Cannot play - not initialized"));
		return;
	}

	// Check if already cached
	if (CachedSounds.Contains(SoundId) || AudioManager->IsSoundCached(SoundId))
	{
		UE_LOG(LogTemp, Log, TEXT("AIDoorComponent: Playing cached sound '%s'"), *SoundId);
		AudioManager->PlaySound(SoundId, AudioComponent);
	}
	// Check if already being generated (pre-gen in progress) - DON'T play late, skip it!
	else if (GeneratingSounds.Contains(SoundId))
	{
		// Don't queue for later playback - just skip to avoid delayed/confusing audio
		UE_LOG(LogTemp, Warning, TEXT("AIDoorComponent: Sound '%s' not ready yet - SKIPPING (no delayed playback)"), *SoundId);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("AIDoorComponent: Sound not cached, generating '%s' now..."), *Prompt);

		// Remember that we want to play this sound once ready
		PendingPlaySoundId = SoundId;
		GeneratingSounds.Add(SoundId);
		AudioManager->RequestSound(Prompt, SoundId, SoundDuration, AIModel);
	}
}

void UAIDoorComponent::OnSoundReady(const FString& SoundId, USoundWaveProcedural* Sound)
{
	// Check if this is our sound
	if (SoundId != GetOpenSoundId() && SoundId != GetCloseSoundId())
		return;

	GeneratingSounds.Remove(SoundId);
	CachedSounds.Add(SoundId);

	FString Prompt = (SoundId == GetOpenSoundId()) ? OpenPrompt : ClosePrompt;

	UE_LOG(LogTemp, Log, TEXT("AIDoorComponent: Sound ready '%s' (Duration: %.2fs)"),
		*SoundId, Sound ? Sound->Duration : 0.0f);

	// NOTE: Do NOT auto-play delayed sounds here
	// If the door was already opened before sound was ready, we skip playback
	// This avoids confusing delayed audio after the visual action already happened
}
