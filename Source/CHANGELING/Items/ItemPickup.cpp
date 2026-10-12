// ItemPickup.cpp

#include "ItemPickup.h"

#include "Components/SphereComponent.h"
#include "GameFramework/Pawn.h"
#include "NiagaraComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogItemPickup, Log, All);

bool IItemReceiver::CanTakeItem_Implementation(const FDataTableRowHandle& Item) const
{
	return true;
}

bool IItemReceiver::ReceiveItem_Implementation(const FDataTableRowHandle& Item)
{
	return false;
}

AItemPickup::AItemPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	// The sphere is the root, not a mesh under one: a pickup has no body of its own, only
	// a reach and a light. A mesh, when an object wants one, hangs off this.
	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->InitSphereRadius(64.0f);
	Trigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	SetRootComponent(Trigger);

	Effect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Effect"));
	Effect->SetupAttachment(Trigger);
}

void AItemPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Set here as well as at runtime so the bloom shows in the level viewport -- a pickup
	// that is invisible until you press Play is a pickup you cannot place.
	if (Effect)
	{
		Effect->SetAsset(Bloom);
	}
}

void AItemPickup::BeginPlay()
{
	Super::BeginPlay();

	if (Trigger)
	{
		Trigger->OnComponentBeginOverlap.AddDynamic(this, &AItemPickup::HandleOverlap);
	}

	if (Item.IsNull())
	{
		UE_LOG(LogItemPickup, Warning,
			TEXT("PICKUP: %s has no Item row set -- it will hand over nothing."),
			*GetName());
	}
}

void AItemPickup::HandleOverlap(UPrimitiveComponent*, AActor* Other,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (!Other || Item.IsNull())
	{
		return;
	}

	// The actor itself, or any component it carries. Checking components too is what lets
	// "this thing can hold objects" be a component you drop on, rather than a base class
	// every such actor has to inherit -- an inventory belongs to a body the same way a
	// hotbar belongs to a controller, and neither should force a class change.
	//
	// No interface, no pockets. This is the whole of the "should a bird flying through the
	// light pick up the sword" question: the bird has neither, so it flies through.
	UObject* Receiver = nullptr;

	if (Other->GetClass()->ImplementsInterface(UItemReceiver::StaticClass()))
	{
		Receiver = Other;
	}
	else
	{
		for (UActorComponent* Component : Other->GetComponents())
		{
			if (Component
				&& Component->GetClass()->ImplementsInterface(UItemReceiver::StaticClass()))
			{
				Receiver = Component;
				break;
			}
		}
	}

	if (!Receiver)
	{
		return;
	}

	// Asked before taken, and the asking is not allowed to change anything -- so a
	// refusal here leaves the object lying in the grass rather than swallowing it.
	if (!IItemReceiver::Execute_CanTakeItem(Receiver, Item))
	{
		return;
	}

	if (!IItemReceiver::Execute_ReceiveItem(Receiver, Item))
	{
		// Refused on the second call: a bag that filled between the two, or something she
		// cannot carry twice. Not an error, and not a reason to go dark.
		return;
	}

	UE_LOG(LogItemPickup, Log, TEXT("PICKUP: %s took '%s'."),
		*GetNameSafe(Other), *Item.RowName.ToString());

	OnItemTaken.Broadcast(this, Other);

	if (bRecycle)
	{
		GoInert();
	}
	else
	{
		Destroy();
	}
}

void AItemPickup::GoInert()
{
	// Collision off FIRST. The effect deactivating is what the player sees, but it is the
	// collision that stops a second overlap arriving in the same frame from a capsule
	// still standing inside the sphere.
	if (Trigger)
	{
		Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (Effect)
	{
		Effect->Deactivate();
	}

	Item = FDataTableRowHandle();
}

void AItemPickup::Rearm(const FDataTableRowHandle& NewItem)
{
	Item = NewItem;

	if (Trigger)
	{
		Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}

	if (Effect)
	{
		Effect->SetAsset(Bloom);
		Effect->Activate(true);
	}
}
