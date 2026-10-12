// ChangelingAIController.cpp

#include "ChangelingAIController.h"

#include "Components/StateTreeAIComponent.h"
#include "Engine/World.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogChangelingAI, Log, All);

AChangelingAIController::AChangelingAIController()
{
	StateTreeAI = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAI"));
}

void AChangelingAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!InPawn)
	{
		return;
	}

	// Home is taken HERE, not at the first wander step. By the time she takes her first
	// step she has already been nudged by capsule settling and by whatever else shares the
	// spot, and anchoring to that drifts the whole patrol a little further every restart.
	WanderHome = InPawn->GetActorLocation();

	// A StateTree, if one was given, is the whole behaviour and the wander must not fight
	// it -- two things issuing MoveTo requests to one pawn produces a pawn that stutters
	// between two destinations and looks possessed in the other sense.
	const bool bHasStateTree = StateTreeAI && StateTreeAI->IsRunning();

	if (bHasStateTree)
	{
		UE_LOG(LogChangelingAI, Log, TEXT("AI %s: possessed %s, running StateTree."),
			*GetName(), *InPawn->GetName());
		return;
	}

	if (!bWanderWithoutStateTree)
	{
		UE_LOG(LogChangelingAI, Log,
			TEXT("AI %s: possessed %s, no StateTree and wander disabled -- standing still."),
			*GetName(), *InPawn->GetName());
		return;
	}

	UE_LOG(LogChangelingAI, Log,
		TEXT("AI %s: possessed %s, no StateTree -- wandering %.0f units around %s."),
		*GetName(), *InPawn->GetName(), WanderRadius, *WanderHome.ToCompactString());

	bWandering = true;
	bWarnedNoNavigation = false;

	// Staggered rather than immediate. Twenty NPCs possessed on the same frame would all
	// query the navmesh on that frame and then move in lockstep forever after, which reads
	// as a formation rather than as a crowd.
	ScheduleNextWander();
}

void AChangelingAIController::OnUnPossess()
{
	StopWandering();
	Super::OnUnPossess();
}

void AChangelingAIController::StopWandering()
{
	bWandering = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WanderTimer);
	}

	StopMovement();
}

void AChangelingAIController::PickWanderTarget()
{
	if (!bWandering)
	{
		return;
	}

	// Not named Pawn -- AController already has a member by that name, and shadowing it
	// compiles as a warning-as-error here.
	const APawn* Wanderer = GetPawn();
	UWorld* World = GetWorld();
	if (!Wanderer || !World)
	{
		return;
	}

	UNavigationSystemV1* Navigation = UNavigationSystemV1::GetCurrent(World);

	FNavLocation Destination;
	const bool bFound = Navigation
		&& Navigation->GetRandomReachablePointInRadius(WanderHome, WanderRadius, Destination);

	if (!bFound)
	{
		// The single most common reason an NPC does nothing, and it is otherwise SILENT:
		// no navmesh under her, or none built yet. The query simply returns false and a
		// pawn with no destination stands exactly as still as a pawn with no behaviour.
		if (!bWarnedNoNavigation)
		{
			bWarnedNoNavigation = true;

			UE_LOG(LogChangelingAI, Warning,
				TEXT("AI %s: no reachable point within %.0f of %s. %s"),
				*GetName(), WanderRadius, *WanderHome.ToCompactString(),
				Navigation
					? TEXT("Navigation exists but found nothing -- is she inside a "
						   "NavMeshBoundsVolume, and has the navmesh built?")
					: TEXT("There is no navigation system in this world at all."));
		}

		// Retried rather than given up on. A dynamic navmesh may simply not have generated
		// around her yet, and a permanent stop would mean she never moves once it does.
		ScheduleNextWander();
		return;
	}

	bWarnedNoNavigation = false;

	MoveToLocation(Destination.Location, WanderAcceptanceRadius);
}

void AChangelingAIController::ScheduleNextWander()
{
	UWorld* World = GetWorld();
	if (!World || !bWandering)
	{
		return;
	}

	const float Pause = FMath::FRandRange(
		FMath::Min(WanderPauseMin, WanderPauseMax),
		FMath::Max(WanderPauseMin, WanderPauseMax));

	World->GetTimerManager().SetTimer(WanderTimer, this,
		&AChangelingAIController::PickWanderTarget,
		FMath::Max(0.05f, Pause), /*bLoop*/ false);
}

void AChangelingAIController::OnMoveCompleted(FAIRequestID RequestID,
	const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	// Every ending schedules the next one, success or not. A wander that stopped on its
	// first blocked path would leave her standing for the rest of the session, and being
	// blocked is normal -- another NPC in a doorway is enough.
	if (bWandering)
	{
		ScheduleNextWander();
	}
}
