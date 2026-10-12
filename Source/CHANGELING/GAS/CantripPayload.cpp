// CantripPayload.cpp

#include "CantripPayload.h"

#include "GA_Cantrip.h"

DEFINE_LOG_CATEGORY_STATIC(LogCantripPayload, Log, All);

void UCantripPayload_Damage::Apply(UGA_Cantrip* Ability, const FCantripOutcome& Outcome,
	const FCantripTargets& Targets)
{
	if (!Ability)
	{
		return;
	}

	DamagePerHit = UGA_Cantrip::ScaleByRoll(BaseDamage, DamagePerSuccess, Outcome);

	// Spread across the duration. Asked of the ability rather than recomputed here,
	// because the duration is the ability's business and the two must not disagree.
	if (bDamageOverTime)
	{
		const float Duration = Ability->GetTotalDuration(Outcome);
		if (Duration > 1.0f)
		{
			DamagePerHit /= Duration;
		}
	}

	// A stream damages what its particles touch, which is not knowable at the moment of
	// casting. Those cantrips call DamageActor per hit instead.
	if (bOnlyOnContact)
	{
		UE_LOG(LogCantripPayload, Log, TEXT("  payload damage armed at %.2f per hit."),
			DamagePerHit);
		return;
	}

	TArray<FActiveGameplayEffectHandle> Handles;
	Ability->ApplyEffectToTargets(Targets, DamageEffect, DamagePerHit, Handles);

	UE_LOG(LogCantripPayload, Log, TEXT("  payload damage %.1f to %d."),
		DamagePerHit, Handles.Num());
}

void UCantripPayload_Damage::DamageActor(UGA_Cantrip* Ability, AActor* Target, float Scale)
{
	if (!Ability || !Target || DamagePerHit <= 0.0f)
	{
		return;
	}

	Ability->ApplyEffectTo(Target, DamageEffect, DamagePerHit * Scale);
}

FString UCantripPayload_Damage::Describe() const
{
	return FString::Printf(TEXT("Damage %.1f+%.1f/success %s%s"),
		BaseDamage, DamagePerSuccess,
		DamageType == ECantripDamageType::Aggravated ? TEXT("aggravated")
			: DamageType == ECantripDamageType::Lethal ? TEXT("lethal") : TEXT("bashing"),
		bOnlyOnContact ? TEXT(" on contact") : TEXT(""));
}
