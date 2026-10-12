// ChangelingAIController.h
//
// The controller every NPC in the garden runs under.
//
// Two ways to drive a pawn, and which one is used depends only on whether a StateTree was
// given. That split is deliberate: authoring a StateTree and getting a navmesh to build are
// two separate problems, and debugging them at the same time means never knowing which one
// is broken. With no tree set the controller wanders, which answers "can this pawn path at
// all" on its own. Set a tree and the wander stands down.
//
// StateTree rather than Behavior Tree because the project already links both StateTree
// modules and Epic's own template controllers here use it -- see Variant_Combat/AI.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ChangelingAIController.generated.h"

class UStateTreeAIComponent;

UCLASS(Blueprintable)
class CHANGELING_API AChangelingAIController : public AAIController
{
	GENERATED_BODY()

public:
	AChangelingAIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	/** Stop wandering and hold position. Safe to call when it was never wandering. */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void StopWandering();

protected:
	/**
	 * The behaviour, when there is one. Left empty, the fallback wander below runs instead.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	TObjectPtr<UStateTreeAIComponent> StateTreeAI;

	// ── The fallback ────────────────────────────────────────────────────────

	/**
	 * Whether to wander when no StateTree is set.
	 *
	 * A crowd of NPCs standing perfectly still is indistinguishable from a crowd with no
	 * navmesh under them, and both look like the AI being broken. Wandering makes the
	 * difference visible from across the plaza.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AI|Wander")
	bool bWanderWithoutStateTree = true;

	/**
	 * How far from where she was PLACED she will stray, in world units.
	 *
	 * Anchored to the spawn point rather than to wherever she currently stands, or a wander
	 * becomes a random walk and a gardener ends up half a mile away in the water after ten
	 * minutes. Anchoring keeps her in the part of the garden she was placed to inhabit.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AI|Wander",
		meta = (ClampMin = "100.0"))
	float WanderRadius = 1200.0f;

	/** Seconds to stand still on arrival, picked at random between the two. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AI|Wander",
		meta = (ClampMin = "0.0"))
	float WanderPauseMin = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AI|Wander",
		meta = (ClampMin = "0.0"))
	float WanderPauseMax = 7.0f;

	/** How near counts as arrived. Loose on purpose -- a metre short of a bench is fine. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AI|Wander",
		meta = (ClampMin = "1.0"))
	float WanderAcceptanceRadius = 60.0f;

private:
	virtual void OnMoveCompleted(FAIRequestID RequestID,
		const FPathFollowingResult& Result) override;

	/** Finds somewhere reachable and starts walking. Logs loudly when it cannot. */
	void PickWanderTarget();

	/** Schedules the next PickWanderTarget after a random pause. */
	void ScheduleNextWander();

	FTimerHandle WanderTimer;

	/** Where she was possessed, which for a placed actor is where she was placed. */
	FVector WanderHome = FVector::ZeroVector;

	bool bWandering = false;

	/** So the "no navmesh" warning is printed once per pawn rather than every few seconds.
	 *  A wander that cannot find a point retries on a timer, and an unthrottled warning
	 *  from twenty NPCs makes the log useless for everything else. */
	bool bWarnedNoNavigation = false;
};
