// CantripPortal.cpp

#include "CantripPortal.h"

#include "Components/BoxComponent.h"
#include "Components/MeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogCantripPortal, Log, All);

namespace
{
	/**
	 * Half a turn about up, applied in the door's own frame.
	 *
	 * You walk IN the front of one door and OUT the front of the other, so the two frames
	 * face each other rather than pointing the same way. Without this the traveller arrives
	 * facing back into the exit, which reads as being spat out backwards -- and the momentum
	 * carried through would point the wrong way too, so running in fast would fling them
	 * back through the door they just came out of.
	 */
	const FQuat PortalFlip(FVector::UpVector, PI);
}

ACantripPortal::ACantripPortal()
{
	PrimaryActorTick.bCanEverTick = true;

	PortalRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PortalRoot"));
	SetRootComponent(PortalRoot);

	// NOT scaled to zero here, though the door does start closed.
	//
	// The constructor builds the class default, and the class default is what the Blueprint
	// editor's viewport shows -- so zeroing it here makes the mesh and the frame invisible
	// at edit time, and the door has to be authored blind. BeginPlay closes it instead,
	// which is early enough: SpawnActor runs BeginPlay before it returns, so there is no
	// frame in which a newly made portal is drawn at full size.

	EffectRoot = CreateDefaultSubobject<USceneComponent>(TEXT("EffectRoot"));
	EffectRoot->SetupAttachment(PortalRoot);

	// Whether this inherits the opening scale is decided at BeginPlay, from
	// bScaleEffectsWithDoor -- a constructor cannot read it, because the Blueprint's value
	// for it has not been loaded yet.

	Threshold = CreateDefaultSubobject<UBoxComponent>(TEXT("Threshold"));
	Threshold->SetupAttachment(PortalRoot);

	// Thin on X, wide on Y, tall on Z: the shape of a doorway, with X as the direction you
	// cross. Overridable in Blueprint -- the authored mesh decides how big the door is, and
	// this is only the default for one that has not been authored yet.
	Threshold->SetBoxExtent(FVector(24.0f, 90.0f, 100.0f));

	// Query only. A door must never PUSH anyone -- a portal that blocks is a wall, and one
	// that shoves a character out of its own trigger cannot be walked through at all.
	Threshold->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Threshold->SetCollisionResponseToAllChannels(ECR_Overlap);
	Threshold->SetGenerateOverlapEvents(true);

	// Defaults for the portals pack this door is dressed from, read off its systems rather
	// than guessed. Editable, and a door built from anything else will want different ones
	// -- these are a working starting point, not a contract.
	OpennessDrivers.Add({ TEXT("Vortex Alpha"), 0.0f, 1.0f });
	OpennessDrivers.Add({ TEXT("Ring Alpha"),   0.0f, 1.0f });
	OpennessDrivers.Add({ TEXT("Ring Scale"),   0.0f, 1.0f });
	OpennessDrivers.Add({ TEXT("--Scale"),      0.0f, 1.0f });
}

void ACantripPortal::BeginPlay()
{
	Super::BeginPlay();

	if (Threshold)
	{
		Threshold->OnComponentBeginOverlap.AddDynamic(this, &ACantripPortal::OnThresholdBegin);
		Threshold->OnComponentEndOverlap.AddDynamic(this, &ACantripPortal::OnThresholdEnd);
	}

	// Absolute scale means "ignore my parent's" -- so it is set when the effects should NOT
	// follow the door. Done here rather than in the constructor because the Blueprint's own
	// value for the flag is not loaded until after construction.
	if (EffectRoot)
	{
		EffectRoot->SetUsingAbsoluteScale(!bScaleEffectsWithDoor);
	}

	// Before measuring, so the bounds read describe the door as it will actually stand.
	ApplyArtOrientation();

	GatherEffects();

	// BEFORE the door is closed below. SetOpenAlpha(0) scales PortalRoot to nothing, and a
	// door measured after that reports a height of zero -- which would then be divided into,
	// giving a scale of infinity for anything asking to be sized.
	MeasureGeometry();

	// Only if nothing told us otherwise. A door placed by hand in a level has no cantrip
	// to give it a lifetime; one made by Portal Passage already has.
	if (!bHoldSet)
	{
		HoldDuration = DefaultHoldSeconds;
		bHoldSet = true;
	}

	Phase = ECantripPortalPhase::Opening;
	PhaseTime = 0.0f;
	SetOpenAlpha(0.0f);

	// After the hold is settled, not before -- logged earlier it reported the -1 placeholder
	// on every door and read as "stands forever" when the lifetime was simply not set yet.
	{
		// UNSCALED. This log runs after the door has been shut to nothing, so the scaled
		// extent is multiplied by an almost-zero and every door reported 0 x 0 x 0 -- which
		// reads as a threshold that was never set rather than one merely not yet open.
		const FVector Extent = Threshold
			? Threshold->GetUnscaledBoxExtent() : FVector::ZeroVector;

		UE_LOG(LogCantripPortal, Log,
			TEXT("PORTAL SPAWN %s: at %s facing %s | threshold %.0f x %.0f x %.0f ")
			TEXT("| authored %.0f | bounds r=%.0f | %d system(s) | hold %.1fs"),
			*GetName(),
			*GetActorLocation().ToCompactString(),
			*GetActorRotation().ToCompactString(),
			Extent.X * 2.0f, Extent.Y * 2.0f, Extent.Z * 2.0f,
			MeasuredHeight,
			GetSimpleCollisionRadius(),
			EffectSystems.Num(),
			HoldDuration);
	}

	OnPortalOpening();
}

void ACantripPortal::OpenFor(float HoldSeconds)
{
	// Negative means "stand until something closes it", and that is deliberate rather
	// than a guard against bad input -- a permanent gateway is a real thing to want.
	HoldDuration = (HoldSeconds > 0.0f) ? HoldSeconds : -1.0f;
	bHoldSet = true;
}

void ACantripPortal::LinkTo(ACantripPortal* Other)
{
	// Symmetrical, and the old partner on each side is released first. Re-linking a door
	// that was already paired would otherwise leave its former twin pointing at a door that
	// no longer points back -- a one-way trip that only shows up when someone takes it.
	if (ACantripPortal* Previous = LinkedPortal.Get())
	{
		if (Previous != Other)
		{
			Previous->LinkedPortal = nullptr;
		}
	}

	LinkedPortal = Other;

	if (Other && Other->LinkedPortal.Get() != this)
	{
		Other->LinkTo(this);
	}
}

void ACantripPortal::SetDestination(const FTransform& Where)
{
	Destination = Where;
	bHasDestination = true;
}

bool ACantripPortal::HasExit() const
{
	return LinkedPortal.IsValid() || bHasDestination;
}

void ACantripPortal::CloseNow()
{
	if (Phase == ECantripPortalPhase::Closing)
	{
		return;
	}

	Phase = ECantripPortalPhase::Closing;

	// The shrink starts from wherever it actually IS, not from fully open. Closing a door
	// that was still growing should carry on from the size it reached, and resetting the
	// timer here would make it jump to full size first.
	PhaseTime = (1.0f - OpenAlpha) * CloseSeconds;

	OnPortalClosing();
}

void ACantripPortal::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	PhaseTime += DeltaSeconds;

	// The guard's backstop. Normally an entry is cleared by the traveller stepping out of
	// the doorway; this is for the ones who never do -- destroyed, or moved elsewhere by
	// something that fired no end overlap. Stale entries left forever would make a door
	// silently refuse one particular actor for the rest of its life.
	if (ArrivedAt.Num() > 0)
	{
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

		for (auto It = ArrivedAt.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid() || (Now - It.Value()) > ReentryFailsafeSeconds)
			{
				It.RemoveCurrent();
			}
		}
	}

	switch (Phase)
	{
	case ECantripPortalPhase::Opening:
	{
		const float Alpha = (OpenSeconds > 0.0f)
			? FMath::Clamp(PhaseTime / OpenSeconds, 0.0f, 1.0f)
			: 1.0f;

		SetOpenAlpha(Alpha);

		if (Alpha >= 1.0f)
		{
			Phase = ECantripPortalPhase::Held;
			PhaseTime = 0.0f;
		}
		break;
	}

	case ECantripPortalPhase::Held:
		SetOpenAlpha(1.0f);

		// Negative hold stands forever. Only a positive one runs down.
		if (HoldDuration >= 0.0f && PhaseTime >= HoldDuration)
		{
			CloseNow();
		}
		break;

	case ECantripPortalPhase::Closing:
	{
		const float Alpha = (CloseSeconds > 0.0f)
			? FMath::Clamp(1.0f - (PhaseTime / CloseSeconds), 0.0f, 1.0f)
			: 0.0f;

		SetOpenAlpha(Alpha);

		if (Alpha <= 0.0f)
		{
			// The far door is told first. A portal whose twin has been destroyed would
			// otherwise keep a stale weak pointer that reads as "linked" for one more frame
			// -- long enough to send someone to a door that no longer exists.
			LinkTo(nullptr);

			// Destroyed rather than hidden. A closed door has nothing left to do, and a
			// pool of invisible portals is a leak that only shows up in a long session.
			Destroy();
		}
		break;
	}
	}
}

void ACantripPortal::OnThresholdBegin(UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* Other, UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/, const FHitResult& /*Sweep*/)
{
	// Never another door.
	//
	// Two portals opened near each other overlap each other's thresholds, and each then
	// sends the other through -- which moves it into a third overlap, and so on. The log
	// showed four doors teleporting each other in a single frame. Nothing good comes of a
	// door walking through a door, so it is refused outright rather than rate-limited.
	if (Other && Other->IsA<ACantripPortal>())
	{
		return;
	}

	Transport(Other);
}

void ACantripPortal::OnThresholdEnd(UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* Other, UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	// Stepping clear of the door you came out of is what re-arms it. Exact, and it handles
	// the player who stands in the arrival doorway getting their bearings -- a timer alone
	// would send them back the moment it expired, while they were standing still.
	ArrivedAt.Remove(Other);
}

bool ACantripPortal::Transport(AActor* Who)
{
	if (!Who || !HasExit())
	{
		return false;
	}

	// Only a door that is fully open. Walking into one that is still growing would take you
	// through a gap the size of a coin, and one that is closing has already been described
	// to the player as finished.
	if (Phase != ECantripPortalPhase::Held)
	{
		return false;
	}

	// They came out of here a moment ago and have not stepped clear yet. Without this the
	// far door sends them straight back and the near one returns them, in the same frame,
	// until something breaks.
	if (ArrivedAt.Contains(Who))
	{
		return false;
	}

	if (bCasterOnly && Who != GetOwner())
	{
		return false;
	}

	ACantripPortal* Exit = LinkedPortal.Get();

	// A linked door beats a stored point even when both are set: a real door on the far side
	// knows where its own front is, and can be told that someone arrived.
	const FTransform ExitTransform = Exit ? Exit->GetActorTransform() : Destination;
	const FTransform Here = GetActorTransform();

	// The whole mapping, once: out of this door's frame, turned to face the other way, into
	// the far door's frame. Everything below is this delta applied to a different quantity.
	const FQuat Delta = ExitTransform.GetRotation() * PortalFlip * Here.GetRotation().Inverse();

	const FVector Local = Here.InverseTransformPosition(Who->GetActorLocation());
	const FVector Landing = ExitTransform.TransformPosition(PortalFlip.RotateVector(Local))
		+ (ExitTransform.GetRotation().GetForwardVector() * ExitClearance);

	// Registered BEFORE the move. Setting the location fires the far door's begin-overlap
	// synchronously, so an entry added afterwards would arrive too late to stop it -- the
	// traveller would already have been sent back by the time this line ran.
	if (Exit)
	{
		Exit->ArrivedAt.Add(Who, Exit->GetWorld() ? Exit->GetWorld()->GetTimeSeconds() : 0.0f);
	}

	Who->SetActorLocationAndRotation(Landing, Delta * Who->GetActorQuat(),
		/*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);

	if (APawn* Pawn = Cast<APawn>(Who))
	{
		// The camera, turned by the same delta rather than set to the exit's facing. Someone
		// who walked in looking over their shoulder should come out looking over their
		// shoulder; snapping the view to straight-ahead is disorienting in exactly the
		// moment the player most needs to know where they are.
		if (AController* Controller = Pawn->GetController())
		{
			Controller->SetControlRotation(
				(Delta * Pawn->GetControlRotation().Quaternion()).Rotator());
		}

		if (bPreserveVelocity)
		{
			if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent())
			{
				Movement->Velocity = Delta.RotateVector(Movement->Velocity);
				Movement->UpdateComponentVelocity();
			}
		}
	}

	OnActorEntered(Who);

	if (Exit)
	{
		Exit->OnActorArrived(Who);
	}

	UE_LOG(LogCantripPortal, Log, TEXT("PORTAL: %s passed through %s"),
		*Who->GetName(), *GetName());

	NotePassage(Who);

	return true;
}

void ACantripPortal::NotePassage(AActor* Who)
{
	if (CloseAfterPassageSeconds < 0.0f)
	{
		return;
	}

	// Owner is the caster: the ability sets it when it spawns the pair. Anyone else going
	// through is a guest, and a guest does not get to decide when her door shuts.
	if (bOnlyCasterTriggersClose && Who != GetOwner())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Already counting. Walking back and forth through her own door should not keep
	// restarting the grace period, or a door could be held open indefinitely by pacing.
	if (World->GetTimerManager().IsTimerActive(PassageCloseTimer))
	{
		return;
	}

	UE_LOG(LogCantripPortal, Log, TEXT("PORTAL %s: closing in %.1fs behind %s"),
		*GetName(), CloseAfterPassageSeconds, *Who->GetName());

	if (CloseAfterPassageSeconds <= 0.0f)
	{
		ShutBothEnds();
		return;
	}

	World->GetTimerManager().SetTimer(PassageCloseTimer, this,
		&ACantripPortal::ShutBothEnds, CloseAfterPassageSeconds, /*bLoop*/ false);
}

void ACantripPortal::ShutBothEnds()
{
	// The far side first. CloseNow is safe to call twice, so it does not matter if the
	// partner has already begun shutting for its own reasons -- but a door whose partner
	// closed without it would be a way back in from wherever she came out.
	if (ACantripPortal* Far = LinkedPortal.Get())
	{
		Far->CloseNow();
	}

	CloseNow();
}

float ACantripPortal::GetRemainingHold() const
{
	if (Phase != ECantripPortalPhase::Held || HoldDuration < 0.0f)
	{
		return 0.0f;
	}

	return FMath::Max(0.0f, HoldDuration - PhaseTime);
}

void ACantripPortal::ApplyArtOrientation()
{
	if (ArtOrientation.IsNearlyZero())
	{
		return;
	}

	const FQuat Fix = ArtOrientation.Quaternion();

	// EffectRoot carries every Niagara system beneath it, so turning it turns all of them
	// at once and any system added later is corrected without being thought about.
	if (EffectRoot)
	{
		EffectRoot->SetRelativeRotation(Fix * EffectRoot->GetRelativeRotation().Quaternion());
	}

	TArray<UMeshComponent*> Meshes;
	GetComponents<UMeshComponent>(Meshes);

	for (UMeshComponent* MeshComponent : Meshes)
	{
		// Anything already beneath EffectRoot has just been turned by its parent. Turning it
		// again here would put it at a hundred and eighty degrees -- facing backwards, into
		// the wall, which reads as the door being inside-out rather than doubly rotated.
		if (!MeshComponent || (EffectRoot && MeshComponent->IsAttachedTo(EffectRoot)))
		{
			continue;
		}

		MeshComponent->SetRelativeRotation(
			Fix * MeshComponent->GetRelativeRotation().Quaternion());
	}
}

void ACantripPortal::MeasureGeometry()
{
	// Mesh bounds only. Niagara bounds are either a fixed box the author typed or a
	// dynamic one that grows with the particles, and neither describes the DOORWAY -- a
	// system whose sparks drift two metres would report a door twice its real size.
	FBox Box(ForceInit);

	TArray<UMeshComponent*> Meshes;
	GetComponents<UMeshComponent>(Meshes);

	for (UMeshComponent* MeshComponent : Meshes)
	{
		if (MeshComponent)
		{
			Box += MeshComponent->Bounds.GetBox();
		}
	}

	// No mesh at all is a legitimate door -- one drawn entirely in Niagara. The threshold is
	// then the only honest statement of how big the opening is meant to be.
	if (!Box.IsValid && Threshold)
	{
		Box = Threshold->Bounds.GetBox();
	}

	// The LARGEST dimension, not the vertical one.
	//
	// Measuring Z assumes the art is authored standing up, and this pack's is not: SM_Portal
	// is a disc lying in the XY plane, so its Z extent is its THICKNESS. Measured that way a
	// six-metre doorway reported 22 units tall, and asking for a door two metres high then
	// scaled it by nineteen -- a flat pancake four hundred units across, which is exactly
	// what appeared in the sky.
	//
	// A doorway is the widest thing about its own geometry whichever way it is turned, so
	// the largest axis is the honest answer and it survives art authored on any plane.
	MeasuredHeight = Box.IsValid ? Box.GetSize().GetMax() : 0.0f;
}

void ACantripPortal::SizeToClearance(float DesiredHeight)
{
	if (DesiredHeight <= 0.0f)
	{
		return;
	}

	if (MeasuredHeight <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogCantripPortal, Warning,
			TEXT("PORTAL %s: cannot size to %.0f -- measured no geometry. ")
			TEXT("Left at scale %.2f."), *GetName(), DesiredHeight, OpenScale);
		return;
	}

	OpenScale = DesiredHeight / MeasuredHeight;

	// Re-applied at the CURRENT alpha rather than left for the next tick. Sizing happens
	// between spawning and the first frame of the opening, and a door that took a frame to
	// notice its new scale would be seen at the old one for exactly that frame.
	SetOpenAlpha(OpenAlpha);

	UE_LOG(LogCantripPortal, Log,
		TEXT("PORTAL %s: authored %.0f tall, sized to %.0f (scale %.2f)"),
		*GetName(), MeasuredHeight, DesiredHeight, OpenScale);
}

void ACantripPortal::GatherEffects()
{
	// Every Niagara on the actor, not only those under EffectRoot. A stray system parented
	// somewhere else is a mistake worth still sizing correctly -- refusing to drive it would
	// leave one emitter at full size on a door that has barely opened, which looks like the
	// system being broken rather than like it being attached to the wrong component.
	EffectSystems.Reset();
	GetComponents<UNiagaraComponent>(EffectSystems);

	// Dynamic instances, made once. The materials on the mesh asset are shared -- writing a
	// scalar straight to them would open every other door in the level at the same rate,
	// and would persist into the editor's own asset.
	PortalMaterials.Reset();

	if (MaterialAlphaParameter.IsNone())
	{
		return;
	}

	TArray<UMeshComponent*> Meshes;
	GetComponents<UMeshComponent>(Meshes);

	for (UMeshComponent* MeshComponent : Meshes)
	{
		if (!MeshComponent)
		{
			continue;
		}

		const int32 Slots = MeshComponent->GetNumMaterials();
		for (int32 Slot = 0; Slot < Slots; ++Slot)
		{
			if (UMaterialInstanceDynamic* Dynamic =
				MeshComponent->CreateAndSetMaterialInstanceDynamic(Slot))
			{
				PortalMaterials.Add(Dynamic);
			}
		}
	}
}

void ACantripPortal::SetOpenAlpha(float Alpha)
{
	OpenAlpha = Alpha;

	if (PortalRoot)
	{
		PortalRoot->SetRelativeScale3D(FVector(FMath::Max(KINDA_SMALL_NUMBER,
			Alpha * OpenScale)));
	}

	// The pack's own idea, generalised: one openness, pushed into every system, so the ring,
	// the vortex and the sparks agree about how far the way is open at every moment of it.
	for (const FPortalOpennessDriver& Driver : OpennessDrivers)
	{
		if (Driver.Parameter.IsNone())
		{
			continue;
		}

		const float Value = FMath::Lerp(Driver.Closed, Driver.Open, Alpha);

		for (UNiagaraComponent* System : EffectSystems)
		{
			if (System)
			{
				System->SetVariableFloat(Driver.Parameter, Value);
			}
		}
	}

	for (UMaterialInstanceDynamic* Dynamic : PortalMaterials)
	{
		if (Dynamic)
		{
			Dynamic->SetScalarParameterValue(MaterialAlphaParameter, Alpha);
		}
	}

	// Every frame rather than on change: a Niagara rate or a material parameter driven
	// from this wants the value while it is moving, and the phases above already only
	// move it when something is actually happening.
	OnPortalAlphaChanged(Alpha);
}
