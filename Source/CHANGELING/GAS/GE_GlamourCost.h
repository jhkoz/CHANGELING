// GE_GlamourCost.h
//
// The Glamour a cantrip spends, in C++.
//
// Replaces the GE_CantripCost Blueprint asset, which failed in exactly the way this file's
// siblings were written to avoid. The asset was configured correctly by every measure that
// can be checked from outside -- Instant, one AddBase modifier on Glamour, SetByCaller on
// Cantrip.Data.GlamourCost, no components, no gates, no chance roll -- and applying it did
// nothing at all. Not an error, not a warning, not a zero: the attribute simply never
// moved, while a C++ effect writing to the same attribute in the same frame worked.
//
// GE_CantripDamage.h already argued this case before it happened:
//
//     "In C++ rather than as a Blueprint asset ... it has exactly one correct shape --
//      the magnitude convention it reads is set in ApplyEffectTo, and an asset that
//      quietly disagreed with that convention would fail silently."
//
// That is the whole of it. A Blueprint GameplayEffect is a binary that agrees with the
// code right up until it does not, and the day it stops there is nothing in a diff and
// nothing in a log to say so -- it cost a full session of instrumenting to find, and every
// intermediate hypothesis (the ability, prediction, authority, the tag, the assignment,
// an aggregator modifier) was wrong.
//
// Subclass it in Blueprint if a particular cantrip needs extra tags. The modifier below is
// the part nothing should re-author.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_GlamourCost.generated.h"

/**
 * Instant Glamour spend, magnitude supplied by the caster on Cantrip.Data.GlamourCost.
 *
 * The magnitude is passed NEGATIVE by the caller -- UGA_Cantrip::ReleaseCantrip sends
 * -Cost -- so this adds rather than subtracts. That is worth stating because the opposite
 * convention is just as reasonable and the two are indistinguishable until a cantrip
 * refills the pool it was supposed to drain.
 */
UCLASS()
class CHANGELING_API UGE_GlamourCost : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_GlamourCost();
};
