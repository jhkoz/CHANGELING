// GE_CantripDamage.cpp

#include "GE_CantripDamage.h"

#include "ChangelingAttributeSet.h"
#include "ChangelingGameplayTags.h"

UGE_CantripDamage::UGE_CantripDamage()
{
	// Instant, even for a cantrip that burns for a minute. The duration belongs to the
	// ABILITY -- a sustained working keeps applying this as it goes, and each application
	// is its own wound. An effect that carried the duration itself would stack with
	// every re-application and take the target apart.
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// The magnitude arrives from the caster rather than being authored here, because the
	// number is the roll's answer: base plus per-success, scaled by what actually
	// happened. ApplyEffectTo sets it on this exact tag, and the two have to agree or the
	// effect applies zero without complaining.
	FSetByCallerFloat FromTheRoll;
	FromTheRoll.DataTag = ChangelingTags::Cantrip_Data_Magnitude;

	FGameplayModifierInfo Wound;
	Wound.Attribute = UChangelingAttributeSet::GetIncomingDamageAttribute();
	Wound.ModifierOp = EGameplayModOp::AddBase;
	Wound.ModifierMagnitude = FGameplayEffectModifierMagnitude(FromTheRoll);

	// Positive in, damage out: PostGameplayEffectExecute consumes IncomingDamage,
	// subtracts it from Health and zeroes the meta attribute again. Nothing here needs
	// to negate anything, and both callers -- UGA_DamageCantrip and
	// UCantripPayload_Damage -- already pass the magnitude as a positive number.
	Modifiers.Add(Wound);
}
