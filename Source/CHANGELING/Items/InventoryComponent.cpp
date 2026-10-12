// InventoryComponent.cpp

#include "InventoryComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"

DEFINE_LOG_CATEGORY_STATIC(LogInventory, Log, All);

namespace
{
	/** Two handles name the same object when the table and the row both match. */
	bool SameItem(const FDataTableRowHandle& A, const FDataTableRowHandle& B)
	{
		return A.DataTable == B.DataTable && A.RowName == B.RowName;
	}
}

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UInventoryComponent::FindItemRow(const FDataTableRowHandle& Item,
	FChangelingItem& OutRow)
{
	if (Item.IsNull())
	{
		return false;
	}

	// FindRow logs its own warning on a missing row, so the context string says which
	// system was asking -- a bare "row not found" in a project this size is a scavenger
	// hunt.
	if (const FChangelingItem* Found =
		Item.DataTable->FindRow<FChangelingItem>(Item.RowName, TEXT("Inventory")))
	{
		OutRow = *Found;
		return true;
	}

	return false;
}

int32 UInventoryComponent::CountOf(const FDataTableRowHandle& Item) const
{
	int32 Total = 0;

	for (const FInventoryEntry& Entry : Entries)
	{
		if (SameItem(Entry.Item, Item))
		{
			Total += Entry.Count;
		}
	}

	return Total;
}

int32 UInventoryComponent::FindStackWithRoom(const FDataTableRowHandle& Item) const
{
	FChangelingItem Row;

	if (!FindItemRow(Item, Row) || Row.MaxStack <= 1)
	{
		// Unstackable things never share a slot, so there is never a stack with room.
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		if (SameItem(Entries[Index].Item, Item) && Entries[Index].Count < Row.MaxStack)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

bool UInventoryComponent::HasRoomFor(const FDataTableRowHandle& Item) const
{
	if (Item.IsNull())
	{
		return false;
	}

	return FindStackWithRoom(Item) != INDEX_NONE || Entries.Num() < Capacity;
}

int32 UInventoryComponent::AddItem(const FDataTableRowHandle& Item, int32 Count)
{
	if (Item.IsNull() || Count <= 0)
	{
		return 0;
	}

	FChangelingItem Row;

	if (!FindItemRow(Item, Row))
	{
		UE_LOG(LogInventory, Warning,
			TEXT("INVENTORY: '%s' is not a row in the table it names -- nothing added."),
			*Item.RowName.ToString());
		return 0;
	}

	const int32 MaxStack = FMath::Max(1, Row.MaxStack);
	int32 Remaining = Count;

	// Fill part-used stacks before opening a new slot, so picking up two arrows at
	// different times does not cost two slots.
	while (Remaining > 0)
	{
		const int32 Stack = FindStackWithRoom(Item);

		if (Stack == INDEX_NONE)
		{
			break;
		}

		const int32 Room = MaxStack - Entries[Stack].Count;
		const int32 Moved = FMath::Min(Room, Remaining);

		Entries[Stack].Count += Moved;
		Remaining -= Moved;
	}

	while (Remaining > 0 && Entries.Num() < Capacity)
	{
		FInventoryEntry& Fresh = Entries.AddDefaulted_GetRef();
		Fresh.Item = Item;
		Fresh.Count = FMath::Min(MaxStack, Remaining);

		Remaining -= Fresh.Count;
	}

	const int32 Taken = Count - Remaining;

	if (Taken > 0)
	{
		OnInventoryChanged.Broadcast();
	}

	return Taken;
}

int32 UInventoryComponent::RemoveItem(const FDataTableRowHandle& Item, int32 Count)
{
	if (Item.IsNull() || Count <= 0)
	{
		return 0;
	}

	int32 Remaining = Count;

	// Backwards, because emptied stacks are removed as we go and walking forwards past a
	// removal skips the entry that slid into its place.
	for (int32 Index = Entries.Num() - 1; Index >= 0 && Remaining > 0; --Index)
	{
		if (!SameItem(Entries[Index].Item, Item))
		{
			continue;
		}

		const int32 Moved = FMath::Min(Entries[Index].Count, Remaining);

		Entries[Index].Count -= Moved;
		Remaining -= Moved;

		if (Entries[Index].Count <= 0)
		{
			Entries.RemoveAt(Index);
		}
	}

	const int32 Removed = Count - Remaining;

	if (Removed > 0)
	{
		// The last one out takes its effect with it. Without this, dropping worn armour
		// -- or drinking the last of something equipped -- leaves an infinite effect
		// applied by an object she is no longer carrying, and nothing left to remove it.
		if (CountOf(Item) <= 0 && IsEquipped(Item))
		{
			UnequipItem(Item);
		}

		OnInventoryChanged.Broadcast();
	}

	return Removed;
}

UAbilitySystemComponent* UInventoryComponent::GetASC() const
{
	// Through the interface rather than FindComponentByClass, so a pawn that keeps its
	// ability system somewhere unusual still answers correctly.
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
}

bool UInventoryComponent::UseItem(const FDataTableRowHandle& Item)
{
	FChangelingItem Row;

	if (!FindItemRow(Item, Row) || !Row.UseAbility || CountOf(Item) <= 0)
	{
		return false;
	}

	UAbilitySystemComponent* ASC = GetASC();

	if (!ASC)
	{
		UE_LOG(LogInventory, Warning,
			TEXT("INVENTORY: %s has no ability system -- '%s' cannot be used."),
			*GetNameSafe(GetOwner()), *Item.RowName.ToString());
		return false;
	}

	// GiveAbilityAndActivateOnce, not GiveAbility: the ability is granted, run, and taken
	// off again in one call. An object's working should not linger on her ability list,
	// where IsKnown would report it as a cantrip she has learned and the spell bar would
	// lay it out beside her real ones.
	FGameplayAbilitySpec Spec(Row.UseAbility, 1, INDEX_NONE, this);

	if (!ASC->GiveAbilityAndActivateOnce(Spec).IsValid())
	{
		// Refused by its own cost or cooldown, most likely. The object is not spent.
		return false;
	}

	if (Row.bConsumeOnUse)
	{
		RemoveItem(Item, 1);
	}
	else
	{
		OnInventoryChanged.Broadcast();
	}

	return true;
}

bool UInventoryComponent::IsEquipped(const FDataTableRowHandle& Item) const
{
	return Equipped.ContainsByPredicate([&Item](const FEquippedItem& Worn)
	{
		return SameItem(Worn.Item, Item);
	});
}

bool UInventoryComponent::GetEquippedInSlot(EEquipSlot Slot,
	FDataTableRowHandle& OutItem) const
{
	if (Slot == EEquipSlot::None)
	{
		return false;
	}

	for (const FEquippedItem& Worn : Equipped)
	{
		if (Worn.Slot == Slot)
		{
			OutItem = Worn.Item;
			return true;
		}
	}

	return false;
}

bool UInventoryComponent::EquipItem(const FDataTableRowHandle& Item)
{
	FChangelingItem Row;

	// The slot is the gate, not the effect: an object can fill a hand and grant nothing.
	if (!FindItemRow(Item, Row) || Row.EquipSlot == EEquipSlot::None
		|| CountOf(Item) <= 0 || IsEquipped(Item))
	{
		return false;
	}

	UAbilitySystemComponent* ASC = GetASC();

	if (!ASC)
	{
		return false;
	}

	// Whatever was in the slot comes off first, taking its effect with it. Done before
	// the new effect is applied so the two never overlap for a frame -- a swap that
	// stacked both bonuses, however briefly, is the kind of thing players find.
	FDataTableRowHandle Occupant;

	if (GetEquippedInSlot(Row.EquipSlot, Occupant))
	{
		UnequipItem(Occupant);
	}

	FEquippedItem Worn;
	Worn.Item = Item;
	Worn.Slot = Row.EquipSlot;

	if (Row.EquipEffect)
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(this);

		const FGameplayEffectSpecHandle SpecHandle =
			ASC->MakeOutgoingSpec(Row.EquipEffect, 1.0f, Context);

		if (!SpecHandle.IsValid())
		{
			return false;
		}

		Worn.Effect = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

		if (!Worn.Effect.IsValid())
		{
			return false;
		}
	}

	Equipped.Add(Worn);
	OnInventoryChanged.Broadcast();

	return true;
}

bool UInventoryComponent::UnequipItem(const FDataTableRowHandle& Item)
{
	const int32 Index = Equipped.IndexOfByPredicate([&Item](const FEquippedItem& Worn)
	{
		return SameItem(Worn.Item, Item);
	});

	if (Index == INDEX_NONE)
	{
		return false;
	}

	if (UAbilitySystemComponent* ASC = GetASC())
	{
		ASC->RemoveActiveGameplayEffect(Equipped[Index].Effect);
	}

	Equipped.RemoveAt(Index);
	OnInventoryChanged.Broadcast();

	return true;
}

void UInventoryComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	// Everything worn comes off. An infinite effect outlives the component that applied
	// it, so a bag destroyed while she is wearing armour would leave the armour's effect
	// on her ability system with nothing left holding its handle.
	if (UAbilitySystemComponent* ASC = GetASC())
	{
		for (const FEquippedItem& Worn : Equipped)
		{
			ASC->RemoveActiveGameplayEffect(Worn.Effect);
		}
	}

	Equipped.Reset();

	Super::EndPlay(Reason);
}

bool UInventoryComponent::CanTakeItem_Implementation(const FDataTableRowHandle& Item) const
{
	// Asked before taken, and this one is not allowed to change anything -- a pickup that
	// asks and is told yes still has to be told yes again by ReceiveItem before it goes
	// dark.
	return HasRoomFor(Item);
}

bool UInventoryComponent::ReceiveItem_Implementation(const FDataTableRowHandle& Item)
{
	return AddItem(Item, 1) > 0;
}
