// CantripPayload_Effect.h
//
// Niagara and a looping sound, for a cantrip whose role class has neither.
//
// UGA_SustainedCantrip owns a full effect rig -- system, socket, intensity parameter,
// light, sound -- and the twelve other role classes own none of it, because they descend
// from UGA_Cantrip directly. That is fine for Instant workings that flash and are gone,
// and it leaves Wind Runner unable to show anything at all: a flight cantrip that lasts
// half a minute and has no visible sign it is running.
//
// A payload rather than a copy of the sustained rig, per this file's own rule -- the
// machinery of a second role, composed in. It buys the same thing for every role at once:
// a Buff can glow, a Portal can hum.
//
// Deliberately NOT a general effect system. It spawns attached systems, scales them by the
// roll, and takes them down gracefully. Anything needing per-particle control, collision
// callbacks or a light rig wants UGA_SustainedCantrip, which already has all three.

#pragma once

#include "CoreMinimal.h"
#include "CantripPayload.h"
#include "CantripPayload_Effect.generated.h"

class UAudioComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;

/**
 * One system on one socket.
 *
 * An array of these rather than a single system, because the interesting cases need
 * several at once and they are not variations of each other: air gathering at both hands
 * AND a cloud boiling under the feet is one working, three systems, three sockets.
 */
USTRUCT(BlueprintType)
struct CHANGELING_API FCantripEffectAttachment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	TObjectPtr<UNiagaraSystem> System;

	/**
	 * Attach to the animated MESH, or to the capsule underneath it.
	 *
	 * The mesh moves with the animation AND with the flight lean, which is what an effect
	 * held in a hand wants -- it should follow the hand wherever the pose puts it.
	 *
	 * A ground effect does not. ACHANGELINGCharacter pitches the MESH up to MaxFlightPitch
	 * while the capsule stays vertical, so anything on a mesh socket tilts with her: a
	 * cloud meant to boil beneath her feet ends up lying at thirty-five degrees. Worse, no
	 * fixed Rotation can correct it, because the lean varies with speed.
	 *
	 * False attaches to the capsule instead, which never pitches and never animates. The
	 * Socket below is then ignored -- sockets live on the skeleton, and the capsule has none.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	bool bAttachToMesh = true;

	/**
	 * Socket on the caster's mesh. Must exist on the SKELETON, not just be a bone name.
	 *
	 * A missing socket does not error -- Niagara attaches at the component origin instead,
	 * which puts the effect at her feet whatever was intended, and looks like the effect
	 * being wrong rather than the socket being absent.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	FName Socket = NAME_None;

	/** Nudge from the socket, in its own space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	FVector Offset = FVector::ZeroVector;

	/**
	 * Rotation relative to the socket. Almost always needed, and rarely zero.
	 *
	 * A socket inherits its bone's axes, and a bone's axes are an authoring accident: the
	 * hand bones run down the arm and across the palm, nowhere near world forward. A system
	 * authored to emit along +X therefore comes out sideways, and no amount of tuning inside
	 * the Niagara asset fixes it -- the asset is correct and the frame it arrives in is not.
	 *
	 * Set it by watching, the same way the bone-axis mappings in the animation layer were.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	FRotator Rotation = FRotator::ZeroRotator;

	/**
	 * The float user parameter that says HOW MUCH this system is doing.
	 *
	 * Per attachment because every pack names it differently and none of them agree:
	 * NS_RocketExhaust_Plume_1 calls it "Spawn", NS_Smoke_Plume calls it "SpawnRate", and
	 * this project's own NS_FlameThrower calls it "Intensity". A single name on the
	 * payload could only ever drive one of them, and writing a name a system does not
	 * have fails silently -- the effect spawns, looks wrong, and says nothing.
	 *
	 * Leave it None to spawn the system and never touch it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	FName ScalarParameter = NAME_None;

	/**
	 * Value written to that parameter at one success, and added per success after.
	 *
	 * Absolute, not a multiplier, because the parameter's own scale is whatever the system
	 * decided: a spawn rate wants 10 or 20, an intensity wants 1. Anything normalised here
	 * would have to be un-normalised by every asset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	float ScalarAtOneSuccess = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	float ScalarPerSuccess = 0.0f;

	/**
	 * Optional second float, written 1 on apply and 0 on the way out.
	 *
	 * For systems that separate "how fierce" from "how present" -- this project's cantrip
	 * systems carry Intensity and FadeAlpha as exactly that pair. Left None, the scalar
	 * above is zeroed instead, which stops emission just as well.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	FName FadeParameter = NAME_None;
};

/**
 * Spawns Niagara and a looping sound while the cantrip runs, and fades them when it ends.
 */
UCLASS(EditInlineNew, DefaultToInstanced, Blueprintable,
	meta = (DisplayName = "Effect Payload"))
class CHANGELING_API UCantripPayload_Effect : public UCantripPayload
{
	GENERATED_BODY()

public:
	virtual void Apply(UGA_Cantrip* Ability, const FCantripOutcome& Outcome,
		const FCantripTargets& Targets) override;

	virtual void Revert(UGA_Cantrip* Ability) override;

	virtual FString Describe() const override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	TArray<FCantripEffectAttachment> Attachments;

	/** Looping sound, attached to the caster so it travels with her. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect|Sound")
	TObjectPtr<USoundBase> LoopSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect|Sound",
		meta = (ClampMin = "0.0"))
	float Volume = 1.0f;

	/**
	 * Seconds for the sound to reach full volume.
	 *
	 * Not zero. A looping sound that arrives at full is a click, and the ear reads it as a
	 * mistake even when the visual is correct -- the same reason the flamethrower ramps.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect|Sound",
		meta = (ClampMin = "0.0"))
	float SoundFadeInSeconds = 0.35f;

	// ── Fading out ──────────────────────────────────────────────────────────

	/**
	 * How long to let the last particles finish after emission stops.
	 *
	 * On the way out each attachment's scalar is written to zero (or its FadeParameter, if
	 * it has one) rather than the component being destroyed. Destroying kills every
	 * particle already in the air, so a cloud vanishes mid-billow and reads as the effect
	 * being switched off; zeroing the rate lets the existing ones live out their lifetimes
	 * and the component is destroyed afterwards, when there is nothing left to see.
	 *
	 * Should be at least the longest particle lifetime in the system, or the tail is cut
	 * off anyway and the graceful stop bought nothing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect",
		meta = (ClampMin = "0.0"))
	float FadeOutSeconds = 1.0f;

private:
	/** Writes an attachment's scalar (and fade, if it has one) onto a live component. */
	static void WriteScalars(UNiagaraComponent* Component,
		const FCantripEffectAttachment& Attachment, float Value, float Fade);

	/** Spawned systems, in Attachments order. Weak: a component can be destroyed by the
	 *  world (level transition, avatar death) before the cantrip gets to end. */
	UPROPERTY() TArray<TWeakObjectPtr<UNiagaraComponent>> Spawned;

	UPROPERTY() TWeakObjectPtr<UAudioComponent> SpawnedAudio;
};
