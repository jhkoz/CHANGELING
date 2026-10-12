// GA_ChannelledCantrip.cpp

#include "GA_ChannelledCantrip.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Animation/AnimInstance.h"
#include "CHANGELINGCharacter.h"
#include "ChangelingGameplayTags.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogChannelledCantrip, Log, All);

UGA_ChannelledCantrip::UGA_ChannelledCantrip()
{
	/**
	 * The duration ladder is KEPT, unlike the first cut of this class.
	 *
	 * It used to be emptied here on the reasoning that a channel lasts as long as it is
	 * held, so the roll had nothing to say about its length. But that left successes
	 * buying only fierceness -- a bigger, brighter, hotter stream -- and quietly dropped
	 * the half of the book's grading that matters most: how long the working can be
	 * maintained before it collapses.
	 *
	 * With the ladder in place it reads as a CEILING rather than a lifetime. Letting go
	 * still ends it early, so the player keeps the decision; the roll decides how long
	 * that decision is theirs to make. A one-success stream gutters out in ten seconds,
	 * a five-success one can be held for three minutes.
	 *
	 * Inherited from UGA_SustainedCantrip, so a cantrip that genuinely should burn until
	 * released can still empty the array on its own Blueprint.
	 */

	// A channelled working plants the caster by its nature -- she is holding something
	// open with both hands. The timeout is long enough to cover the End animation and no
	// longer; it doubles as the deadline after which she is unrooted whether the
	// recovery cue reported in or not.
	bLockMovementWhileCasting = true;
	CastRecoveryTimeout = 1.5f;

	// bAimWhileCasting is deliberately left OFF, even though a channel is the obvious
	// candidate for it.
	//
	// Taking the stance automatically means the camera starts steering the flame the
	// instant the key goes down, which takes the decision away from the player: there is
	// no way to hold a stream on a fixed heading while looking elsewhere. Aiming is an
	// input, and it stays one -- the right mouse button, held, for as long as it is
	// wanted. The property remains available for a cantrip that genuinely should aim
	// itself.
}

bool UGA_ChannelledCantrip::IsForwardBlocked(AActor* Avatar, float Distance, float Height)
{
	if (!Avatar || Distance <= 0.0f)
	{
		return false;
	}

	const UWorld* World = Avatar->GetWorld();
	if (!World)
	{
		return false;
	}

	// Probed at chest height rather than the actor origin: from the origin a kerb or
	// a shallow step reads as a wall, and the cast is refused on open ground.
	const FVector Start = Avatar->GetActorLocation() + FVector(0.0f, 0.0f, Height);
	const FVector End   = Start + Avatar->GetActorForwardVector() * Distance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ChannelClearance), false, Avatar);

	FHitResult Hit;
	return World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
}

void UGA_ChannelledCantrip::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	bEffectStartSeen = false;

	// The gesture begins with the press, not with the roll. Everything else about a
	// channel already works this way -- the effect-start listener below registers here
	// too, for the same reason.
	if (ShouldStartMontageOnActivate())
	{
		StartSustainedMontage();
	}

	StartChannelPresentation();

	// Listening from activation, not from the moment the working opens. The wind-up
	// animation finishes when the clip finishes; the roll resolves when the cast ladder
	// says so. Either can come first, and a listener registered on the second of them
	// would simply miss a cue that had already gone past.
	ListenForEffectStart();

	/**
	 * The gather is scheduled from the GESTURE, not from the roll.
	 *
	 * This deadline used to be armed in ScheduleEffectSpawn, which runs when the working
	 * opens -- so the earliest the flame could appear was the roll plus the timeout, and
	 * the gather it was meant to back up could never happen at all. Measured from here it
	 * lands inside the raise, where the curve is already climbing, whether or not the
	 * animation's cue ever arrives.
	 *
	 * The cue still wins if it fires first; StopListeningForEffectStart clears whichever
	 * of the two came second.
	 */
	if (UWorld* GatherWorld = GetWorld())
	{
		if (GatherDelay > 0.0f)
		{
			GatherWorld->GetTimerManager().SetTimer(EffectStartFallbackTimer,
				FTimerDelegate::CreateUObject(this,
					&UGA_ChannelledCantrip::HandleEffectStartEvent,
					static_cast<const FGameplayEventData*>(nullptr)),
				GatherDelay, false);
		}
		else
		{
			HandleEffectStartEvent(nullptr);
		}
	}

	// A channel does not wait for the key to come up before it resolves -- the key
	// coming up is what ENDS it. So the working opens on the gesture completing
	// instead, and holding from there simply keeps it open.
	float OpenDelay = ChannelOpensAfterSeconds;
	if (OpenDelay <= 0.0f)
	{
		if (const FCantripSpec* Spec = GetCantripSpec())
		{
			OpenDelay = Spec->CastTiers.FullHoldSeconds();
		}
	}

	UWorld* World = GetWorld();
	if (World && OpenDelay > 0.0f)
	{
		World->GetTimerManager().SetTimer(ChannelOpenTimer,
			FTimerDelegate::CreateUObject(this, &UGA_ChannelledCantrip::ReleaseCantrip),
			OpenDelay, false);
	}
	else
	{
		ReleaseCantrip();
	}
}

void UGA_ChannelledCantrip::StopChannelling(AActor* Avatar)
{
	if (!Avatar)
	{
		return;
	}

	UAbilitySystemComponent* ASC = Avatar->FindComponentByClass<UAbilitySystemComponent>();
	if (!ASC)
	{
		if (const IAbilitySystemInterface* Interface = Cast<IAbilitySystemInterface>(Avatar))
		{
			ASC = Interface->GetAbilitySystemComponent();
		}
	}

	if (!ASC)
	{
		return;
	}

	// Gathered before anything is ended. Ending an ability mutates both the ASC's
	// activatable list and the spec's instance array, which are the two containers
	// this would otherwise be walking.
	TArray<TWeakObjectPtr<UGA_ChannelledCantrip>> Running;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.IsActive())
		{
			continue;
		}

		for (UGameplayAbility* Instance : Spec.GetAbilityInstances())
		{
			if (UGA_ChannelledCantrip* Channel = Cast<UGA_ChannelledCantrip>(Instance))
			{
				Running.Add(Channel);
			}
		}
	}

	for (const TWeakObjectPtr<UGA_ChannelledCantrip>& Weak : Running)
	{
		UGA_ChannelledCantrip* Channel = Weak.Get();
		if (!Channel)
		{
			continue;
		}

		if (Channel->IsSustaining())
		{
			Channel->CancelSustain();
		}
		else
		{
			// Still winding up: the working never opened, so there is nothing to put
			// out and nothing has been spent. Cancelled outright rather than resolved.
			Channel->CancelAbility(Channel->CurrentSpecHandle, Channel->CurrentActorInfo,
				Channel->CurrentActivationInfo, true);
		}
	}
}

void UGA_ChannelledCantrip::PostResolve(const FCantripOutcome& Outcome, const FCantripSpec& Spec)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();

	// Checked AFTER the roll, deliberately: the Glamour is spent either way, as the
	// book spends it before the dice. Refusing earlier would make a wall a free retry.
	if (Outcome.bSucceeded && IsForwardBlocked(Avatar, ForwardClearance, ClearanceProbeHeight))
	{
		UE_LOG(LogChannelledCantrip, Log,
			TEXT("%s refused: obstruction within %.0f units ahead."),
			*GetName(), ForwardClearance);

		OnChannelRefused();
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	Super::PostResolve(Outcome, Spec);

	if (!IsSustaining())
	{
		// The roll failed; the parent has already ended it.
		return;
	}

	// Here rather than on activate: this is the frame the working actually opens, and
	// the roll that decides whether it opens at all has only just happened.
	if (bLockMovementWhileCasting)
	{
		TakeCastMovementLock();
	}

	// Loose rather than an ActivationOwnedTag: those apply the moment the ability
	// activates, which is the start of the WIND-UP. The loop must not begin until the
	// working is actually open, and this is the frame where that becomes true.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->AddLooseGameplayTag(ChangelingTags::Cantrip_State_Channelling);
		bChannellingTagged = true;
	}

	// The loop and the sampling started with the press, not here -- see
	// StartChannelPresentation. By the time the working opens the curve has already
	// carried both up to full.
}

void UGA_ChannelledCantrip::ListenForEffectStart()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	UWorld* World = GetWorld();

	if (!ASC || !World || EffectStartDelegate.IsValid())
	{
		return;
	}

	EffectStartDelegate = ASC->GenericGameplayEventCallbacks
		.FindOrAdd(ChangelingTags::Cantrip_Event_EffectStart)
		.AddUObject(this, &UGA_ChannelledCantrip::HandleEffectStartEvent);
}

void UGA_ChannelledCantrip::ScheduleEffectSpawn()
{
	// The gesture already landed while the cast was still building, so there is nothing
	// left to wait for.
	if (bEffectStartSeen)
	{
		SpawnSustainedEffect();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		Super::ScheduleEffectSpawn();
		return;
	}

	ListenForEffectStart();

	// The deadline exists so that a cantrip with no animation yet, or one whose event
	// name was never set, still produces its effect. Being a fraction of a second late
	// is a tuning problem; never appearing at all reads as the cantrip being broken.
	if (EffectStartTimeout > 0.0f)
	{
		World->GetTimerManager().SetTimer(EffectStartFallbackTimer,
			FTimerDelegate::CreateUObject(this, &UGA_ChannelledCantrip::HandleEffectStartEvent,
				static_cast<const FGameplayEventData*>(nullptr)),
			EffectStartTimeout, false);
	}
}

void UGA_ChannelledCantrip::HandleEffectStartEvent(const FGameplayEventData* /*Payload*/)
{
	bEffectStartSeen = true;

	// Whichever of the cue and the deadline arrives first wins; the other is discarded
	// by unsubscribing rather than by a guard that could survive a reuse of this
	// instance.
	StopListeningForEffectStart();

	// Spawned whether or not the working has opened yet: for a channel the cue marks
	// the flame GATHERING in her hands, which happens during the wind-up. The intensity
	// curve keeps it small until the roll lands, and an abandoned cast tears it down
	// through StopSustain.
	SpawnSustainedEffect();
}

void UGA_ChannelledCantrip::StopListeningForEffectStart()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EffectStartFallbackTimer);
	}

	if (EffectStartDelegate.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			if (FGameplayEventMulticastDelegate* Callbacks = ASC->GenericGameplayEventCallbacks
				.Find(ChangelingTags::Cantrip_Event_EffectStart))
			{
				Callbacks->Remove(EffectStartDelegate);
			}
		}
		EffectStartDelegate.Reset();
	}
}

void UGA_ChannelledCantrip::CancelSustain()
{
	UWorld* World = GetWorld();

	// Nothing to hold onto, or already holding: fall through to the immediate path.
	if (!IsSustaining() || !bHoldPoseUntilFaded || FadeOutSeconds <= 0.0f || !World)
	{
		Super::CancelSustain();
		return;
	}

	// Put the fire out and start its fade, but do NOT end the ability yet. The
	// Channelling tag stays, so the graph keeps playing the loop and the caster's hands
	// stay up while the flame dies in front of them.
	StopSustain(/*bRanOut*/ false);

	World->GetTimerManager().SetTimer(RecoveryHoldTimer,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		}),
		FadeOutSeconds, false);
}

void UGA_ChannelledCantrip::StartChannelPresentation()
{
	/**
	 * The loop and the intensity sampling begin with the PRESS.
	 *
	 * They used to begin when the working opened, which meant the sound arrived already
	 * at whatever volume the curve happened to read on that frame -- a flamethrower that
	 * started mid-roar with no gathering before it. There was nothing wrong with the
	 * curve; there was simply nothing alive to apply it to until the roll had landed.
	 *
	 * Starting here gives the curve the whole wind-up to work across, which is what the
	 * class always claimed to do: the flame swells as the arms come up, and the sound
	 * swells with it because both read the same number.
	 */
	AActor* Avatar = GetAvatarActorFromActorInfo();

	if (ChannelSound)
	{
		if (const ACharacter* Character = Cast<ACharacter>(Avatar))
		{
			if (USkeletalMeshComponent* Mesh = Character->GetMesh())
			{
				ChannelAudio = UGameplayStatics::SpawnSoundAttached(ChannelSound, Mesh);
			}
		}
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(IntensityTimer,
			FTimerDelegate::CreateUObject(this, &UGA_ChannelledCantrip::SampleIntensity),
			FMath::Max(IntensitySampleInterval, 0.01f), true);
	}

	/**
	 * Silenced explicitly, and deliberately NOT sampled here.
	 *
	 * The montage started on this same frame, so the animation graph has not evaluated it
	 * yet and its curves are not in the pose. GetCurveValue would fail to find
	 * EffectIntensity -- not because the curve is missing, but because it is not live yet
	 * -- and the "no curve authored means full strength" fallback would read that failure
	 * as an instruction to open at full volume. The result is a blast on the frame the
	 * gesture begins, corrected a sample interval later once the montage evaluates: the
	 * loudest possible version of exactly the wrong thing.
	 *
	 * Starting at silence costs nothing. The first timer tick lands after the graph has
	 * run, so a curve is read correctly, and a cantrip with no curve at all still comes up
	 * to full strength one interval in.
	 */
	CurrentIntensity = 0.0f;

	if (ChannelAudio)
	{
		ChannelAudio->SetVolumeMultiplier(0.0f);
	}
}

void UGA_ChannelledCantrip::SampleIntensity()
{
	if (!IsSustaining())
	{
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	const ACharacter* Character = Cast<ACharacter>(Avatar);
	const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	const UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;

	// No curve authored means full strength throughout, so an ability works before its
	// animation exists rather than being silently mute and invisible.
	//
	// Asked with the overload that REPORTS whether the curve was found. The single-argument
	// GetCurveValue returns 0.0 for a curve that does not exist, which is indistinguishable
	// from a curve authored to zero -- and this line used to take that 0.0 as the answer.
	// The result was a montage with no EffectIntensity track driving the flame to zero
	// scale and the channel audio to silence, on the frame after it spawned: an effect
	// that attached correctly, logged its success, and could not be seen or heard.
	float Sampled = 1.0f;
	const bool bCurveAuthored = Anim && !IntensityCurveName.IsNone()
		&& Anim->GetCurveValue(IntensityCurveName, Sampled);

	CurrentIntensity = bCurveAuthored ? FMath::Clamp(Sampled, 0.0f, 1.0f) : 1.0f;

	/**
	 * The curve says WHEN, the roll says HOW MUCH, and the Niagara gets their product.
	 *
	 * Kept as two separate numbers rather than folding the roll into CurrentIntensity,
	 * because CurrentIntensity is also the audio's volume multiplier and that genuinely
	 * wants to stay inside 0..1 -- a five-success stream should be a bigger fire, not a
	 * cue played at five times its authored level.
	 *
	 * SustainSuccesses is still its placeholder 1 through the wind-up, so the gather
	 * builds at the one-success size and steps up to its true fierceness on the frame the
	 * roll lands. That reads as the working catching rather than as a pop.
	 */
	const float ByRoll = IntensityByRoll();

	if (UNiagaraComponent* Effect = GetSustainedComponent())
	{
		if (!IntensityParameter.IsNone())
		{
			Effect->SetVariableFloat(IntensityParameter, CurrentIntensity * ByRoll);
		}
	}

	if (ChannelAudio)
	{
		/**
		 * Loudness follows the fire on a power law, not in step with it.
		 *
		 * Hearing is roughly logarithmic: about ten times the acoustic power to sound
		 * twice as loud. So a stream three times the size is nothing like three times the
		 * volume -- multiplying the gain by the same number the Niagara gets would put a
		 * strong casting some nine decibels up, which is not "fiercer", it is clipping.
		 * The exponent is the honest version of that relationship, and it also keeps the
		 * multiplier near enough to 1 that a cue mastered close to full scale survives.
		 */
		const float ByRollLoudness = FMath::Pow(ByRoll, FMath::Max(0.0f, LoudnessExponent));

		ChannelAudio->SetVolumeMultiplier(CurrentIntensity * ByRollLoudness);

		/**
		 * And a bigger fire is a DEEPER one.
		 *
		 * Larger bodies of flame resonate lower, so fierceness drops the pitch rather
		 * than raising it -- the opposite of what scaling a parameter usually does, and
		 * the reason a big blaze reads as a roar where a small one hisses.
		 *
		 * Clamped after the fact because the two adjustments compound: the curve's own
		 * ramp already spans ChannelPitchRange, and past roughly twenty percent either way
		 * a looping cue stops sounding lower and starts sounding resampled.
		 */
		const float ByRollPitch = FMath::Pow(ByRoll, -FMath::Max(0.0f, PitchDropExponent));

		const float ShapedPitch = FMath::Lerp(
			static_cast<float>(ChannelPitchRange.X),
			static_cast<float>(ChannelPitchRange.Y),
			CurrentIntensity);

		ChannelAudio->SetPitchMultiplier(
			FMath::Clamp(ShapedPitch * ByRollPitch, 0.8f, 1.25f));
	}

	if (bRecheckClearanceWhileChannelling &&
		IsForwardBlocked(Avatar, ForwardClearance, ClearanceProbeHeight))
	{
		CancelSustain();
	}
}

void UGA_ChannelledCantrip::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(IntensityTimer);
		World->GetTimerManager().ClearTimer(ChannelOpenTimer);
		World->GetTimerManager().ClearTimer(RecoveryHoldTimer);
	}

	StopListeningForEffectStart();

	// Dropping this is what lets the graph leave the loop and play the recovery. It
	// happens on every exit -- released, interrupted, cancelled, avatar destroyed --
	// because a stuck Channelling tag is a character stuck in the loop forever.
	//
	// Guarded on having actually added it. A cast that fizzles -- failed roll, no
	// clearance, missing row -- ends without the channel ever opening, and removing a
	// tag that was never added decrements a count that is already zero. GAS warns
	// about that, and the warning is the only symptom until some later cantrip's
	// legitimate tag is cancelled out by the deficit.
	if (bChannellingTagged)
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveLooseGameplayTag(ChangelingTags::Cantrip_State_Channelling);
		}
		bChannellingTagged = false;
	}

	// The root is given back by UGA_Cantrip::EndAbility, below, which hands it to the
	// character with CastRecoveryTimeout so the End animation still gets to play.

	if (ChannelAudio)
	{
		ChannelAudio->FadeOut(0.3f, 0.0f);
		ChannelAudio = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
