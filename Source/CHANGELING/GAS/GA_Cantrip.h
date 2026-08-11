// GA_Cantrip.h
//
// One ability class per Art, not one per cantrip.
//
// Eighteen Arts at five levels is ninety cantrips; as separate ability assets that
// is unmanageable and every shared fix has to be made ninety times. Instead the Art
// is the class, the level is a row in DT_Cantrips, and adding a cantrip is data.
//
// Casting is a HOLD. Pressing begins the channel and releasing fires it at whatever
// tier has been reached -- the replacement for the book's bunk. Being interrupted
// therefore does not waste the input; it casts weaker. Glamour is spent on release
// whatever the outcome, as the book spends it before the roll rather than after.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "CantripTypes.h"
#include "GA_Cantrip.generated.h"

class UDataTable;

UCLASS(Abstract, Blueprintable)
class CHANGELING_API UGA_Cantrip : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Cantrip();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	/** The actor this cantrip is aimed at. Unset applies the effect to the caster. */
	UFUNCTION(BlueprintCallable, Category = "Cantrip")
	void SetCantripTarget(AActor* Target) { CantripTarget = Target; }

	/**
	 * Fire the cantrip at whatever tier has been reached.
	 *
	 * Call this from the Enhanced Input Completed/Canceled event. GAS's own
	 * InputReleased only fires for abilities bound through its input-ID system, which
	 * Enhanced Input does not use -- so without this the channel would never release
	 * and every cantrip would sit until MaxHoldSeconds timed it out.
	 *
	 * Safe to call more than once; only the first does anything.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip")
	void ReleaseCantripFromInput() { ReleaseCantrip(); }

	/**
	 * Release every cantrip this actor is currently channelling. Call from the
	 * Enhanced Input Completed AND Canceled events -- one node, no ASC digging.
	 *
	 * Do NOT use AbilitySystemComponent::ReleaseInputID for this. That matches specs
	 * by the input ID they were GRANTED with, and abilities granted from
	 * DefaultAbilities have no input ID at all (-1), so it silently matches nothing
	 * and the channel hangs until MaxHoldSeconds fires it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip", meta = (DefaultToSelf = "Avatar"))
	static void ReleaseActiveCantrips(AActor* Avatar);

	/** Row data for this cantrip, or null if the table or row is unset. Public so UI
	 *  can describe a cantrip without activating it. */
	const FCantripSpec* GetCantripSpec() const { return FindSpec(); }

	FName GetCantripRowName() const { return CantripRow; }

	const TArray<ECantripRealm>& GetRealmsUsed() const { return RealmsUsed; }

	/** 0-5, rising as the cast is held. Drive the escalating VFX from this. */
	UFUNCTION(BlueprintPure, Category = "Cantrip")
	int32 GetCurrentCastTier() const;

	UFUNCTION(BlueprintPure, Category = "Cantrip")
	float GetHeldSeconds() const;

	/** 0-1 toward the next tier, for a wind-up meter that shows commitment
	 *  without ever showing a difficulty. */
	UFUNCTION(BlueprintPure, Category = "Cantrip")
	float GetTierProgress() const;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip")
	TObjectPtr<UDataTable> CantripTable;

	/** Row in CantripTable, by convention "<Art>_<Level>" — e.g. Wayfare_3. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip")
	FName CantripRow;

	/**
	 * Realms this casting aims through. The FIRST primary Realm listed that the
	 * caster actually knows supplies the dice; any beyond the first cost Glamour.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip")
	TArray<ECantripRealm> RealmsUsed = { ECantripRealm::Actor };

	/** Glamour cost, applied with a SetByCaller magnitude on Cantrip.Data.GlamourCost. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip")
	TSubclassOf<UGameplayEffect> GlamourCostEffect;

	/** Safety valve so a stuck input cannot channel forever. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip",
		meta = (ClampMin = "0.5"))
	float MaxHoldSeconds = 8.0f;

	/** Fires once the channel begins, so the caster can be rooted and lit up. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Cantrip")
	void OnChannelStarted(const FCantripSpec& Spec);

	/** Fires on release with the graded outcome. Nothing here should be shown to the
	 *  player as a number — express it as something happening in the world. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Cantrip")
	void OnCantripResolved(const FCantripOutcome& Outcome, const FCantripSpec& Spec, AActor* Target);

	/** Not enough Glamour, or no usable Realm. The cast fizzles without a roll. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Cantrip")
	void OnCantripFizzled(const FCantripSpec& Spec);

	/**
	 * Banality imposed by the PLACE, added to the caster's own.
	 *
	 * Zero by default. Override to make a shopping centre hostile to magic and a
	 * freehold generous — that difference is the setting's whole argument, and it
	 * belongs in level data rather than in this class.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Cantrip")
	int32 GetEnvironmentalBanality() const;
	virtual int32 GetEnvironmentalBanality_Implementation() const { return 0; }

	const FCantripSpec* FindSpec() const;

	/**
	 * Called once the roll is resolved and any effect applied.
	 *
	 * The base implementation ends the ability, because an ordinary cantrip is over
	 * the moment it lands. Sustained cantrips override this to stay running instead.
	 */
	virtual void PostResolve(const FCantripOutcome& Outcome, const FCantripSpec& Spec);

	void ReleaseCantrip();

private:
	bool BuildAttempt(const FCantripSpec& Spec, FCantripAttempt& OutAttempt) const;
	float ReadMagicAttribute(FName PropertyName) const;

	UPROPERTY() TObjectPtr<AActor> CantripTarget;

	float CastStartTime = 0.0f;
	bool bReleaseHandled = false;
	FTimerHandle AutoReleaseTimer;
};
