// InventoryComponent.h
//
// What carries objects. Add it to anything that should be able to pick things up.
//
// It implements IItemReceiver, and AItemPickup looks for the interface on the actor OR on
// any of its components -- so enabling pickups on a character is adding this component and
// nothing else. No class to change, no Blueprint interface to implement, no event to
// wire. That is deliberate: the alternative made every actor that might ever hold a thing
// into a code change.
//
// Note this sits on the PAWN, not the controller, which is the opposite of where the
// cantrip hotbar lives. The bar is a set of shortcuts belonging to whoever is playing;
// a bag belongs to the body carrying it, and an adversary needs one without ever having a
// PlayerController.

#pragma once

#include "CoreMinimal.h"
#include "ChangelingItem.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "ItemPickup.h"
#include "InventoryComponent.generated.h"

class UAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FInventoryChanged);

/**
 * One worn object and the effect it is holding open.
 *
 * The handle is kept because that is the only way to take the effect off again: an
 * infinite-duration effect has no end of its own, so unequipping means removing exactly
 * the instance that equipping created. Losing the handle leaks a permanent buff.
 */
USTRUCT(BlueprintType)
struct CHANGELING_API FEquippedItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FDataTableRowHandle Item;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	EEquipSlot Slot = EEquipSlot::None;

	FActiveGameplayEffectHandle Effect;
};

UCLASS(ClassGroup = (Changeling), meta = (BlueprintSpawnableComponent))
class CHANGELING_API UInventoryComponent : public UActorComponent, public IItemReceiver
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	/**
	 * How many separate stacks fit. Not a weight and not a volume.
	 *
	 * Slots because a slot count is the only capacity rule a player can count at a glance
	 * without the UI doing arithmetic at them.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Inventory", meta = (ClampMin = "1"))
	int32 Capacity = 20;

	/** Everything carried, in the order it was picked up. */
	UFUNCTION(BlueprintPure, Category = "Item|Inventory")
	const TArray<FInventoryEntry>& GetEntries() const { return Entries; }

	/** How many of a thing she is carrying, across every stack. */
	UFUNCTION(BlueprintPure, Category = "Item|Inventory")
	int32 CountOf(const FDataTableRowHandle& Item) const;

	/** Whether there is room for one more of this, in a stack or a free slot. */
	UFUNCTION(BlueprintPure, Category = "Item|Inventory")
	bool HasRoomFor(const FDataTableRowHandle& Item) const;

	/** Put something in. Returns how many were actually taken (0 when full). */
	UFUNCTION(BlueprintCallable, Category = "Item|Inventory")
	int32 AddItem(const FDataTableRowHandle& Item, int32 Count = 1);

	/** Take something out. Returns how many were actually removed. */
	UFUNCTION(BlueprintCallable, Category = "Item|Inventory")
	int32 RemoveItem(const FDataTableRowHandle& Item, int32 Count = 1);

	/** The authored row behind a handle, or null if the handle is empty or stale. */
	UFUNCTION(BlueprintPure, Category = "Item|Inventory")
	static bool FindItemRow(const FDataTableRowHandle& Item, FChangelingItem& OutRow);

	/** Fired whenever anything is added, removed, equipped or unequipped. */
	UPROPERTY(BlueprintAssignable, Category = "Item|Inventory")
	FInventoryChanged OnInventoryChanged;

	// -- Using and equipping ------------------------------------------------------

	/**
	 * Use it: grant its ability and activate it once, spending the object if it says so.
	 *
	 * Granted at the moment of use rather than when it is picked up, so a bag of ten
	 * draughts does not put ten abilities on her ability list -- and so the spell bar,
	 * which reads that list to decide what she knows, does not fill up with objects.
	 */
	UFUNCTION(BlueprintCallable, Category = "Item|Use")
	bool UseItem(const FDataTableRowHandle& Item);

	/**
	 * Put it on. Applies the row's effect and holds the handle until it comes off.
	 *
	 * Equipping into an occupied slot takes the other thing off first. A refusal would be
	 * the safer-looking choice and the worse one: it makes changing a sword a two-step
	 * chore, and every game that does it teaches the player to unequip before equipping.
	 */
	UFUNCTION(BlueprintCallable, Category = "Item|Equip")
	bool EquipItem(const FDataTableRowHandle& Item);

	/** What is worn in a slot, if anything. */
	UFUNCTION(BlueprintPure, Category = "Item|Equip")
	bool GetEquippedInSlot(EEquipSlot Slot, FDataTableRowHandle& OutItem) const;

	/** Take it off, removing exactly the effect this equipping applied. */
	UFUNCTION(BlueprintCallable, Category = "Item|Equip")
	bool UnequipItem(const FDataTableRowHandle& Item);

	UFUNCTION(BlueprintPure, Category = "Item|Equip")
	bool IsEquipped(const FDataTableRowHandle& Item) const;

	UFUNCTION(BlueprintPure, Category = "Item|Equip")
	const TArray<FEquippedItem>& GetEquipped() const { return Equipped; }

	// IItemReceiver.
	virtual bool CanTakeItem_Implementation(const FDataTableRowHandle& Item) const override;
	virtual bool ReceiveItem_Implementation(const FDataTableRowHandle& Item) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** Index of a stack of this item with room left, or INDEX_NONE. */
	int32 FindStackWithRoom(const FDataTableRowHandle& Item) const;

	/** The ability system on whatever owns this bag, or null. */
	UAbilitySystemComponent* GetASC() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item|Inventory")
	TArray<FInventoryEntry> Entries;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item|Equip")
	TArray<FEquippedItem> Equipped;
};
