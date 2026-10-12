// GA_InstantCantrip.h
//
// A cantrip that happens and is over: a bolt of levin, a curse spoken, a wound closed.
//
// Twenty-five of the ninety are this, and C20 marks them by saying nothing at all about
// duration -- there is no duration to state. The working lands, the Glamour is spent, and
// the ability ends in the same breath.
//
// It exists as its own class rather than letting these Blueprints parent straight to
// UGA_Cantrip for two reasons. The DataTable's Kind and the Blueprint's parent then line
// up one-for-one, so "which class do I derive from" is answered by a column rather than by
// remembering that one of the four Kinds is special. And UGA_Cantrip stays the family
// base: anything added here is instant behaviour, not behaviour every sustained and
// channelled cantrip silently inherits.
//
// What it adds over the base is presentation and a target. The roll, the Glamour and the
// row's effect are all handled upstream; this decides WHERE the cantrip lands and what is
// seen and heard when it does.

#pragma once

#include "CoreMinimal.h"
#include "GA_Cantrip.h"
#include "GA_InstantCantrip.generated.h"

class UAnimMontage;
class UNiagaraSystem;
class USoundBase;

UCLASS(Abstract, Blueprintable)
class CHANGELING_API UGA_InstantCantrip : public UGA_Cantrip
{
	GENERATED_BODY()

public:
	/** Where the cantrip landed. Resolved by the parent from Delivery. */
	UFUNCTION(BlueprintPure, Category = "Cantrip|Instant")
	FVector GetImpactLocation() const { return ResolvedTargets.Point; }

	/** What it landed on, if anything. Null for a miss into open air. */
	UFUNCTION(BlueprintPure, Category = "Cantrip|Instant")
	AActor* GetImpactActor() const { return ResolvedTargets.Primary(); }

protected:
	virtual void PostResolve(const FCantripOutcome& Outcome, const FCantripSpec& Spec) override;

	/**
	 * Fires once the cantrip has landed, with where and what it struck.
	 *
	 * The hook for anything this particular cantrip does that no other one does --
	 * knocking a door shut, freezing a puddle, marking a name. Blueprint-only on purpose:
	 * the C++ owns the shape of a cantrip, not what any single working means.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Cantrip|Instant")
	void OnCantripLanded(const FCantripOutcome& Outcome, FVector Location, AActor* Target);

	// ── Presentation ────────────────────────────────────────────────────────

	/**
	 * CastMontage plays as the cantrip LANDS, not as it activates.
	 *
	 * An instant cantrip has already resolved by the time the arm moves, and the montage
	 * is a flourish after the fact. It is not waited on either: holding the ability open
	 * for the animation would mean a cantrip that cannot be cast again until the
	 * flourish finishes.
	 */
	virtual bool ShouldPlayCastMontageOnActivate() const override { return false; }

	/** Spawned at the impact point, oriented to the surface it struck. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Instant|Effect")
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Instant|Effect")
	TObjectPtr<USoundBase> ImpactSound;

	/**
	 * Effect scale at one success, and what each further success adds.
	 *
	 * The roll should be visible in the result. A cantrip that looks identical whether it
	 * scraped through or landed perfectly throws away the only feedback the player gets,
	 * since the dice themselves are never shown.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Instant|Effect",
		meta = (ClampMin = "0.01"))
	float BaseEffectScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Instant|Effect",
		meta = (ClampMin = "0.0"))
	float ScalePerSuccess = 0.15f;

	/** Niagara user float the success count is pushed into. */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Cantrip|Instant|Effect")
	FName SuccessParameter = TEXT("Intensity");

};
