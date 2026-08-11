// ChangelingMagicSet.cpp

#include "ChangelingMagicSet.h"

#include "Net/UnrealNetwork.h"

UChangelingMagicSet::UChangelingMagicSet()
{
	// Everything unknown. Character creation grants Arts (3) and Realms (5), so a
	// fresh changeling knows a little of a few things rather than a little of all.
}

void UChangelingMagicSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// Arts and Realms share one range, so clamp by set. Listing twenty-four
	// comparisons would only create something to forget when the next Art arrives.
	NewValue = FMath::Clamp(NewValue, 0.0f, 5.0f);
}

void UChangelingMagicSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingMagicSet, Autumn, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingMagicSet, Chicanery, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingMagicSet, Chronos, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingMagicSet, Contract, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingMagicSet, DragonsIre, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingMagicSet, Legerdemain, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingMagicSet, Metamorphosis, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingMagicSet, Naming, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingMagicSet, Oneiromancy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingMagicSet, Primal, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingMagicSet, Pyretics, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingMagicSet, Skycraft, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingMagicSet, Soothsay, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingMagicSet, Sovereign, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingMagicSet, Spring, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingMagicSet, Summer, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingMagicSet, Wayfare, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingMagicSet, Winter, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingMagicSet, Fae, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingMagicSet, Actor, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingMagicSet, Nature, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingMagicSet, Prop, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingMagicSet, Time, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UChangelingMagicSet, Scene, COND_None, REPNOTIFY_Always);
}

void UChangelingMagicSet::OnRep_Autumn(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingMagicSet, Autumn, Old);
}

void UChangelingMagicSet::OnRep_Chicanery(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingMagicSet, Chicanery, Old);
}

void UChangelingMagicSet::OnRep_Chronos(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingMagicSet, Chronos, Old);
}

void UChangelingMagicSet::OnRep_Contract(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingMagicSet, Contract, Old);
}

void UChangelingMagicSet::OnRep_DragonsIre(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingMagicSet, DragonsIre, Old);
}

void UChangelingMagicSet::OnRep_Legerdemain(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingMagicSet, Legerdemain, Old);
}

void UChangelingMagicSet::OnRep_Metamorphosis(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingMagicSet, Metamorphosis, Old);
}

void UChangelingMagicSet::OnRep_Naming(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingMagicSet, Naming, Old);
}

void UChangelingMagicSet::OnRep_Oneiromancy(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingMagicSet, Oneiromancy, Old);
}

void UChangelingMagicSet::OnRep_Primal(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingMagicSet, Primal, Old);
}

void UChangelingMagicSet::OnRep_Pyretics(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingMagicSet, Pyretics, Old);
}

void UChangelingMagicSet::OnRep_Skycraft(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingMagicSet, Skycraft, Old);
}

void UChangelingMagicSet::OnRep_Soothsay(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingMagicSet, Soothsay, Old);
}

void UChangelingMagicSet::OnRep_Sovereign(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingMagicSet, Sovereign, Old);
}

void UChangelingMagicSet::OnRep_Spring(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingMagicSet, Spring, Old);
}

void UChangelingMagicSet::OnRep_Summer(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingMagicSet, Summer, Old);
}

void UChangelingMagicSet::OnRep_Wayfare(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingMagicSet, Wayfare, Old);
}

void UChangelingMagicSet::OnRep_Winter(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingMagicSet, Winter, Old);
}

void UChangelingMagicSet::OnRep_Fae(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingMagicSet, Fae, Old);
}

void UChangelingMagicSet::OnRep_Actor(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingMagicSet, Actor, Old);
}

void UChangelingMagicSet::OnRep_Nature(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingMagicSet, Nature, Old);
}

void UChangelingMagicSet::OnRep_Prop(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingMagicSet, Prop, Old);
}

void UChangelingMagicSet::OnRep_Time(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingMagicSet, Time, Old);
}

void UChangelingMagicSet::OnRep_Scene(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UChangelingMagicSet, Scene, Old);
}

