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
	InitStamina(1.0f);
	InitCharisma(1.0f);
	InitManipulation(1.0f);
	InitAppearance(1.0f);
	InitPerception(1.0f);
	InitIntelligence(1.0f);
	InitWits(1.0f);

	InitGlamour(4.0f);
	InitGlamourMax(10.0f);
	InitBanality(3.0f);
	InitWillpower(4.0f);
	InitWillpowerMax(10.0f);
	InitNightmare(0.0f);

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
	else if (Attribute == GetBanalityAttribute() || Attribute == GetNightmareAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, 10.0f);
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
REPNOTIFY_IMPL(Health)
REPNOTIFY_IMPL(HealthMax)

#undef REPNOTIFY_IMPL
