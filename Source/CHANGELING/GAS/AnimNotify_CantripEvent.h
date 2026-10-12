// AnimNotify_CantripEvent.h
//
// Sends a gameplay event from a point in an animation.
//
// The alternative is timing the moment in code -- "release at 0.4 seconds" -- which
// works right up until somebody retimes the animation, and then the fireball leaves a
// hand that has not swung yet. Only the animation knows where its own apex is, so the
// animation is what says when.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "AnimNotify_CantripEvent.generated.h"

UCLASS(meta = (DisplayName = "Cantrip Event"))
class CHANGELING_API UAnimNotify_CantripEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_CantripEvent();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

	/**
	 * Which event to send. Defaults to the release tag, since that is what a thrown
	 * cantrip needs and typing a tag by hand for every montage is how they end up
	 * misspelled.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cantrip")
	FGameplayTag EventTag;
};
