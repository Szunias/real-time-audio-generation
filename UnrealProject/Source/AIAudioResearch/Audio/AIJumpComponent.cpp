// Copyright 2025 Igor Szuniewicz. Audio Generation Research Project.

#include "AIJumpComponent.h"
#include "AIAudioManager.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Sound/SoundWaveProcedural.h"
#include "TimerManager.h"

UAIJumpComponent::UAIJumpComponent()
{
	PrimaryComponentTick.bCanEverTick = true; // Need tick for state detection
}

void UAIJumpComponent::BeginPlay()
{
	Super::BeginPlay();

	// Get Character reference
	CharacterOwner = Cast<ACharacter>(GetOwner());
	if (!CharacterOwner)
	{
		UE_LOG(LogTemp, Warning, TEXT("AIJumpComponent: Owner is not a Character!"));
		SetComponentTickEnabled(false);
		return;
	}

	// Create audio manager
	AudioManager = NewObject<UAudioManager>(this);
	
	if (AudioManager)
	{
		// Bind events
		AudioManager->OnSoundReady.AddDynamic(this, &UAIJumpComponent::OnSoundReady);
		
		// Connect to server
		AudioManager->Initialize(ServerURL);
		
		// Wait for connection safe timer
		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &UAIJumpComponent::TryPreGenerate, 1.0f, false);
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
}

void UAIJumpComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
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

void UAIJumpComponent::TryPreGenerate()
{
	if (!AudioManager) return;
	
	if (AudioManager->IsConnected())
	{
		PreGenerateSounds();
	}
	else
	{
		FTimerHandle RetryHandle;
		GetWorld()->GetTimerManager().SetTimer(RetryHandle, this, &UAIJumpComponent::TryPreGenerate, 1.0f, false);
	}
}

void UAIJumpComponent::PreGenerateSounds()
{
	if (!AudioManager || bSoundsRequested) return;
	
	bSoundsRequested = true;
	UE_LOG(LogTemp, Log, TEXT("AIJumpComponent: Pre-generating jump/land sounds..."));
	
	// Request both sounds
	AudioManager->RequestSound(JumpPrompt, TEXT("jump_01"), 1.0f, AIModel);
	AudioManager->RequestSound(LandPrompt, TEXT("land_01"), 1.0f, AIModel);
}

void UAIJumpComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if (!CharacterOwner) return;
	
	CheckMovementState();
}

void UAIJumpComponent::CheckMovementState()
{
	if (!CharacterOwner->GetCharacterMovement()) return;
	
	bool bIsFalling = CharacterOwner->GetCharacterMovement()->IsFalling();
	
	// Detect Jump (Start Falling)
	if (bIsFalling && !bWasFalling)
	{
		// Only trigger jump sound if we have upward velocity (actual jump, not just walking off ledge)
		if (CharacterOwner->GetVelocity().Z > 100.0f)
		{
			OnJump();
		}
	}
	// Detect Land (Stop Falling)
	else if (!bIsFalling && bWasFalling)
	{
		OnLand();
	}
	
	bWasFalling = bIsFalling;
}

void UAIJumpComponent::OnJump()
{
	if (AudioManager && AudioComponent)
	{
		UE_LOG(LogTemp, Log, TEXT("AIJumpComponent: Jump detected -> Playing Jump Sound"));
		AudioManager->PlaySound(TEXT("jump_01"), AudioComponent);
	}
}

void UAIJumpComponent::OnLand()
{
	if (AudioManager && AudioComponent)
	{
		UE_LOG(LogTemp, Log, TEXT("AIJumpComponent: Land detected -> Playing Land Sound"));
		AudioManager->PlaySound(TEXT("land_01"), AudioComponent);
	}
}

void UAIJumpComponent::OnSoundReady(const FString& SoundId, USoundWaveProcedural* Sound)
{
	UE_LOG(LogTemp, Log, TEXT("AIJumpComponent: Sound Ready: %s (Duration: %.2fs)"), *SoundId, Sound ? Sound->Duration : 0.0f);
	
	// We don't auto-play, we wait for events
}
