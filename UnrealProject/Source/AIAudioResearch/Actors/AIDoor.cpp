// Copyright 2025 Igor Szuniewicz. Audio Generation Research Project.

#include "AIDoor.h"
#include "Audio/AIDoorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "Engine/Engine.h"

AAIDoor::AAIDoor()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create root scene component
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	
	// Frame mesh (static) - thin rectangle around door
	FrameMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrameMesh"));
	FrameMesh->SetupAttachment(RootComponent);
	
	// Load default cube mesh
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		// Frame: vertical post at hinge side
		FrameMesh->SetStaticMesh(CubeMesh.Object);
		FrameMesh->SetRelativeScale3D(FVector(0.1f, 0.1f, 2.0f));  // Thin post
		FrameMesh->SetRelativeLocation(FVector(0, -5, 100));  // At hinge edge
	}
	
	// Door pivot - THIS is what rotates (positioned at hinge edge)
	DoorPivot = CreateDefaultSubobject<USceneComponent>(TEXT("DoorPivot"));
	DoorPivot->SetupAttachment(RootComponent);
	DoorPivot->SetRelativeLocation(FVector(0, 0, 0));  // At hinge
	
	// Door mesh - attached to pivot, offset so hinge is at edge
	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(DoorPivot);  // Attached to PIVOT!
	
	if (CubeMesh.Succeeded())
	{
		DoorMesh->SetStaticMesh(CubeMesh.Object);
		DoorMesh->SetRelativeScale3D(FVector(0.05f, 0.9f, 1.8f));  // Door panel
		// Offset so mesh center is away from pivot (door swings from edge)
		DoorMesh->SetRelativeLocation(FVector(0, 45, 90));  // Y offset = half door width
	}
	
	// Interaction zone - small, just for E-key detection (pre-gen is via AILookPreGenComponent)
	InteractionZone = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionZone"));
	InteractionZone->SetupAttachment(RootComponent);
	InteractionZone->SetBoxExtent(FVector(100, 100, 100));  // Just for interaction
	InteractionZone->SetRelativeLocation(FVector(0, 50, 100));
	InteractionZone->SetCollisionProfileName(TEXT("Trigger"));
	InteractionZone->SetHiddenInGame(false);
	InteractionZone->ShapeColor = FColor::Yellow;
	
	// AI Audio component
	DoorAudio = CreateDefaultSubobject<UAIDoorComponent>(TEXT("DoorAudio"));
}

void AAIDoor::BeginPlay()
{
	Super::BeginPlay();
	
	// Store rotations for the PIVOT (not mesh)
	if (DoorPivot)
	{
		ClosedRotation = DoorPivot->GetRelativeRotation();
		OpenRotation = ClosedRotation + FRotator(0, OpenAngle, 0);
	}
	
	// Bind overlap events
	InteractionZone->OnComponentBeginOverlap.AddDynamic(this, &AAIDoor::OnInteractionBeginOverlap);
	InteractionZone->OnComponentEndOverlap.AddDynamic(this, &AAIDoor::OnInteractionEndOverlap);
	
	// Set prompts on the audio component (they have defaults but can be overridden in AAIDoor)
	if (DoorAudio)
	{
		DoorAudio->OpenPrompt = OpenSoundPrompt;
		DoorAudio->ClosePrompt = CloseSoundPrompt;
	}
	
	UE_LOG(LogTemp, Log, TEXT("AIDoor: '%s' ready (OpenAngle: %.0f, Duration: %.2fs)"), 
		*GetName(), OpenAngle, OpenDuration);
}

void AAIDoor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Smooth door animation
	if (CurrentState == EDoorState::Opening || CurrentState == EDoorState::Closing)
	{
		float Speed = 1.0f / OpenDuration;
		
		if (CurrentState == EDoorState::Opening)
		{
			CurrentAlpha = FMath::Min(CurrentAlpha + Speed * DeltaTime, 1.0f);
			if (CurrentAlpha >= 1.0f)
			{
				CurrentState = EDoorState::Open;
			}
		}
		else // Closing
		{
			CurrentAlpha = FMath::Max(CurrentAlpha - Speed * DeltaTime, 0.0f);
			if (CurrentAlpha <= 0.0f)
			{
				CurrentState = EDoorState::Closed;
			}
		}
		
		UpdateDoorRotation(CurrentAlpha);
	}
	
	// Check for E key input when player is in zone
	if (bCanInteract && PlayerInZone)
	{
		ACharacter* Character = Cast<ACharacter>(PlayerInZone);
		if (Character && Character->IsLocallyControlled())
		{
			APlayerController* PC = Cast<APlayerController>(Character->GetController());
			if (PC && PC->WasInputKeyJustPressed(EKeys::E))
			{
				ToggleDoor();
			}
		}
	}
}

void AAIDoor::ToggleDoor()
{
	if (CurrentState == EDoorState::Closed || CurrentState == EDoorState::Closing)
	{
		OpenDoor();
	}
	else
	{
		CloseDoor();
	}
}

void AAIDoor::OpenDoor()
{
	if (CurrentState == EDoorState::Open || CurrentState == EDoorState::Opening)
		return;
	
	CurrentState = EDoorState::Opening;
	
	// Play OPEN sound
	if (DoorAudio)
	{
		DoorAudio->PlayOpenSound();
	}
	
	UE_LOG(LogTemp, Log, TEXT("AIDoor: Opening '%s'"), *GetName());
	
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(910, 2.0f, FColor::Cyan, 
			TEXT("Door: OPENING"));
	}
}

void AAIDoor::CloseDoor()
{
	if (CurrentState == EDoorState::Closed || CurrentState == EDoorState::Closing)
		return;
	
	CurrentState = EDoorState::Closing;
	
	// Play CLOSE sound
	if (DoorAudio)
	{
		DoorAudio->PlayCloseSound();
	}
	
	UE_LOG(LogTemp, Log, TEXT("AIDoor: Closing '%s'"), *GetName());
	
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(910, 2.0f, FColor::Yellow, 
			TEXT("Door: CLOSING"));
	}
}

void AAIDoor::UpdateDoorRotation(float Alpha)
{
	if (DoorPivot)
	{
		FRotator NewRotation = FMath::Lerp(ClosedRotation, OpenRotation, Alpha);
		DoorPivot->SetRelativeRotation(NewRotation);  // Rotate PIVOT, not mesh!
	}
}

void AAIDoor::OnInteractionBeginOverlap(UPrimitiveComponent* OverlappedComponent, 
	AActor* OtherActor, UPrimitiveComponent* OtherComp, 
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor->IsA<ACharacter>())
	{
		bCanInteract = true;
		PlayerInZone = OtherActor;
		OnPlayerEnterZone(OtherActor);
		
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(911, 0.0f, FColor::White, 
				TEXT("Press [E] to open/close door"));
		}
	}
}

void AAIDoor::OnInteractionEndOverlap(UPrimitiveComponent* OverlappedComponent, 
	AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor == PlayerInZone)
	{
		bCanInteract = false;
		PlayerInZone = nullptr;
		OnPlayerExitZone(OtherActor);
	}
}

void AAIDoor::OnPlayerEnterZone_Implementation(AActor* Player)
{
	// Pre-cache BOTH open and close sounds when player approaches
	if (DoorAudio)
	{
		DoorAudio->PreCacheSounds();
	}
}

void AAIDoor::OnPlayerExitZone_Implementation(AActor* Player)
{
	// Override in Blueprint if needed
}
