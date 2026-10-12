// GA_CantripRoles.h
//
// One ability class per cantrip ROLE -- what the working does to the world.
//
// Thirteen classes in one header on purpose. Each is a handful of properties and a single
// overridden function, because UGA_Cantrip already owns everything they share: the pool,
// the difficulty, the hold-to-cast, the Glamour, the roll, where the cantrip lands, how
// long it holds, and how an effect is applied. A role decides WHAT happens and nothing
// else. Split across twenty-six files that shape would be invisible.
//
// Every one overrides ApplyCantrip. The ones that leave something behind also override
// RevertCantrip, which the parent calls when a sustained duration runs out or something
// ends the working early.
//
// Delivery is a PROPERTY, not a class. A Damage cantrip may be a touch, a look or a
// thrown fireball, and so may a Healing one -- expressed as classes that would be fifty
// combinations rather than thirteen.

#pragma once

#include "CoreMinimal.h"
#include "GA_Cantrip.h"
#include "GA_CantripRoles.generated.h"

class UNiagaraSystem;
class USoundBase;

// ── Damage ──────────────────────────────────────────────────────────────────

/**
 * Harms directly. Nine of the ninety, which is fewer than a fight-first design expects.
 */
UCLASS(Abstract, Blueprintable)
class CHANGELING_API UGA_DamageCantrip : public UGA_Cantrip
{
	GENERATED_BODY()

protected:
	virtual void ApplyCantrip(const FCantripOutcome& Outcome, const FCantripSpec& Spec,
		const FCantripTargets& Targets) override;

	/** Health levels at one success, and per success after. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Damage")
	float BaseDamage = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Damage")
	float DamagePerSuccess = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Damage")
	ECantripDamageType DamageType = ECantripDamageType::Lethal;

	/** Carries the magnitude on Cantrip.Data.Magnitude. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Damage")
	TSubclassOf<UGameplayEffect> DamageEffect;

	/**
	 * Damage over the duration rather than all at once. Sustained rows only.
	 *
	 * A cantrip that sets someone ablaze should burn them for as long as it burns, not
	 * hand them the whole wound the instant it catches.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Damage")
	bool bDamageOverTime = false;
};

// ── Control ─────────────────────────────────────────────────────────────────

/** What the target loses. */
UENUM(BlueprintType)
enum class ECantripControlKind : uint8
{
	Root    UMETA(DisplayName = "Root (held in place)"),
	Stun    UMETA(DisplayName = "Stun (cannot act)"),
	Fear    UMETA(DisplayName = "Fear (flees)"),
	Compel  UMETA(DisplayName = "Compel (obeys)"),
	Sleep   UMETA(DisplayName = "Sleep"),
	Silence UMETA(DisplayName = "Silence (cannot cast)")
};

/**
 * Takes away the target's choices. The largest role at fourteen, and twelve of those
 * hold for a duration -- Changeling's answer to a fight is mostly to stop it.
 */
UCLASS(Abstract, Blueprintable)
class CHANGELING_API UGA_ControlCantrip : public UGA_Cantrip
{
	GENERATED_BODY()

protected:
	virtual void ApplyCantrip(const FCantripOutcome& Outcome, const FCantripSpec& Spec,
		const FCantripTargets& Targets) override;
	virtual void RevertCantrip() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Control")
	ECantripControlKind ControlKind = ECantripControlKind::Root;

	/** Should apply the state tag and whatever blocks movement or input. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Control")
	TSubclassOf<UGameplayEffect> ControlEffect;

	/**
	 * Successes the target may roll off with, per success of the caster's.
	 *
	 * The book lets most control be resisted. Zero makes it absolute, which should be
	 * rare -- an unbreakable hold is the difference between a fight and a cutscene.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Control",
		meta = (ClampMin = "0.0"))
	float ResistDifficultyPerSuccess = 1.0f;

private:
	UPROPERTY() TArray<FActiveGameplayEffectHandle> Applied;
};

// ── Debuff and Buff ─────────────────────────────────────────────────────────

/** Leaves the target able to act, but worse at it. */
UCLASS(Abstract, Blueprintable)
class CHANGELING_API UGA_DebuffCantrip : public UGA_Cantrip
{
	GENERATED_BODY()

protected:
	virtual void ApplyCantrip(const FCantripOutcome& Outcome, const FCantripSpec& Spec,
		const FCantripTargets& Targets) override;
	virtual void RevertCantrip() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Debuff")
	TSubclassOf<UGameplayEffect> DebuffEffect;

	/** Dice taken away at one success, and per success after. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Debuff")
	float BasePenalty = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Debuff")
	float PenaltyPerSuccess = 1.0f;

private:
	UPROPERTY() TArray<FActiveGameplayEffectHandle> Applied;
};

/** Makes the caster or an ally better at something. */
UCLASS(Abstract, Blueprintable)
class CHANGELING_API UGA_BuffCantrip : public UGA_Cantrip
{
	GENERATED_BODY()

protected:
	virtual void ApplyCantrip(const FCantripOutcome& Outcome, const FCantripSpec& Spec,
		const FCantripTargets& Targets) override;
	virtual void RevertCantrip() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Buff")
	TSubclassOf<UGameplayEffect> BuffEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Buff")
	float BaseBonus = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Buff")
	float BonusPerSuccess = 1.0f;

private:
	UPROPERTY() TArray<FActiveGameplayEffectHandle> Applied;
};

// ── Protection ──────────────────────────────────────────────────────────────

/**
 * Turns harm aside. Its own role rather than a Buff because it answers an attack rather
 * than improving a capability, and the player reaches for it at a different moment.
 */
UCLASS(Abstract, Blueprintable)
class CHANGELING_API UGA_ProtectionCantrip : public UGA_Cantrip
{
	GENERATED_BODY()

protected:
	virtual void ApplyCantrip(const FCantripOutcome& Outcome, const FCantripSpec& Spec,
		const FCantripTargets& Targets) override;
	virtual void RevertCantrip() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Protection")
	TSubclassOf<UGameplayEffect> ProtectionEffect;

	/** Health levels of harm turned aside, or dice of soak, as the effect reads it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Protection")
	float BaseMitigation = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Protection")
	float MitigationPerSuccess = 1.0f;

	/**
	 * Ends the moment the shield is spent, rather than running its full duration.
	 *
	 * A ward that has already absorbed everything it can is a lie on the status bar.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Protection")
	bool bEndsWhenSpent = true;

private:
	UPROPERTY() TArray<FActiveGameplayEffectHandle> Applied;
};

// ── Healing ─────────────────────────────────────────────────────────────────

/** Restores what was lost. */
UCLASS(Abstract, Blueprintable)
class CHANGELING_API UGA_HealingCantrip : public UGA_Cantrip
{
	GENERATED_BODY()

protected:
	virtual void ApplyCantrip(const FCantripOutcome& Outcome, const FCantripSpec& Spec,
		const FCantripTargets& Targets) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Healing")
	TSubclassOf<UGameplayEffect> HealEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Healing")
	float BaseHealing = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Healing")
	float HealingPerSuccess = 1.0f;

	/**
	 * Also mends aggravated wounds -- the ones that do not simply heal.
	 *
	 * Off by default and deliberately expensive to turn on: if every healing cantrip
	 * clears aggravated damage, cold iron stops meaning anything.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Healing")
	bool bMendsAggravated = false;

	/** Raise the dead rather than heal the living. Renewal, and little else. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Healing")
	bool bRestoresLife = false;
};

// ── Mobility ────────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class ECantripMobilityMode : uint8
{
	Leap     UMETA(DisplayName = "Leap (launched along the aim)"),
	Blink    UMETA(DisplayName = "Blink (instant, to the target point)"),
	Speed    UMETA(DisplayName = "Speed (movement multiplied)"),
	Flight   UMETA(DisplayName = "Flight"),
	Portal   UMETA(DisplayName = "Portal (a door that others may use)"),

	/**
	 * Held rather than spent: for as long as it lasts, a HELD jump carries and drifts.
	 *
	 * Distinct from Leap, which is one launch at the moment of casting. Leap is a
	 * changeling clearing a wall; Bound is a changeling for whom the ground has stopped
	 * being the argument it was. The book's Hopscotch reads either way, and this is the
	 * one that gives the player something to get good AT rather than something to point
	 * at a wall.
	 *
	 * Two clocks, deliberately. The cantrip's duration is how long she MAY bound; the
	 * jump control is when she IS -- the effect carries an ongoing requirement on
	 * State.JumpHeld, so letting go returns full gravity on that frame. Choosing when to
	 * stop flying is choosing where to land, and dropping on purpose is half of what the
	 * ability is for.
	 */
	Bound    UMETA(DisplayName = "Bound (held jumps carry and drift)")
};

/** Moves a body through space that would otherwise stop it. Wayfare is five of these. */
UCLASS(Abstract, Blueprintable)
class CHANGELING_API UGA_MobilityCantrip : public UGA_Cantrip
{
	GENERATED_BODY()

protected:
	virtual void ApplyCantrip(const FCantripOutcome& Outcome, const FCantripSpec& Spec,
		const FCantripTargets& Targets) override;
	virtual void RevertCantrip() override;

	/**
	 * Safety net for the infinite movement effect.
	 *
	 * Speed, Bound and Flight apply an effect that only RevertCantrip removes, and
	 * RevertCantrip is reached only from a Sustained row's duration timer. Any other exit
	 * -- a misconfigured row, a cancel that bypasses EndCantrip, dying mid-flight --
	 * would otherwise leave the effect applied forever, which for Flight means a
	 * character permanently in the air with no way down.
	 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Mobility")
	ECantripMobilityMode Mode = ECantripMobilityMode::Leap;

	/**
	 * Centimetres for Leap and Blink; a BONUS MULTIPLIER for Speed, Bound and Flight.
	 *
	 * The two readings are wildly different in scale and that is worth saying twice: a
	 * Speed row wants 1.5 here, not 600. Left as one property because it is one question
	 * -- how much did the roll buy -- and two properties would mean every row carrying a
	 * number that does nothing.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Mobility")
	float BaseDistance = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Mobility")
	float DistancePerSuccess = 400.0f;

	/** Applied for Speed, Bound and Flight, which change how the body moves rather than
	 *  where it is. See GE_CantripMovement.h for the three that fit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Mobility")
	TSubclassOf<UGameplayEffect> MovementEffect;

	// ── Blink ───────────────────────────────────────────────────────────────

	/**
	 * Whether a blink may cross solid matter, or must stop at the first thing in the way.
	 *
	 * True is the whole point of the cantrip. Swept, a blink slides to the nearest wall and
	 * is strictly worse than running -- it costs Glamour to travel less far than her own
	 * legs would. Going THROUGH the wall is what makes it worth casting.
	 *
	 * False is the conservative reading, for a level where being on the far side of an
	 * arbitrary wall would break something.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Mobility|Blink")
	bool bBlinkThroughObstacles = true;

	/**
	 * How far below a landing spot to look for ground, in centimetres.
	 *
	 * A blink aimed at a wall, or up a slope, resolves to a point in mid-air. Dropped to the
	 * floor beneath it she arrives standing; left where the aim landed she arrives falling,
	 * which reads as the cantrip having misfired even when it went exactly where it was told.
	 *
	 * Beyond this she is left where she was aimed and simply falls, which is correct: a
	 * blink out over a chasm should put her over the chasm.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Mobility|Blink",
		meta = (ClampMin = "0.0"))
	float BlinkGroundSearch = 500.0f;

	/**
	 * How many spots to try between the target and where she stands, if the target is solid.
	 *
	 * A blink into a wall should not simply fail -- she meant to go that way, and the
	 * nearest place along that line where she fits is a better answer than nothing. Walking
	 * BACK from the target rather than forward from her, so a blocked blink still travels as
	 * far as it possibly can.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Mobility|Blink",
		meta = (ClampMin = "1", ClampMax = "32"))
	int32 BlinkLandingAttempts = 8;

	/**
	 * Spawned at the place she LEFT, unattached, as she goes.
	 *
	 * Unattached is the whole point: a system parented to her would teleport with her, and
	 * you would see one puff jump across the room rather than a departure and an arrival.
	 * Both of these are spawned at world locations and then left behind to finish.
	 *
	 * The vanishing and the appearing are separate slots because they are usually not the
	 * same effect -- smoke rushing INTO the space she vacated reads differently from smoke
	 * thrown OUT of the space she displaced. Setting both to one system is fine and reads
	 * perfectly well; having to is not.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Mobility|Blink")
	TObjectPtr<class UNiagaraSystem> BlinkDepartureEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Mobility|Blink")
	TObjectPtr<class UNiagaraSystem> BlinkArrivalEffect;

	/** Scale for both, in case the chosen system is authored bigger than a person. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Mobility|Blink",
		meta = (ClampMin = "0.01"))
	float BlinkEffectScale = 1.0f;

	/**
	 * Lifted from the actor's origin so the cloud sits around her body, not at her ankles.
	 *
	 * Blink locations are capsule CENTRES, so zero is already chest height -- but a smoke
	 * system authored to billow upward from its origin wants to start lower than that.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Mobility|Blink")
	FVector BlinkEffectOffset = FVector(0.0f, 0.0f, -40.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Mobility|Blink")
	TObjectPtr<class USoundBase> BlinkDepartureSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Mobility|Blink")
	TObjectPtr<class USoundBase> BlinkArrivalSound;

	/** Fires where she LEFT, before she is moved. For anything beyond the slots above. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Cantrip|Mobility|Blink")
	void OnBlinkDeparted(const FVector& From);

	/** Fires where she ARRIVED, after the move. The pair for the appearing effect. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Cantrip|Mobility|Blink")
	void OnBlinkArrived(const FVector& To);

	/**
	 * How fast she falls while Bound AND holding jump, as a fraction of normal.
	 *
	 * Authored rather than rolled, deliberately. A better roll should let her clear
	 * something she could not clear before -- it should not make her hang in the air
	 * longer, because hanging longer is only ever a longer wait for the ground.
	 *
	 * A fifth is the value that plays: enough that a fall from a roof becomes a decision
	 * rather than a mistake, not so little that she stops coming down at all. It costs
	 * less than it looks like it should, because it only applies while the player is
	 * actively asking for it -- the moment they let go she falls like anyone else.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Mobility",
		meta = (ClampMin = "0.01", ClampMax = "1.0",
			EditCondition = "Mode == ECantripMobilityMode::Bound"))
	float BoundFallScale = 0.2f;

	/**
	 * Spawned for Portal -- the door itself, which outlives the casting.
	 *
	 * Typed as ACantripPortal rather than a bare actor because the door has to be told
	 * how long to last and has to know how to shut itself. Portal Passage is an INSTANT
	 * row: the ability is finished the moment the door exists, so anything the ability
	 * still had to do for it afterwards would simply never happen.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Mobility")
	TSubclassOf<class ACantripPortal> PortalClass;

	/** Seconds the door stands fully open at one success, and per success after. The
	 *  grow and shrink are on top, so this is time it is actually usable. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Mobility",
		meta = (ClampMin = "0.0"))
	float PortalBaseLifetime = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Mobility",
		meta = (ClampMin = "0.0"))
	float PortalLifetimePerSuccess = 15.0f;

	/**
	 * How thick an obstacle she can open a way through, at one success and per success after.
	 *
	 * This is what the roll BUYS. Lifetime above is how long the door stands; this is whether
	 * there is a door at all, and it is the more interesting of the two -- a marginal success
	 * gets her through a partition, a good one through a curtain wall.
	 *
	 * Failing on thickness is a real outcome, not an error. A cantrip that always works
	 * regardless of what it is aimed at has no geography in it.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Mobility",
		meta = (ClampMin = "0.0"))
	float PortalBaseThickness = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Mobility",
		meta = (ClampMin = "0.0"))
	float PortalThicknessPerSuccess = 60.0f;

	/**
	 * Opening height at one success, in world units. Zero measures the caster instead.
	 *
	 * A marginal success should buy a way through and nothing more -- a door she has to duck
	 * into, not a triumphal arch. Zero is the better answer for a player character, since it
	 * derives the clearance from her actual capsule and so stays correct if she is ever
	 * rescaled, mounted, or replaced by a taller model.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Mobility",
		meta = (ClampMin = "0.0"))
	float PortalBaseClearance = 0.0f;

	/**
	 * Extra height per success beyond the first.
	 *
	 * What a good roll looks like when it is not buying thickness: a wider way, which others
	 * can follow her through more easily. Kept modest -- a five-success door should be
	 * generous, not a hole in the side of the building.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Mobility",
		meta = (ClampMin = "0.0"))
	float PortalClearancePerSuccess = 35.0f;

	/**
	 * How far from vertical a surface may lean and still take a door, in degrees.
	 *
	 * A portal is a doorway, and a doorway in the floor is a trapdoor she falls through --
	 * the near door would open under her feet, the far one in the ceiling of the room below,
	 * and walking "into" either is meaningless. Refusing the cast is the honest outcome.
	 *
	 * Thirty allows a leaning wall or a buttress while still rejecting anything walkable.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Mobility",
		meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float PortalMaxSurfaceLean = 30.0f;

	/**
	 * How far ahead to look for a wall when the aim lands on the floor.
	 *
	 * In third person the aim ray leaves her eyes along the CAMERA's pitch, and the camera
	 * looks down at her -- so a player aiming at the wall in front of them very often puts
	 * the ray into the ground a few metres short of it. Rather than refuse a casting that
	 * was plainly meant for that wall, the cantrip looks for it: level, straight ahead, from
	 * chest height.
	 *
	 * A recovery, not the primary aim. If she really is standing in open ground facing
	 * nothing, this finds nothing and the casting still fails honestly.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Mobility",
		meta = (ClampMin = "0.0"))
	float PortalWallProbeRange = 400.0f;

	/**
	 * How far the two doors sit off the faces they are cut into.
	 *
	 * Not zero. A door coplanar with a wall z-fights with it along its whole frame, and the
	 * threshold volume ends up half-buried in geometry, which makes entering it depend on
	 * exactly where the character's capsule is rather than on walking into the door.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Mobility",
		meta = (ClampMin = "0.0"))
	float PortalSurfaceOffset = 8.0f;

private:
	/**
	 * Finds the far side of the obstacle at Entry, looking along Direction.
	 *
	 * Traces BACKWARDS from a point beyond the wall toward the entry face. A forward trace
	 * cannot do this: it starts inside the geometry and reports nothing, because a trace
	 * beginning within a collider does not register leaving it.
	 *
	 * Returns false when there is no far face within Thickness -- the wall is too thick, or
	 * it is a solid mass rather than a wall, or she aimed at terrain. That is a failed
	 * casting, not a malfunction.
	 */
	bool FindFarFace(const FVector& Entry, const FVector& Direction, float Thickness,
		const AActor* Caster, FVector& OutExit) const;

	/**
	 * Somewhere along the blink's line that she actually fits, standing on something.
	 *
	 * Two problems solved together. A destination may be inside geometry -- she aimed at a
	 * wall, or at a pillar -- and it may be in mid-air, because an aim ray that struck a
	 * vertical face resolves to a point on that face rather than to any floor.
	 *
	 * Candidates are tried from the TARGET backwards toward her, so a blocked blink still
	 * carries her as far along the line as it can rather than failing outright. Each is
	 * dropped to the ground beneath it, then tested with her own capsule.
	 *
	 * Returns false only when every candidate is solid, which means the whole line is
	 * inside something. That is a genuine failure and worth reporting.
	 */
	bool FindBlinkLanding(const FVector& From, const FVector& To, const AActor* Caster,
		FVector& OutLanding) const;

	UPROPERTY() TArray<FActiveGameplayEffectHandle> Applied;

	/** Weak, and only so a dispel can ask it to shut early. The door owns its own life
	 *  and will be long gone -- correctly -- while this still points at nothing. */
	UPROPERTY() TWeakObjectPtr<class ACantripPortal> SpawnedPortal;

	// No list of who is flying. This class used to keep one and set MOVE_Flying itself,
	// and that is exactly why Wind Runner could not land: landing depended on the list
	// outliving the cast. The character derives the mode from FallSpeedMultiplier every
	// tick instead -- see ACHANGELINGCharacter::UpdateFlightMode.
};

// ── Divination ──────────────────────────────────────────────────────────────

/**
 * Returns information and changes nothing. Ten of the ninety.
 *
 * The only role with no gameplay effect at all, and the one most likely to be left
 * unimplemented because a combat system has nowhere to put its answer. The result goes
 * to Blueprint as an event, to become a marker, a journal line or a piece of dialogue.
 */
UCLASS(Abstract, Blueprintable)
class CHANGELING_API UGA_DivinationCantrip : public UGA_Cantrip
{
	GENERATED_BODY()

protected:
	virtual void ApplyCantrip(const FCantripOutcome& Outcome, const FCantripSpec& Spec,
		const FCantripTargets& Targets) override;

	/**
	 * What the seeing found. Answer it in Blueprint.
	 *
	 * Degree carries how good the roll was, so a marginal success can return less than a
	 * clean one rather than the same answer more slowly.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Cantrip|Divination")
	void OnDivined(const FCantripTargets& Found, int32 Degree, float Radius);

	/** How far the seeing reaches, before successes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Divination")
	float BaseSightRadius = 1000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Divination")
	float SightRadiusPerSuccess = 1000.0f;

	/** Also gather what is hidden -- the point of most divination. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Divination")
	bool bRevealsHidden = true;

	/** Applied to what was found, to light it up for a while. Optional. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Divination")
	TSubclassOf<UGameplayEffect> RevealEffect;
};

// ── Illusion ────────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class ECantripIllusionMode : uint8
{
	Conceal  UMETA(DisplayName = "Conceal (overlooked)"),
	Disguise UMETA(DisplayName = "Disguise (seen as something else)"),
	Phantasm UMETA(DisplayName = "Phantasm (something that is not there)")
};

/** Lies to the senses. Not Control: it does not stop you, it misleads you. */
UCLASS(Abstract, Blueprintable)
class CHANGELING_API UGA_IllusionCantrip : public UGA_Cantrip
{
	GENERATED_BODY()

protected:
	virtual void ApplyCantrip(const FCantripOutcome& Outcome, const FCantripSpec& Spec,
		const FCantripTargets& Targets) override;
	virtual void RevertCantrip() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Illusion")
	ECantripIllusionMode Mode = ECantripIllusionMode::Conceal;

	/** Should carry the tag AI perception checks against. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Illusion")
	TSubclassOf<UGameplayEffect> IllusionEffect;

	/** Spawned for a Phantasm -- the thing that is not there. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Illusion")
	TSubclassOf<AActor> PhantasmClass;

	/**
	 * The illusion breaks when its subject attacks.
	 *
	 * On by default. Concealment that survives a blow is invisibility, which is a
	 * different and much stronger thing than being overlooked.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Illusion")
	bool bBreaksOnAttack = true;

private:
	UPROPERTY() TArray<FActiveGameplayEffectHandle> Applied;
	UPROPERTY() TWeakObjectPtr<AActor> SpawnedPhantasm;
};

// ── Transformation ──────────────────────────────────────────────────────────

/** Rewrites the form of a creature or thing. Metamorphosis is five of these. */
UCLASS(Abstract, Blueprintable)
class CHANGELING_API UGA_TransformationCantrip : public UGA_Cantrip
{
	GENERATED_BODY()

protected:
	virtual void ApplyCantrip(const FCantripOutcome& Outcome, const FCantripSpec& Spec,
		const FCantripTargets& Targets) override;
	virtual void RevertCantrip() override;

	/** Swapped onto the target's mesh. Leave empty to change size or traits only. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Transformation")
	TObjectPtr<USkeletalMesh> NewMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Transformation")
	TSubclassOf<UAnimInstance> NewAnimClass;

	/** 1.0 leaves size alone. Worms and Giants is this and nothing else. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Transformation",
		meta = (ClampMin = "0.05"))
	float ScaleMultiplier = 1.0f;

	/** Traits the new shape brings with it -- a bear's Strength, a bird's Dexterity. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Transformation")
	TSubclassOf<UGameplayEffect> FormEffect;

	/**
	 * The shape holds even after the cantrip ends.
	 *
	 * Reweaving only. Everything else must revert, or a changeling who blinks out
	 * mid-transformation is left a sparrow for good.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Transformation")
	bool bPermanent = false;

private:
	UPROPERTY() TArray<FActiveGameplayEffectHandle> Applied;

	/** What to put back. Captured per target, because a cantrip may transform several. */
	UPROPERTY() TArray<TWeakObjectPtr<AActor>> Transformed;
	UPROPERTY() TArray<TObjectPtr<USkeletalMesh>> OriginalMeshes;
	UPROPERTY() TArray<FVector> OriginalScales;
};

// ── Creation ────────────────────────────────────────────────────────────────

/** How several summoned things are arranged. */
UENUM(BlueprintType)
enum class ECantripPlacement : uint8
{
	Stacked   UMETA(DisplayName = "Stacked"),
	Ring      UMETA(DisplayName = "Ring"),
	Scattered UMETA(DisplayName = "Scattered")
};

/**
 * Brings matter, light or a creature into being.
 *
 * What persists is an ACTOR with its own lifetime, so the ability is over as soon as it
 * has cast -- which is why five of the six summons are Instant rows.
 */
UCLASS(Abstract, Blueprintable)
class CHANGELING_API UGA_CreationCantrip : public UGA_Cantrip
{
	GENERATED_BODY()

public:
	UGA_CreationCantrip();

protected:
	virtual void ApplyCantrip(const FCantripOutcome& Outcome, const FCantripSpec& Spec,
		const FCantripTargets& Targets) override;
	virtual void RevertCantrip() override;

	/** Fires per spawned actor, for a Blueprint to give it its orders. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Cantrip|Creation")
	void OnCreated(AActor* Spawn, int32 Index, const FCantripOutcome& Outcome);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Creation")
	TSubclassOf<AActor> CreatedClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Creation",
		meta = (ClampMin = "1"))
	int32 BaseCount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Creation",
		meta = (ClampMin = "0"))
	int32 CountPerSuccess = 0;

	/** A wisp is a wisp; forty of them is a framerate. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Creation",
		meta = (ClampMin = "1"))
	int32 MaxCount = 12;

	/**
	 * Seconds each stands, before successes. Zero leaves them until something else
	 * removes them -- an effigy left in a room, a portal that waits.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Creation",
		meta = (ClampMin = "0.0"))
	float BaseLifetime = 30.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Creation",
		meta = (ClampMin = "0.0"))
	float LifetimePerSuccess = 30.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Creation")
	ECantripPlacement Placement = ECantripPlacement::Ring;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Creation",
		meta = (ClampMin = "0.0"))
	float PlacementRadius = 150.0f;

	/** Drop each onto the ground beneath it. Off for anything that flies. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Creation")
	bool bDropToGround = true;

	/** Banish what this casting made when it ends. Off for things meant to outlast it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Creation")
	bool bBanishOnEnd = false;

private:
	FTransform PlacementFor(int32 Index, int32 Count, const FVector& Origin) const;

	UPROPERTY() TArray<TWeakObjectPtr<AActor>> Created;
};

// ── Binding ─────────────────────────────────────────────────────────────────

/** What kind of promise is being made. */
UENUM(BlueprintType)
enum class ECantripOathKind : uint8
{
	Bargain UMETA(DisplayName = "Bargain (both sides give)"),
	Geas    UMETA(DisplayName = "Geas (a quest or a ban)"),
	TrueName UMETA(DisplayName = "True Name (power over the named)")
};

/**
 * Oaths, true names and geasa -- the Dreaming enforcing a promise.
 *
 * No other game's taxonomy has this role, and Changeling cannot do without it: an oath
 * here is not a buff with a downside, it is a thing the world itself will collect on.
 * The breach effect is the whole point, so it is applied by the world when the oath is
 * broken rather than by this ability, which is long finished by then.
 */
UCLASS(Abstract, Blueprintable)
class CHANGELING_API UGA_BindingCantrip : public UGA_Cantrip
{
	GENERATED_BODY()

protected:
	virtual void ApplyCantrip(const FCantripOutcome& Outcome, const FCantripSpec& Spec,
		const FCantripTargets& Targets) override;

	/** Record the oath here -- terms, parties, and what watches over it. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Cantrip|Binding")
	void OnOathSworn(const FCantripTargets& Parties, int32 Degree);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Binding")
	ECantripOathKind OathKind = ECantripOathKind::Bargain;

	/** Marks the bound party for as long as the oath stands. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Binding")
	TSubclassOf<UGameplayEffect> OathEffect;

	/**
	 * Visited on an oathbreaker. NOT applied here -- whatever watches the oath applies
	 * it when the promise is broken, which may be days later and nowhere near the caster.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Binding")
	TSubclassOf<UGameplayEffect> BreachEffect;

	/** Strength of the binding, for whatever later tries to break or read it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Binding")
	float BaseWeight = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Binding")
	float WeightPerSuccess = 1.0f;
};

// ── Utility ─────────────────────────────────────────────────────────────────

/**
 * Acts on the world rather than on a character: light, weather, growth, moving objects.
 *
 * The honest catch-all. A cantrip lands here because what it does is particular to
 * itself, so the class carries an optional effect and hands the rest to Blueprint --
 * rather than pretending eight unrelated workings share machinery they do not.
 */
UCLASS(Abstract, Blueprintable)
class CHANGELING_API UGA_UtilityCantrip : public UGA_Cantrip
{
	GENERATED_BODY()

protected:
	virtual void ApplyCantrip(const FCantripOutcome& Outcome, const FCantripSpec& Spec,
		const FCantripTargets& Targets) override;
	virtual void RevertCantrip() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Utility")
	TSubclassOf<UGameplayEffect> UtilityEffect;

	/** Magnitude handed to that effect and to the Blueprint event. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Utility")
	float BaseMagnitude = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Utility")
	float MagnitudePerSuccess = 1.0f;

	/** Spawned at the point it landed. A light, a fog, a patch of growth. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Utility")
	TSubclassOf<AActor> SpawnedClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Utility")
	bool bDespawnOnEnd = true;

private:
	UPROPERTY() TArray<FActiveGameplayEffectHandle> Applied;
	UPROPERTY() TWeakObjectPtr<AActor> Spawned;
};
