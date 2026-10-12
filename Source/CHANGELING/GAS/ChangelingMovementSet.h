// ChangelingMovementSet.h
//
// How the body moves, as multipliers a GameplayEffect can bend.
//
// Its own set rather than four more fields on UChangelingAttributeSet, because that set
// is the BOOK: nine Attributes, Glamour, Banality, Willpower, Health. Nothing in C20 has
// a "gravity scale", and mixing engine knobs into the trait sheet makes it stop reading
// as one. UChangelingMagicSet already split off the Arts and Realms for the same reason.
//
// Every value here is a MULTIPLIER on a base the character owns, and every one sits at
// 1.0 with nothing acting on it -- so an effect adds a BONUS rather than an absolute:
// +1.5 on MoveSpeedMultiplier means two and a half times normal, and removing the effect
// restores 1.0 without anyone having to remember what normal was. Two speed cantrips
// running at once therefore add rather than compound, which is the stacking a player
// expects and the one that cannot run away from the level designer.
//
// NOTHING here writes to the character movement component. These are the ASK; the single
// place that reads them and does the writing is ACHANGELINGCharacter::RefreshLocomotion,
// and it needs to stay the only one -- see the long note there.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "ChangelingMovementSet.generated.h"

#define MOVEMENT_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class CHANGELING_API UChangelingMovementSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UChangelingMovementSet();

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Multiplies the character's base walking or running speed. Quicksilver.
	 *
	 * Clamped well above zero at the bottom: an effect that lands on this wrong should
	 * make a changeling slow, never make her unable to move at all. Taking away movement
	 * is Control's job, and it does it by movement mode, where the animation graph can
	 * see it.
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MoveSpeedMultiplier,
		Category = "Movement")
	FGameplayAttributeData MoveSpeedMultiplier;
	MOVEMENT_ATTRIBUTE_ACCESSORS(UChangelingMovementSet, MoveSpeedMultiplier)

	/** Multiplies jump take-off velocity. Hopscotch. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_JumpPowerMultiplier,
		Category = "Movement")
	FGameplayAttributeData JumpPowerMultiplier;
	MOVEMENT_ATTRIBUTE_ACCESSORS(UChangelingMovementSet, JumpPowerMultiplier)

	/**
	 * Multiplies gravity. BELOW one is the interesting direction.
	 *
	 * Named for what the player sees rather than for the engine field: at 0.2 she does
	 * not "have less gravity", she falls slowly -- and the whole feel of a bounding
	 * cantrip is in the fall, not in the jump.
	 *
	 * ZERO IS THE DEFINITION OF FLIGHT, and that is a contract, not a coincidence.
	 * ACHANGELINGCharacter::RefreshLocomotion reads exactly this: at zero it holds her in
	 * MOVE_Flying, and the instant it rises above zero it puts her back into MOVE_Falling.
	 * Deriving the movement mode from the attribute rather than from an ability's own
	 * bookkeeping is what makes landing reliable -- there is no list to fall out of date,
	 * no handle to go stale, and nothing that stays airborne because the thing holding it
	 * up was torn down without being asked politely.
	 *
	 * So: do not author an effect that lands this on zero unless it means flight.
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_FallSpeedMultiplier,
		Category = "Movement")
	FGameplayAttributeData FallSpeedMultiplier;
	MOVEMENT_ATTRIBUTE_ACCESSORS(UChangelingMovementSet, FallSpeedMultiplier)

	/**
	 * Multiplies how much steering she has in the air.
	 *
	 * Separate from FallSpeedMultiplier on purpose. Slow falling without steering is a
	 * long helpless drop -- which is a punishment, not an ability -- and the two are
	 * tuned against each other: the longer she hangs, the more she needs to be able to
	 * do about where she lands.
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AirControlMultiplier,
		Category = "Movement")
	FGameplayAttributeData AirControlMultiplier;
	MOVEMENT_ATTRIBUTE_ACCESSORS(UChangelingMovementSet, AirControlMultiplier)

protected:
	UFUNCTION() void OnRep_MoveSpeedMultiplier(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_JumpPowerMultiplier(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_FallSpeedMultiplier(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_AirControlMultiplier(const FGameplayAttributeData& Old);
};

#undef MOVEMENT_ATTRIBUTE_ACCESSORS
