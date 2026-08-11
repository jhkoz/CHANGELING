// ChangelingAbilityTraitSet.h
//
// The book's ABILITY traits -- Talents, Skills and Knowledges -- as GAS attributes.
//
// Named "AbilityTrait" rather than "Ability" on purpose: in GAS an Ability is a
// UGameplayAbility (a thing you activate), while these are ratings you roll against.
// Conflating the two names would make every later conversation ambiguous.
//
// All run 0-5. Zero is meaningful and common: it means untrained, which for a
// Knowledge usually means you cannot attempt the roll at all, while for a Talent it
// merely costs you the dice.
//
// Kenning and Gremayre are the two that carry the setting rather than the genre --
// sensing the Dreaming, and the study of faerie lore.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "ChangelingAbilityTraitSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) 	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) 	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) 	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) 	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class CHANGELING_API UChangelingAbilityTraitSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UChangelingAbilityTraitSet();

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// -- Talents: Innate knacks, learnable without instruction.

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Alertness, Category = "Abilities|Talents")
	FGameplayAttributeData Alertness;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, Alertness)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Athletics, Category = "Abilities|Talents")
	FGameplayAttributeData Athletics;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, Athletics)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Brawl, Category = "Abilities|Talents")
	FGameplayAttributeData Brawl;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, Brawl)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Empathy, Category = "Abilities|Talents")
	FGameplayAttributeData Empathy;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, Empathy)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Expression, Category = "Abilities|Talents")
	FGameplayAttributeData Expression;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, Expression)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Intimidation, Category = "Abilities|Talents")
	FGameplayAttributeData Intimidation;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, Intimidation)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Kenning, Category = "Abilities|Talents")
	FGameplayAttributeData Kenning;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, Kenning)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Leadership, Category = "Abilities|Talents")
	FGameplayAttributeData Leadership;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, Leadership)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Streetwise, Category = "Abilities|Talents")
	FGameplayAttributeData Streetwise;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, Streetwise)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Subterfuge, Category = "Abilities|Talents")
	FGameplayAttributeData Subterfuge;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, Subterfuge)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HobbyTalent, Category = "Abilities|Talents")
	FGameplayAttributeData HobbyTalent;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, HobbyTalent)

	// -- Skills: Trained aptitudes; practice matters more than talent.

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AnimalKen, Category = "Abilities|Skills")
	FGameplayAttributeData AnimalKen;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, AnimalKen)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Crafts, Category = "Abilities|Skills")
	FGameplayAttributeData Crafts;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, Crafts)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Drive, Category = "Abilities|Skills")
	FGameplayAttributeData Drive;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, Drive)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Etiquette, Category = "Abilities|Skills")
	FGameplayAttributeData Etiquette;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, Etiquette)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Firearms, Category = "Abilities|Skills")
	FGameplayAttributeData Firearms;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, Firearms)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Larceny, Category = "Abilities|Skills")
	FGameplayAttributeData Larceny;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, Larceny)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Melee, Category = "Abilities|Skills")
	FGameplayAttributeData Melee;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, Melee)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Performance, Category = "Abilities|Skills")
	FGameplayAttributeData Performance;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, Performance)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stealth, Category = "Abilities|Skills")
	FGameplayAttributeData Stealth;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, Stealth)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Survival, Category = "Abilities|Skills")
	FGameplayAttributeData Survival;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, Survival)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ProfessionalSkill, Category = "Abilities|Skills")
	FGameplayAttributeData ProfessionalSkill;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, ProfessionalSkill)

	// -- Knowledges: Studied learning. Rarely usable untrained.

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Academics, Category = "Abilities|Knowledges")
	FGameplayAttributeData Academics;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, Academics)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Computer, Category = "Abilities|Knowledges")
	FGameplayAttributeData Computer;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, Computer)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Enigmas, Category = "Abilities|Knowledges")
	FGameplayAttributeData Enigmas;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, Enigmas)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Gremayre, Category = "Abilities|Knowledges")
	FGameplayAttributeData Gremayre;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, Gremayre)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Investigation, Category = "Abilities|Knowledges")
	FGameplayAttributeData Investigation;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, Investigation)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Law, Category = "Abilities|Knowledges")
	FGameplayAttributeData Law;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, Law)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Medicine, Category = "Abilities|Knowledges")
	FGameplayAttributeData Medicine;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, Medicine)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Politics, Category = "Abilities|Knowledges")
	FGameplayAttributeData Politics;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, Politics)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Science, Category = "Abilities|Knowledges")
	FGameplayAttributeData Science;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, Science)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Technology, Category = "Abilities|Knowledges")
	FGameplayAttributeData Technology;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, Technology)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ExpertKnowledge, Category = "Abilities|Knowledges")
	FGameplayAttributeData ExpertKnowledge;
	ATTRIBUTE_ACCESSORS(UChangelingAbilityTraitSet, ExpertKnowledge)

protected:
	UFUNCTION() void OnRep_Alertness(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Athletics(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Brawl(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Empathy(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Expression(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Intimidation(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Kenning(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Leadership(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Streetwise(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Subterfuge(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_HobbyTalent(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_AnimalKen(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Crafts(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Drive(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Etiquette(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Firearms(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Larceny(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Melee(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Performance(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Stealth(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Survival(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_ProfessionalSkill(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Academics(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Computer(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Enigmas(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Gremayre(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Investigation(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Law(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Medicine(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Politics(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Science(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Technology(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_ExpertKnowledge(const FGameplayAttributeData& Old);
};

#undef ATTRIBUTE_ACCESSORS
