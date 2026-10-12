// GA_SustainedCantrip.cpp

#include "GA_SustainedCantrip.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Animation/AnimMontage.h"
#include "CHANGELINGCharacter.h"
#include "CantripBurnComponent.h"
#include "CantripInternal.h"
#include "ChangelingAnimInstance.h"
#include "ChangelingAttributeSet.h"
#include "ChangelingGameplayTags.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSustainedCantrip, Log, All);

using CantripInternal::ResolveASC;

UGA_SustainedCantrip::UGA_SustainedCantrip()
{
	// Re-pressing must not stack a second copy on top of the running one; the toggle
	// below decides whether a press starts or stops it.
	bRetriggerInstancedAbility = false;
}

bool UGA_SustainedCantrip::ToggleSustainedCantrip(AActor* Avatar,
	TSubclassOf<UGA_SustainedCantrip> CantripClass)
{
	if (!CantripClass)
	{
		return false;
	}

	// Running? Then this press is the "off" half. Asking the ability itself means
	// there is no separate bool to fall out of step with it.
	if (DouseSustainedCantrips(Avatar, CantripClass) > 0)
	{
		UE_LOG(LogSustainedCantrip, Log, TEXT("TOGGLE %s: was sustaining -> doused"),
			*CantripClass->GetName());
		return false;
	}

	UAbilitySystemComponent* ASC = ResolveASC(Avatar);
	const bool bActivated = ASC ? ASC->TryActivateAbilityByClass(CantripClass) : false;

	UE_LOG(LogSustainedCantrip, Log, TEXT("TOGGLE %s: not sustaining -> TryActivate=%d (asc=%d)"),
		*CantripClass->GetName(), bActivated ? 1 : 0, ASC ? 1 : 0);

	return bActivated;
}

int32 UGA_SustainedCantrip::DouseSustainedCantrips(AActor* Avatar,
	TSubclassOf<UGA_SustainedCantrip> CantripClass)
{
	UAbilitySystemComponent* ASC = ResolveASC(Avatar);
	if (!ASC)
	{
		return 0;
	}

	// Gather BEFORE ending any of them. Ending an ability mutates the activatable
	// list, and cancelling mid-iteration would walk a container being rewritten.
	// Weak, because cancelling one can cascade -- an OnSustainEnded handler is free to
	// douse the rest -- and a raw pointer gathered a moment ago may already be gone.
	TArray<TWeakObjectPtr<UGA_SustainedCantrip>> Running;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.IsActive() || !Spec.Ability)
		{
			continue;
		}

		if (CantripClass && !Spec.Ability->IsA(CantripClass))
		{
			continue;
		}

		for (UGameplayAbility* Instance : Spec.GetAbilityInstances())
		{
			UGA_SustainedCantrip* Cantrip = Cast<UGA_SustainedCantrip>(Instance);
			if (Cantrip && Cantrip->IsSustaining())
			{
				Running.Add(Cantrip);
			}
		}
	}

	int32 Doused = 0;
	for (const TWeakObjectPtr<UGA_SustainedCantrip>& Cantrip : Running)
	{
		// Re-check IsSustaining as well as validity: a previous cancel's
		// OnSustainEnded may already have put this one out, and counting it twice
		// would misreport how much was interrupted.
		if (Cantrip.IsValid() && Cantrip->IsSustaining())
		{
			Cantrip->CancelSustain();
			++Doused;
		}
	}

	return Doused;
}

void UGA_SustainedCantrip::InputPressed(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);

	// Only once the working is actually open. A press arriving during the wind-up is the
	// player still casting, not the player changing their mind.
	if (bDouseOnSecondPress && bSustaining)
	{
		CancelSustain();
	}
}

void UGA_SustainedCantrip::CancelSustain()
{
	if (!bSustaining)
	{
		return;
	}

	// bRanOut false: this was cut short, not spent. The distinction reaches Blueprint
	// so a light snuffed by a blow can look different from one that simply burned out.
	StopSustain(/*bRanOut*/ false);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_SustainedCantrip::PostResolve(const FCantripOutcome& Outcome, const FCantripSpec& Spec)
{
	// A failed working never opens. Fall through to the base, which ends the ability,
	// so a botched torch costs the Glamour and gives nothing -- which is the point of
	// spending before the roll.
	if (!Outcome.bSucceeded)
	{
		Super::PostResolve(Outcome, Spec);
		return;
	}

	BeginSustain(Outcome.Successes);
}

float UGA_SustainedCantrip::DurationForSuccesses(int32 Successes) const
{
	if (DurationBySuccesses.Num() == 0)
	{
		return 0.0f;
	}

	// Clamp rather than index blindly: a roll can beat the table's length, and the
	// top entry is the intended ceiling, not an invitation to read past it.
	const int32 Index = FMath::Clamp(Successes - 1, 0, DurationBySuccesses.Num() - 1);
	return DurationBySuccesses[Index];
}

float UGA_SustainedCantrip::GetRemainingSeconds() const
{
	const UWorld* World = GetWorld();
	if (!World || !bSustaining)
	{
		return 0.0f;
	}
	return FMath::Max(0.0f, World->GetTimerManager().GetTimerRemaining(DurationTimer));
}

void UGA_SustainedCantrip::HandleExpired()
{
	StopSustain(/*bRanOut*/ true);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_SustainedCantrip::BeginSustain(int32 Successes)
{
	bSustaining = true;
	SustainSuccesses = FMath::Max(1, Successes);

	// The working is lit, so she has stopped CASTING it -- and the casting tag is what
	// stops her casting anything else. Left alone, a torch held up for two minutes would
	// be two minutes of no other magic.
	//
	// This used to remove the tag by hand and put it back in EndAbility, so that the
	// engine's own unconditional removal of an ActivationOwnedTag still had a count of
	// its own to take. That balancing act is gone: the base class no longer OWNS the tag,
	// it takes and drops it deliberately, and DropCastingTag is idempotent -- so this is
	// now simply the moment her hands come free, said once.
	DropCastingTag();

	UE_LOG(LogSustainedCantrip, Log, TEXT("LIT: successes=%d duration=%.1fs pose=%d"),
		Successes, DurationForSuccesses(Successes), static_cast<int32>(SustainedPose));

	if (ACHANGELINGCharacter* Character = Cast<ACHANGELINGCharacter>(GetAvatarActorFromActorInfo()))
	{
		// One of the two drives the arm, never both. A montage raising the hand while the
		// pose blend also raises it gives a doubled rotation -- an arm bent somewhere no
		// animator put it, and the montage gets the blame because it is the visible half.
		Character->SetCantripPose(SustainedMontage ? ECantripPose::None : SustainedPose);
	}

	// Only if the wind-up has not already started it. A channelled cantrip plays its
	// Begin section from ActivateAbility, so the gesture accompanies the HOLD instead of
	// arriving after the roll -- replaying here would restart the raise on the frame the
	// working opens, which is what made the whole cast read as late.
	if (SustainedMontage && !bSustainedMontageStarted)
	{
		StartSustainedMontage();
	}

	FinishBeginSustain(Successes);
}

void UGA_SustainedCantrip::StartSustainedMontage()
{
	bSustainedMontageStarted = true;

	if (ACHANGELINGCharacter* Character = Cast<ACHANGELINGCharacter>(GetAvatarActorFromActorInfo()))
	{
		// One of the two drives the arms, never both -- and now that this can run during
		// the wind-up, the pose has to be cleared here rather than waiting for the
		// working to open. A montage raising the hands while the pose blend also raises
		// them gives a doubled rotation, and the montage gets the blame.
		Character->SetCantripPose(ECantripPose::None);
	}

	if (SustainedMontage)
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			const float PlayedLength = ASC->PlayMontage(this, CurrentActivationInfo,
				SustainedMontage, MontagePlayRate, MontageBeginSection);

			/**
			 * Every way this can fail is silent, so all of it is reported at once.
			 *
			 * A length of zero means the montage never started -- wrong skeleton, or the
			 * ability was not the one animating. A section index of -1 means the name on
			 * this ability does not match the asset, and SetNextSectionName below will
			 * quietly do nothing, so the montage plays through and blends out instead of
			 * holding. And a slot the AnimGraph has no node for evaluates into nothing at
			 * all: the montage runs, the log looks healthy, and the body never moves.
			 */
			const int32 BeginIdx = SustainedMontage->GetSectionIndex(MontageBeginSection);
			const int32 LoopIdx  = SustainedMontage->GetSectionIndex(MontageLoopSection);
			const int32 EndIdx   = SustainedMontage->GetSectionIndex(MontageEndSection);

			const FName SlotName = SustainedMontage->SlotAnimTracks.Num() > 0
				? SustainedMontage->SlotAnimTracks[0].SlotName
				: NAME_None;

			UE_LOG(LogSustainedCantrip, Log,
				TEXT("MONTAGE '%s': played=%.2fs slot='%s' sections begin='%s'(%d) loop='%s'(%d) end='%s'(%d)"),
				*GetNameSafe(SustainedMontage), PlayedLength, *SlotName.ToString(),
				*MontageBeginSection.ToString(), BeginIdx,
				*MontageLoopSection.ToString(), LoopIdx,
				*MontageEndSection.ToString(), EndIdx);

			if (PlayedLength <= 0.0f || BeginIdx == INDEX_NONE || LoopIdx == INDEX_NONE)
			{
				UE_LOG(LogSustainedCantrip, Warning,
					TEXT("MONTAGE '%s' will not hold: %s"), *GetNameSafe(SustainedMontage),
					PlayedLength <= 0.0f ? TEXT("it did not start at all")
						: TEXT("a section name does not match the asset"));
			}

			if (!MontageLoopSection.IsNone())
			{
				// Set after Play, not before: the section chain is per-playback, and
				// starting the montage resets it to whatever the asset says.
				ASC->CurrentMontageSetNextSectionName(MontageBeginSection, MontageLoopSection);
				ASC->CurrentMontageSetNextSectionName(MontageLoopSection, MontageLoopSection);

				if (bFreezeOnLoopSection)
				{
					// Timed off the ASSET's own section boundaries rather than a number
					// typed beside them, so retiming the montage moves the freeze with it.
					const int32 BeginIndex = SustainedMontage->GetSectionIndex(MontageBeginSection);
					const int32 LoopIndex = SustainedMontage->GetSectionIndex(MontageLoopSection);

					if (BeginIndex != INDEX_NONE && LoopIndex != INDEX_NONE)
					{
						float BeginStart = 0.0f, BeginEnd = 0.0f;
						float LoopStart = 0.0f, LoopEnd = 0.0f;

						SustainedMontage->GetSectionStartAndEndTime(BeginIndex, BeginStart, BeginEnd);
						SustainedMontage->GetSectionStartAndEndTime(LoopIndex, LoopStart, LoopEnd);

						// A hair past the boundary, so the pause lands inside the loop
						// section rather than on the last frame of the raise.
						const float Rate = FMath::Max(0.01f, MontagePlayRate);
						const float Wait = ((LoopStart - BeginStart) / Rate) + 0.01f;

						if (UWorld* World = GetWorld())
						{
							World->GetTimerManager().SetTimer(FreezeTimer, this,
								&UGA_SustainedCantrip::FreezeSustainedMontage,
								FMath::Max(0.01f, Wait), false);
						}
					}
				}
			}
		}
	}
}

void UGA_SustainedCantrip::FinishBeginSustain(int32 Successes)
{
	// The roll is known NOW. Anything a channelled cantrip already gathered during the
	// wind-up was sized against a placeholder, so it gets resized here; anything spawning
	// later reads the same values and is unaffected by this running early.
	ApplySuccessScaling();

	UWorld* World = GetWorld();

	ScheduleEffectSpawn();

	if (SustainedGameplayEffect)
	{
		FGameplayEffectSpecHandle EffectSpec =
			MakeOutgoingGameplayEffectSpec(SustainedGameplayEffect, GetAbilityLevel());
		if (EffectSpec.IsValid())
		{
			SustainedEffectHandle = ApplyGameplayEffectSpecToOwner(CurrentSpecHandle,
				CurrentActorInfo, CurrentActivationInfo, EffectSpec);
		}
	}

	const float Duration = DurationForSuccesses(Successes);
	if (World && Duration > 0.0f)
	{
		World->GetTimerManager().SetTimer(DurationTimer,
			FTimerDelegate::CreateUObject(this, &UGA_SustainedCantrip::HandleExpired),
			Duration, false);
	}

	OnSustainBegan();
}

void UGA_SustainedCantrip::ScheduleEffectSpawn()
{
	UWorld* World = GetWorld();

	// The hand has to arrive before the fire does. Lighting the flame on the same
	// frame the arm starts rising reads as the flame dragging the arm up after it.
	if (World && EffectSpawnDelay > 0.0f)
	{
		World->GetTimerManager().SetTimer(SpawnDelayTimer,
			FTimerDelegate::CreateUObject(this, &UGA_SustainedCantrip::SpawnSustainedEffect),
			EffectSpawnDelay, false);
	}
	else
	{
		SpawnSustainedEffect();
	}
}

void UGA_SustainedCantrip::ApplySuccessScaling()
{
	/**
	 * Applied wherever it is called from, because the effect can now outlive the moment
	 * the successes are known.
	 *
	 * A channelled cantrip GATHERS: the flame spawns during the wind-up, before the roll
	 * has happened, when SustainSuccesses is still its placeholder 1. Computing the scale
	 * once at spawn therefore gave every casting the baseline, and a five-success stream
	 * looked exactly like a one-success one. So the sizing is a function that runs again
	 * the moment the roll lands.
	 *
	 * Idempotent on purpose: it sets absolute values rather than multiplying what is
	 * already there, so calling it twice does not compound.
	 */
	const int32 Extra = FMath::Max(0, SustainSuccesses - 1);

	if (SustainedComponent)
	{
		// Successes past the first, matching how the light's radius is derived -- a bare
		// success is the baseline rather than a penalty.
		const float Scale = EffectScale + (EffectScalePerSuccess * Extra);

		SustainedComponent->SetRelativeScale3D(FVector(FMath::Max(0.01f, Scale)));

		// Written here rather than only from a channel's per-frame sampling, so a working
		// with no intensity curve at all -- a torch, which simply burns -- still answers
		// to the roll. A channel overwrites this every sample with the curve-shaped
		// version; for everything else this single write is the whole story.
		if (!IntensityParameter.IsNone())
		{
			SustainedComponent->SetVariableFloat(IntensityParameter, IntensityByRoll());
		}
	}

	if (SustainedLight)
	{
		SustainedLight->SetAttenuationRadius(LightRadius + (LightRadiusPerSuccess * Extra));

		// Brightness grows on the square root while radius grows linearly: doubling a
		// light's reach does not make it look twice as bright, and scaling both together
		// blows out everything within arm's reach on a strong casting.
		LightBaseIntensity = LightIntensity * FMath::Sqrt(static_cast<float>(SustainSuccesses));
		SustainedLight->SetIntensity(LightBaseIntensity);
	}
}

void UGA_SustainedCantrip::SpawnSustainedEffect()
{
	if (!CanSpawnSustainedEffect())
	{
		return;
	}

	/**
	 * Already burning: do nothing.
	 *
	 * Two paths can reach here for one casting now that a channel gathers during the
	 * wind-up. The notify spawns it, and then ScheduleEffectSpawn -- which runs when the
	 * working opens -- sees bEffectStartSeen already set and spawns "the one that must
	 * have been missed". Without this guard the second call overwrites SustainedComponent
	 * and the first system is orphaned: still attached, still burning, and no longer held
	 * by anything that knows how to put it out.
	 */
	if (SustainedComponent)
	{
		return;
	}

	// Before the early-out below: the light is not conditional on there being a Niagara
	// system. A working can legitimately light a room without any visible flame, and a
	// cantrip whose effect asset is still missing should at least still do its job.
	SpawnSustainedLight();

	if (!SustainedEffect)
	{
		return;
	}

	const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}

	if (!Mesh->DoesSocketExist(AttachSocket))
	{
		// Loud, because the symptom is simply no fire, and a socket name typed one
		// character wrong is invisible from the viewport.
		UE_LOG(LogSustainedCantrip, Warning,
			TEXT("Socket '%s' not found on %s -- sustained effect will not appear."),
			*AttachSocket.ToString(), *GetNameSafe(Mesh->GetSkeletalMeshAsset()));
		return;
	}

	SustainedComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
		SustainedEffect, Mesh, AttachSocket, AttachOffset, AttachRotation,
		EAttachLocation::SnapToTargetIncludingScale, /*bAutoDestroy*/ false,
		/*bAutoActivate*/ true);

	// After spawning, because SnapToTargetIncludingScale takes the socket's scale and
	// would discard anything set beforehand. Nothing else touches scale afterwards --
	// UpdateHandSpan only ever sets relative LOCATION -- so this survives.
	ApplySuccessScaling();

	// Full strength on spawn. The fade IN is the emitter's own curve on age -- it
	// knows when it started; only the ending needs telling.
	if (SustainedComponent && !FadeParameter.IsNone())
	{
		SustainedComponent->SetVariableFloat(FadeParameter, 1.0f);
	}

	// Hand Niagara the thing that will receive its collisions. The component lives on
	// the character rather than on this ability deliberately: Niagara keeps this as a
	// plain object pointer, and an ability can end while its last particles are still
	// in the air -- so pointing it here would point it at something that may be gone
	// when the batch arrives.
	if (SustainedComponent && !BurnHandlerParameter.IsNone())
	{
		if (UCantripBurnComponent* Burn =
			Character->FindComponentByClass<UCantripBurnComponent>())
		{
			SustainedComponent->SetVariableObject(BurnHandlerParameter, Burn);

			UE_LOG(LogSustainedCantrip, Log,
				TEXT("BURN: handler '%s' set on %s."),
				*BurnHandlerParameter.ToString(), *GetNameSafe(SustainedEffect));
		}
		else
		{
			// Silent otherwise, and the symptom is simply no marks -- which looks like a
			// Niagara problem rather than a missing component on the character.
			UE_LOG(LogSustainedCantrip, Warning,
				TEXT("BURN: no UCantripBurnComponent on %s -- no marks will appear."),
				*GetNameSafe(Character));
		}
	}

	const bool bDrivesAim = (AimSource != ECantripAimSource::Socket);

	if (SustainedComponent && bDrivesAim)
	{
		// Absolute rotation: the component keeps taking its POSITION from the socket
		// while its direction stops being inherited at all. Without this the socket's
		// rotation would keep composing with whatever we set.
		SustainedComponent->SetUsingAbsoluteRotation(true);
	}

	if (SustainedComponent && (bDrivesAim || !SecondaryAttachSocket.IsNone()))
	{
		UpdateHandSpan();

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(HandSpanTimer,
				FTimerDelegate::CreateUObject(this, &UGA_SustainedCantrip::UpdateHandSpan),
				1.0f / 60.0f, true);
		}
	}
}

void UGA_SustainedCantrip::UpdateHandSpan()
{
	if (!SustainedComponent)
	{
		return;
	}

	// Kept current rather than captured once. A collided particle knows where it
	// stopped but not what it hit, so the surface is found by tracing along the flame's
	// path -- and that path starts wherever the hand is NOW, not where it was when the
	// working opened.
	if (AActor* BurnOwner = GetAvatarActorFromActorInfo())
	{
		if (UCantripBurnComponent* Burn =
			BurnOwner->FindComponentByClass<UCantripBurnComponent>())
		{
			Burn->SetFireOrigin(SustainedComponent->GetComponentLocation());
		}
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	const ACharacter* Character = Cast<ACharacter>(Avatar);
	const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}

	if (AimSource != ECantripAimSource::Socket && Avatar)
	{
		FRotator Aim = Avatar->GetActorRotation();

		if (AimSource == ECantripAimSource::ViewDirection)
		{
			// Where the BODY got to, not where the camera is looking. The two agree
			// until the camera turns further than a spine can, and past that point the
			// eyeline would keep tracking while the arms stopped -- leaving the flame
			// pointing sideways out of hands that are facing somewhere else.
			bool bUsedPose = false;
			if (const UChangelingAnimInstance* Anim =
				Cast<UChangelingAnimInstance>(Mesh->GetAnimInstance()))
			{
				Aim = Anim->GetAimWorldRotation();
				bUsedPose = true;
			}

			// No Changeling anim instance -- an NPC on a plain AnimBP, say. Fall back to
			// the raw aim, which is right for anything that has no spine twist to match.
			if (!bUsedPose)
			{
				if (const APawn* Pawn = Cast<APawn>(Avatar))
				{
					Aim = Pawn->GetBaseAimRotation();
				}
			}
		}

		// Quaternion product, NOT rotator addition.
		//
		// Adding two FRotators adds their components, which is not what composing two
		// rotations means. FRotator(P, Y, 0) is "yaw by Y, then pitch by P about the
		// axis that yaw produced" -- so folding a yaw trim into Y changes which axis
		// the pitch is then applied about. A -90 trim stays correct while the aim is
		// level and swings wildly the moment lateral aim and pitch combine, which is
		// exactly the shape of the bug this replaces.
		//
		// Multiplying applies AttachRotation in the AIM's frame, so a yaw trim turns
		// the effect about its own up axis whatever direction the caster is pointing.
		SustainedComponent->SetWorldRotation(Aim.Quaternion() * AttachRotation.Quaternion());
	}

	if (SecondaryAttachSocket.IsNone())
	{
		return;
	}

	if (!Mesh->DoesSocketExist(SecondaryAttachSocket))
	{
		// Loud once, then stop trying: the symptom is an effect that simply sits on one
		// hand, which looks like a design choice rather than a typo.
		UE_LOG(LogSustainedCantrip, Warning,
			TEXT("Secondary socket '%s' not found -- effect stays on '%s'."),
			*SecondaryAttachSocket.ToString(), *AttachSocket.ToString());

		SecondaryAttachSocket = NAME_None;
		return;
	}

	// Expressed in the PRIMARY socket's space, because that is what the component is
	// parented to. Setting a relative offset leaves the parent to carry all the motion,
	// so the effect tracks the hand perfectly however fast the character moves and only
	// the slowly-changing span between the hands is resampled.
	const FTransform Primary = Mesh->GetSocketTransform(AttachSocket);
	const FVector Secondary = Mesh->GetSocketLocation(SecondaryAttachSocket);
	const FVector MidLocal = Primary.InverseTransformPosition(Secondary) * 0.5f;

	SustainedComponent->SetRelativeLocation(MidLocal + AttachOffset);
}

void UGA_SustainedCantrip::SpawnSustainedLight()
{
	if (LightRadius <= 0.0f)
	{
		return;
	}

	const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}

	// Successes past the first extend the reach. A bare success still lights something,
	// so a marginal casting is dim rather than useless.
	const int32 Extra = FMath::Max(0, SustainSuccesses - 1);
	const float Radius = LightRadius + (LightRadiusPerSuccess * Extra);

	// Brightness grows more slowly than radius, on the square root, because doubling a
	// light's reach does not make it look twice as bright -- scaling both linearly gives
	// a strong casting that blows out everything within arm's reach.
	LightBaseIntensity = LightIntensity * FMath::Sqrt(static_cast<float>(SustainSuccesses));

	SustainedLight = NewObject<UPointLightComponent>(Mesh->GetOwner());
	if (!SustainedLight)
	{
		return;
	}

	// Mobility has to be set before the component is registered; a light that registers
	// as Static cannot be moved afterwards and would stay where the hand was.
	SustainedLight->SetMobility(EComponentMobility::Movable);
	SustainedLight->SetAttenuationRadius(Radius);
	SustainedLight->SetIntensity(LightBaseIntensity);
	SustainedLight->SetLightColor(LightColour);
	SustainedLight->SetCastShadows(bLightCastsShadows);

	SustainedLight->RegisterComponent();
	SustainedLight->AttachToComponent(Mesh,
		FAttachmentTransformRules::SnapToTargetIncludingScale, AttachSocket);
	SustainedLight->SetRelativeLocation(AttachOffset + LightOffset);

	if (LightFlickerAmount > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(FlickerTimer,
				FTimerDelegate::CreateUObject(this, &UGA_SustainedCantrip::UpdateLightFlicker),
				1.0f / 30.0f, true);
		}
	}
}

void UGA_SustainedCantrip::UpdateLightFlicker()
{
	const UWorld* World = GetWorld();
	if (!SustainedLight || !World)
	{
		return;
	}

	const float T = World->GetTimeSeconds() * LightFlickerSpeed;

	// Three sines at incommensurate frequencies. One sine reads as a pulse and the eye
	// finds the period within a second or two; layering ratios that never line up gives
	// something that stays unpredictable without needing a noise texture.
	const float Wander =
		  0.50f * FMath::Sin(T)
		+ 0.30f * FMath::Sin(T * 2.37f)
		+ 0.20f * FMath::Sin(T * 5.13f);

	// Floored well above zero: a flame gutters, it does not switch off and back on.
	const float Scale = FMath::Max(0.35f, 1.0f + (Wander * LightFlickerAmount));

	SustainedLight->SetIntensity(LightBaseIntensity * Scale);
}

void UGA_SustainedCantrip::FreezeSustainedMontage()
{
	if (!bSustaining || !SustainedMontage)
	{
		return;
	}

	if (UAnimInstance* AnimInstance =
		GetCurrentActorInfo() ? GetCurrentActorInfo()->GetAnimInstance() : nullptr)
	{
		// Paused, not stopped: the pose stays exactly where the animator left it, the
		// slot keeps its weight, and the lowering is still there to resume into.
		if (AnimInstance->Montage_IsPlaying(SustainedMontage))
		{
			AnimInstance->Montage_Pause(SustainedMontage);
		}
	}
}

void UGA_SustainedCantrip::StopSustain(bool bRanOut)
{
	/**
	 * A cast abandoned during the WIND-UP never sustained -- but a channelled cantrip has
	 * by then already started its montage and gathered its flame in her hands. Falling
	 * straight out here would leave both running on someone who never finished the cast:
	 * a fire burning out of a pair of hands that have gone back to walking.
	 *
	 * So the teardown below runs for that case too. Everything in it is a release --
	 * jump the montage to its End section, clear the timers, destroy the effect -- and
	 * releasing something that was never taken is already handled by each of them.
	 */
	const bool bAbandonedWindup = !bSustaining && bSustainedMontageStarted;

	if (!bSustaining && !bAbandonedWindup)
	{
		return;
	}
	bSustaining = false;

	// Before anything else in this function, and long before EndAbility: the section
	// command is refused once this is no longer the animating ability, and the arm would
	// be left up until something else happened to move it.
	if (SustainedMontage && !MontageEndSection.IsNone())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			if (ASC->GetCurrentMontage() == SustainedMontage)
			{
				// Resumed first. A paused montage takes the jump and then sits on the
				// first frame of the lowering, which looks like the arm sticking rather
				// than coming down.
				if (UAnimInstance* AnimInstance = GetCurrentActorInfo()
					? GetCurrentActorInfo()->GetAnimInstance() : nullptr)
				{
					AnimInstance->Montage_Resume(SustainedMontage);
				}

				ASC->CurrentMontageJumpToSection(MontageEndSection);
			}
		}
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FreezeTimer);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DurationTimer);
		World->GetTimerManager().ClearTimer(SpawnDelayTimer);
		World->GetTimerManager().ClearTimer(FlickerTimer);
		World->GetTimerManager().ClearTimer(HandSpanTimer);
	}

	if (SustainedLight)
	{
		// Handed off and forgotten here, exactly as the effect is: the dim-down outlives
		// this ability, and anything bound to `this` would be torn down mid-ramp and
		// leave the light snapping to black.
		UPointLightComponent* Dimming = SustainedLight;
		SustainedLight = nullptr;

		UWorld* World = GetWorld();
		if (World && FadeOutSeconds > 0.0f)
		{
			const float Start = LightBaseIntensity;
			const float Duration = FadeOutSeconds;
			const float Step = 1.0f / 30.0f;

			TSharedRef<float> Elapsed = MakeShared<float>(0.0f);
			TSharedRef<FTimerHandle> Handle = MakeShared<FTimerHandle>();

			World->GetTimerManager().SetTimer(*Handle,
				FTimerDelegate::CreateWeakLambda(Dimming,
					[Dimming, Start, Duration, Step, Elapsed, Handle]()
					{
						*Elapsed += Step;
						const float Alpha = FMath::Clamp(1.0f - (*Elapsed / Duration), 0.0f, 1.0f);
						Dimming->SetIntensity(Start * Alpha);

						if (Alpha <= 0.0f)
						{
							if (UWorld* TimerWorld = Dimming->GetWorld())
							{
								TimerWorld->GetTimerManager().ClearTimer(*Handle);
							}
							Dimming->DestroyComponent();
						}
					}),
				Step, true);
		}
		else
		{
			Dimming->DestroyComponent();
		}
	}

	if (SustainedComponent)
	{
		// Hand the component off and forget it here. The fade outlives this ability --
		// StopSustain is followed by EndAbility, so anything bound to `this` would be
		// torn down mid-ramp and the flame would vanish instead of dying down.
		UNiagaraComponent* Fading = SustainedComponent;
		SustainedComponent = nullptr;

		UWorld* World = GetWorld();
		if (World && FadeOutSeconds > 0.0f && !FadeParameter.IsNone())
		{
			const FName Parameter = FadeParameter;
			const float Duration = FadeOutSeconds;
			const float Step = 1.0f / 30.0f;

			TSharedRef<float> Elapsed = MakeShared<float>(0.0f);
			TSharedRef<FTimerHandle> Handle = MakeShared<FTimerHandle>();

			// Weak lambda bound to the COMPONENT: if it is destroyed for any other
			// reason mid-fade, the timer stops itself rather than touching freed memory.
			World->GetTimerManager().SetTimer(*Handle,
				FTimerDelegate::CreateWeakLambda(Fading,
					[Fading, Parameter, Duration, Step, Elapsed, Handle]()
					{
						*Elapsed += Step;
						const float Alpha = FMath::Clamp(1.0f - (*Elapsed / Duration), 0.0f, 1.0f);
						Fading->SetVariableFloat(Parameter, Alpha);

						if (Alpha <= 0.0f)
						{
							if (UWorld* TimerWorld = Fading->GetWorld())
							{
								TimerWorld->GetTimerManager().ClearTimer(*Handle);
							}
							// Deactivate rather than destroy: particles already in the
							// air finish their lives instead of blinking out mid-flight.
							Fading->Deactivate();
							Fading->SetAutoDestroy(true);
						}
					}),
				Step, true);
		}
		else
		{
			Fading->Deactivate();
			Fading->SetAutoDestroy(true);
		}
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (SustainedEffectHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(SustainedEffectHandle);
			SustainedEffectHandle.Invalidate();
		}
	}

	if (ACHANGELINGCharacter* Character = Cast<ACHANGELINGCharacter>(GetAvatarActorFromActorInfo()))
	{
		Character->SetCantripPose(ECantripPose::None);
	}

	OnSustainEnded(bRanOut);
}

void UGA_SustainedCantrip::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Covers every exit -- cancelled, interrupted, avatar destroyed -- so the arm
	// cannot be left raised holding a fire that is no longer there.
	StopSustain(/*bRanOut*/ false);

	/**
	 * Per-CASTING state, cleared for the next one.
	 *
	 * These abilities are InstancedPerActor: one object serves every cast she ever makes,
	 * so anything latched during a casting is still latched at the start of the next. Left
	 * set, this flag made BeginSustain skip the montage on every cast after the first --
	 * the working still lit, the light still came on, and she simply never raised her arm
	 * again. A channel hid it, because its montage starts from ActivateAbility without
	 * consulting the flag at all.
	 */
	bSustainedMontageStarted = false;

	// Back to a bare success for the same reason. Left at the last roll's value, a cast
	// that gathers before it rolls would gather at the FIERCENESS OF THE PREVIOUS ONE --
	// so a weak casting following a strong one would flare and then shrink as the roll
	// landed, which reads as the cantrip losing its nerve.
	SustainSuccesses = 1;

	// Nothing to put back. The casting tag used to be re-added here so that the engine's
	// unconditional removal of an ActivationOwnedTag would have a count of its own to
	// take; the base class no longer owns it, so there is no removal left to feed and
	// Super::EndAbility drops it exactly once whether BeginSustain got there first or not.
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
