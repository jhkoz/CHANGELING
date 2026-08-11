// ChangelingAttributeSet.h
//
// The nine Attributes plus the changeling resource pools, as GAS attributes.
//
// Why attributes rather than a plain struct: everything that temporarily alters a
// character — a chimera's curse, a Treasure's blessing, the drag of a Banality-soaked
// place — becomes a stock GameplayEffect with a duration instead of bespoke code, and
// the stacking, replication and prediction come free.
//
// Ratings are 1-5 for Attributes and 0-10 for the pools, matching the book. Nothing
// here rolls dice; see StorytellerRoll.h for that. An ability reads these, builds a
// pool, and resolves out of sight of the player.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "ChangelingAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class CHANGELING_API UChangelingAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UChangelingAttributeSet();

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ── Physical ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Strength, Category = "Attributes|Physical")
	FGameplayAttributeData Strength;
	ATTRIBUTE_ACCESSORS(UChangelingAttributeSet, Strength)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Dexterity, Category = "Attributes|Physical")
	FGameplayAttributeData Dexterity;
	ATTRIBUTE_ACCESSORS(UChangelingAttributeSet, Dexterity)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stamina, Category = "Attributes|Physical")
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS(UChangelingAttributeSet, Stamina)

	// ── Social ──────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Charisma, Category = "Attributes|Social")
	FGameplayAttributeData Charisma;
	ATTRIBUTE_ACCESSORS(UChangelingAttributeSet, Charisma)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Manipulation, Category = "Attributes|Social")
	FGameplayAttributeData Manipulation;
	ATTRIBUTE_ACCESSORS(UChangelingAttributeSet, Manipulation)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Appearance, Category = "Attributes|Social")
	FGameplayAttributeData Appearance;
	ATTRIBUTE_ACCESSORS(UChangelingAttributeSet, Appearance)

	// ── Mental ──────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Perception, Category = "Attributes|Mental")
	FGameplayAttributeData Perception;
	ATTRIBUTE_ACCESSORS(UChangelingAttributeSet, Perception)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Intelligence, Category = "Attributes|Mental")
	FGameplayAttributeData Intelligence;
	ATTRIBUTE_ACCESSORS(UChangelingAttributeSet, Intelligence)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Wits, Category = "Attributes|Mental")
	FGameplayAttributeData Wits;
	ATTRIBUTE_ACCESSORS(UChangelingAttributeSet, Wits)

	// ── Pools ───────────────────────────────────────────────────────────────

	/** Faerie essence. The cost of cantrips — GAS treats it as the ability's cost. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Glamour, Category = "Attributes|Pools")
	FGameplayAttributeData Glamour;
	ATTRIBUTE_ACCESSORS(UChangelingAttributeSet, Glamour)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_GlamourMax, Category = "Attributes|Pools")
	FGameplayAttributeData GlamourMax;
	ATTRIBUTE_ACCESSORS(UChangelingAttributeSet, GlamourMax)

	/**
	 * The weight of the mundane. NOT a resource — it is never spent, it accrues, and
	 * it works against the character: it raises cantrip difficulties and resists
	 * enchantment. Rising Banality is the changeling's slow death, so anything that
	 * increases it should be a story event, not a stat tick.
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Banality, Category = "Attributes|Pools")
	FGameplayAttributeData Banality;
	ATTRIBUTE_ACCESSORS(UChangelingAttributeSet, Banality)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Willpower, Category = "Attributes|Pools")
	FGameplayAttributeData Willpower;
	ATTRIBUTE_ACCESSORS(UChangelingAttributeSet, Willpower)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_WillpowerMax, Category = "Attributes|Pools")
	FGameplayAttributeData WillpowerMax;
	ATTRIBUTE_ACCESSORS(UChangelingAttributeSet, WillpowerMax)

	/** The Unseelie counterweight to Glamour, fed by fear and cruelty. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Nightmare, Category = "Attributes|Pools")
	FGameplayAttributeData Nightmare;
	ATTRIBUTE_ACCESSORS(UChangelingAttributeSet, Nightmare)

	// ── Health ──────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Attributes|Health")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UChangelingAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HealthMax, Category = "Attributes|Health")
	FGameplayAttributeData HealthMax;
	ATTRIBUTE_ACCESSORS(UChangelingAttributeSet, HealthMax)

	/**
	 * Damage applied this execution, then consumed and zeroed.
	 *
	 * A meta attribute rather than a direct Health modifier: it gives one place to
	 * apply soak, armour and chimerical-versus-real damage before Health moves, and
	 * it never replicates.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Health")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS(UChangelingAttributeSet, IncomingDamage)

protected:
	UFUNCTION() void OnRep_Strength(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Dexterity(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Stamina(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Charisma(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Manipulation(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Appearance(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Perception(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Intelligence(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Wits(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Glamour(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_GlamourMax(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Banality(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Willpower(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_WillpowerMax(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Nightmare(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Health(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_HealthMax(const FGameplayAttributeData& Old);
};

#undef ATTRIBUTE_ACCESSORS
