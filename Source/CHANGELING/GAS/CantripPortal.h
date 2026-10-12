// CantripPortal.h
//
// A door made by a cantrip, which closes itself.
//
// Its own actor rather than something the ability holds open, because Portal Passage is
// an INSTANT row: the working is finished the moment the door exists. Nothing is being
// maintained, there is no channel to break and no duration for the caster to run out --
// she opens it and walks away, and it shuts on its own some time later.
//
// That is also why the lifetime lives here rather than on the ability. An ability that
// ended a frame after casting cannot be responsible for tidying up something that outlives
// it by a minute; a Sustained row could be, but making Portal Passage Sustained would mean
// a changeling standing still concentrating to hold a door open, which is neither what the
// book describes nor any fun to play.
//
// It closes by SHRINKING. A door that vanishes between one frame and the next reads as a
// bug -- the player assumes they broke it, or that something despawned. A door that draws
// itself shut reads as the magic running out, which is what actually happened.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CantripPortal.generated.h"

/** Which part of its life the door is in. */
UENUM(BlueprintType)
enum class ECantripPortalPhase : uint8
{
	/** Growing open. */
	Opening,

	/** Fully open and simply standing. Most of its life. */
	Held,

	/** Drawing shut. Destroyed at the end of it. */
	Closing
};

/** One Niagara float parameter, mapped across the door's opening. */
USTRUCT(BlueprintType)
struct CHANGELING_API FPortalOpennessDriver
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
	FName Parameter = NAME_None;

	/** Value at fully shut. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
	float Closed = 0.0f;

	/**
	 * Value at fully open.
	 *
	 * Not always larger than Closed. A distortion that should be fiercest at the moment the
	 * way is torn and settle once it is open runs the other way, and inverting it here is
	 * cheaper than authoring a second curve inside the system.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
	float Open = 1.0f;
};

UCLASS(Blueprintable)
class CHANGELING_API ACantripPortal : public AActor
{
	GENERATED_BODY()

public:
	ACantripPortal();

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Start the clock. Called by the cantrip that made it, once, at spawn.
	 *
	 * HoldSeconds is the time spent fully OPEN -- the grow and shrink are on top, so a
	 * door told to hold for thirty seconds is usable for thirty seconds rather than for
	 * thirty minus however long the animation takes. Zero or less means it stands until
	 * something closes it, which is a perfectly good thing for a door to do and is how a
	 * permanent gateway would be built.
	 */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	void OpenFor(float HoldSeconds);

	/**
	 * Begin closing now, wherever it is in its life. Safe to call twice.
	 *
	 * The graceful counterpart to Destroy, and what a dispel should use: the door still
	 * draws shut over CloseSeconds rather than blinking out, so being dispelled looks
	 * like the magic being unmade rather than like the actor being deleted.
	 */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	void CloseNow();

	/**
	 * Pair this door with another. Sets BOTH sides -- a one-sided link is a trapdoor.
	 *
	 * Linking rather than storing a destination point, because a linked pair keeps working
	 * when either end moves, and because coming out of a door wants the door's facing: an
	 * exit stored as a bare location has no front, so there is no way to know which way to
	 * put the traveller down or which way to turn the camera.
	 *
	 * Passing null unlinks, and unlinks the far side too.
	 */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	void LinkTo(ACantripPortal* Other);

	/**
	 * A one-way exit with no door at the far end.
	 *
	 * For the case where the destination is a place rather than another portal -- a fixed
	 * gateway, a scripted arrival. Ignored while a LinkedPortal is set, since a real door
	 * on the far side is always the better answer.
	 */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	void SetDestination(const FTransform& Where);

	/** Whether this door leads anywhere at all. A door that does not is only scenery. */
	UFUNCTION(BlueprintPure, Category = "Portal")
	bool HasExit() const;

	/**
	 * Resize so the opening stands DesiredHeight tall, whatever the art was authored at.
	 *
	 * A clearance rather than a scale factor, because a scale factor means nothing without
	 * knowing the mesh: "0.4" is a guess that has to be re-found by hand every time the art
	 * changes, while "tall enough for her to walk through" survives the swap. The door
	 * measures its own geometry at BeginPlay and works out the factor itself.
	 *
	 * Called after spawning and before the door has grown, so nothing is seen to resize.
	 */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	void SizeToClearance(float DesiredHeight);

	/** The door's authored height at scale 1, measured from its own geometry. */
	UFUNCTION(BlueprintPure, Category = "Portal")
	float GetMeasuredHeight() const { return MeasuredHeight; }

	UFUNCTION(BlueprintPure, Category = "Portal")
	ECantripPortalPhase GetPhase() const { return Phase; }

	/** Seconds left before it starts closing, or 0 if it already is (or never will). */
	UFUNCTION(BlueprintPure, Category = "Portal")
	float GetRemainingHold() const;

	/** 0 to 1, the scale it is currently drawn at. Drive a material or a Niagara rate. */
	UFUNCTION(BlueprintPure, Category = "Portal")
	float GetOpenAlpha() const { return OpenAlpha; }

protected:
	virtual void BeginPlay() override;

	/** Fires once as it starts to grow. Hang the opening effect and sound here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Portal")
	void OnPortalOpening();

	/** Fires once as it starts to shrink -- the cue to stop any looping effect. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Portal")
	void OnPortalClosing();

	/**
	 * Every frame, with the current 0-1 openness.
	 *
	 * The C++ only scales the root. Anything else the door does as it opens -- a material
	 * parameter, a light, the rate of a Niagara system -- belongs in Blueprint, and this
	 * is where to drive it from.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Portal")
	void OnPortalAlphaChanged(float Alpha);

	/** Fires on whichever door was WALKED INTO, once, after the traveller has been moved. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Portal")
	void OnActorEntered(AActor* Who);

	/** Fires on the door they came OUT of. The pair for an arrival effect. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Portal")
	void OnActorArrived(AActor* Who);

	/**
	 * Attach the MESH here. Scales from nothing to OpenScale as the door opens.
	 *
	 * Geometry only. A Niagara system parented to a scaling component does not get bigger
	 * in any useful sense -- particle sizes, velocities and spawn volumes are authored in
	 * absolute units, so the transform scales the emitter's frame while the particles keep
	 * their own dimensions and the effect comes apart. Effects go under EffectRoot.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	TObjectPtr<USceneComponent> PortalRoot;

	/**
	 * Attach NIAGARA here. Never scales.
	 *
	 * The systems are sized by writing RadiusParameter into each of them instead, which is
	 * how the effects pack this door is dressed from does it: one number, pushed into every
	 * child system, so ring, vortex and sparks agree about how big the opening is. A system
	 * told its own radius resizes correctly; one that is merely scaled does not.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	TObjectPtr<USceneComponent> EffectRoot;

	/**
	 * Float parameters driven from the door's openness, written to EVERY child system.
	 *
	 * A list rather than one name, because the systems disagree with each other. In the
	 * portals pack the vortex exposes "Vortex Alpha", "Ring Alpha" and "Ring Scale" and has
	 * no radius at all; the sparks expose "--Scale" and a "Cylinder Height/Radius" that is a
	 * vector rather than a float. A single name could only ever have driven one of them.
	 *
	 * Written to every system rather than matched to one, because writing a name a system
	 * does not have is a silent no-op -- harmless, and far cheaper than making the author
	 * say which parameter belongs to which component.
	 *
	 * That same silence is the hazard: a MISSPELLED name behaves exactly like a correct one
	 * on a system that lacks it. Read the names off the system's User Parameters rather than
	 * typing them from memory.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Portal|Effects")
	TArray<struct FPortalOpennessDriver> OpennessDrivers;

	/**
	 * Whether the effects grow and shrink with the door, or hold their authored size.
	 *
	 * TRUE for a system with no size parameter of its own -- which is most of them, and is
	 * true of this pack: the vortex exposes Vortex Alpha, Ring Alpha and Ring Scale and
	 * nothing that sizes the vortex itself. For those, the component's scale is the only
	 * lever there is, and without it the aura renders at full size around a door scaled to
	 * half, spilling far past the rim.
	 *
	 * FALSE for a system that IS sized by an openness driver, where scaling as well would
	 * compound the two and a half-open door would show its effects at a quarter.
	 *
	 * If the effects are the wrong size relative to the rim, this is the switch.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Portal|Effects")
	bool bScaleEffectsWithDoor = true;

	/**
	 * Correction turning the ART to face the door's forward (+X), applied at BeginPlay.
	 *
	 * Portal art is almost always authored as a disc lying in the XY plane, facing +Z --
	 * that is how a flat mesh is naturally modelled and how this pack ships. A door, though,
	 * is something you walk THROUGH, so its opening has to face along +X, where the
	 * threshold volume, the exit clearance and the traveller's 180-degree flip all are.
	 *
	 * Done here rather than by rotating components in the Blueprint because that has to be
	 * remembered separately for the mesh and for every effect, on every portal ever built,
	 * and being forgotten looks exactly like a bug in the cantrip rather than a missing
	 * rotation on a component.
	 *
	 * Composed ONTO whatever the components already carry, so a component deliberately
	 * turned in the Blueprint keeps its own offset. That also means a component already
	 * hand-corrected by -90 will end up at -180: zero those out and let this do the work.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Portal|Effects")
	FRotator ArtOrientation = FRotator(-90.0f, 0.0f, 0.0f);

	/**
	 * Optional scalar written to every material on the mesh, 0 to 1 with the opening.
	 *
	 * For the frame: a decorative ring wants to fade and brighten rather than grow from a
	 * point, because a ring scaled from zero reads as a prop being resized while a ring that
	 * bleeds into existence reads as something being conjured.
	 *
	 * None by default -- it depends entirely on what the chosen material actually exposes,
	 * and a made-up parameter name would be another silent failure.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Portal|Effects")
	FName MaterialAlphaParameter = NAME_None;

	/**
	 * The doorway itself -- the volume that, when entered, sends you through.
	 *
	 * A box rather than a sphere because a door is a plane you cross, and a sphere has no
	 * front: walking past one at arm's length would take you through it. Sized thin on X so
	 * it triggers at the threshold rather than a metre before it.
	 *
	 * A child of PortalRoot, so it scales with the opening animation. That is deliberate
	 * belt-and-braces: the phase check in Transport already refuses a door that is still
	 * growing, and this makes a half-open door physically too small to walk into as well.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	TObjectPtr<class UBoxComponent> Threshold;

	/**
	 * Who may use it.
	 *
	 * False lets anything with a collider through, which is the interesting answer -- a door
	 * others may use is the whole point of the Portal role, and a pursuer following you
	 * through is a better scene than one stopped at a rope. True restricts it to the caster,
	 * for the escape-hatch reading of the same cantrip.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Portal")
	bool bCasterOnly = false;

	/**
	 * Carry momentum through, rotated into the far door's frame.
	 *
	 * This is the whole difference between a portal and a teleport pad. Running in fast and
	 * arriving at a standstill reads as the door stopping you; arriving still running reads
	 * as the door being a hole in space, which is what it is meant to be.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Portal")
	bool bPreserveVelocity = true;

	/**
	 * How far in front of the exit to put the traveller down.
	 *
	 * Zero would place them exactly in the far doorway, standing in the volume that sends
	 * people back. The re-entry guard below already covers that, but landing a pace clear
	 * also just looks like coming out of a door rather than materialising in its frame.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Portal",
		meta = (ClampMin = "0.0"))
	float ExitClearance = 60.0f;

	/**
	 * Seconds after the caster goes through before BOTH doors shut. Negative never does.
	 *
	 * Not zero, and not instant. A door that slams the moment she is through cannot be
	 * followed, and being followed through your own escape is the better scene -- the
	 * pursuer at her heels makes it, the one three paces back watches it close. Two seconds
	 * is about one running stride's grace.
	 *
	 * Both ends close together because they are one working. A door left standing at the far
	 * side is a way back in for anything that saw where she went.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Portal")
	float CloseAfterPassageSeconds = 2.0f;

	/**
	 * Whether only the CASTER's passage starts that countdown.
	 *
	 * True by default: the working is hers, and it closes behind her. False starts it on
	 * whoever goes through first, which makes the cantrip a trap as readily as an escape --
	 * a real thing to want, and not the default.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Portal")
	bool bOnlyCasterTriggersClose = true;

	/**
	 * Backstop on the re-entry guard.
	 *
	 * Someone who arrives at the far door is standing INSIDE it, and without a guard the far
	 * door would immediately send them back -- and then the near one would, forever, in the
	 * same frame. The guard is normally cleared when they step out of the exit volume, which
	 * is exact and handles a player who lingers in the doorway. This is the case where that
	 * never happens: they were destroyed, or something else moved them out without an end
	 * overlap firing. Then the entry simply ages out.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Portal",
		meta = (ClampMin = "0.1"))
	float ReentryFailsafeSeconds = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal",
		meta = (ClampMin = "0.0"))
	float OpenSeconds = 0.6f;

	/**
	 * How long it takes to draw shut.
	 *
	 * Longer than the open on purpose. Opening is a thing being done TO the world and
	 * wants to feel decisive; closing is something running out, and a slow fade is what
	 * makes it read as the magic ending rather than as the door being switched off.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal",
		meta = (ClampMin = "0.0"))
	float CloseSeconds = 1.2f;

	/**
	 * Fallback hold, used when nothing calls OpenFor.
	 *
	 * A door dropped straight into a level by hand has no cantrip to tell it how long to
	 * last. Without this it would sit at its opening frame forever, which looks like a
	 * broken actor rather than like a permanent gateway.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal",
		meta = (ClampMin = "0.0"))
	float DefaultHoldSeconds = 30.0f;

	/** Scale at full openness. The door's authored size lives on the mesh, not here. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal",
		meta = (ClampMin = "0.01"))
	float OpenScale = 1.0f;

private:
	void SetOpenAlpha(float Alpha);

	/** Turns the art to face +X. See ArtOrientation. */
	void ApplyArtOrientation();

	/** Finds the Niagara systems and builds the dynamic materials, once. */
	void GatherEffects();

	/** Reads the door's own height off its geometry, before the opening animation shrinks
	 *  it. Must run at BeginPlay while the actor is still at its authored scale -- once
	 *  SetOpenAlpha(0) has run there is nothing left to measure. */
	void MeasureGeometry();

	float MeasuredHeight = 0.0f;

	/** Cached at BeginPlay rather than searched every frame -- SetOpenAlpha runs each tick
	 *  while the door is moving, and a component search per frame for a value that changes
	 *  only when the Blueprint is edited is work done ten thousand times for nothing. */
	UPROPERTY() TArray<TObjectPtr<class UNiagaraComponent>> EffectSystems;

	UPROPERTY() TArray<TObjectPtr<class UMaterialInstanceDynamic>> PortalMaterials;

	UFUNCTION()
	void OnThresholdBegin(UPrimitiveComponent* OverlappedComponent, AActor* Other,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& Sweep);

	UFUNCTION()
	void OnThresholdEnd(UPrimitiveComponent* OverlappedComponent, AActor* Other,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** Moves Who to the far side. Returns false, having done nothing, if it may not. */
	bool Transport(AActor* Who);

	/** Starts the closing countdown, if this passage is one that should. */
	void NotePassage(AActor* Who);

	/** Shuts this door and its partner together. */
	UFUNCTION()
	void ShutBothEnds();

	FTimerHandle PassageCloseTimer;

	/** The far door. Weak: it has its own lifetime and closes on its own schedule, and a
	 *  pair does not die together -- outliving your twin is normal, and a door whose partner
	 *  has shut simply stops leading anywhere. */
	TWeakObjectPtr<ACantripPortal> LinkedPortal;

	FTransform Destination = FTransform::Identity;
	bool bHasDestination = false;

	/** Actors that arrived here and have not yet stepped clear, with the time they landed.
	 *  Keyed weakly and pruned in Tick, so a traveller destroyed mid-doorway leaves nothing
	 *  behind. */
	TMap<TWeakObjectPtr<AActor>, float> ArrivedAt;

	ECantripPortalPhase Phase = ECantripPortalPhase::Opening;

	/** Seconds spent in the current phase. */
	float PhaseTime = 0.0f;

	/** How long to stand fully open. Negative means indefinitely. */
	float HoldDuration = -1.0f;

	float OpenAlpha = 0.0f;

	/** Whether OpenFor has been called, so BeginPlay's fallback does not overwrite it.
	 *  Spawn order puts OpenFor before BeginPlay only sometimes, and "sometimes" is
	 *  exactly the kind of thing that produces a door with the wrong lifetime once a
	 *  session and never in the editor. */
	bool bHoldSet = false;
};
