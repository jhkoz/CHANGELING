// GA_InstantCantrip.cpp

#include "GA_InstantCantrip.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

DEFINE_LOG_CATEGORY_STATIC(LogInstantCantrip, Log, All);

void UGA_InstantCantrip::PostResolve(const FCantripOutcome& Outcome, const FCantripSpec& Spec)
{
	// A failed working leaves nothing behind. Fall through to the base, which ends the
	// ability -- the Glamour is still spent, which is the whole point of paying before
	// the roll rather than after it.
	if (!Outcome.bSucceeded)
	{
		Super::PostResolve(Outcome, Spec);
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();

	if (World && Avatar)
	{
		const float Scale = BaseEffectScale + (ScalePerSuccess * FMath::Max(0, Outcome.Successes - 1));

		if (ImpactEffect)
		{
			// Oriented to the surface rather than to the caster: a scorch, a bloom of
			// frost or a spray of sparks all read as belonging to what was struck, and a
			// world-facing effect standing upright on a wall does not.
			UNiagaraComponent* Spawned = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World, ImpactEffect, ResolvedTargets.Point,
				FRotationMatrix::MakeFromZ(ResolvedTargets.Normal).Rotator(),
				FVector(FMath::Max(0.01f, Scale)), /*bAutoDestroy*/ true);

			// The roll carried all the way to the end, so a system authored to read it can
			// spawn fewer motes as well as smaller ones.
			if (Spawned && !SuccessParameter.IsNone())
			{
				Spawned->SetVariableFloat(SuccessParameter,
					static_cast<float>(Outcome.Successes));
			}
		}

		if (ImpactSound)
		{
			UGameplayStatics::PlaySoundAtLocation(World, ImpactSound, ResolvedTargets.Point);
		}

		if (CastMontage)
		{
			if (const ACharacter* Character = Cast<ACharacter>(Avatar))
			{
				// Played directly on the mesh rather than through an ability task. The
				// cantrip has already resolved; waiting on the montage would hold the
				// ability open through a flourish that changes nothing.
				if (USkeletalMeshComponent* Mesh = Character->GetMesh())
				{
					if (UAnimInstance* Anim = Mesh->GetAnimInstance())
					{
						Anim->Montage_Play(CastMontage, MontagePlayRate);
					}
				}
			}
		}
	}

	UE_LOG(LogInstantCantrip, Log, TEXT("LANDED %s: %d successes at %s%s"),
		*GetName(), Outcome.Successes, *ResolvedTargets.Point.ToCompactString(),
		ResolvedTargets.Primary() ? *FString::Printf(TEXT(" on %s"), *GetNameSafe(ResolvedTargets.Primary()))
							  : TEXT(""));

	OnCantripLanded(Outcome, ResolvedTargets.Point, ResolvedTargets.Primary());

	Super::PostResolve(Outcome, Spec);
}
