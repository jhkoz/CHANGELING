// GE_CantripMovement.cpp

#include "GE_CantripMovement.h"

#include "ChangelingGameplayTags.h"
#include "ChangelingMovementSet.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"

namespace
{
	/** One modifier, magnitude supplied by the caster on the given tag. */
	FGameplayModifierInfo MakeSetByCallerBonus(const FGameplayAttribute& Attribute,
		const FGameplayTag& DataTag)
	{
		FSetByCallerFloat FromTheCaster;
		FromTheCaster.DataTag = DataTag;

		FGameplayModifierInfo Bonus;
		Bonus.Attribute = Attribute;

		// AddBase throughout, never Override. These attributes start at 1.0 and the
		// number handed in is a BONUS on top of it, so removing the effect returns the
		// value to 1.0 with nobody having to remember what normal was -- and two effects
		// running at once add up instead of one of them silently winning.
		Bonus.ModifierOp = EGameplayModOp::AddBase;
		Bonus.ModifierMagnitude = FGameplayEffectModifierMagnitude(FromTheCaster);

		return Bonus;
	}
}

UGE_CantripSpeed::UGE_CantripSpeed()
{
	// The ABILITY owns the duration; see the header. Infinite here means "until removed",
	// and UGA_MobilityCantrip::RevertCantrip is what removes it.
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	Modifiers.Add(MakeSetByCallerBonus(
		UChangelingMovementSet::GetMoveSpeedMultiplierAttribute(),
		ChangelingTags::Cantrip_Data_Magnitude));
}

UGE_CantripBound::UGE_CantripBound(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// From the roll: how much higher she leaves the ground.
	Modifiers.Add(MakeSetByCallerBonus(
		UChangelingMovementSet::GetJumpPowerMultiplierAttribute(),
		ChangelingTags::Cantrip_Data_Magnitude));

	// Authored: how slowly she comes back down. Passed as a NEGATIVE bonus -- the ability
	// computes it from a designer-facing "fall at a fifth of normal" and hands over the
	// -0.8 -- so that nobody editing a cantrip row ever has to think in signed deltas.
	Modifiers.Add(MakeSetByCallerBonus(
		UChangelingMovementSet::GetFallSpeedMultiplierAttribute(),
		ChangelingTags::Cantrip_Data_Secondary));

	// Steering rides on the JUMP number rather than getting a tag of its own, and that is
	// a design decision rather than a shortcut: a drift she cannot steer is a helpless
	// fall dressed up as a power, so the two must never be tunable apart. Tying it to the
	// roll also means a good cast is felt twice -- higher off the ground AND more say
	// about where she comes down -- which is what makes the cantrip worth getting good at.
	Modifiers.Add(MakeSetByCallerBonus(
		UChangelingMovementSet::GetAirControlMultiplierAttribute(),
		ChangelingTags::Cantrip_Data_Magnitude));

	// APPLIED for the cantrip's whole duration, but only ON while the jump control is
	// held. An ongoing requirement is exactly this distinction: the effect stays on the
	// character and its modifiers stop contributing the instant the tag goes away, then
	// resume the instant it comes back.
	//
	// Two separate clocks, and they mean different things to the player. The duration is
	// how long she MAY bound; the key is when she is doing it. Letting go drops her out
	// of the air on the same frame, which is what makes the cantrip something to be good
	// at rather than something to switch on -- and what lets her choose to fall, which is
	// half of what the ability is for.
	//
	// Done here rather than by adding and removing the effect on every keypress. The
	// input is local and this ability is server-initiated, so driving GE application from
	// the key would mean a round trip per jump; GAS re-evaluates a tag requirement
	// synchronously wherever the tag changes.
	//
	// CreateDefaultSubobject, NOT FindOrAddComponent. The latter reads as the intended
	// API and crashes on startup: it calls NewObject with an empty name, which is illegal
	// inside a UObject constructor and asserts before the editor finishes loading. Those
	// helpers exist for GameplayEffects assembled at RUNTIME. A component that belongs to
	// a class has to be a default subobject, named, so that every instance and every
	// Blueprint subclass inherits the same one.
	UTargetTagRequirementsGameplayEffectComponent* Gate =
		ObjectInitializer.CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(
			this, TEXT("JumpHeldGate"));

	if (ensure(Gate))
	{
		Gate->OngoingTagRequirements.RequireTags.AddTag(ChangelingTags::State_JumpHeld);

		// GEComponents is protected on UGameplayEffect and is the array the runtime
		// actually consults; creating the subobject alone would leave it inert.
		GEComponents.Add(Gate);
	}
}

UGE_CantripFlight::UGE_CantripFlight()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// A flat -1.0, authored rather than rolled: flight is flight. This is the one place
	// a fixed ScalableFloat beats SetByCaller, because there is no version of this number
	// the roll should be allowed to change -- partial gravity while MOVE_Flying is on
	// reads as a bug, not as a weaker cantrip.
	FGameplayModifierInfo Weightless;
	Weightless.Attribute = UChangelingMovementSet::GetFallSpeedMultiplierAttribute();
	Weightless.ModifierOp = EGameplayModOp::AddBase;
	Weightless.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(-1.0f));
	Modifiers.Add(Weightless);

	// Steering does come from the roll: a better flier handles better.
	Modifiers.Add(MakeSetByCallerBonus(
		UChangelingMovementSet::GetAirControlMultiplierAttribute(),
		ChangelingTags::Cantrip_Data_Magnitude));
}
