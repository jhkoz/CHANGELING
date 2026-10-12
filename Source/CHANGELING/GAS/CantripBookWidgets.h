// CantripBookWidgets.h
//
// The spellbook's appearance, in code.
//
// Every colour, outline and type size in the book used to be typed into eight widgets
// by hand, which meant the palette existed in eight places and drifted in all of them.
// These four classes hold it once. A Blueprint reparented to one of them is styled by
// NativePreConstruct, which runs in the Designer as well as at runtime -- so the book
// looks finished while it is being laid out, and nobody has to remember what shade of
// grey the leading was.
//
// Widgets are bound with BindWidgetOptional rather than BindWidget on purpose: a
// missing name should leave that one element unstyled, not refuse to compile the
// Blueprint. Half a styled book is a thing you can look at and fix; a Blueprint that
// will not open is not.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "Blueprint/UserWidget.h"
#include "CantripTypes.h"
#include "CantripBookWidgets.generated.h"

class UBorder;
class UButton;
class UCantripEntryObject;
class UCantripHotbarComponent;
class UFont;
class UGameplayAbility;
class UImage;
class UListView;
class USizeBox;
class UPanelWidget;
class UTextBlock;
class UTileView;

struct FCantripBookEntry;

/** One Realm chip was pressed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCantripRealmChipClicked, ECantripRealm, Realm);

/**
 * A cantrip in transit -- between two slots, or out of the book onto the bar.
 *
 * Carries the slot it left rather than the widget it left, so a drop only ever has to
 * talk to the hotbar component. Widgets are rebuilt on every OnHotbarChanged and the
 * one a drag started from may well not exist by the time it lands.
 */
UCLASS()
class CHANGELING_API UCantripDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	/** Where it came from. -1 when dragged out of the spellbook rather than off the bar. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Hotbar")
	int32 SourceSlotIndex = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Hotbar")
	TSubclassOf<UGameplayAbility> AbilityClass;
};

/**
 * Anything in the book that paints itself from the glass palette.
 *
 * The palette is a member rather than a global so a Blueprint's class defaults can
 * still tune it without a rebuild -- UI is tuned by eye, and a colour that needs a
 * compile to change is a colour that never gets changed.
 */
UCLASS(Abstract)
class CHANGELING_API UCantripGlassWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UCantripGlassWidget(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Style")
	FCantripGlass Glass;

	// Two faces, and the split is what each kind of text is FOR. A serif is read --
	// names, prose, the Art she has open. A sans is scanned -- labels, chips, the
	// rung on a row. Cormorant at 9pt tracked out to 300 would fall apart, so the
	// small caps stay on the engine's neutral face and are not overridden here.

	/** Titles and names. Cormorant Garamond Medium. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Style|Type")
	TObjectPtr<UFont> DisplayFont;

	/** Running prose. Cormorant Garamond Regular. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Style|Type")
	TObjectPtr<UFont> BodyFont;

	/** A judgement rather than a label -- the ease verdict. Cormorant Italic. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Style|Type")
	TObjectPtr<UFont> AsideFont;

	/** Repaint from the current palette. Called for you on construct. */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Style")
	void ApplyGlass();

	/**
	 * The hotbar every cantrip widget should be reading -- the one on the CONTROLLER.
	 *
	 * There is a second component of the same class on the character, and a widget that
	 * finds its way to that one gets a component with no DataTables set: it answers every
	 * query truthfully and emptily, so nothing errors and nothing draws. That has cost
	 * two debugging sessions -- the blank full description, and the missing lore -- both
	 * of which looked like data bugs and were neither.
	 *
	 * Widgets should call this instead of getting a component off whatever actor is to
	 * hand. Returns null before possession, which is worth a Branch in Construct.
	 */
	UFUNCTION(BlueprintPure, Category = "Cantrip|Hotbar")
	UCantripHotbarComponent* GetPlayerHotbar() const;

protected:
	virtual void NativePreConstruct() override;

	/** Subclasses paint their own widgets here. */
	virtual void OnApplyGlass() {}

	/** A leaded pane: filled ground, came outline, rounded corners. */
	void PaintPane(UBorder* Pane, const FLinearColor& Fill) const;

	/** Text at a given size and face. A null face leaves whatever the Designer set. */
	static void PaintText(UTextBlock* Text, const FLinearColor& Colour, float Size,
		UFont* Face = nullptr);

	/** An eyebrow label: small, tracked out, quiet. */
	static void PaintLabel(UTextBlock* Text, const FLinearColor& Colour,
		float Size = 9.0f, int32 Tracking = 300);
};

/**
 * WBP_Spellbook -- the book itself: three leaded panes and the page header.
 */
UCLASS(Abstract)
class CHANGELING_API UCantripBookWidget : public UCantripGlassWidget
{
	GENERATED_BODY()

public:
	/**
	 * Roundel size in the rail. The TileView fits as many across as the width allows.
	 *
	 * A real setting now. It used to be computed from RailWidth by halving, which meant
	 * the roundels grew with the rail and the columns stayed stubbornly at two -- so
	 * widening the rail bought nothing but bigger circles, and past a point they no
	 * longer fitted down the screen. Size and width are separate questions and this is
	 * the one worth answering by eye.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Style")
	float ArtTileSize = 112.0f;

	/**
	 * Rail width. At 496 with 112 roundels that is four columns: eighteen Arts in five
	 * rows, well inside a 1080-tall viewport with nothing to scroll.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Style")
	float RailWidth = 496.0f;

	/**
	 * Width of the cantrip list, the middle column.
	 *
	 * Zero means "leave it to the layout", which is what the book did before this
	 * existed -- the column took whatever the Designer slot gave it, so narrowing the
	 * rail handed the space to whichever pane happened to be set to fill rather than to
	 * the list. Set a number and the list takes exactly that, the way the rail does.
	 * Needs a SizeBox named Box_List around the column to have anywhere to apply.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Style")
	float ListWidth = 0.0f;

	/**
	 * Width of the detail pane, the right-hand column. Zero leaves it to the layout.
	 *
	 * Worth leaving at zero if the other two are set: with the rail and the list both
	 * pinned, the detail pane is the only column left to absorb the slack, and pinning
	 * all three means any width the book does not add up to shows as a gap. Needs a
	 * SizeBox named Box_Detail around the column.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Style")
	float DetailWidth = 0.0f;

	/** Side of the close button, in pixels. Square. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Style")
	float CloseButtonSize = 28.0f;

	/**
	 * Shut the book.
	 *
	 * Lives here rather than in the character's input graph so the close button and
	 * the key cannot disagree about whether the book is open -- which a FlipFlop and
	 * a button between them will always eventually manage.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Spellbook")
	void CloseBook();

protected:
	virtual void OnApplyGlass() override;

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UBorder> Bdr_Box;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UBorder> Bdr_Rail;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UBorder> Bdr_List;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_ArtsLabel;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Eyebrow;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_ArtTitle;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_ArtProgress;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<USizeBox> Box_Rail;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<USizeBox> Box_List;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<USizeBox> Box_Detail;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UTileView> Tile_Arts;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UListView> Lst_Entries;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<USizeBox> Box_Close;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Close;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Close;
};

/**
 * WBP_CantripEntry -- one cantrip in the list.
 *
 * Only the parts that never change live here. The row's tint, name colour, chip and
 * icon dimming come from FCantripPalette through StyleFor, because those say what
 * standing the cantrip is in and that is not a style question.
 */
UCLASS(Abstract)
class CHANGELING_API UCantripEntryWidget : public UCantripGlassWidget
{
	GENERATED_BODY()

public:
	UCantripEntryWidget(const FObjectInitializer& ObjectInitializer);

	/**
	 * Which button lifts a cantrip out of the book and onto the bar.
	 *
	 * The MIDDLE one, matching the hotbar. Left belongs to reading -- clicking a row is
	 * how the detail pane changes. Right is not available either, whatever it looks
	 * like: STableViewBase claims it for drag-to-scroll, so a right-drag anywhere
	 * inside a ListView or TileView scrolls the list and the row never sees it.
	 *
	 * Keep this and the slot's DragButton the same key: "pick this up" should not mean
	 * two different things eight inches apart on the same screen.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Spellbook")
	FKey DragButton;

protected:
	virtual void OnApplyGlass() override;

	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;

	virtual void NativeOnDragDetected(const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;

	/** The row's item, or null if the list has not assigned one yet. */
	UCantripEntryObject* GetEntryItem() const;

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Row;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UBorder> Bdr_Row;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UImage> Img_Level;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Name;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Desc;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UBorder> Bdr_Chip;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Chip;
};

/**
 * WBP_RealmChip -- one Realm the cantrip may be aimed through, as a pressable pane.
 *
 * Three states, and they answer three different questions. CHOSEN is "this casting goes
 * through here" -- gold, because that is the same thing gold means everywhere else in
 * the book. LEARNED-but-unchosen is available and quiet. UNLEARNED is still shown and
 * still pressable: aiming through a Realm she has no dots in is legal and costs an extra
 * Glamour, and hiding it would hide a decision the player is allowed to make.
 */
UCLASS(Abstract)
class CHANGELING_API UCantripRealmChipWidget : public UCantripGlassWidget
{
	GENERATED_BODY()

public:
	/**
	 * Fill the chip in. Called by the detail pane as it builds the row.
	 *
	 * bInPrimary separates the two kinds sharing the row: a primary Realm puts dice in
	 * the pool, a modifier only extends what the working reaches. They sit together
	 * because they are one decision, but they are not the same promise, and the chip
	 * has to say which it is or the row lies about what pressing it will do.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Spellbook")
	void SetRealm(ECantripRealm InRealm, bool bInChosen, bool bInLearned, bool bInPrimary);

	UFUNCTION(BlueprintPure, Category = "Cantrip|Spellbook")
	ECantripRealm GetRealm() const { return Realm; }

	UFUNCTION(BlueprintPure, Category = "Cantrip|Spellbook")
	bool IsChosen() const { return bChosen; }

	/** Fired on press. The detail pane listens and asks the component to toggle. */
	UPROPERTY(BlueprintAssignable, Category = "Cantrip|Spellbook")
	FCantripRealmChipClicked OnChipClicked;

protected:
	virtual void OnApplyGlass() override;
	virtual void NativeConstruct() override;

	UFUNCTION() void HandleClicked();

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UBorder> Bdr_Chip;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Chip;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Chip;

	/** The Realm's roundel, tinted with the chip's state. Optional -- text alone works. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UImage> Img_Realm;

	/**
	 * Roundel size in the chip, and the reason it is not smaller.
	 *
	 * A rank dot is 7.9% of the roundel's width, so the roundel sets the dot: at 28 the
	 * dots are mush, at 36 they are notches you cannot count, at 44 they resolve, and
	 * at 52 they are unambiguous. 48 is the smallest that reads with a pixel in hand
	 * for DPI scaling -- below it the mark still identifies the Realm but stops
	 * carrying her rating, which is half of what the chip is for.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Style")
	float SymbolSize = 48.0f;

private:
	ECantripRealm Realm = ECantripRealm::None;
	bool bChosen = false;
	bool bLearned = false;
	bool bPrimary = true;
};

/**
 * WBP_ArtTab -- one Art roundel in the rail, ringed in came.
 */
UCLASS(Abstract)
class CHANGELING_API UCantripArtTabWidget : public UCantripGlassWidget
{
	GENERATED_BODY()

public:
	/**
	 * The Art she is reading gets a gold ring: the one pane with light behind it.
	 *
	 * Replaces the Blueprint's RefreshTabVisuals, which had to be told the colour.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Spellbook")
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintPure, Category = "Cantrip|Spellbook")
	bool IsSelected() const { return bSelected; }

	/** Roundel size. The book sets this on its tiles; overriding it here is for previews. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Style")
	float TileSize = 112.0f;

	/** How much came shows around the glass, per side. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Style")
	float RingInset = 8.0f;

protected:
	virtual void OnApplyGlass() override;

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<USizeBox> Box_Tab;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Tab;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UImage> Img_Symbol;

private:
	bool bSelected = false;
};

/**
 * WBP_CantripDetail -- the right-hand pane.
 */
UCLASS(Abstract)
class CHANGELING_API UCantripDetailWidget : public UCantripGlassWidget
{
	GENERATED_BODY()

public:
	/**
	 * Empty the pane.
	 *
	 * Switching Arts used to leave the previous Art's cantrip sitting there, which
	 * reads as though the click failed. Clearing is a decision the pane can make for
	 * itself, so it does.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Spellbook")
	void Clear();

	/** Width the Realms line holds even when it has nothing to say. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Style")
	float RealmsMinWidth = 160.0f;

	/** The chip Blueprint. Leave unset and the Realms row simply stays empty. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Spellbook")
	TSubclassOf<UCantripRealmChipWidget> RealmChipClass;

	/** Gap between chips. Applied in code so the row spaces itself however it is built. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Style")
	float ChipSpacing = 8.0f;

	/**
	 * Rebuild the Realm chips for this cantrip. Call from Refresh, after the text.
	 *
	 * Reads the current aim from the hotbar component and marks the chosen ones, so a
	 * page reopened later comes back arranged the way she left it. Clicking a chip is
	 * routed straight back into ToggleRealm and the row is rebuilt -- the component
	 * owns the decision, and this pane only ever draws it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Spellbook")
	void BuildRealmChips(const FCantripBookEntry& Entry);

private:
	UFUNCTION() void HandleRealmChipClicked(ECantripRealm Realm);

	/** The cantrip the current chips belong to, so a click knows what it is toggling. */
	FName ChipRow;

public:

protected:
	virtual void OnApplyGlass() override;

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UBorder> Bdr_Detail;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<USizeBox> Box_Symbol;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UImage> Img_Symbol;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_ArtLine;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Name;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Full;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Ease;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_RealmsLabel;

	/**
	 * The Realms themselves, which the pane had a heading for and never printed.
	 *
	 * The most decisive line on the page and the one that was missing: her LOWEST
	 * primary Realm among these sets the pool, so a strong Art reached through a Realm
	 * she barely knows is a weak casting -- and without the list there is no way for
	 * the player to see why a cantrip she "has" keeps failing.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Realms;

	/** The row the chips are built into. A HorizontalBox or WrapBox both work. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> Box_Realms;

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Cost;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_HoldLabel;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Tiers;

	/** Eyebrow over the systems passage. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_SystemLabel;

	/**
	 * How the working actually resolves -- FCantripSlotDisplay::SystemText.
	 *
	 * Already loaded from the Lore Table for every cantrip and displayed nowhere. Set
	 * beneath the prose because that is the order the book reads in: what it IS, then
	 * what it DOES.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_System;
};
