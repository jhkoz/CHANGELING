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

	// Both conditions are about the same thing: the clip driving the legs has no
	// ground contact worth solving against.
	bAllowFootIK = !bCantripCasting && !bIsFalling;
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

	bCantripCasting = ASC->HasMatchingGameplayTag(ChangelingTags::Cantrip_State_Casting);
	bCantripChannelling = ASC->HasMatchingGameplayTag(ChangelingTags::Cantrip_State_Channelling);

	// Not simply "casting but not channelling" -- that is equally true of the WIND-UP,
	// which has not recovered from anything yet. Recovery is the far side of a channel,
	// so it needs to remember that one happened.
	bHasChannelledThisCast |= bCantripChannelling;
	bCantripRecovering = bCantripCasting && !bCantripChannelling && bHasChannelledThisCast;

	if (!bCantripCasting)
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
