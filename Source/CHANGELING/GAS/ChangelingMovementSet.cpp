// ChangelingMovementSet.cpp

#include "ChangelingMovementSet.h"

#include "Net/UnrealNetwork.h"

UChangelingMovementSet::UChangelingMovementSet()
{
	// One means "exactly as the character was built". Every one of these has to start
	// here rather than at zero: an attribute set is constructed before anything grants
	// an effect, and a MoveSpeedMultiplier that begins at 0 would leave a changeling
	// rooted to the floor for however many frames pass before something sets it.
	InitMoveSpeedMultiplier(1.0f);
	InitJumpPowerMultiplier(1.0f);
	InitFallSpeedMultiplier(1.0f);
	InitAirControlMultiplier(1.0f);
}

void UChangelingMovementSet::PreAttributeChange(const FGameplayAttribute& Attribute,
	float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// Clamped here rather than trusting effect authors, for the same reason the trait
	// set clamps: the failure is silent and it is the character movement component that
	// pays for it. A negative speed multiplier walks backwards; a negative gravity
	// multiplier accelerates upward forever.
	if (Attribute == GetMoveSpeedMultiplierAttribute())
	{
		// The floor is a crawl, not a stop. See the note on the property.
		NewValue = FMath::Clamp(NewValue, 0.1f, 8.0f);
	}
	else if (Attribute == GetJumpPowerMultiplierAttribute())
	{
		// Four is already a rooftop. The ceiling exists so that a stacked pair of
		// effects cannot put the player outside the level, which is unrecoverable in a
		// way that being too fast is not.
		NewValue = FMath::Clamp(NewValue, 0.1f, 4.0f);
	}
	else if (Attribute == GetFallSpeedMultiplierAttribute())
	{
		// Zero is allowed and means flight. Above 1 is allowed too -- a cantrip that
		// makes someone else heavy is a perfectly good Wayfare-shaped attack.
		NewValue = FMath::Clamp(NewValue, 0.0f, 5.0f);
	}
	else if (Attribute == GetAirControlMultiplierAttribute())
	{
		// The product still has to land somewhere sane: CharacterMovement treats air
		// control above 1 as 1, so a large number here is not wrong, just wasted.
		NewValue = FMath::Clamp(NewValue, 0.0f, 20.0f);
	}
}

void UChangelingMovementSet::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// REPNOTIFY_Always, matching the other two sets: GAS needs the notify even when the
	// replicated value is unchanged, or a predicted change that the server confirms
	// identically never gets rolled back into the client's aggregator.
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingMovementSet, MoveSpeedMultiplier,
		COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingMovementSet, JumpPowerMultiplier,
		COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingMovementSet, FallSpeedMultiplier,
		COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingMovementSet, AirControlMultiplier,
		COND_None, REPNOTIFY_Always);
}

void UChangelingMovementSet::OnRep_MoveSpeedMultiplier(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingMovementSet, MoveSpeedMultiplier, Old);
}

void UChangelingMovementSet::OnRep_JumpPowerMultiplier(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingMovementSet, JumpPowerMultiplier, Old);
}

void UChangelingMovementSet::OnRep_FallSpeedMultiplier(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingMovementSet, FallSpeedMultiplier, Old);
}

void UChangelingMovementSet::OnRep_AirControlMultiplier(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingMovementSet, AirControlMultiplier, Old);
}
