// CantripBookWidgets.cpp

#include "CantripBookWidgets.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ListView.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/TileView.h"
#include "Abilities/GameplayAbility.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "CantripBookList.h"
#include "CantripHotbarComponent.h"
#include "CantripResolver.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** A rounded-box brush with no fill and a coloured outline: a came ring. */
	FSlateBrush Ring(const FLinearColor& Outline, float Width, float Radius,
		const FLinearColor& Fill = FLinearColor::Transparent)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings.Color = FSlateColor(Outline);
		Brush.OutlineSettings.Width = Width;
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		return Brush;
	}

	/** The same, but circular -- for the Art roundels. */
	FSlateBrush Roundel(const FLinearColor& Outline, float Width,
		const FLinearColor& Fill = FLinearColor::Transparent)
	{
		FSlateBrush Brush = Ring(Outline, Width, 0.0f, Fill);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		return Brush;
	}

	/** The same colour, dimmed. Used for hover states, which should whisper. */
	FLinearColor Faint(const FLinearColor& Colour, float Alpha)
	{
		FLinearColor Out = Colour;
		Out.A = Alpha;
		return Out;
	}
}

// ---------------------------------------------------------------------------
// UCantripGlassWidget
// ---------------------------------------------------------------------------

UCantripGlassWidget::UCantripGlassWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Found by path so the book is set in Cormorant out of the box rather than in
	// whatever the engine defaults to, which reads like a settings menu. Still
	// EditDefaultsOnly, so a Blueprint can swap the face without a rebuild.
	static ConstructorHelpers::FObjectFinder<UFont> DisplayFace(
		TEXT("/Game/_Custom/UI/Fonts/static/CormorantGaramond-Medium_Font.CormorantGaramond-Medium_Font"));
	static ConstructorHelpers::FObjectFinder<UFont> BodyFace(
		TEXT("/Game/_Custom/UI/Fonts/static/CormorantGaramond-Regular_Font.CormorantGaramond-Regular_Font"));
	static ConstructorHelpers::FObjectFinder<UFont> AsideFace(
		TEXT("/Game/_Custom/UI/Fonts/static/CormorantGaramond-Italic_Font.CormorantGaramond-Italic_Font"));

	if (DisplayFace.Succeeded()) { DisplayFont = DisplayFace.Object; }
	if (BodyFace.Succeeded())    { BodyFont    = BodyFace.Object; }
	if (AsideFace.Succeeded())   { AsideFont   = AsideFace.Object; }
}

void UCantripGlassWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyGlass();
}

void UCantripGlassWidget::ApplyGlass()
{
	OnApplyGlass();
}

UCantripHotbarComponent* UCantripGlassWidget::GetPlayerHotbar() const
{
	const APlayerController* PC = GetOwningPlayer();
	return PC ? PC->FindComponentByClass<UCantripHotbarComponent>() : nullptr;
}

void UCantripGlassWidget::PaintPane(UBorder* Pane, const FLinearColor& Fill) const
{
	if (!Pane)
	{
		return;
	}

	// Built fresh rather than edited in place: a brush carried over from the Designer
	// may have any DrawAs, and a pane that silently keeps DrawAs::None is exactly the
	// bug that had the book rendering over open sky for an afternoon.
	//
	// The brush's own tint must be WHITE, not the fill. SBorder paints the box with
	// BrushColor * Brush.TintColor, so a transparent tint multiplies the ground away
	// however opaque BrushColor is -- which is the same afternoon, by another route.
	Pane->SetBrush(Ring(Glass.Came, Glass.CameWidth, Glass.PaneRadius, FLinearColor::White));
	Pane->SetBrushColor(Fill);

	// White, so the pane tints its backing and not its contents. These two fields are
	// one letter apart in the Details panel and do entirely different things.
	Pane->SetContentColorAndOpacity(FLinearColor::White);
}

void UCantripGlassWidget::PaintText(UTextBlock* Text, const FLinearColor& Colour, float Size,
	UFont* Face)
{
	if (!Text)
	{
		return;
	}

	Text->SetColorAndOpacity(FSlateColor(Colour));

	FSlateFontInfo Font = Text->GetFont();
	Font.Size = Size;
	Font.LetterSpacing = 0;

	if (Face)
	{
		Font.FontObject = Face;
		// The imported faces each carry a single typeface under this name; leaving
		// the Designer's old typeface name behind resolves to nothing and the text
		// silently disappears.
		Font.TypefaceFontName = TEXT("Default");
	}

	Text->SetFont(Font);
}

void UCantripGlassWidget::PaintLabel(UTextBlock* Text, const FLinearColor& Colour,
	float Size, int32 Tracking)
{
	if (!Text)
	{
		return;
	}

	Text->SetColorAndOpacity(FSlateColor(Colour));

	FSlateFontInfo Font = Text->GetFont();
	Font.Size = Size;
	Font.LetterSpacing = Tracking;
	Text->SetFont(Font);
}

// ---------------------------------------------------------------------------
// UCantripBookWidget
// ---------------------------------------------------------------------------

void UCantripBookWidget::OnApplyGlass()
{
	// Three leaded panes, each a shade lighter than the one behind it.
	PaintPane(Bdr_Box,  Glass.Page);
	PaintPane(Bdr_Rail, Glass.Rail);
	PaintPane(Bdr_List, Glass.Page);

	PaintLabel(Txt_ArtsLabel, Glass.Muted, 9.0f, 240);
	PaintLabel(Txt_Eyebrow,   Glass.Muted, 9.0f, 300);

	// Sizes are a couple of points up on where they sat in the sans. Cormorant is a
	// light, high-contrast face with a small x-height -- set at the old numbers it
	// reads a size smaller than it measures.
	PaintText(Txt_ArtTitle,    Glass.Parchment, 38.0f, DisplayFont);
	PaintText(Txt_ArtProgress, Glass.Gold,      13.0f, BodyFont);

	if (Box_Rail)
	{
		Box_Rail->SetWidthOverride(RailWidth);
	}

	// Cleared rather than set to zero when unset, so leaving the width alone really does
	// hand the column back to the layout instead of collapsing it to nothing.
	auto PinWidth = [](USizeBox* Box, float Width)
	{
		if (!Box)
		{
			return;
		}

		if (Width > 0.0f)
		{
			Box->SetWidthOverride(Width);
		}
		else
		{
			Box->ClearWidthOverride();
		}
	};

	PinWidth(Box_List,   ListWidth);
	PinWidth(Box_Detail, DetailWidth);

	if (Tile_Arts)
	{
		// Told, not derived. The TileView already knows how to divide a width by a tile
		// size; computing the tile size FROM the width only guaranteed the answer was
		// always two columns, however wide the rail became.
		Tile_Arts->SetEntryWidth(ArtTileSize);
		Tile_Arts->SetEntryHeight(ArtTileSize);
	}

	if (Box_Close)
	{
		Box_Close->SetWidthOverride(CloseButtonSize);
		Box_Close->SetHeightOverride(CloseButtonSize);
	}

	if (Btn_Close)
	{
		// A small leaded pane of its own, so it belongs to the window rather than
		// sitting on top of it. Square-ish corners: the roundels are the only circles
		// in the book and that is what makes them read as roundels.
		FButtonStyle Style = Btn_Close->GetStyle();

		Style.NormalPadding = FMargin(0.0f);
		Style.PressedPadding = FMargin(0.0f);

		Style.SetNormal  (Ring(Glass.Came, Glass.CameWidth, 3.0f));
		Style.SetHovered (Ring(Glass.Pewter, Glass.CameWidth, 3.0f,
			Faint(Glass.Parchment, 0.06f)));
		Style.SetPressed (Ring(Glass.Parchment, Glass.CameWidth, 3.0f,
			Faint(Glass.Parchment, 0.10f)));
		Style.SetDisabled(Ring(Glass.Came, Glass.CameWidth, 3.0f));

		Style.SetNormalForeground  (FSlateColor(Glass.Muted));
		Style.SetHoveredForeground (FSlateColor(Glass.Parchment));
		Style.SetPressedForeground (FSlateColor(Glass.Parchment));
		Style.SetDisabledForeground(FSlateColor(Glass.Muted));

		Btn_Close->SetStyle(Style);
	}

	PaintText(Txt_Close, Glass.Muted, 13.0f);
}

void UCantripBookWidget::CloseBook()
{
	SetVisibility(ESlateVisibility::Collapsed);

	if (APlayerController* PC = GetOwningPlayer())
	{
		UWidgetBlueprintLibrary::SetInputMode_GameOnly(PC);
		PC->SetShowMouseCursor(false);
	}
}

// ---------------------------------------------------------------------------
// UCantripEntryWidget
// ---------------------------------------------------------------------------

UCantripEntryWidget::UCantripEntryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DragButton = EKeys::MiddleMouseButton;
}

UCantripEntryObject* UCantripEntryWidget::GetEntryItem() const
{
	// Deliberately not the templated GetListItem<>: that needs the interface natively
	// on this class, and the Blueprint already implements it. The library resolves the
	// item through the owning ListView instead, which works either way and costs one
	// wrapper to say so.
	TScriptInterface<IUserObjectListEntry> Self;
	Self.SetObject(const_cast<UCantripEntryWidget*>(this));

	return Cast<UCantripEntryObject>(
		UUserObjectListEntryLibrary::GetListItemObject(Self));
}

FReply UCantripEntryWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == DragButton)
	{
		return FReply::Handled().DetectDrag(TakeWidget(), DragButton);
	}

	// Anything else falls through untouched, which is what leaves Btn_Row's On Pressed
	// -- the click that changes the detail pane -- working exactly as it did.
	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

void UCantripEntryWidget::NativeOnDragDetected(const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	const UCantripEntryObject* Item = GetEntryItem();

	// Only what she has actually learned. The book lists the rungs above her on
	// purpose, and letting one of those be dragged to a key would put a cantrip on the
	// bar that AssignSlot is right to refuse -- a drag that visibly succeeds and then
	// silently does nothing is worse than one that never starts.
	if (!Item || !Item->Entry.bKnown || !Item->Entry.Display.AbilityClass)
	{
		return;
	}

	UCantripDragDropOperation* Op = NewObject<UCantripDragDropOperation>(this);

	Op->SourceSlotIndex = INDEX_NONE;
	Op->AbilityClass    = Item->Entry.Display.AbilityClass;
	Op->Pivot           = EDragPivot::CenterCenter;

	UImage* Ghost = NewObject<UImage>(this);

	// The same emblem the row is already showing, and the same one it will wear once it
	// lands on the bar. The book resolved it hard when it built the entry, so this costs
	// nothing; Display.Icon is the fallback for an Art whose roundels are not drawn yet.
	UTexture2D* Face = Item->Entry.LevelSymbolTexture;

	if (!Face)
	{
		Face = Item->Entry.Display.Icon.LoadSynchronous();
	}

	if (Face)
	{
		Ghost->SetBrushFromTexture(Face);
	}

	Ghost->SetDesiredSizeOverride(FVector2D(56.0f, 56.0f));
	Ghost->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.75f));

	Op->DefaultDragVisual = Ghost;

	OutOperation = Op;
}

void UCantripEntryWidget::OnApplyGlass()
{
	if (Btn_Row)
	{
		// The button is structure, not surface: Bdr_Row paints the row and the
		// palette decides its colour, so Normal draws nothing at all. Only the
		// states the palette has no opinion about are drawn here.
		FButtonStyle Style = Btn_Row->GetStyle();

		Style.NormalPadding = FMargin(0.0f);
		Style.PressedPadding = FMargin(0.0f);

		Style.SetNormal  (Ring(FLinearColor::Transparent, 0.0f, Glass.PaneRadius));
		Style.SetHovered (Ring(FLinearColor::Transparent, 0.0f, Glass.PaneRadius,
			Faint(Glass.Parchment, 0.05f)));
		Style.SetPressed (Ring(FLinearColor::Transparent, 0.0f, Glass.PaneRadius,
			Faint(Glass.Parchment, 0.09f)));
		Style.SetDisabled(Ring(FLinearColor::Transparent, 0.0f, Glass.PaneRadius));

		// Left at the template's cyan, anything that inherits foreground comes out
		// the colour of a debug draw.
		Style.SetNormalForeground  (FSlateColor(Glass.Parchment));
		Style.SetHoveredForeground (FSlateColor(Glass.Parchment));
		Style.SetPressedForeground (FSlateColor(Glass.Parchment));
		Style.SetDisabledForeground(FSlateColor(Glass.Muted));

		Btn_Row->SetStyle(Style);
	}

	if (Bdr_Row)
	{
		// Fill deliberately left alone -- Refresh sets it from the palette every time
		// the row is filled, and whatever it last set is the truth.
		FSlateBrush Brush = Ring(Glass.Came, Glass.CameWidth, Glass.PaneRadius);
		Brush.TintColor = FSlateColor(FLinearColor::White);
		Bdr_Row->SetBrush(Brush);
		Bdr_Row->SetContentColorAndOpacity(FLinearColor::White);
	}

	if (Bdr_Chip)
	{
		// White tint for the same reason as the panes: Refresh drives BrushColor from
		// the palette's ChipTint, and that is a multiply.
		Bdr_Chip->SetBrush(Ring(Faint(Glass.Parchment, 0.06f), 1.0f, 3.0f, FLinearColor::White));
		Bdr_Chip->SetContentColorAndOpacity(FLinearColor::White);
	}

	// Sizes and tracking only. Every colour on these three is a standing, and
	// standings belong to FCantripPalette.
	PaintText (Txt_Name, Glass.Parchment, 22.0f, DisplayFont);
	PaintText (Txt_Desc, Glass.Muted,     13.0f, BodyFont);
	PaintLabel(Txt_Chip, Glass.Pewter,     9.0f, 131);
}

// ---------------------------------------------------------------------------
// UCantripArtTabWidget
// ---------------------------------------------------------------------------

void UCantripArtTabWidget::SetSelected(bool bInSelected)
{
	if (bSelected != bInSelected)
	{
		bSelected = bInSelected;
		ApplyGlass();
	}
}

void UCantripArtTabWidget::OnApplyGlass()
{
	if (Box_Tab)
	{
		if (IsDesignTime())
		{
			// So the tab has a sensible size to be edited at on its own canvas.
			Box_Tab->SetWidthOverride(TileSize);
			Box_Tab->SetHeightOverride(TileSize);
		}
		else
		{
			// In the rail the TileView allots the cell, and a SizeBox that insists on
			// its own number only fights it -- change the rail width and every tile
			// would keep the old size and overflow its cell.
			Box_Tab->ClearWidthOverride();
			Box_Tab->ClearHeightOverride();
		}
	}

	if (Img_Symbol)
	{
		// White: the roundel is already stained glass and does not want help.
		Img_Symbol->SetColorAndOpacity(FLinearColor::White);

		// Inset by padding rather than by brush size -- the button's content slot
		// fills by default, so a brush size would simply be stretched over. This is
		// the gap the ring sits in.
		if (UButtonSlot* SymbolSlot = Cast<UButtonSlot>(Img_Symbol->Slot))
		{
			SymbolSlot->SetPadding(FMargin(RingInset));
		}
	}

	if (Btn_Tab)
	{
		FButtonStyle Style = Btn_Tab->GetStyle();

		// Zero, or the ring is drawn off-centre from the glass it surrounds.
		Style.NormalPadding = FMargin(0.0f);
		Style.PressedPadding = FMargin(0.0f);

		// Selected is the only Art with light behind it, so it is the only gold ring
		// on the rail. Everything else is lead.
		const FLinearColor Resting = bSelected ? Glass.Gold : Glass.Came;
		const float Width = bSelected ? 3.0f : 2.0f;

		Style.SetNormal  (Roundel(Resting, Width));
		Style.SetHovered (Roundel(bSelected ? Glass.Gold : Glass.Pewter, Width,
			Faint(Glass.Parchment, 0.04f)));
		Style.SetPressed (Roundel(Glass.Parchment, Width,
			Faint(Glass.Parchment, 0.08f)));
		Style.SetDisabled(Roundel(Glass.Came, Width));

		Style.SetNormalForeground  (FSlateColor(Glass.Parchment));
		Style.SetHoveredForeground (FSlateColor(Glass.Parchment));
		Style.SetPressedForeground (FSlateColor(Glass.Parchment));
		Style.SetDisabledForeground(FSlateColor(Glass.Muted));

		Btn_Tab->SetStyle(Style);
	}
}

// ---------------------------------------------------------------------------
// UCantripDetailWidget
// ---------------------------------------------------------------------------

void UCantripDetailWidget::OnApplyGlass()
{
	PaintPane(Bdr_Detail, Glass.Detail);

	if (Box_Symbol)
	{
		Box_Symbol->SetWidthOverride(220.0f);
		Box_Symbol->SetHeightOverride(220.0f);
	}

	if (Img_Symbol)
	{
		Img_Symbol->SetColorAndOpacity(FLinearColor::White);
	}

	// The cantrip's address -- which Art, which rung -- is identity, not light, and
	// it is scanned rather than read, so it keeps the label face.
	PaintLabel(Txt_ArtLine,     Glass.Pewter,    11.0f, 160);
	PaintText (Txt_Name,        Glass.Parchment, 30.0f, DisplayFont);
	PaintText (Txt_Full,        Glass.Parchment, 17.0f, BodyFont);

	// Gold and italic: reachability is the one line on the pane that answers "can she
	// actually do this". An aside, not a field -- so it is set as one.
	PaintText (Txt_Ease,        Glass.Gold,      15.0f, AsideFont);

	PaintLabel(Txt_RealmsLabel, Glass.Muted,      9.0f, 300);
	PaintLabel(Txt_HoldLabel,   Glass.Muted,      9.0f, 300);
	PaintLabel(Txt_SystemLabel, Glass.Muted,      9.0f, 300);

	PaintText (Txt_Cost,        Glass.Pewter,    13.0f, BodyFont);
	PaintText (Txt_Tiers,       Glass.Pewter,    13.0f, BodyFont);

	// Parchment rather than pewter: the Realms are not a footnote, they are the reason
	// a casting succeeds or fails. Set at the same weight as the cost and hold beside
	// them, but bright enough to be read rather than merely noticed.
	PaintText (Txt_Realms,      Glass.Parchment, 13.0f, BodyFont);

	if (Txt_Realms)
	{
		// A line that reserves its height even while empty. Without this an empty
		// Realms string collapses the block to nothing, its parent shrinks with it,
		// and a widget that is merely SILENT is indistinguishable from one that was
		// never laid out -- which is exactly the ambiguity that made this hard to find.
		Txt_Realms->SetMinDesiredWidth(RealmsMinWidth);
	}

	// A shade under the prose above it -- the same passage in a quieter voice, because
	// it is consulted rather than read through.
	PaintText (Txt_System,      Glass.Pewter,    15.0f, BodyFont);
}

// ---------------------------------------------------------------------------
// UCantripRealmChipWidget
// ---------------------------------------------------------------------------

void UCantripRealmChipWidget::SetRealm(ECantripRealm InRealm, bool bInChosen,
	bool bInLearned, bool bInPrimary)
{
	Realm    = InRealm;
	bChosen  = bInChosen;
	bLearned = bInLearned;
	bPrimary = bInPrimary;

	if (Txt_Chip)
	{
		const UEnum* RealmEnum = StaticEnum<ECantripRealm>();

		Txt_Chip->SetText(RealmEnum
			? RealmEnum->GetDisplayNameTextByValue(static_cast<int64>(InRealm))
			: FText::GetEmpty());
	}

	if (Img_Realm)
	{
		UTexture2D* Face = nullptr;

		if (const UCantripHotbarComponent* Hotbar = GetPlayerHotbar())
		{
			// The roundel wears her rating in dots, the same way an Art symbol wears
			// the cantrip's level -- so the chip answers "can I use this, and how
			// well" in one mark rather than needing a number beside it.
			const int32 Ranks = Hotbar->GetRealmRating(InRealm);

			// Six small textures on a page open, so synchronous is honest here: the
			// alternative is a frame of white squares every time she turns the page.
			Face = Hotbar->RealmSymbol(InRealm, Ranks).LoadSynchronous();

			if (!Face && Ranks > 0)
			{
				// The dotted variant is missing, so fall back to the undotted roundel:
				// a Realm shown without its rating is worth far more than a white
				// square, and the square is what a failed soft load actually leaves --
				// SetBrushFromSoftTexture never reaches SetBrushFromTexture when the
				// path resolves to nothing, so the Image keeps its default brush and
				// the chip loses its identity rather than merely its dots.
				Face = Hotbar->RealmSymbol(InRealm, 0).LoadSynchronous();
			}
		}

		// Built whole, rather than SetBrushFromTexture plus SetDesiredSizeOverride, and
		// the difference is the whole point of doing it this way. The override writes
		// ONLY to the Slate widget (Image.cpp:122) and SetRealm runs before AddChild --
		// so MyImage is still null, the call is silently dropped, and the chip takes
		// whatever ImageSize the Designer happened to leave on the brush. ImageSize is a
		// UPROPERTY on the UImage, so it holds until Slate is built and is re-applied on
		// every rebuild: the row is the same height on the first page and the thousandth,
		// and stays that height even when the texture is missing.
		FSlateBrush FaceBrush;
		FaceBrush.DrawAs    = ESlateBrushDrawType::Image;
		FaceBrush.ImageSize = FVector2D(SymbolSize, SymbolSize);
		FaceBrush.TintColor = FSlateColor(FLinearColor::White);
		FaceBrush.SetResourceObject(Face);

		Img_Realm->SetBrush(FaceBrush);

		if (Face)
		{
			// Carried over from SetBrushFromTexture, which we are no longer going
			// through: a UI texture left to the streamer arrives blurred and pops in
			// while the page is already being read.
			Face->bForceMiplevelsToBeResident = true;
			Face->bIgnoreStreamingMipBias     = true;
		}
	}

	ApplyGlass();
}

void UCantripRealmChipWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Chip)
	{
		// On Pressed, not On Clicked. SButton only raises OnClicked if the cursor is
		// still over it at mouse-up, which is why the spellbook rows had to move to
		// Pressed as well -- no reason to relearn that here.
		Btn_Chip->OnPressed.RemoveDynamic(this, &UCantripRealmChipWidget::HandleClicked);
		Btn_Chip->OnPressed.AddDynamic(this, &UCantripRealmChipWidget::HandleClicked);
	}

}

void UCantripRealmChipWidget::HandleClicked()
{
	OnChipClicked.Broadcast(Realm);
}

void UCantripRealmChipWidget::OnApplyGlass()
{
	// Two lit colours, because the row holds two kinds of thing. Gold is the book's
	// word for "this is in play and it counts" -- it lights the pool. Amethyst is for
	// the modifiers: also chosen, also part of the casting, but adding reach rather
	// than dice. One glance should say how many of her Realms are actually carrying the
	// working, which a single colour could not.
	const FLinearColor Lit = bPrimary ? Glass.Gold : Glass.Amethyst;

	// Three states, and the third is a refusal rather than a shade. A Realm she has no
	// dots in is not offered at all: greyed, and the button under it switched off, so
	// the row never invites a press it would only reject.
	//
	// 0.70, not the 0.30 this started at. Thirty percent of Muted over the pane came out
	// at 1.5:1 -- not "dimmed" but genuinely unreadable, so a Realm she has yet to learn
	// was a blank space rather than a thing to work toward, which is the opposite of what
	// the row is for. At 0.70 it reads at 3.3:1: plainly quieter than the learned chips
	// beside it, and plainly still a word.
	const FLinearColor Ghost = FLinearColor(Glass.Muted.R, Glass.Muted.G,
		Glass.Muted.B, 0.70f);

	if (Bdr_Chip)
	{
		// Muted came on the ones she CAN reach: present, quiet, clearly a thing to
		// press. The lit ones then have the only strong edges on the row, which is what
		// makes the arrangement readable at a glance.
		const FLinearColor Edge  = bChosen  ? Lit
		                         : bLearned ? Glass.Muted
		                                    : Ghost;

		const float        Width = bChosen ? 2.0f : Glass.CameWidth;
		const FLinearColor Fill  = bChosen ? FLinearColor(Lit.R, Lit.G, Lit.B, 0.14f)
		                                   : FLinearColor::Transparent;

		Bdr_Chip->SetBrush(Ring(Edge, Width, Glass.PaneRadius, FLinearColor::White));
		Bdr_Chip->SetBrushColor(Fill);
		Bdr_Chip->SetContentColorAndOpacity(FLinearColor::White);
	}

	if (Btn_Chip)
	{
		// Switched off rather than merely ignored, so it does not light on hover and
		// promise something that will not happen.
		Btn_Chip->SetIsEnabled(bLearned);
	}

	if (Btn_Chip)
	{
		FButtonStyle Style = Btn_Chip->GetStyle();

		Style.NormalPadding  = FMargin(0.0f);
		Style.PressedPadding = FMargin(0.0f);

		Style.SetNormal  (Ring(FLinearColor::Transparent, 0.0f, Glass.PaneRadius,
			FLinearColor::Transparent));
		Style.SetHovered (Ring(FLinearColor::Transparent, 0.0f, Glass.PaneRadius,
			FLinearColor(Glass.Parchment.R, Glass.Parchment.G, Glass.Parchment.B, 0.08f)));
		Style.SetPressed (Ring(FLinearColor::Transparent, 0.0f, Glass.PaneRadius,
			FLinearColor(Glass.Parchment.R, Glass.Parchment.G, Glass.Parchment.B, 0.14f)));
		Style.SetDisabled(Ring(FLinearColor::Transparent, 0.0f, Glass.PaneRadius,
			FLinearColor::Transparent));

		Btn_Chip->SetStyle(Style);
	}

	// Still shown when she cannot use it, because the row is also a map of what she
	// might one day learn -- a Realm that vanished until bought would make the page
	// change shape as she advances, and hide the shape of the choice she is working
	// toward.
	const FLinearColor Ink = bChosen  ? Lit
	                       : bLearned ? Glass.Parchment
	                                  : Ghost;

	PaintText(Txt_Chip, Ink, 12.0f, BodyFont);

	if (Img_Realm)
	{
		// The roundel takes the same three states as the text. Full white when chosen
		// so the stained glass reads at its own colours; dimmed otherwise, and dimmed
		// hardest when she cannot use it -- the same greying as the label, so the two
		// halves of the chip never disagree about whether it is available.
		//
		// Floors raised with the label's: at 0.25 the unlearned roundel was a smudge and
		// its rank dots could not be counted at all, which is most of what the roundel is
		// there to say.
		const float Alpha = bChosen ? 1.0f : bLearned ? 0.80f : 0.45f;

		Img_Realm->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, Alpha));
	}
}

// ---------------------------------------------------------------------------

void UCantripDetailWidget::BuildRealmChips(const FCantripBookEntry& Entry)
{
	if (!Box_Realms)
	{
		return;
	}

	// An entry with no row is not a cantrip, and rebuilding for one would replace a
	// working aiming row with six chips that answer to nothing -- ChipRow would go None
	// and every later press would be refused by ToggleRealm's first line. The pane gets
	// refreshed with an empty entry more than once per open, so this is not a defensive
	// nicety: without it the FIRST toggle destroys the chips it just repainted, and the
	// row is dead for the rest of the session.
	if (Entry.Display.CantripRow.IsNone())
	{
		return;
	}

	Box_Realms->ClearChildren();

	UCantripHotbarComponent* Hotbar = GetPlayerHotbar();

	if (!Hotbar || !RealmChipClass)
	{
		return;
	}

	const FName Row = Entry.Display.CantripRow;

	// Every cantrip offers every Realm now -- the chips are the same four on every page,
	// and only which are lit changes. That is the point: the player is choosing HOW to
	// aim a working, not discovering what the table will let her do.
	const TArray<ECantripRealm>& Aimable = Hotbar->AimableRealms;
	const TArray<ECantripRealm> Chosen = Hotbar->GetChosenRealms(Row);

	for (const ECantripRealm Realm : Aimable)
	{
		UCantripRealmChipWidget* Chip =
			CreateWidget<UCantripRealmChipWidget>(this, RealmChipClass);

		if (!Chip)
		{
			continue;
		}

		Chip->SetRealm(Realm, Chosen.Contains(Realm),
			Hotbar->GetRealmRating(Realm) > 0,
			UCantripResolver::IsPrimaryRealm(Realm));

		Chip->OnChipClicked.AddDynamic(this, &UCantripDetailWidget::HandleRealmChipClicked);

		// Named ChipSlot, not Slot: UWidget already has a member called Slot, and this
		// widget IS a UWidget -- the plain name compiles as a shadow warning, which the
		// editor target promotes to an error.
		UPanelSlot* ChipSlot = Box_Realms->AddChild(Chip);

		// Spaced here rather than in the chip's own padding, because the gap belongs to
		// the row and the chip should not have to know it is in one. Handled for both
		// panels worth using: a HorizontalBox if the six fit, a WrapBox if they do not.
		const FMargin Gap(0.0f, 0.0f, ChipSpacing, 0.0f);

		if (UHorizontalBoxSlot* AsHorizontal = Cast<UHorizontalBoxSlot>(ChipSlot))
		{
			AsHorizontal->SetPadding(Gap);
		}
		else if (UWrapBoxSlot* AsWrap = Cast<UWrapBoxSlot>(ChipSlot))
		{
			AsWrap->SetPadding(Gap);
		}
	}

	// Remembered so a click can be turned into a toggle without the pane having to be
	// handed the entry again -- the chips outlive the call that built them.
	ChipRow = Row;
}

void UCantripDetailWidget::HandleRealmChipClicked(ECantripRealm Realm)
{
	UCantripHotbarComponent* Hotbar = GetPlayerHotbar();

	if (!Hotbar || ChipRow.IsNone())
	{
		return;
	}

	if (Hotbar->ToggleRealm(ChipRow, Realm))
	{
		// Repainted from the component rather than toggled in place: it may have
		// refused the change (unaiming the last Realm) or reordered the selection, and
		// one source of truth redrawn is simpler to reason about than several chips
		// each nudged toward agreement.
		const TArray<ECantripRealm> Chosen = Hotbar->GetChosenRealms(ChipRow);

		for (UWidget* Child : Box_Realms ? Box_Realms->GetAllChildren()
		                                 : TArray<UWidget*>())
		{
			if (UCantripRealmChipWidget* Chip = Cast<UCantripRealmChipWidget>(Child))
			{
				const ECantripRealm Held = Chip->GetRealm();

				Chip->SetRealm(Held, Chosen.Contains(Held),
					Hotbar->GetRealmRating(Held) > 0,
					UCantripResolver::IsPrimaryRealm(Held));
			}
		}
	}
}

void UCantripDetailWidget::Clear()
{
	const FText Empty = FText::GetEmpty();

	if (Txt_ArtLine) { Txt_ArtLine->SetText(Empty); }
	if (Txt_Name)    { Txt_Name->SetText(Empty); }
	if (Txt_Full)    { Txt_Full->SetText(Empty); }
	if (Txt_Ease)    { Txt_Ease->SetText(Empty); }
	if (Txt_Cost)    { Txt_Cost->SetText(Empty); }
	if (Txt_Tiers)   { Txt_Tiers->SetText(Empty); }
	if (Txt_Realms)  { Txt_Realms->SetText(Empty); }
	if (Txt_System)  { Txt_System->SetText(Empty); }

	if (Img_Symbol)
	{
		Img_Symbol->SetBrushFromTexture(nullptr);
	}
}
