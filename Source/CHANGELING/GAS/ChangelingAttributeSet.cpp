// ChangelingAttributeSet.cpp

#include "ChangelingAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UChangelingAttributeSet::UChangelingAttributeSet()
{
	// One dot in everything: the book's baseline for a competent adult, and the
	// floor character creation spends up from. Zero would mean "cannot try at all".
	InitStrength(1.0f);
	InitDexterity(1.0f);
	InitStamina(5.0f);   // TESTING: full physical Stamina, so the pool is 100. Back to 1 for real starting traits.
	InitCharisma(1.0f);
	InitManipulation(1.0f);
	InitAppearance(1.0f);
	InitPerception(1.0f);
	InitIntelligence(1.0f);
	InitWits(1.0f);

	// The four Tempers.
	//
	// Glamour high and Willpower low, deliberately and by a wide margin. She is a
	// changeling before she is a hero: brimming with the Dreaming and short on the
	// ordinary human resolve to refuse it. It is also the more interesting position to
	// play from -- the resource she has plenty of is the one that gets her into trouble,
	// and the one she is short of is what would get her out.
	//
	// Their MAXIMA are not capped. Ten is only where they begin; GlamourMax and
	// WillpowerMax are ordinary attributes that a Treasure, a freehold or an advancement
	// can raise past it, and PreAttributeChange defends only their floor.
	InitGlamour(7.0f);
	InitGlamourMax(10.0f);

	InitWillpower(3.0f);
	InitWillpowerMax(10.0f);

	// These two rise AGAINST her, and they DO stop at ten -- a hard cap, not a starting
	// ceiling. Both are measures of how much of her is already lost, and a scale that
	// could keep growing would mean there was always further to fall; ten is the end of
	// the character, and it has to be reachable and final for that to mean anything.
	//
	// So they need no Max attribute: the cap is the rule rather than a property of the
	// character, and the HUD bar fills against the widget's FlatMaximum.
	InitBanality(2.0f);

	// Half. Temporarily high so the bar can be judged on screen -- at 1 the fill is a
	// sliver and says nothing about whether a near-black bar reads at all. Drop it back
	// to 1 once the colour is settled; a starting changeling should not be halfway to
	// her own nightmare.
	InitNightmare(5.0f);

	// Wind. Full and generous: running out of breath should be a consequence of what she
	// just did, not a state she wakes up in. Not a Temper -- it sits with Health as
	// something the body does rather than something the Dreaming does to her.
	// Kept in step with the trait by hand HERE only, because a constructor cannot run
	// PostGameplayEffectExecute. Everywhere else the derivation below does it.
	InitStaminaPoolMax(GetStamina() * StaminaPointsPerDot);
	InitStaminaPool(GetStaminaPoolMax());

	InitHealth(7.0f);
	InitHealthMax(7.0f);
	InitIncomingDamage(0.0f);
}

void UChangelingAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// Clamped here rather than in every effect: an effect author who forgets is the
	// normal case, and an Attribute at 7 dots would silently break every pool that
	// reads it.
	if (Attribute == GetStrengthAttribute()     || Attribute == GetDexterityAttribute()
	 || Attribute == GetStaminaAttribute()      || Attribute == GetCharismaAttribute()
	 || Attribute == GetManipulationAttribute() || Attribute == GetAppearanceAttribute()
	 || Attribute == GetPerceptionAttribute()   || Attribute == GetIntelligenceAttribute()
	 || Attribute == GetWitsAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, 5.0f);
	}
	else if (Attribute == GetGlamourAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetGlamourMax());
	}
	else if (Attribute == GetWillpowerAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetWillpowerMax());
	}
	else if (Attribute == GetStaminaPoolAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetStaminaPoolMax());
	}
	else if (Attribute == GetBanalityAttribute() || Attribute == GetNightmareAttribute())
	{
		// A HARD ten, and the only hard cap among the Tempers. Both measure how much of
		// her is already gone, and that has to be able to arrive: a scale that kept
		// growing would mean there was always further to fall, which is a slow death with
		// no end rather than a fate.
		NewValue = FMath::Clamp(NewValue, 0.0f, 10.0f);
	}
	else if (Attribute == GetGlamourMaxAttribute() || Attribute == GetWillpowerMaxAttribute()
	      || Attribute == GetStaminaPoolMaxAttribute())
	{
		// No ceiling on the ceilings. Ten is where they start, not where they stop, so a
		// Treasure or an advancement can raise them and the bar simply reads against the
		// larger number.
		//
		// The floor is one rather than zero: a Temper whose maximum is zero can never
		// hold anything, and every bar drawn from it divides by nothing.
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetHealthMax());
	}
}

void UChangelingAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		// Drain the meta attribute in the same breath as applying it, or the next
		// execution adds to a stale total and the character takes the damage twice.
		const float Damage = GetIncomingDamage();
		SetIncomingDamage(0.0f);

		if (Damage > 0.0f)
		{
			SetHealth(FMath::Clamp(GetHealth() - Damage, 0.0f, GetHealthMax()));
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealthMaxAttribute())
	{
		// A raised ceiling should not retroactively heal, but a lowered one must not
		// leave Health above it.
		SetHealth(FMath::Min(GetHealth(), GetHealthMax()));
	}
	else if (Data.EvaluatedData.Attribute == GetGlamourMaxAttribute())
	{
		SetGlamour(FMath::Min(GetGlamour(), GetGlamourMax()));
	}
	else if (Data.EvaluatedData.Attribute == GetWillpowerMaxAttribute())
	{
		SetWillpower(FMath::Min(GetWillpower(), GetWillpowerMax()));
	}
	else if (Data.EvaluatedData.Attribute == GetStaminaPoolMaxAttribute())
	{
		SetStaminaPool(FMath::Min(GetStaminaPool(), GetStaminaPoolMax()));
	}
	else if (Data.EvaluatedData.Attribute == GetStaminaAttribute())
	{
		// The trait sizes the pool. Proportional rather than absolute, so a changeling
		// whose Stamina rises mid-fight does not suddenly find herself winded: she keeps
		// the FRACTION of wind she had, against a bigger total.
		const float OldMax = GetStaminaPoolMax();
		const float Fraction = (OldMax > KINDA_SMALL_NUMBER)
			? (GetStaminaPool() / OldMax) : 1.0f;

		SetStaminaPoolMax(GetStamina() * StaminaPointsPerDot);
		SetStaminaPool(GetStaminaPoolMax() * Fraction);
	}
}

void UChangelingAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// COND_None / REPNOTIFY_Always: GAS needs the notify even when the value is
	// unchanged, so prediction can be corrected on the client.
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAttributeSet, Strength,     COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAttributeSet, Dexterity,    COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAttributeSet, Stamina,      COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAttributeSet, Charisma,     COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAttributeSet, Manipulation, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAttributeSet, Appearance,   COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAttributeSet, Perception,   COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAttributeSet, Intelligence, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAttributeSet, Wits,         COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAttributeSet, Glamour,      COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAttributeSet, GlamourMax,   COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAttributeSet, Banality,     COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAttributeSet, Willpower,    COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAttributeSet, WillpowerMax, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAttributeSet, Nightmare,    COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAttributeSet, StaminaPool,    COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAttributeSet, StaminaPoolMax, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAttributeSet, Health,       COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAttributeSet, HealthMax,    COND_None, REPNOTIFY_Always);
	// IncomingDamage is deliberately absent: it is consumed server-side within a
	// single execution and has no meaning on a client.
}

#define REPNOTIFY_IMPL(Prop) \
	void UChangelingAttributeSet::OnRep_##Prop(const FGameplayAttributeData& Old) \
	{ GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAttributeSet, Prop, Old); }

REPNOTIFY_IMPL(Strength)
REPNOTIFY_IMPL(Dexterity)
REPNOTIFY_IMPL(Stamina)
REPNOTIFY_IMPL(Charisma)
REPNOTIFY_IMPL(Manipulation)
REPNOTIFY_IMPL(Appearance)
REPNOTIFY_IMPL(Perception)
REPNOTIFY_IMPL(Intelligence)
REPNOTIFY_IMPL(Wits)
REPNOTIFY_IMPL(Glamour)
REPNOTIFY_IMPL(GlamourMax)
REPNOTIFY_IMPL(Banality)
REPNOTIFY_IMPL(Willpower)
REPNOTIFY_IMPL(WillpowerMax)
REPNOTIFY_IMPL(Nightmare)
REPNOTIFY_IMPL(StaminaPool)
REPNOTIFY_IMPL(StaminaPoolMax)
REPNOTIFY_IMPL(Health)
REPNOTIFY_IMPL(HealthMax)

#undef REPNOTIFY_IMPL
