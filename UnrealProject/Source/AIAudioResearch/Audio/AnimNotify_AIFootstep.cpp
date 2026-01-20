// Copyright 2025 Igor Szuniewicz. Audio Generation Research Project.

#include "AnimNotify_AIFootstep.h"
#include "AIFootstepComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void UAnimNotify_AIFootstep::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	
	if (!MeshComp)
	{
		return;
	}
	
	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}
	
	// Find AIFootstepComponent on the owner
	UAIFootstepComponent* FootstepComp = Owner->FindComponentByClass<UAIFootstepComponent>();
	if (FootstepComp)
	{
		FootstepComp->PlayFootstep();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimNotify_AIFootstep: No AIFootstepComponent found on %s"), *Owner->GetName());
	}
}
