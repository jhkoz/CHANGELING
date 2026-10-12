// ChangelingItem.h
//
// What an object IS, as a row in a table.
//
// Authored the same way a cantrip is -- a DataTable row, named by an FName -- so adding a
// thing to the world is editing a spreadsheet rather than making an asset. Everything
// that refers to an object refers to it by row handle, which is why the pickup could be
// written before this struct existed and needs no changes now that it does.
//
// Deliberately thin. There is no durability, no charges, no attunement and no equip slot
// here yet, because none of those can be designed sensibly before there is an inventory
// screen to look at. What IS here is the minimum an object needs to be picked up, listed,
// and told apart from another one.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ChangelingItem.generated.h"

class UGameplayAbility;
class UGameplayEffect;
class UTexture2D;

/**
 * Where a thing is worn. One item per slot, and equipping into a full one swaps.
 *
 * None is the default and means the object is not equippable at all -- the slot is the
 * gate rather than a separate bEquippable bool, so a row cannot claim to be wearable and
 * have nowhere to go. The effect stays optional: an object can occupy a slot and do
 * nothing but be seen.
 */
UENUM(BlueprintType)
enum class EEquipSlot : uint8
{
	None     UMETA(DisplayName = "Not equippable"),
	Head,
	Body,
	Hands,
	MainHand,
	OffHand,
	Trinket
};

/**
 * One object, as authored.
 */
USTRUCT(BlueprintType)
struct CHANGELING_API FChangelingItem : public FTableRowBase
{
	GENERATED_BODY()

	/** What she calls it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	/** What it looks like written down. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText Description;

	/** Soft, like the cantrip emblems -- a bag of thirty things should not load thirty
	 *  textures before one of them is drawn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TSoftObjectPtr<UTexture2D> Icon;

	/**
	 * How many fit in one stack. One means it does not stack.
	 *
	 * A count rather than a bool because "stackable" is never really a yes or no --
	 * arrows go in hundreds, dross in handfuls, a sword on its own.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1"))
	int32 MaxStack = 1;

	/**
	 * Using it activates this ability. Empty means it cannot be used.
	 *
	 * The ability IS the usability, rather than a bool beside one -- two fields would let
	 * a row claim to be usable and do nothing, and that row would look correct in the
	 * table. Granted and activated once at the moment of use, so an object's working is
	 * not sitting on her ability list while it is in the bag.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Use")
	TSubclassOf<UGameplayAbility> UseAbility;

	/**
	 * Whether using it spends it. A draught is drunk; a wand is not.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Use")
	bool bConsumeOnUse = false;

	/** Where it is worn. None means it cannot be equipped at all. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Equip")
	EEquipSlot EquipSlot = EEquipSlot::None;

	/**
	 * Applied while it is worn, and removed when it comes off. Optional -- a thing can
	 * fill a slot and grant nothing.
	 *
	 * An infinite-duration effect is what this wants: worn armour is not a three-second
	 * buff, and the inventory removes the effect by handle when it comes off rather than
	 * waiting for a duration nobody set.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Equip")
	TSubclassOf<UGameplayEffect> EquipEffect;

	/**
	 * Whether the object is chimerical rather than mundane.
	 *
	 * A marker only for now: nothing hides a chimerical object from mortal eyes yet. It
	 * is here at the start because it is the one property of a thing in this setting that
	 * cannot be added later without going back through every row and every save -- what
	 * is real and what is dreamed is not a display detail, and a table authored without
	 * it would have to be authored twice.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	bool bChimerical = false;
};

/**
 * One stack in a bag: what it is, and how many.
 */
USTRUCT(BlueprintType)
struct CHANGELING_API FInventoryEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FDataTableRowHandle Item;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	int32 Count = 0;
};
