// GA_SustainedCantrip.h
//
// A cantrip that STAYS ON: a flame held in the palm, a glamour worn like a coat.
//
// The ordinary cantrip in GA_Cantrip is over the instant it lands. This one resolves
// the same way -- same pool, same difficulty, same hold-to-cast -- and then keeps
// running for a duration the ROLL decides, until it is toggled off or interrupted.
//
// Duration comes off successes rather than an upkeep drain. That follows the book,
// which grades how long a cantrip holds by how well it was rolled, and it makes the
// hold-to-cast pay twice: the beat you spend committing decides both whether the
// light kindles and how long you get to keep it.
//
// The toggle lives here in C++ rather than in the character's Event Graph. Done in
// Blueprint it needs a validity branch, a stored component reference, and a
// retriggerable delay to survive a player mashing the key -- three fragile pieces
// guarding one piece of state. An ability that knows whether it is running needs
// none of them.

#pragma once

#include "CoreMinimal.h"
#include "GA_Cantrip.h"
#include "GA_SustainedCantrip.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class UPointLightComponent;

UCLASS(Abstract, Blueprintable)
class CHANGELING_API UGA_SustainedCantrip : public UGA_Cantrip
{
	GENERATED_BODY()

public:
	UGA_SustainedCantrip();

	/**
	 * One node for the whole toggle. Bind press to this.
	 *
	 * Running -> ends it and returns false. Not running -> tries to start it and
	 * returns whether activation took. Mashing the key cannot desynchronise it,
	 * because the ability's own activation state is the only state there is.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Sustained",
		meta = (DefaultToSelf = "Avatar"))
	static bool ToggleSustainedCantrip(AActor* Avatar,
		TSubclassOf<UGA_SustainedCantrip> CantripClass);

	/**
	 * Put out every sustained cantrip this actor is holding, early.
	 *
	 * Leave CantripClass empty to douse all of them; pass one to douse only that kind.
	 * Returns how many were actually running, so a caller can react to having
	 * interrupted something rather than guessing.
	 *
	 * This is the hook for everything that is NOT the player choosing to stop -- a hit
	 * landing, wading in past the waist, walking into somewhere too mundane to hold a
	 * working open. The input toggle already handles the deliberate case.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Sustained",
		meta = (DefaultToSelf = "Avatar", AdvancedDisplay = "CantripClass"))
	static int32 DouseSustainedCantrips(AActor* Avatar,
		TSubclassOf<UGA_SustainedCantrip> CantripClass);

	/** End this one early. Reports as doused rather than expired. */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Sustained")
	virtual void CancelSustain();

	/** True while the working is held open. */
	UFUNCTION(BlueprintPure, Category = "Cantrip|Sustained")
	bool IsSustaining() const { return bSustaining; }

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	virtual void PostResolve(const FCantripOutcome& Outcome, const FCantripSpec& Spec) override;

	/** Effect attached to the caster for as long as this runs. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Sustained")
	TObjectPtr<UNiagaraSystem> SustainedEffect;

	/** Socket on the character mesh to attach it to. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Sustained")
	FName AttachSocket = TEXT("hand_rSocket");

	/**
	 * Optional second socket. When set, the effect sits MIDWAY between the two.
	 *
	 * A two-handed working should burn in the space the hands are holding open, not
	 * hang off one wrist. Leave it None and the effect stays on AttachSocket alone,
	 *·which is what a torch wants.
	 *
	 * The component still parents to the primary socket, so it inherits that socket's
	 * motion smoothly every frame; only the midway OFFSET is recomputed on a timer.
	 * Attaching to nothing and driving the whole world transform at the sample rate
	 * would visibly step whenever the character moved.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Sustained")
	FName SecondaryAttachSocket = NAME_None;

	/** Nudge relative to the socket. Slightly negative Z seats a flame IN the palm
	 *  rather than hovering above it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Sustained")
	FVector AttachOffset = FVector(0.0f, 0.0f, -20.0f);

	/**
	 * Uniform scale for the whole effect.
	 *
	 * Here rather than in the emitters because resizing a system otherwise means
	 * editing sprite size, spawn radius and velocity in step across every emitter --
	 * three numbers each, and getting one wrong changes the effect's shape rather than
	 * its size. A component scale moves all of them together and leaves the authored
	 * proportions intact.
	 *
	 * Meant for FITTING an effect to a hand, not for rescuing one built at the wrong
	 * size: scaling far from 1 will also scale how fast particles appear to travel.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Sustained",
		meta = (ClampMin = "0.01"))
	float EffectScale = 1.0f;

	/**
	 * Rotation relative to the socket.
	 *
	 * Matters for anything that projects rather than just sitting there. A socket's
	 * axes follow the BONE, and hand bones point back along the forearm as often as
	 * they point out of the palm -- so an emitter firing along its local +X can come
	 * out backwards through the caster's own arm.
	 *
	 * Correcting it here rather than in the emitter keeps one number in one place,
	 * instead of negated velocities scattered through every emitter of a system whose
	 * tuning came from someone else. Yaw 180 is the usual answer.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Sustained")
	FRotator AttachRotation = FRotator::ZeroRotator;

	/**
	 * What decides the direction, as opposed to AttachSocket deciding the position.
	 *
	 * Socket is the old behaviour and right for a torch. Anything PROJECTED wants one
	 * of the other two, which removes the whole class of problem where a wrist bone's
	 * axes send the flame backwards or the arm's animation swings the stream around.
	 *
	 * AttachRotation still applies on top, so with a non-Socket source it reads as a
	 * plain aim adjustment -- pitch to raise the nose, yaw to lead the off-hand --
	 * rather than a correction for an axis convention you cannot see.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Sustained")
	ECantripAimSource AimSource = ECantripAimSource::Socket;

	/** Attitude forced on the body while this runs. Read by the AnimBP. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Sustained")
	ECantripPose SustainedPose = ECantripPose::RightHandRaised;

	/** Delay before the effect appears, so the hand arrives before the fire does.
	 *  Without it the flame lights in a hand that has not finished rising. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Sustained",
		meta = (ClampMin = "0.0"))
	float EffectSpawnDelay = 0.2f;

	/**
	 * Niagara user parameter, 0..1, multiplied into the emitter's Spawn Rate.
	 *
	 * A system without this parameter is unharmed -- the write simply lands on
	 * nothing and the effect pops in and out as before.
	 */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Cantrip|Sustained")
	FName FadeParameter = TEXT("FadeAlpha");

	/**
	 * Niagara user parameter (Object) that receives particle collisions.
	 *
	 * The system's Export Particle Data module writes its collided particles to
	 * whatever object is bound here; we bind the caster's burn component, which turns
	 * them into scorch marks. Leave empty on anything that should not mark the world.
	 */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Cantrip|Sustained")
	FName BurnHandlerParameter = TEXT("BurnHandler");

	/**
	 * Seconds to ramp the spawn rate down when the working ends.
	 *
	 * Spawning stops smoothly and the particles already alive finish their own
	 * lifetimes, so the flame thins and dies rather than being cut off. The component
	 * destroys itself once the ramp completes.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Sustained",
		meta = (ClampMin = "0.0"))
	float FadeOutSeconds = 0.5f;

	/**
	 * Seconds the working lasts, indexed by SUCCESSES on the cantrip roll.
	 *
	 * The book grades a cantrip's duration by how well it was rolled rather than
	 * charging rent to hold it, and that is the better shape here: the same roll that
	 * decides whether the light kindles also decides how long you get, so the decision
	 * to hold the cast a beat longer pays twice.
	 *
	 * Index 0 is a bare success. Anything past the end of the array uses the last
	 * entry, so adding degrees later never reads off the end.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Sustained")
	TArray<float> DurationBySuccesses = { 10.0f, 20.0f, 45.0f, 90.0f, 180.0f, 300.0f };

	/** Seconds left before it goes out; 0 once expired. For a UI meter. */
	UFUNCTION(BlueprintPure, Category = "Cantrip|Sustained")
	float GetRemainingSeconds() const;

	/** Applied for the duration; the natural home for a warmth aura or a Banality ward. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Sustained")
	TSubclassOf<UGameplayEffect> SustainedGameplayEffect;

	// ── Light ───────────────────────────────────────────────────────────────
	//
	// A working that looks like fire but lights nothing is the single clearest way to
	// tell the player it is not real. Illuminate in particular is only worth casting if
	// it changes what you can see, so the light is the mechanic and the flame is the
	// costume -- not the other way round.

	/**
	 * Radius the working lights, in centimetres, at a bare single success.
	 *
	 * Zero means the working casts no light at all, which is the default: most sustained
	 * cantrips are not torches, and a glamour worn like a coat should not glow.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Sustained|Light",
		meta = (ClampMin = "0.0"))
	float LightRadius = 0.0f;

	/**
	 * Extra radius per success beyond the first.
	 *
	 * The same argument as the duration ladder: the roll that decides whether the light
	 * kindles should also decide how far it reaches, so holding the cast a beat longer
	 * pays in something the player can actually see.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Sustained|Light",
		meta = (ClampMin = "0.0"))
	float LightRadiusPerSuccess = 150.0f;

	/** Brightness in the point light's own units. Scales with successes alongside the
	 *  radius, so a strong casting is brighter as well as further-reaching. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Sustained|Light",
		meta = (ClampMin = "0.0"))
	float LightIntensity = 8.0f;

	/** Warm by default -- firelight, not a torch bulb. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Sustained|Light")
	FLinearColor LightColour = FLinearColor(1.0f, 0.72f, 0.36f);

	/**
	 * How far the brightness wanders, as a fraction. 0 is a dead steady lamp.
	 *
	 * Worth having even at small values: a perfectly constant light reads as electric
	 * however warm its colour, and the flame it is supposed to be coming from is
	 * visibly moving.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Sustained|Light",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LightFlickerAmount = 0.18f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Sustained|Light",
		meta = (ClampMin = "0.0"))
	float LightFlickerSpeed = 6.5f;

	/**
	 * Shadow-casting. Off by default and worth leaving off.
	 *
	 * A shadowing point light held in the hand re-shadows everything around the caster
	 * every frame as they walk, which is both the most expensive thing on this class and
	 * the most likely to shimmer.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Sustained|Light")
	bool bLightCastsShadows = false;

	/** Nudge relative to the socket, separate from the effect's. A light sitting exactly
	 *  in the palm is half-occluded by the hand holding it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Sustained|Light")
	FVector LightOffset = FVector(0.0f, 0.0f, 5.0f);

	/** The light while it burns, for subclasses that want to drive it further. */
	UPointLightComponent* GetSustainedLight() const { return SustainedLight; }

	/** The attached effect while it runs, for subclasses that drive its parameters. */
	UNiagaraComponent* GetSustainedComponent() const { return SustainedComponent; }

	/**
	 * Decide WHEN the effect appears. The base waits EffectSpawnDelay seconds.
	 *
	 * Split out so a subclass can wait on something better than a clock -- the
	 * channelled kind waits for the animation to say the gesture has landed, which a
	 * fixed delay only approximates and stops approximating the moment the animation
	 * is retimed.
	 */
	virtual void ScheduleEffectSpawn();

	void SpawnSustainedEffect();

	UFUNCTION(BlueprintImplementableEvent, Category = "Cantrip|Sustained")
	void OnSustainBegan();

	/** bRanOut true means the duration elapsed and it guttered on its own; false
	 *  means the player doused it. Worth different sound and different VFX -- a light
	 *  that dies on you is not the same event as one you chose to put out. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Cantrip|Sustained")
	void OnSustainEnded(bool bRanOut);

protected:
	/**
	 * Put the working out and start its fade, WITHOUT ending the ability.
	 *
	 * Separated from CancelSustain so a subclass can keep the caster committed while
	 * the effect dies -- releasing and recovering are not the same instant.
	 */
	void StopSustain(bool bRanOut);

private:
	void BeginSustain(int32 Successes);
	void HandleExpired();
	float DurationForSuccesses(int32 Successes) const;
	void SpawnSustainedLight();
	void UpdateLightFlicker();
	void UpdateHandSpan();

	UPROPERTY() TObjectPtr<UNiagaraComponent> SustainedComponent;
	UPROPERTY() TObjectPtr<UPointLightComponent> SustainedLight;

	FActiveGameplayEffectHandle SustainedEffectHandle;
	FTimerHandle DurationTimer;
	FTimerHandle SpawnDelayTimer;
	FTimerHandle FlickerTimer;
	FTimerHandle HandSpanTimer;

	/** Successes on the roll that lit this, kept so the light can be sized from it at
	 *  spawn time -- which happens a beat later than the roll. */
	int32 SustainSuccesses = 1;

	/** Brightness before flicker is applied, so the flicker is not compounding on its
	 *  own previous output. */
	float LightBaseIntensity = 0.0f;

	bool bSustaining = false;
};
