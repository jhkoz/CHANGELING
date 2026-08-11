// ChangelingMagicSet.h
//
// Art and Realm ratings -- the two halves of a cantrip.
//
// An Art is the verb (what the magic does); a Realm is the noun (what you are
// permitted to aim it at). A cantrip needs both, and its dice pool is the Art rating
// plus the LOWEST primary Realm involved -- so breadth of Realms costs you nothing in
// power, but a weak Realm drags an otherwise strong Art down to its level.
//
// Fae, Actor, Nature and Prop are primary and contribute dice. Time and Scene are
// modifiers: they extend what a cantrip can reach but never add a die. That
// distinction is enforced in the resolver, not here.
//
// All ratings 0-5. Zero means the Art or Realm is simply unknown.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "ChangelingMagicSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) 	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) 	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) 	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) 	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class CHANGELING_API UChangelingMagicSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UChangelingMagicSet();

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// -- Arts: the verbs of faerie magic.

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Autumn, Category = "Magic|Arts")
	FGameplayAttributeData Autumn;
	ATTRIBUTE_ACCESSORS(UChangelingMagicSet, Autumn)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Chicanery, Category = "Magic|Arts")
	FGameplayAttributeData Chicanery;
	ATTRIBUTE_ACCESSORS(UChangelingMagicSet, Chicanery)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Chronos, Category = "Magic|Arts")
	FGameplayAttributeData Chronos;
	ATTRIBUTE_ACCESSORS(UChangelingMagicSet, Chronos)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Contract, Category = "Magic|Arts")
	FGameplayAttributeData Contract;
	ATTRIBUTE_ACCESSORS(UChangelingMagicSet, Contract)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DragonsIre, Category = "Magic|Arts")
	FGameplayAttributeData DragonsIre;
	ATTRIBUTE_ACCESSORS(UChangelingMagicSet, DragonsIre)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Legerdemain, Category = "Magic|Arts")
	FGameplayAttributeData Legerdemain;
	ATTRIBUTE_ACCESSORS(UChangelingMagicSet, Legerdemain)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Metamorphosis, Category = "Magic|Arts")
	FGameplayAttributeData Metamorphosis;
	ATTRIBUTE_ACCESSORS(UChangelingMagicSet, Metamorphosis)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Naming, Category = "Magic|Arts")
	FGameplayAttributeData Naming;
	ATTRIBUTE_ACCESSORS(UChangelingMagicSet, Naming)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Oneiromancy, Category = "Magic|Arts")
	FGameplayAttributeData Oneiromancy;
	ATTRIBUTE_ACCESSORS(UChangelingMagicSet, Oneiromancy)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Primal, Category = "Magic|Arts")
	FGameplayAttributeData Primal;
	ATTRIBUTE_ACCESSORS(UChangelingMagicSet, Primal)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Pyretics, Category = "Magic|Arts")
	FGameplayAttributeData Pyretics;
	ATTRIBUTE_ACCESSORS(UChangelingMagicSet, Pyretics)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Skycraft, Category = "Magic|Arts")
	FGameplayAttributeData Skycraft;
	ATTRIBUTE_ACCESSORS(UChangelingMagicSet, Skycraft)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Soothsay, Category = "Magic|Arts")
	FGameplayAttributeData Soothsay;
	ATTRIBUTE_ACCESSORS(UChangelingMagicSet, Soothsay)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Sovereign, Category = "Magic|Arts")
	FGameplayAttributeData Sovereign;
	ATTRIBUTE_ACCESSORS(UChangelingMagicSet, Sovereign)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Spring, Category = "Magic|Arts")
	FGameplayAttributeData Spring;
	ATTRIBUTE_ACCESSORS(UChangelingMagicSet, Spring)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Summer, Category = "Magic|Arts")
	FGameplayAttributeData Summer;
	ATTRIBUTE_ACCESSORS(UChangelingMagicSet, Summer)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Wayfare, Category = "Magic|Arts")
	FGameplayAttributeData Wayfare;
	ATTRIBUTE_ACCESSORS(UChangelingMagicSet, Wayfare)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Winter, Category = "Magic|Arts")
	FGameplayAttributeData Winter;
	ATTRIBUTE_ACCESSORS(UChangelingMagicSet, Winter)

	// -- Primary Realms: these contribute dice.

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Fae, Category = "Magic|Realms")
	FGameplayAttributeData Fae;
	ATTRIBUTE_ACCESSORS(UChangelingMagicSet, Fae)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Actor, Category = "Magic|Realms")
	FGameplayAttributeData Actor;
	ATTRIBUTE_ACCESSORS(UChangelingMagicSet, Actor)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Nature, Category = "Magic|Realms")
	FGameplayAttributeData Nature;
	ATTRIBUTE_ACCESSORS(UChangelingMagicSet, Nature)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Prop, Category = "Magic|Realms")
	FGameplayAttributeData Prop;
	ATTRIBUTE_ACCESSORS(UChangelingMagicSet, Prop)

	// -- Modifier Realms: these extend reach but never add dice.

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Time, Category = "Magic|Realms")
	FGameplayAttributeData Time;
	ATTRIBUTE_ACCESSORS(UChangelingMagicSet, Time)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Scene, Category = "Magic|Realms")
	FGameplayAttributeData Scene;
	ATTRIBUTE_ACCESSORS(UChangelingMagicSet, Scene)

protected:
	UFUNCTION() void OnRep_Autumn(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Chicanery(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Chronos(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Contract(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_DragonsIre(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Legerdemain(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Metamorphosis(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Naming(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Oneiromancy(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Primal(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Pyretics(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Skycraft(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Soothsay(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Sovereign(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Spring(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Summer(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Wayfare(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Winter(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Fae(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Actor(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Nature(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Prop(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Time(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Scene(const FGameplayAttributeData& Old);
};

#undef ATTRIBUTE_ACCESSORS
