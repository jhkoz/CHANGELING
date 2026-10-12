// CantripPayload_Effect.cpp

#include "CantripPayload_Effect.h"

#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GA_Cantrip.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCantripEffect, Log, All);

void UCantripPayload_Effect::WriteScalars(UNiagaraComponent* Component,
	const FCantripEffectAttachment& Attachment, float Value, float Fade)
{
	if (!Component)
	{
		return;
	}

	if (!Attachment.ScalarParameter.IsNone())
	{
		Component->SetVariableFloat(Attachment.ScalarParameter, Value);
	}

	if (!Attachment.FadeParameter.IsNone())
	{
		Component->SetVariableFloat(Attachment.FadeParameter, Fade);
	}
}

void UCantripPayload_Effect::Apply(UGA_Cantrip* Ability, const FCantripOutcome& Outcome,
	const FCantripTargets& /*Targets*/)
{
	if (!Ability)
	{
		return;
	}

	ACharacter* Caster = Cast<ACharacter>(Ability->GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* Mesh = Caster ? Caster->GetMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}

	// The capsule, for anything that must stay upright. See bAttachToMesh: the mesh leans
	// in flight and the capsule never does, so a ground effect on a mesh socket ends up
	// tilted by however fast she happens to be going.
	USceneComponent* Capsule = Caster->GetRootComponent();

	// Left over from a previous casting that never reverted -- a cancel that bypassed the
	// normal path, or an avatar that died mid-working. Cleared rather than added to, or a
	// cantrip spammed on and off accumulates a system per press and the emission rate is
	// decided by how fast the player can press the key.
	Revert(Ability);

	// Base at ONE success plus a step for each after -- not a straight multiply by the
	// count, which would make a marginal success invisible and a good one absurd.
	const int32 Extra = FMath::Max(0, Outcome.Successes - 1);

	for (const FCantripEffectAttachment& Attachment : Attachments)
	{
		if (!Attachment.System)
		{
			continue;
		}

		// KeepRelativeOffset, NOT SnapToTarget. The Snap modes discard the location and
		// rotation arguments entirely and adopt the socket's own transform -- so the offset
		// silently did nothing, and the system inherited the hand bone's axes, which run
		// down the arm and across the palm rather than anywhere near world forward. An
		// effect authored to emit along +X arrived ninety degrees out, and nothing inside
		// the Niagara asset could correct it because the asset was never wrong.

		// Sockets belong to the skeleton, so one is only meaningful on the mesh. Passed as
		// None on the capsule rather than left to fail the way a misspelling would --
		// silently, at the component origin.
		USceneComponent* Target = Attachment.bAttachToMesh
			? static_cast<USceneComponent*>(Mesh) : Capsule;
		const FName Socket = Attachment.bAttachToMesh ? Attachment.Socket : NAME_None;

		if (!Target)
		{
			continue;
		}

		UNiagaraComponent* Component = UNiagaraFunctionLibrary::SpawnSystemAttached(
			Attachment.System, Target, Socket, Attachment.Offset,
			Attachment.Rotation, EAttachLocation::KeepRelativeOffset,
			/*bAutoDestroy*/ false, /*bAutoActivate*/ true);

		if (!Component)
		{
			continue;
		}

		// bAutoDestroy is FALSE on purpose. An auto-destroying component tears itself
		// down the moment emission stops, which is exactly the frame the graceful fade
		// below is trying to survive.
		const float Value = FMath::Max(0.0f,
			Attachment.ScalarAtOneSuccess + (Attachment.ScalarPerSuccess * Extra));

		WriteScalars(Component, Attachment, Value, 1.0f);

		Spawned.Add(Component);
	}

	if (LoopSound)
	{
		UAudioComponent* Audio = UGameplayStatics::SpawnSoundAttached(LoopSound, Mesh);
		if (Audio)
		{
			// From silence. FadeIn rather than setting the volume and playing, because a
			// looping sound that begins at full is heard as a click.
			Audio->FadeIn(SoundFadeInSeconds, Volume);
			SpawnedAudio = Audio;
		}
	}

	UE_LOG(LogCantripEffect, Log, TEXT("EFFECT: %d system(s) at %d success(es), sound %s"),
		Spawned.Num(), Outcome.Successes, LoopSound ? TEXT("yes") : TEXT("no"));
}

void UCantripPayload_Effect::Revert(UGA_Cantrip* Ability)
{
	UWorld* World = Ability ? Ability->GetWorld() : nullptr;

	if (UAudioComponent* Audio = SpawnedAudio.Get())
	{
		Audio->FadeOut(FadeOutSeconds, 0.0f);
	}
	SpawnedAudio = nullptr;

	// Taken by value: the timer below outlives this call, and Spawned is cleared before
	// it fires so a re-cast in the meantime cannot have its own systems destroyed by the
	// previous casting's fade.
	TArray<TWeakObjectPtr<UNiagaraComponent>> Fading = Spawned;
	Spawned.Reset();

	// Index-matched to Attachments, because each system's stop switch is named differently
	// and only its own attachment knows which. A component with no scalar at all falls
	// back to Deactivate, which stops emission one layer up and survives the tail equally.
	for (int32 Index = 0; Index < Fading.Num(); ++Index)
	{
		UNiagaraComponent* Component = Fading[Index].Get();
		if (!Component)
		{
			continue;
		}

		if (Attachments.IsValidIndex(Index)
			&& (!Attachments[Index].ScalarParameter.IsNone()
			 || !Attachments[Index].FadeParameter.IsNone()))
		{
			// Stop MAKING particles; do not destroy. The ones already in the air finish
			// their own lifetimes, so a cloud disperses instead of blinking out.
			WriteScalars(Component, Attachments[Index], 0.0f, 0.0f);
		}
		else
		{
			Component->Deactivate();
		}
	}

	if (!World || FadeOutSeconds <= 0.0f)
	{
		// Nowhere to hang a timer, or nothing to wait for. Destroy now rather than leak
		// -- an abrupt end is worse than a graceful one and far better than a permanent
		// system attached to a character for the rest of the session.
		for (const TWeakObjectPtr<UNiagaraComponent>& Weak : Fading)
		{
			if (UNiagaraComponent* Component = Weak.Get())
			{
				Component->DestroyComponent();
			}
		}
		return;
	}

	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle,
		FTimerDelegate::CreateWeakLambda(this, [Fading]()
		{
			for (const TWeakObjectPtr<UNiagaraComponent>& Weak : Fading)
			{
				if (UNiagaraComponent* Component = Weak.Get())
				{
					Component->DestroyComponent();
				}
			}
		}),
		FadeOutSeconds, /*bLoop*/ false);
}

FString UCantripPayload_Effect::Describe() const
{
	return FString::Printf(TEXT("Effect(%d system%s%s)"), Attachments.Num(),
		Attachments.Num() == 1 ? TEXT("") : TEXT("s"),
		LoopSound ? TEXT(" + sound") : TEXT(""));
}
