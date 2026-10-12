// CantripBookList.h
//
// The spellbook's rows and tabs as UObjects, so ListView can drive them.
//
// UMG's ListView needs UObjects, not structs -- which is a small tax with a large
// return. Handing a list its items replaces the loop that built rows by hand, and
// with it goes the widget array, the click dispatcher, the selection bookkeeping,
// and the dangling loop variable that had the detail pane showing the last tab's
// first cantrip instead of the one that was clicked.
//
// Each item carries its own hotbar reference. A row that is GIVEN everything it
// needs cannot be built missing something: there is no second pin to forget.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "CantripHotbarComponent.h"
#include "CantripBookList.generated.h"

/** One cantrip row, as a ListView item. */
UCLASS(BlueprintType)
class CHANGELING_API UCantripEntryObject : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook")
	FCantripBookEntry Entry;

	/** So the row can ask for its own style without being wired to the book. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook")
	TObjectPtr<UCantripHotbarComponent> Hotbar;
};

/** One Art tab, as a ListView item. */
UCLASS(BlueprintType)
class CHANGELING_API UCantripArtObject : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook")
	FCantripArtPage Page;

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook")
	TObjectPtr<UCantripHotbarComponent> Hotbar;
};

/**
 * Turning the book into list items.
 *
 * A library rather than methods on the component, only because the component's
 * header would otherwise have to know about these classes and they already know
 * about it.
 */
UCLASS()
class CHANGELING_API UCantripBookLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Every Art, in book order, ready for the rail's Set List Items.
	 *
	 * Returns an empty array rather than failing when the hotbar is null, so a
	 * mis-wired rail draws nothing instead of crashing -- and "nothing" is a symptom
	 * you can see, which is more than the old silent defaults managed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Spellbook")
	static TArray<UObject*> MakeArtItems(UCantripHotbarComponent* Hotbar);

	/** One Art's cantrips, ascending by level, ready for the list's Set List Items. */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Spellbook")
	static TArray<UObject*> MakeCantripItems(UCantripHotbarComponent* Hotbar, FGameplayTag ArtTag);

	/**
	 * The same, from a page already in hand.
	 *
	 * ShowArt reads the page to fill its header and then asked for it a second time
	 * to fill the list, and GetArtPage rebuilds all ninety rows each call -- so every
	 * click on the rail walked the whole catalogue twice. Pass the page you have.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Spellbook")
	static TArray<UObject*> MakeItemsFromPage(UCantripHotbarComponent* Hotbar,
		const FCantripArtPage& Page);
};
