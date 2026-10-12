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

class UAnimMontage;
class UCantripPayload;
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

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	/** The actor this cantrip is aimed at. Unset applies the effect to the caster. */
	UFUNCTION(BlueprintCallable, Category = "Cantrip")
	void SetCantripTarget(AActor* Target) { CantripTarget = Target; }

	/** Whatever SetCantripTarget was handed, or null for an untargeted casting. */
	UFUNCTION(BlueprintPure, Category = "Cantrip")
	AActor* GetCantripTarget() const { return CantripTarget; }

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

	// ── Rooting ─────────────────────────────────────────────────────────────

	/**
	 * Plant the caster for as long as this cantrip is being cast.
	 *
	 * Off by default, because most cantrips are a gesture and a changeling who cannot
	 * walk while she works is a changeling who never casts in a fight. On for the ones
	 * where standing still IS the working -- Creation defaults it true, since shaping a
	 * thing out of nothing while strolling reads as nothing happening at all.
	 *
	 * Released when the ability ends, however it ends. The lock lives on the character
	 * rather than here, so it survives this ability being destroyed mid-cast; this only
	 * ever asks, and only ever un-asks for a lock it actually took.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Rooting")
	bool bLockMovementWhileCasting = false;

	/**
	 * How long the character may stay rooted after this ability ends, waiting for the
	 * recovery animation to report in.
	 *
	 * Zero frees her on the frame the cantrip ends, which is right for a cantrip with no
	 * recovery to play. A channelled working sets this, because its End clip is still
	 * running when the ability is over and a caster who regains her feet on that frame
	 * slides through it.
	 *
	 * The character owns the wait, not this ability: it un-roots itself when the
	 * `CantripRecovered` notify arrives OR when this deadline passes, so the lock cannot
	 * outlive the cast even if the animation never reports.
	 */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, BlueprintReadOnly, Category = "Cantrip|Rooting",
		meta = (ClampMin = "0.0", EditCondition = "bLockMovementWhileCasting"))
	float CastRecoveryTimeout = 0.0f;

	/**
	 * Whether the root is taken as the ability activates.
	 *
	 * True for an ordinary cantrip: the wind-up already commits her. Channelled overrides
	 * it to false and takes the root itself once the working is actually OPEN -- rooting
	 * during a wind-up that the roll may yet fail would plant her for a cantrip that never
	 * happened.
	 */
	virtual bool ShouldTakeRootOnActivate() const { return true; }

	/** Plant the caster now. For a role that roots at a moment of its own choosing. */
	void TakeCastMovementLock();

	/** Give the root back, honouring CastRecoveryTimeout. Safe to call when not rooted. */
	void ReleaseCastMovementLock();

	// ── Aiming ──────────────────────────────────────────────────────────────

	/**
	 * Hold the aiming stance for as long as this cantrip is cast.
	 *
	 * The stance is what turns the spine toward the camera, and the spine twist is what
	 * `ECantripAimSource::ViewDirection` reads -- it uses where the BODY got to, not the
	 * raw eyeline. Without this, `bAiming` is false unless the player is separately
	 * holding an aim input, `SpineAim` stays at zero, and a cantrip that claims to aim
	 * where you look quietly fires along the hips instead.
	 *
	 * Off by default. On for Channelled, where pointing the thing IS the cantrip.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Aiming")
	bool bAimWhileCasting = false;

	// ── Animation ───────────────────────────────────────────────────────────
	//
	// Here on the base rather than on the roles, because the gesture is a property of
	// the CANTRIP, not of what the cantrip happens to do. It lived on Instant and on
	// Projectile before, under the same name and to the same purpose, which left the
	// thirteen role classes -- where most cantrips are actually parented -- unable to
	// move an arm at all.

	/**
	 * Played on the caster while this cantrip is cast. Optional.
	 *
	 * Which part of the body it claims is decided by the montage asset's SLOT, not by
	 * anything here: UpperBody for a gesture she can walk through, DefaultSlot for a
	 * working that plants her feet. Every cantrip in the game can therefore share one
	 * pair of Slot nodes in the AnimGraph, and adding a cantrip never touches it.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Animation")
	TObjectPtr<UAnimMontage> CastMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Animation",
		meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.0f;

	/**
	 * Whether the base should start CastMontage when the channel opens.
	 *
	 * True for an ordinary cantrip, where the gesture IS the wind-up. Overridden to
	 * false by the roles that own their montage's timing for their own reasons --
	 * Projectile ends the ability on it, Instant fires it late and deliberately does
	 * not wait. Those play the same property themselves; this only decides who starts it.
	 */
	virtual bool ShouldPlayCastMontageOnActivate() const { return true; }

	/**
	 * Start CastMontage through the ASC. Returns its length, or 0 if nothing played.
	 *
	 * Through the ability system rather than straight at the AnimInstance so GAS knows
	 * this ability owns the montage -- which is what lets it be stopped when the ability
	 * is cancelled instead of outliving the cast that started it.
	 */
	float PlayCastMontage();

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

	// ── Delivery, targeting and lifetime ────────────────────────────────────
	//
	// Shared by every role class, so that thirteen of them do not each re-solve where a
	// cantrip lands or how long it stays. A role decides WHAT happens; these decide to
	// whom, and for how long.

	/** How this cantrip reaches what it acts on. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Delivery")
	ECantripDelivery Delivery = ECantripDelivery::SelfCast;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Delivery",
		meta = (ClampMin = "0.0",
			EditCondition = "Delivery == ECantripDelivery::AimTrace"))
	float MaxRange = 2000.0f;

	/** Radius of an Area Burst, and of the sweep that finds who a Projectile caught. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Delivery",
		meta = (ClampMin = "0.0"))
	float EffectRadius = 300.0f;

	/** Include the caster in an Area Burst. Off for anything hostile. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Delivery")
	bool bAreaHitsCaster = false;

	/**
	 * How long the working holds, at one success and per success after.
	 *
	 * Read only when the row's Kind is Sustained. C20 grades most cantrips this way, so
	 * the roll decides the duration rather than an upkeep drain -- and the hold-to-cast
	 * pays twice: the beat spent committing decides both whether it lands and how long
	 * it is kept.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Lifetime",
		meta = (ClampMin = "0.0"))
	float BaseDuration = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Lifetime",
		meta = (ClampMin = "0.0"))
	float DurationPerSuccess = 10.0f;

public:
	// Public, not protected. Both were reachable from Blueprint and from nowhere else in
	// C++, which is a strange place for them to sit: the things that end a working early
	// -- a dispel, a cutscene, the player pressing the key again -- are by definition
	// outside the ability. The hotbar's countdown has the same shape of problem.

	/**
	 * Seconds this casting has left, or 0. Drives a hotbar's countdown.
	 *
	 * Also the honest test for "has this working LANDED": it reports zero until
	 * PostResolve arms the duration timer, so a cantrip still winding up reads as zero
	 * and one in force does not.
	 */
	UFUNCTION(BlueprintPure, Category = "Cantrip|Lifetime")
	float GetRemainingDuration() const;

	/** End a running cantrip early -- a dispel, an interruption, the player toggling off. */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Lifetime")
	void EndCantrip();

protected:
	/**
	 * What this cantrip DOES. The one hook a role class overrides.
	 *
	 * Everything around it is already handled: the roll succeeded, the Glamour is spent,
	 * the targets are resolved and the lifetime is about to be managed. A role
	 * implementation should do its own thing and nothing else.
	 */
	virtual void ApplyCantrip(const FCantripOutcome& Outcome, const FCantripSpec& Spec,
		const FCantripTargets& Targets) {}

	/** Undo whatever ApplyCantrip did. Only called for a Sustained or Channelled cantrip,
	 *  when its duration runs out or something ends it early. */
	virtual void RevertCantrip() {}

	/** Blueprint half of ApplyCantrip, fired straight after it. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Cantrip")
	void OnCantripApplied(const FCantripOutcome& Outcome, const FCantripTargets& Targets);

	/** Blueprint half of RevertCantrip. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Cantrip")
	void OnCantripReverted();

	/** Where this casting landed and who it caught. Valid from ApplyCantrip onward. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	FCantripTargets ResolvedTargets;

	/** Resolves Delivery into targets. Called for you before ApplyCantrip. */
	void ResolveTargets(FCantripTargets& Out) const;

	/** Everyone within EffectRadius of a point, honouring bAreaHitsCaster. */
	void GatherArea(const FVector& Origin, FCantripTargets& Out) const;

public:
	// ── Applying effects ────────────────────────────────────────────────────
	//
	// Shared so thirteen role classes do not each write the same GAS boilerplate. The
	// magnitude always rides on Cantrip.Data.Magnitude: every role asks the roll the
	// same question -- how much -- and the effect asset decides what that means.

	/**
	 * Apply one effect to one actor. Returns a handle, invalid on failure.
	 *
	 * Secondary is unset for almost everything, and should stay that way -- one magnitude
	 * covers every role because every role asks the roll the same question. It exists for
	 * the effects that need two INDEPENDENT feelings tuned against each other, of which
	 * there is currently exactly one (see Cantrip.Data.Secondary). An effect that does not
	 * declare a modifier on that tag simply ignores whatever is passed here.
	 */
	FActiveGameplayEffectHandle ApplyEffectTo(AActor* Target,
		TSubclassOf<UGameplayEffect> Effect, float Magnitude,
		TOptional<float> Secondary = TOptional<float>()) const;

	/** Apply to everyone resolved, collecting handles so RevertCantrip can undo them. */
	void ApplyEffectToTargets(const FCantripTargets& Targets,
		TSubclassOf<UGameplayEffect> Effect, float Magnitude,
		TArray<FActiveGameplayEffectHandle>& OutHandles,
		TOptional<float> Secondary = TOptional<float>()) const;

	/** Remove effects applied earlier, and forget them. */
	void RemoveAppliedEffects(TArray<FActiveGameplayEffectHandle>& Handles) const;

	/** Base + per-success, the shape every role scales its numbers by. */
	static float ScaleByRoll(float Base, float PerSuccess, const FCantripOutcome& Outcome)
	{
		return Base + (PerSuccess * FMath::Max(0, Outcome.Successes - 1));
	}

	/** How long this casting will hold, for anything that needs to spread itself across
	 *  the duration. Zero for an instant row. */
	UFUNCTION(BlueprintPure, Category = "Cantrip|Lifetime")
	float GetTotalDuration(const FCantripOutcome& Outcome) const;

protected:
	// ── Payloads ────────────────────────────────────────────────────────────

	/**
	 * Second things this cantrip does, beyond its role.
	 *
	 * A class cannot have two parents, but a few cantrips genuinely need the machinery of
	 * two roles -- Eldritch Prime conjures a stream of fire AND burns what it touches.
	 * Those arrive here instead: applied after the role's own ApplyCantrip and reverted
	 * with it.
	 *
	 * If a cantrip needs three, its role was probably misjudged.
	 */
	UPROPERTY(EditDefaultsOnly, Instanced, BlueprintReadOnly, Category = "Cantrip|Payloads")
	TArray<TObjectPtr<UCantripPayload>> Payloads;

	/** The first payload of a kind, for a Blueprint or a component that needs to reach
	 *  one -- the burn component asking its damage payload to hurt what it just burned. */
	UFUNCTION(BlueprintPure, Category = "Cantrip|Payloads",
		meta = (DeterminesOutputType = "PayloadClass"))
	UCantripPayload* FindPayload(TSubclassOf<UCantripPayload> PayloadClass) const;

protected:
	// ── The wind-up tag ─────────────────────────────────────────────────────
	//
	// Cantrip.State.Casting is applied and removed by hand rather than through
	// ActivationOwnedTags, because GAS holds an owned tag for the ability's ENTIRE
	// lifetime and the whole point is that this one has to come off early -- the instant
	// a working stops being cast and starts merely holding.
	//
	// The old comment on the constructor was right that a hand-managed flag is a flag
	// somebody forgets to lower, so it is lowered in exactly one guaranteed place:
	// EndAbility, which every exit path already runs through, on top of the early drop.
	// DropCastingTag is idempotent, so the two never fight.
	//
	// Protected rather than private because UGA_SustainedCantrip does not route through
	// the base PostResolve at all -- it opens the working itself in BeginSustain, and
	// that is the moment its hands come free.

	void TakeCastingTag();

	/** Free her hands. Safe to call twice, and safe to call having never taken it. */
	void DropCastingTag();

private:
	bool BuildAttempt(const FCantripSpec& Spec, FCantripAttempt& OutAttempt) const;
	float ReadMagicAttribute(FName PropertyName) const;

	/** Whether THIS casting is the one holding the lock. Checked before releasing, so a
	 *  cantrip that never rooted cannot free a caster something else has planted. */
	bool bMovementLocked = false;

	bool bCastingTagHeld = false;

	/** Enter and leave the aiming stance, remembering whether we were the one to enter
	 *  it — a cantrip must not switch off a stance the player was already holding. */
	void TakeCastAim();
	void ReleaseCastAim();

	bool bAimTaken = false;
	bool bAimWasAlreadyHeld = false;

	UPROPERTY() TObjectPtr<AActor> CantripTarget;

	float CastStartTime = 0.0f;
	bool bReleaseHandled = false;
	FTimerHandle AutoReleaseTimer;

	/** Runs down a Sustained cantrip. Channelled ones end on input instead. */
	FTimerHandle DurationTimer;
	float DurationEndTime = 0.0f;
	bool bCantripRunning = false;
};
