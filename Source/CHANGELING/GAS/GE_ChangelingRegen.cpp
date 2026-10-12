// GE_ChangelingRegen.cpp

#include "GE_ChangelingRegen.h"

#include "ChangelingAttributeSet.h"
#include "ChangelingGameplayTags.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"

namespace
{
	/**
	 * One second, for both.
	 *
	 * A short period with a small step rather than a long one with a big step, because
	 * the HUD bars read the attribute directly: at a five-second period the Glamour bar
	 * would sit still and then jump, which looks like a bug in the bar rather than like
	 * recovery. A second is under the threshold at which a bar reads as continuous.
	 */
	constexpr float RegenPeriodSeconds = 1.0f;

	/** Builds the periodic shape both effects share. */
	void MakeRegen(UGameplayEffect& Effect, const FGameplayAttribute& Attribute,
		float PerSecond)
	{
		// Infinite, not Duration: it should run for as long as she is alive, and anything
		// that should stop it removes it. A duration would need renewing by something,
		// and the thing that forgets to renew it is a character who never heals again.
		Effect.DurationPolicy = EGameplayEffectDurationType::Infinite;
		Effect.Period = FScalableFloat(RegenPeriodSeconds);

		// Executes on application as well as on each period. Off, deliberately: applying
		// this at BeginPlay would otherwise hand out a free point of recovery the instant
		// the character spawns, which is invisible at full health and confusing when
		// something has already hurt her.
		Effect.bExecutePeriodicEffectOnApplication = false;

		FGameplayModifierInfo Tick;
		Tick.Attribute = Attribute;

		// AddBase, and for a PERIODIC effect that means each execution permanently moves
		// the base value -- exactly like a small instant heal every second. It is not the
		// same as AddBase on a duration effect, which would be one temporary bonus that
		// vanishes when the effect ends.
		Tick.ModifierOp = EGameplayModOp::AddBase;
		Tick.ModifierMagnitude = FGameplayEffectModifierMagnitude(
			FScalableFloat(PerSecond * RegenPeriodSeconds));

		Effect.Modifiers.Add(Tick);
	}
}

UGE_GlamourRegen::UGE_GlamourRegen()
{
	// A point every twenty seconds. Slower than any cantrip costs, so casting always
	// spends faster than waiting refills -- which is what makes Glamour a thing to spend
	// carefully rather than a bar to watch.
	MakeRegen(*this, UChangelingAttributeSet::GetGlamourAttribute(), 0.05f);
}

UGE_HealthRegen::UGE_HealthRegen()
{
	// A point every fifty seconds: a full health track from empty takes around six
	// minutes. Long enough that a wound is a reason to leave a fight rather than a pause
	// in one, which is the whole purpose of the penalties it carries.
	MakeRegen(*this, UChangelingAttributeSet::GetHealthAttribute(), 0.02f);
}

namespace
{
	/**
	 * Adds the ongoing tag gate to an effect. Built as a DEFAULT SUBOBJECT.
	 *
	 * Not FindOrAddComponent: that calls NewObject with an empty name, which asserts
	 * outright inside a UObject constructor and takes the editor down before it finishes
	 * loading. Those helpers are for GameplayEffects assembled at runtime.
	 */
	UTargetTagRequirementsGameplayEffectComponent& AddGate(UGameplayEffect& Effect,
		const FObjectInitializer& ObjectInitializer, TArray<TObjectPtr<UGameplayEffectComponent>>& Components,
		const TCHAR* SubobjectName)
	{
		UTargetTagRequirementsGameplayEffectComponent* Gate =
			ObjectInitializer.CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(
				&Effect, SubobjectName);

		check(Gate);
		Components.Add(Gate);
		return *Gate;
	}
}

UGE_StaminaDrain::UGE_StaminaDrain(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Eight a second: a hundred points of wind at Stamina 5 is about twelve seconds of
	// flat-out running. Long enough to cross a courtyard, short enough that sprinting
	// everywhere is not free.
	MakeRegen(*this, UChangelingAttributeSet::GetStaminaPoolAttribute(), -8.0f);

	AddGate(*this, ObjectInitializer, GEComponents, TEXT("SprintingGate"))
		.OngoingTagRequirements.RequireTags.AddTag(ChangelingTags::State_Sprinting);
}

UGE_StaminaRegen::UGE_StaminaRegen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Five a second: twenty seconds from empty to full, against twelve to spend it. The
	// asymmetry is the point -- she can outrun her wind, and then has to wait.
	MakeRegen(*this, UChangelingAttributeSet::GetStaminaPoolAttribute(), 5.0f);

	AddGate(*this, ObjectInitializer, GEComponents, TEXT("NotSprintingGate"))
		.OngoingTagRequirements.IgnoreTags.AddTag(ChangelingTags::State_Sprinting);
}
