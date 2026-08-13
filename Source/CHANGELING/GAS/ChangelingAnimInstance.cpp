// ChangelingAnimInstance.cpp

#include "ChangelingAnimInstance.h"

#include "AbilitySystemComponent.h"
#include "CHANGELINGCharacter.h"
#include "ChangelingGameplayTags.h"
#include "GA_Cantrip.h"
#include "GameFramework/CharacterMovementComponent.h"

void UChangelingAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	ChangelingCharacter = Cast<ACHANGELINGCharacter>(TryGetPawnOwner());
	if (!ChangelingCharacter)
	{
		return;
	}

	Movement = ChangelingCharacter->GetCharacterMovement();
	AbilitySystem = ChangelingCharacter->GetAbilitySystemComponent();
}

void UChangelingAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// The pawn arrives after the AnimBP on a spawned character, so initialisation
	// cannot be the only place this is tried.
	if (!ChangelingCharacter)
	{
		NativeInitializeAnimation();
		if (!ChangelingCharacter)
		{
			return;
		}
	}

	if (const UCharacterMovementComponent* MoveComp = Movement.Get())
	{
		bIsFalling = MoveComp->IsFalling();
	}

	RefreshCantripState();
	RefreshAim(DeltaSeconds);

	// Both conditions are about the same thing: the clip driving the legs has no
	// ground contact worth solving against.
	bAllowFootIK = !bCantripCasting && !bIsFalling;
}

void UChangelingAnimInstance::RefreshAim(float DeltaSeconds)
{
	// The stance belongs to the act of casting: a character who twists at the waist
	// every time the camera moves looks possessed.
	bAimingActive = ChangelingCharacter && ChangelingCharacter->IsAiming()
		&& (!bAimOnlyWhileCasting || bCantripCasting);

	FRotator Target = FRotator::ZeroRotator;

	if (bAimingActive)
	{
		// Base aim rotation rather than the controller's, so an AI caster aims by the
		// same rule a player does. Normalized delta, because a raw subtraction wraps at
		// 180 and would send the spine the long way round.
		const FRotator Look = ChangelingCharacter->GetBaseAimRotation();
		const FRotator Body = ChangelingCharacter->GetActorRotation();
		Target = (Look - Body).GetNormalized();

		Target.Yaw   = FMath::Clamp(Target.Yaw, -MaxAimYaw, MaxAimYaw);
		Target.Pitch = FMath::Clamp(Target.Pitch, -MaxAimPitchDown, MaxAimPitchUp);
		Target.Roll  = 0.0f;
	}

	// Interpolated in both directions, so releasing the stance unwinds rather than
	// snapping straight. Zero is just another target.
	SpineAim = FMath::RInterpTo(SpineAim, Target, DeltaSeconds, AimInterpSpeed);

	// The two shares SUM to the whole aim, they do not stack. Rotations compose down
	// the hierarchy, so the head arrives carrying everything its ancestors did; giving
	// the neck its own slice on top would point the face at twice the angle asked for.
	const float NeckPart = FMath::Clamp(NeckAimFraction, 0.0f, 1.0f);
	const float SpinePart = 1.0f - NeckPart;

	SpineBoneAim = MakeBoneAim(SpineAim, SpinePart / FMath::Max(1, SpineBoneCount));
	NeckBoneAim  = MakeBoneAim(SpineAim, NeckPart  / FMath::Max(1, NeckBoneCount));
}

FRotator UChangelingAnimInstance::GetAimWorldRotation() const
{
	if (!ChangelingCharacter)
	{
		return FRotator::ZeroRotator;
	}

	// Actor rotation plus the CLAMPED offset -- deliberately not the camera. This is
	// where the body got to, and anything leaving the hands has to agree with it.
	return (ChangelingCharacter->GetActorRotation() + SpineAim).GetNormalized();
}

FRotator UChangelingAnimInstance::MakeBoneAim(const FRotator& Aim, float Share) const
{
	float Pitch = Aim.Pitch * Share * AimPitchSign;
	float Yaw   = Aim.Yaw   * Share * AimYawSign;
	float Roll  = Aim.Roll  * Share * AimRollSign;

	// A bone whose long axis runs up the spine turns about that axis to twist, and that
	// axis is named roll. Asking such a bone for yaw folds it sideways instead.
	if (bSwapYawAndRoll)
	{
		Swap(Yaw, Roll);
	}

	// Clamped per bone rather than on the total, because the ceiling is a property of
	// the joint. However far the camera swings, no single vertebra is asked for more
	// than one vertebra can give.
	const float Limit = FMath::Abs(MaxPerBoneAngle);

	return FRotator(
		FMath::Clamp(Pitch, -Limit, Limit),
		FMath::Clamp(Yaw,   -Limit, Limit),
		FMath::Clamp(Roll,  -Limit, Limit));
}

void UChangelingAnimInstance::RefreshCantripState()
{
	CantripPose = ChangelingCharacter->GetCantripPose();
	bCantripHandBlocked = ChangelingCharacter->IsCantripHandBlocked();

	UAbilitySystemComponent* ASC = AbilitySystem.Get();
	if (!ASC)
	{
		bCantripCasting = false;
		bCantripChannelling = false;
		bCantripRecovering = false;
		return;
	}

	const bool bTagCasting = ASC->HasMatchingGameplayTag(ChangelingTags::Cantrip_State_Casting);

	if (!bTagCasting)
	{
		bCantripCasting = false;
		bCantripChannelling = false;
		bCantripRecovering = false;
	}

	if (!bTagCasting)
	{
		bHasChannelledThisCast = false;
		ActiveCantrip.Reset();

		// The clips are deliberately left alone: the spellcasting state machine is
		// still blending out and its sequence players still need something to read.
		// They are replaced wholesale by the next cast. The intensity does go to zero,
		// because nothing should still be sounding or burning by now.
		ChannelIntensity = 0.0f;
		return;
	}

	if (!ActiveCantrip.IsValid())
	{
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (!Spec.IsActive())
			{
				continue;
			}

			// The spec's Ability is the class default for an instanced ability; the
			// live instance is what carries this cast's state.
			const UGameplayAbility* Instance = Spec.GetPrimaryInstance();
			if (const UGA_Cantrip* Cantrip = Cast<UGA_Cantrip>(Instance ? Instance : Spec.Ability))
			{
				ActiveCantrip = Cantrip;
				break;
			}
		}
	}

	if (const UGA_Cantrip* Cantrip = ActiveCantrip.Get())
	{
		if (const FCantripSpec* Row = Cantrip->GetCantripSpec())
		{
			CantripAnims = Row->Anims;
			CantripElement = Row->Element;
		}
	}

	/**
	 * A cantrip only claims the body if it actually has an animation authored.
	 *
	 * Without this, EVERY cantrip routes into the spellcasting state machine, because
	 * they all carry the casting tag. A torch held up while you walk around would be
	 * frozen into a full-body casting pose it has no clips for -- which is the state
	 * machine's Idle animation with a raised arm bolted on top, and looks exactly as
	 * wrong as it sounds. Locomotion keeps the body; the pose is expressed by the
	 * Modify Bone nodes reading CantripPose, as it was before this class existed.
	 */
	bCantripCasting = CantripAnims.IsSet();

	bCantripChannelling = bCantripCasting &&
		ASC->HasMatchingGameplayTag(ChangelingTags::Cantrip_State_Channelling);

	// Not simply "casting but not channelling" -- that is equally true of the WIND-UP,
	// which has not recovered from anything yet. Recovery is the far side of a channel,
	// so it needs to remember that one happened.
	bHasChannelledThisCast |= bCantripChannelling;
	bCantripRecovering = bCantripCasting && !bCantripChannelling && bHasChannelledThisCast;

	// Republished from the curve rather than read straight off it in Blueprint, so a
	// state blending out still reports the value its own clip is authoring.
	ChannelIntensity = FMath::Clamp(GetCurveValue(TEXT("EffectIntensity")), 0.0f, 1.0f);
}

void UChangelingAnimInstance::AnimNotify_CantripEffectStart()
{
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		FGameplayEventData Payload;
		Payload.EventTag = ChangelingTags::Cantrip_Event_EffectStart;
		Payload.Instigator = ChangelingCharacter;

		ASC->HandleGameplayEvent(ChangelingTags::Cantrip_Event_EffectStart, &Payload);
	}
}

void UChangelingAnimInstance::AnimNotify_CantripRecovered()
{
	// The character first, and unconditionally. Releasing the movement lock is the one
	// thing here that must survive a missing or torn-down ability system -- everything
	// else failing is a cosmetic problem, this failing is a character that cannot walk.
	if (ChangelingCharacter)
	{
		ChangelingCharacter->NotifyCantripRecovered();
	}

	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		FGameplayEventData Payload;
		Payload.EventTag = ChangelingTags::Cantrip_Event_Recovered;
		Payload.Instigator = ChangelingCharacter;

		ASC->HandleGameplayEvent(ChangelingTags::Cantrip_Event_Recovered, &Payload);
	}
}
