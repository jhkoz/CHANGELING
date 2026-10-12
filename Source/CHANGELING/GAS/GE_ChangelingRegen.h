// GE_ChangelingRegen.h
//
// Glamour and health coming back on their own.
//
// Periodic infinite effects rather than anything ticking on the character. A periodic
// GameplayEffect executes like a tiny instant effect every Period seconds, which means
// the recovery goes through the same PreAttributeChange clamps, the same replication and
// the same prediction as every other change to those attributes -- and it can be
// suppressed, scaled or cancelled by other effects without a single branch being written
// for it. A Banality-soaked place that stops Glamour returning is then an effect that
// removes this one, not a special case inside a tick function.
//
// Two classes rather than one with two modifiers, because the two want to be stopped
// independently: bleeding out should not pause her magic, and standing in a shopping
// centre should not stop her bruises healing.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_ChangelingRegen.generated.h"

class UTargetTagRequirementsGameplayEffectComponent;

/**
 * Glamour returning, slowly.
 *
 * The rate is deliberately below the cost of casting anything: a changeling should feel
 * the drain of working magic and have to husband it, not idle for a moment and be full
 * again. Roughly one point every twenty seconds, so a spent pool takes a few minutes --
 * long enough that Glamour is a resource she plans around rather than a cooldown.
 */
UCLASS()
class CHANGELING_API UGE_GlamourRegen : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_GlamourRegen();
};

/**
 * Wounds closing, more slowly still.
 *
 * Slower than Glamour on purpose, and the gap matters: health is the one track whose loss
 * is supposed to change how she plays for a while. Recovering it in seconds would make the
 * wound penalties -- the whole point of the health track -- a momentary inconvenience
 * rather than a reason to withdraw.
 *
 * This heals ordinary damage only, by adding to Health directly. Aggravated wounds are not
 * modelled separately yet; when they are, they want their own far slower effect rather
 * than a slower rate here.
 */
UCLASS()
class CHANGELING_API UGE_HealthRegen : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_HealthRegen();
};

/**
 * Wind spent running.
 *
 * Gated on State.Sprinting by an ongoing tag requirement, so it is simply OFF whenever
 * she is not sprinting -- no branch, no timer, nothing to switch back on. The character
 * sets that tag; this effect and the recovery below never have to know about each other,
 * and cannot both be running.
 */
UCLASS()
class CHANGELING_API UGE_StaminaDrain : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_StaminaDrain(const FObjectInitializer& ObjectInitializer);
};

/**
 * Wind coming back.
 *
 * BLOCKED by State.Sprinting rather than merely slowed: recovering while sprinting would
 * mean the two effects fight each other every second and the real drain rate becomes the
 * difference between two numbers nobody tuned together.
 *
 * Faster than Glamour or health, and deliberately so -- being winded should shape the next
 * few seconds, not the next few minutes. It is the only one of the three whose recovery
 * the player is meant to wait out in the moment.
 */
UCLASS()
class CHANGELING_API UGE_StaminaRegen : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_StaminaRegen(const FObjectInitializer& ObjectInitializer);
};
