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
	// A channel lasts as long as it is held, so the duration ladder the sustained
	// class rolls is not used. Zero entries means no expiry timer is ever set.
	DurationBySuccesses.Empty();
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

	// Listening from activation, not from the moment the working opens. The wind-up
	// animation finishes when the clip finishes; the roll resolves when the cast ladder
	// says so. Either can come first, and a listener registered on the second of them
	// would simply miss a cue that had already gone past.
	ListenForEffectStart();

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

	SetMovementLocked(bLockMovementWhileChannelling);

	// Loose rather than an ActivationOwnedTag: those apply the moment the ability
	// activates, which is the start of the WIND-UP. The loop must not begin until the
	// working is actually open, and this is the frame where that becomes true.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->AddLooseGameplayTag(ChangelingTags::Cantrip_State_Channelling);
	}

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

	SampleIntensity();
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

	// Only spawns if the working is actually open. If the gesture finished first, the
	// latch above means BeginSustain will spawn the moment the roll resolves.
	if (IsSustaining())
	{
		SpawnSustainedEffect();
	}
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
	CurrentIntensity = (Anim && !IntensityCurveName.IsNone())
		? FMath::Clamp(Anim->GetCurveValue(IntensityCurveName), 0.0f, 1.0f)
		: 1.0f;

	if (UNiagaraComponent* Effect = GetSustainedComponent())
	{
		if (!IntensityParameter.IsNone())
		{
			Effect->SetVariableFloat(IntensityParameter, CurrentIntensity);
		}
	}

	if (ChannelAudio)
	{
		ChannelAudio->SetVolumeMultiplier(CurrentIntensity);
		ChannelAudio->SetPitchMultiplier(FMath::Lerp(
			static_cast<float>(ChannelPitchRange.X),
			static_cast<float>(ChannelPitchRange.Y),
			CurrentIntensity));
	}

	if (bRecheckClearanceWhileChannelling &&
		IsForwardBlocked(Avatar, ForwardClearance, ClearanceProbeHeight))
	{
		CancelSustain();
	}
}

void UGA_ChannelledCantrip::SetMovementLocked(bool bLocked)
{
	if (bMovementLocked == bLocked)
	{
		return;
	}

	ACHANGELINGCharacter* Character = Cast<ACHANGELINGCharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}

	// The character owns the lock. It has to outlive this ability -- the body is still
	// recovering when the cantrip ends -- so the ability only ever asks.
	Character->SetCantripMovementLocked(bLocked);
	bMovementLocked = bLocked;
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
	}

	StopListeningForEffectStart();

	// Dropping this is what lets the graph leave the loop and play the recovery. It
	// happens on every exit -- released, interrupted, cancelled, avatar destroyed --
	// because a stuck Channelling tag is a character stuck in the loop forever.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RemoveLooseGameplayTag(ChangelingTags::Cantrip_State_Channelling);
	}

	// Handed to the character rather than released here. The recovery animation is
	// still to play, and a character that regains its feet on this frame slides
	// through it. The character unroots itself when the animation reports in, or when
	// its own deadline passes -- so the lock cannot outlive the ability that set it
	// even if the animation never arrives.
	if (bMovementLocked)
	{
		if (ACHANGELINGCharacter* Character = Cast<ACHANGELINGCharacter>(GetAvatarActorFromActorInfo()))
		{
			Character->ReleaseCantripMovementLockOnRecovery(RecoveryTimeout);
		}
		bMovementLocked = false;
	}

	if (ChannelAudio)
	{
		ChannelAudio->FadeOut(0.3f, 0.0f);
		ChannelAudio = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
