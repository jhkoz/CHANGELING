// CantripPayload.h
//
// A second thing a cantrip does, bolted onto whatever its role class already does.
//
// Thirteen role classes answer "what is this cantrip FOR", and for most of the ninety one
// answer is enough. Thirty-six carry a secondary role, and a handful genuinely need the
// MACHINERY of two: Eldritch Prime conjures a stream of elemental fire and burns what it
// touches; Phoenix Song destroys and then restores; Storm Shroud protects and shocks.
//
// A class cannot have two parents, so the second role arrives as a payload instead --
// an instanced object the ability owns, applied after its own ApplyCantrip and reverted
// with it. Composition where inheritance ran out, rather than a second class tree that
// would need every Role x Role pair.
//
// Payloads are NOT a general effect system. If a cantrip needs three of them, its role
// was probably misjudged.

#pragma once

#include "CoreMinimal.h"
#include "CantripTypes.h"
#include "UObject/Object.h"
#include "CantripPayload.generated.h"

class UGA_Cantrip;

/** Base for anything a cantrip does in addition to its role. */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, Blueprintable)
class CHANGELING_API UCantripPayload : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Do the thing. Called straight after the owning ability's ApplyCantrip.
	 *
	 * The ability is passed rather than stored, so a payload can reach the shared
	 * helpers -- effect application, roll scaling -- without holding a reference that
	 * would outlive the cast.
	 */
	virtual void Apply(UGA_Cantrip* Ability, const FCantripOutcome& Outcome,
		const FCantripTargets& Targets) {}

	/** Undo it, when a sustained cantrip ends. */
	virtual void Revert(UGA_Cantrip* Ability) {}

	/** Short line for the log, so a cantrip's payloads are visible when it fires. */
	virtual FString Describe() const { return GetClass()->GetName(); }
};

/**
 * Harms what the cantrip touched.
 *
 * The same numbers UGA_DamageCantrip carries, in a form any other role can adopt. Both
 * routes end up here, so damage scaling exists once -- a second copy inside the role
 * class is exactly the sort of thing that drifts a year later.
 */
UCLASS(EditInlineNew, DefaultToInstanced, Blueprintable,
	meta = (DisplayName = "Damage Payload"))
class CHANGELING_API UCantripPayload_Damage : public UCantripPayload
{
	GENERATED_BODY()

public:
	virtual void Apply(UGA_Cantrip* Ability, const FCantripOutcome& Outcome,
		const FCantripTargets& Targets) override;

	virtual FString Describe() const override;

	/**
	 * Damage one actor, right now, at the strength this casting rolled.
	 *
	 * For damage that arrives over time from somewhere other than the cast -- the
	 * flamethrower's particles reporting what they touched, a projectile's detonation.
	 * The burn component calls this per hit rather than the ability guessing who is in
	 * the fire at the moment it was cast.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Damage")
	void DamageActor(UGA_Cantrip* Ability, AActor* Target, float Scale = 1.0f);

	/** Health levels at one success, and per success after. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float BaseDamage = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float DamagePerSuccess = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	ECantripDamageType DamageType = ECantripDamageType::Lethal;

	/** Reads the amount off Cantrip.Data.Magnitude. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	TSubclassOf<UGameplayEffect> DamageEffect;

	/**
	 * Spread the wound across the duration instead of dealing it whole.
	 *
	 * A cantrip that sets someone ablaze should burn them for as long as it burns.
	 * Ignored on an instant row, which has no duration to spread across.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	bool bDamageOverTime = false;

	/**
	 * Do nothing at the moment of casting.
	 *
	 * For a stream or a lingering flame, where the damage arrives from particle hits
	 * through DamageActor rather than from whoever happened to be standing in front of
	 * the caster when the key came up.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	bool bOnlyOnContact = false;

	/** The strength this casting rolled, remembered for DamageActor to use later. */
	UFUNCTION(BlueprintPure, Category = "Cantrip|Damage")
	float GetDamagePerHit() const { return DamagePerHit; }

private:
	float DamagePerHit = 0.0f;
};
