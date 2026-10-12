// GE_GlamourCost.cpp

#include "GE_GlamourCost.h"

#include "ChangelingAttributeSet.h"
#include "ChangelingGameplayTags.h"

UGE_GlamourCost::UGE_GlamourCost()
{
	// Instant: the Glamour leaves her and stays gone. A duration effect would give it
	// back when it expired, which is a loan rather than a cost.
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat FromTheCast;
	FromTheCast.DataTag = ChangelingTags::Cantrip_Data_GlamourCost;

	FGameplayModifierInfo Spend;
	Spend.Attribute = UChangelingAttributeSet::GetGlamourAttribute();

	// AddBase with a NEGATIVE magnitude from the caller, rather than a positive magnitude
	// subtracted here. Both work; this one keeps the sign in a single place -- the caller
	// already computed how much leaves her, and re-negating it at this end is how a cost
	// becomes a refund.
	Spend.ModifierOp = EGameplayModOp::AddBase;
	Spend.ModifierMagnitude = FGameplayEffectModifierMagnitude(FromTheCast);

	Modifiers.Add(Spend);
}
