// ChangelingAbilityTraitSet.cpp

#include "ChangelingAbilityTraitSet.h"

#include "Net/UnrealNetwork.h"

UChangelingAbilityTraitSet::UChangelingAbilityTraitSet()
{
	// Everything starts untrained. Character creation spends the 13/9/5 allocation up
	// from here, so a fresh set is deliberately useless rather than quietly competent.
}

void UChangelingAbilityTraitSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// Every trait in this set shares one range, so clamp by set rather than listing
	// thirty comparisons that would drift out of date the first time one is added.
	NewValue = FMath::Clamp(NewValue, 0.0f, 5.0f);
}

void UChangelingAbilityTraitSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, Alertness, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, Athletics, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, Brawl, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, Empathy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, Expression, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, Intimidation, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, Kenning, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, Leadership, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, Streetwise, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, Subterfuge, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, HobbyTalent, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, AnimalKen, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, Crafts, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, Drive, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, Etiquette, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, Firearms, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, Larceny, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, Melee, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, Performance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, Stealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, Survival, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, ProfessionalSkill, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, Academics, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, Computer, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, Enigmas, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, Gremayre, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, Investigation, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, Law, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, Medicine, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, Politics, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, Science, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, Technology, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingAbilityTraitSet, ExpertKnowledge, COND_None, REPNOTIFY_Always);
}

void UChangelingAbilityTraitSet::OnRep_Alertness(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, Alertness, Old);
}

void UChangelingAbilityTraitSet::OnRep_Athletics(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, Athletics, Old);
}

void UChangelingAbilityTraitSet::OnRep_Brawl(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, Brawl, Old);
}

void UChangelingAbilityTraitSet::OnRep_Empathy(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, Empathy, Old);
}

void UChangelingAbilityTraitSet::OnRep_Expression(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, Expression, Old);
}

void UChangelingAbilityTraitSet::OnRep_Intimidation(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, Intimidation, Old);
}

void UChangelingAbilityTraitSet::OnRep_Kenning(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, Kenning, Old);
}

void UChangelingAbilityTraitSet::OnRep_Leadership(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, Leadership, Old);
}

void UChangelingAbilityTraitSet::OnRep_Streetwise(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, Streetwise, Old);
}

void UChangelingAbilityTraitSet::OnRep_Subterfuge(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, Subterfuge, Old);
}

void UChangelingAbilityTraitSet::OnRep_HobbyTalent(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, HobbyTalent, Old);
}

void UChangelingAbilityTraitSet::OnRep_AnimalKen(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, AnimalKen, Old);
}

void UChangelingAbilityTraitSet::OnRep_Crafts(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, Crafts, Old);
}

void UChangelingAbilityTraitSet::OnRep_Drive(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, Drive, Old);
}

void UChangelingAbilityTraitSet::OnRep_Etiquette(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, Etiquette, Old);
}

void UChangelingAbilityTraitSet::OnRep_Firearms(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, Firearms, Old);
}

void UChangelingAbilityTraitSet::OnRep_Larceny(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, Larceny, Old);
}

void UChangelingAbilityTraitSet::OnRep_Melee(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, Melee, Old);
}

void UChangelingAbilityTraitSet::OnRep_Performance(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, Performance, Old);
}

void UChangelingAbilityTraitSet::OnRep_Stealth(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, Stealth, Old);
}

void UChangelingAbilityTraitSet::OnRep_Survival(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, Survival, Old);
}

void UChangelingAbilityTraitSet::OnRep_ProfessionalSkill(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, ProfessionalSkill, Old);
}

void UChangelingAbilityTraitSet::OnRep_Academics(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, Academics, Old);
}

void UChangelingAbilityTraitSet::OnRep_Computer(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, Computer, Old);
}

void UChangelingAbilityTraitSet::OnRep_Enigmas(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, Enigmas, Old);
}

void UChangelingAbilityTraitSet::OnRep_Gremayre(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, Gremayre, Old);
}

void UChangelingAbilityTraitSet::OnRep_Investigation(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, Investigation, Old);
}

void UChangelingAbilityTraitSet::OnRep_Law(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, Law, Old);
}

void UChangelingAbilityTraitSet::OnRep_Medicine(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, Medicine, Old);
}

void UChangelingAbilityTraitSet::OnRep_Politics(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, Politics, Old);
}

void UChangelingAbilityTraitSet::OnRep_Science(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, Science, Old);
}

void UChangelingAbilityTraitSet::OnRep_Technology(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, Technology, Old);
}

void UChangelingAbilityTraitSet::OnRep_ExpertKnowledge(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingAbilityTraitSet, ExpertKnowledge, Old);
}
