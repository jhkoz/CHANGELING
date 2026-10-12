// GE_CantripMovement.h
//
// The three effects that make a body move differently. Wayfare, mostly.
//
// In C++ for the same reasons as GE_CantripDamage: they are a few lines of configuration
// the whole Art depends on, they have exactly one correct shape, and the SetByCaller tags
// they read are a convention that fails SILENTLY when an asset disagrees with it -- a
// Blueprint effect with the wrong tag applies a bonus of zero and logs nothing, which
// looks precisely like a cantrip that never fired.
//
// All three are INFINITE. The duration belongs to the ability: UGA_MobilityCantrip keeps
// the handle and removes it in RevertCantrip, which is what happens when a Sustained row's
// duration runs out, when the player ends it early, or when something dispels it. An
// effect that carried its own duration would need the roll's answer plumbed into it twice
// and would drift out of step with the hotbar's countdown.
//
// All three add BONUSES to attributes that sit at 1.0. Nothing here overrides: two
// cantrips running at once should add up, and Override would silently make the older one
// stop mattering with no way for the player to tell which is which.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_CantripMovement.generated.h"

/**
 * Runs faster. Quicksilver.
 *
 * Magnitude is the bonus, not the multiplier: +1.5 on Cantrip.Data.Magnitude means two and
 * a half times normal speed, because the attribute already sits at 1.0.
 */
UCLASS()
class CHANGELING_API UGE_CantripSpeed : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_CantripSpeed();
};

/**
 * Jumps carry, and falls slow to a drift. Hopscotch, held rather than spent.
 *
 * GATED ON THE JUMP CONTROL. Applied for the cantrip's whole duration, but in force only
 * while State.JumpHeld is set -- so the duration says how long she may bound and the key
 * says when she is bounding. Release it in mid-air and full gravity returns on that frame.
 * That is the whole skill of the ability: choosing when to stop flying is choosing where
 * to land, and choosing to drop is how she gets out of the way of something.
 *
 * The one effect in the game that reads two magnitudes. Cantrip.Data.Magnitude is the jump
 * bonus and comes from the roll; Cantrip.Data.Secondary is the fall bonus and is authored,
 * because how slowly she drifts is a feel the designer owns and not something a good roll
 * should make more of. A player who rolls well should clear a wall she could not clear
 * before -- not hang in the air twice as long, which is only ever a longer wait.
 *
 * Air control rides on the jump number, so the two can never be tuned apart. Drifting
 * without steering is a helpless fall dressed up as a power.
 */
UCLASS()
class CHANGELING_API UGE_CantripBound : public UGameplayEffect
{
	GENERATED_BODY()

public:
	/**
	 * Takes an FObjectInitializer, unlike its two siblings, because it is the only one
	 * that needs a GameplayEffectComponent -- and a component on a CDO has to be a
	 * DEFAULT SUBOBJECT.
	 *
	 * UGameplayEffect::AddComponent/FindOrAddComponent look like the obvious way to do
	 * this and are not: they call NewObject with an empty name, which asserts outright
	 * inside a UObject constructor ("use ObjectInitializer.CreateDefaultSubobject
	 * instead"). Those helpers are for building a GameplayEffect dynamically at runtime,
	 * which is what the comment above them means by "your own dynamic GameplayEffect".
	 */
	UGE_CantripBound(const FObjectInitializer& ObjectInitializer);
};

/**
 * Gravity stops applying. Wind Runner.
 *
 * Only half the working: this frees her from the ground, and UGA_MobilityCantrip puts the
 * movement component into MOVE_Flying, which is what actually lets her climb. Both, because
 * either alone is wrong -- flying with gravity still pulling is a slow sink, and killing
 * gravity without the flying mode is a character who falls forever at zero miles an hour
 * while the animation graph insists she is falling.
 *
 * The magnitude is the air-control bonus. Nothing scales the gravity: flight is flight, and
 * a marginal success should buy a shorter flight rather than a heavier one.
 */
UCLASS()
class CHANGELING_API UGE_CantripFlight : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_CantripFlight();
};
