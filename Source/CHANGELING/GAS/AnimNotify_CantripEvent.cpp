// AnimNotify_CantripEvent.cpp

#include "AnimNotify_CantripEvent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "ChangelingGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogCantripNotify, Log, All);

UAnimNotify_CantripEvent::UAnimNotify_CantripEvent()
{
	EventTag = ChangelingTags::Cantrip_Event_Release;
}

void UAnimNotify_CantripEvent::Notify(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	// Each branch says which one it took. "The notify is set" and "the notify fired"
	// are different claims, and only one of them can be checked from a timeline.
	if (!MeshComp)
	{
		UE_LOG(LogCantripNotify, Warning, TEXT("NOTIFY fired with no mesh component."));
		return;
	}

	if (!EventTag.IsValid())
	{
		UE_LOG(LogCantripNotify, Warning,
			TEXT("NOTIFY fired on %s but its Event Tag is unset."),
			*GetNameSafe(Animation));
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		UE_LOG(LogCantripNotify, Warning,
			TEXT("NOTIFY '%s' fired but the mesh has no owner to send it to."),
			*EventTag.ToString());
		return;
	}

	UE_LOG(LogCantripNotify, Log, TEXT("NOTIFY '%s' -> %s (from %s)"),
		*EventTag.ToString(), *GetNameSafe(Owner), *GetNameSafe(Animation));

	// Sent to the OWNER rather than the mesh, because that is where the ability system
	// component lives and where a waiting ability is listening.
	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = Owner;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, EventTag, Payload);
}

FString UAnimNotify_CantripEvent::GetNotifyName_Implementation() const
{
	// Named by its tag in the timeline, so a montage with several notifies reads as a
	// sequence of events rather than a row of identical markers.
	return EventTag.IsValid() ? EventTag.ToString() : TEXT("Cantrip Event");
}
