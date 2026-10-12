// CantripBookList.cpp

#include "CantripBookList.h"

TArray<UObject*> UCantripBookLibrary::MakeArtItems(UCantripHotbarComponent* Hotbar)
{
	TArray<UObject*> Items;

	if (!Hotbar)
	{
		return Items;
	}

	const TArray<FCantripArtPage> Pages = Hotbar->GetSpellbook();
	Items.Reserve(Pages.Num());

	for (const FCantripArtPage& Page : Pages)
	{
		// Outered to the hotbar rather than to the widget: the rail is rebuilt
		// whenever she learns something, and items outered to a widget that is
		// about to be replaced are garbage the next collection has to chase.
		UCantripArtObject* Item = NewObject<UCantripArtObject>(Hotbar);
		Item->Page = Page;
		Item->Hotbar = Hotbar;
		Items.Add(Item);
	}

	return Items;
}

TArray<UObject*> UCantripBookLibrary::MakeCantripItems(UCantripHotbarComponent* Hotbar,
	FGameplayTag ArtTag)
{
	TArray<UObject*> Items;

	if (!Hotbar)
	{
		return Items;
	}

	// GetArtPage already sorts by level and returns an empty page for a tag with no
	// rows, so an unknown Art clears the list rather than leaving the previous one
	// up -- which is the bug the old graph had when a page came back empty.
	return MakeItemsFromPage(Hotbar, Hotbar->GetArtPage(ArtTag));
}

TArray<UObject*> UCantripBookLibrary::MakeItemsFromPage(UCantripHotbarComponent* Hotbar,
	const FCantripArtPage& Page)
{
	TArray<UObject*> Items;

	if (!Hotbar)
	{
		return Items;
	}

	Items.Reserve(Page.Entries.Num());

	for (const FCantripBookEntry& Entry : Page.Entries)
	{
		UCantripEntryObject* Item = NewObject<UCantripEntryObject>(Hotbar);
		Item->Entry = Entry;
		Item->Hotbar = Hotbar;
		Items.Add(Item);
	}

	return Items;
}
