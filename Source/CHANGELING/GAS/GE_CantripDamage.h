// GE_CantripDamage.h
//
// The one effect every cantrip that hurts something applies.
//
// In C++ rather than as a Blueprint asset for two reasons. It is three lines of
// configuration that the whole magic system depends on, so it belongs in version control
// where a bad edit shows up in a diff rather than in a binary. And it has exactly one
// correct shape -- the magnitude convention it reads is set in ApplyEffectTo, and an
// asset that quietly disagreed with that convention would fail silently, dealing zero
// damage with nothing in the log to say why.
//
// Subclass it in Blueprint if a particular cantrip needs to carry extra tags or a
// different damage type; the modifier below is the part nothing should change.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_CantripDamage.generated.h"

/**
 * Instant damage, magnitude supplied by the caster on Cantrip.Data.Magnitude.
 *
 * Writes to IncomingDamage, not to Health. That meta attribute is where soak, armour and
 * the chimerical-versus-real distinction get their chance before anything reaches the
 * health track -- a cantrip that subtracted from Health directly would bypass all of it,
 * and a chimerical wound would kill a mortal.
 */
UCLASS()
class CHANGELING_API UGE_CantripDamage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_CantripDamage();
};
