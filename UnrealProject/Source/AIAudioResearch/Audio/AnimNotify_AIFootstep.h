// Copyright 2025 Igor Szuniewicz. Audio Generation Research Project.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_AIFootstep.generated.h"

/**
 * AnimNotify that plays AI-generated footstep sounds
 * Add this to your walk/run animation at foot contact frames
 */
UCLASS()
class AIAUDIORESEARCH_API UAnimNotify_AIFootstep : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	
	virtual FString GetNotifyName_Implementation() const override { return TEXT("AI Footstep"); }
};
