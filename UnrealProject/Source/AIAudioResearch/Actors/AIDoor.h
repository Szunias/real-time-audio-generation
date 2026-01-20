// Copyright 2025 Igor Szuniewicz. Audio Generation Research Project.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AIDoor.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UAIDoorComponent;
class UTimelineComponent;
class UCurveFloat;

UENUM(BlueprintType)
enum class EDoorState : uint8
{
	Closed,
	Opening,
	Open,
	Closing
};

/**
 * Interactive door with AI-generated sound effects.
 * Create Blueprint child class and assign mesh + animations.
 */
UCLASS(Blueprintable)
class AIAUDIORESEARCH_API AAIDoor : public AActor
{
	GENERATED_BODY()
	
public:	
	AAIDoor();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	// ============ COMPONENTS ============
	
	/** Door frame (static) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* FrameMesh;
	
	/** Pivot point for door rotation (at hinge) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* DoorPivot;
	
	/** Door panel (attached to pivot, rotates with it) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* DoorMesh;
	
	/** Interaction trigger zone */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* InteractionZone;
	
	/** AI Audio component for door sounds */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAIDoorComponent* DoorAudio;

	// ============ SETTINGS ============
	
	/** How far the door rotates (degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door Settings")
	float OpenAngle = 90.0f;
	
	/** How fast the door opens (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door Settings")
	float OpenDuration = 0.5f;
	
	/** Sound prompt for opening */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door Settings|Audio")
	FString OpenSoundPrompt = TEXT("wooden door creaking open");
	
	/** Sound prompt for closing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door Settings|Audio")
	FString CloseSoundPrompt = TEXT("wooden door closing shut");

	// ============ STATE ============
	
	UPROPERTY(BlueprintReadOnly, Category = "Door State")
	EDoorState CurrentState = EDoorState::Closed;
	
	UPROPERTY(BlueprintReadOnly, Category = "Door State")
	bool bCanInteract = false;

	// ============ FUNCTIONS ============
	
	/** Toggle door open/closed */
	UFUNCTION(BlueprintCallable, Category = "Door")
	void ToggleDoor();
	
	/** Open the door */
	UFUNCTION(BlueprintCallable, Category = "Door")
	void OpenDoor();
	
	/** Close the door */
	UFUNCTION(BlueprintCallable, Category = "Door")
	void CloseDoor();
	
	/** Called when player enters interaction zone */
	UFUNCTION(BlueprintNativeEvent, Category = "Door")
	void OnPlayerEnterZone(AActor* Player);
	
	/** Called when player exits interaction zone */
	UFUNCTION(BlueprintNativeEvent, Category = "Door")
	void OnPlayerExitZone(AActor* Player);

protected:
	UFUNCTION()
	void OnInteractionBeginOverlap(UPrimitiveComponent* OverlappedComponent, 
		AActor* OtherActor, UPrimitiveComponent* OtherComp, 
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	
	UFUNCTION()
	void OnInteractionEndOverlap(UPrimitiveComponent* OverlappedComponent, 
		AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
	
	void UpdateDoorRotation(float Alpha);

private:
	FRotator ClosedRotation;
	FRotator OpenRotation;
	float CurrentAlpha = 0.0f;
	float TargetAlpha = 0.0f;
	
	UPROPERTY()
	AActor* PlayerInZone;
};
