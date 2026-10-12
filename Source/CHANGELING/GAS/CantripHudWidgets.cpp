// CantripHudWidgets.cpp

#include "CantripHudWidgets.h"

#include "CantripHotbarComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/Texture2D.h"

namespace
{
	/** A rounded-box brush with a coloured outline. Tint is the FILL, not the edge. */
	FSlateBrush Pane(const FLinearColor& Outline, float Width, float Radius,
		const FLinearColor& Tint = FLinearColor::White)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Tint);
		Brush.OutlineSettings.Color = FSlateColor(Outline);
		Brush.OutlineSettings.Width = Width;
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		return Brush;
	}

	FLinearColor Faded(const FLinearColor& Colour, float Alpha)
	{
		FLinearColor Out = Colour;
		Out.A = Alpha;
		return Out;
	}
}

// ---------------------------------------------------------------------------
// UCantripHotbarWidget
// ---------------------------------------------------------------------------

void UCantripHotbarWidget::OnApplyGlass()
{
	if (Box_Hotbar)
	{
		Box_Hotbar->SetWidthOverride(BarWidth);
		Box_Hotbar->SetHeightOverride(BarHeight);
	}

	if (Bdr_Hotbar)
	{
		// Fainter than the book's panes on purpose. This sits over the game the whole
		// time she is playing; the spellbook only has to look good while it is open.
		// The row needs just enough ground to read as one object.
		Bdr_Hotbar->SetBrush(Pane(Faded(Glass.Came, BackingOpacity), Glass.CameWidth,
			Glass.PaneRadius, FLinearColor::White));
		Bdr_Hotbar->SetBrushColor(Faded(Glass.Page, BackingOpacity));
		Bdr_Hotbar->SetContentColorAndOpacity(FLinearColor::White);
	}
}

// ---------------------------------------------------------------------------
// UCantripSlotWidget
// ---------------------------------------------------------------------------

UCantripSlotWidget::UCantripSlotWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set here rather than inline: EKeys' statics are built during engine startup, and
	// a default that depends on them is clearer where the order is obviously safe.
	DragButton = EKeys::MiddleMouseButton;
}

void UCantripSlotWidget::SetFilled(bool bInFilled)
{
	if (bFilled != bInFilled)
	{
		bFilled = bInFilled;
		ApplyGlass();
	}
}

void UCantripSlotWidget::SetHeld(bool bInHeld)
{
	if (bHeld != bInHeld)
	{
		bHeld = bInHeld;
		ApplyGlass();
	}
}

void UCantripSlotWidget::SetDropTarget(bool bInDropTarget)
{
	if (bDropTarget != bInDropTarget)
	{
		bDropTarget = bInDropTarget;
		ApplyGlass();
	}
}

void UCantripSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Bound here rather than left to the Blueprint, because the pane is painted in C++
	// and a refusal is only a fourth colour for the same border -- there is nothing for
	// a Blueprint to add, and one binding per slot is one more thing to forget on nine
	// of the ten.
	if (UCantripHotbarComponent* Hotbar = GetPlayerHotbar())
	{
		Hotbar->OnSlotRefused.RemoveDynamic(this, &UCantripSlotWidget::HandleSlotRefused);
		Hotbar->OnSlotRefused.AddDynamic(this, &UCantripSlotWidget::HandleSlotRefused);
	}

	if (Btn_Slot)
	{
		// The pane answers the mouse the same way it answers its key. Both ends bound,
		// because a press with no release leaves the ability believing the key is still
		// down: it never rolls, and runs to MaxHoldSeconds before resolving itself.
		Btn_Slot->OnPressed.RemoveDynamic(this, &UCantripSlotWidget::HandleButtonPressed);
		Btn_Slot->OnPressed.AddDynamic(this, &UCantripSlotWidget::HandleButtonPressed);

		Btn_Slot->OnReleased.RemoveDynamic(this, &UCantripSlotWidget::HandleButtonReleased);
		Btn_Slot->OnReleased.AddDynamic(this, &UCantripSlotWidget::HandleButtonReleased);
	}
}

void UCantripSlotWidget::HandleButtonPressed()
{
	if (UCantripHotbarComponent* Hotbar = GetPlayerHotbar())
	{
		Hotbar->PressSlot(SlotIndex);
	}
}

void UCantripSlotWidget::HandleButtonReleased()
{
	if (UCantripHotbarComponent* Hotbar = GetPlayerHotbar())
	{
		Hotbar->ReleaseSlot(SlotIndex);
	}
}

void UCantripSlotWidget::HandleSlotRefused(int32 RefusedIndex)
{
	if (RefusedIndex == SlotIndex)
	{
		FlashRefused();
	}
}

void UCantripSlotWidget::FlashRefused()
{
	bRefused = true;
	ApplyGlass();

	UWorld* World = GetWorld();

	if (!World)
	{
		bRefused = false;
		return;
	}

	// Retriggerable: a second refusal during the flash restarts it rather than leaving
	// the first timer to clear a pane that has just been refused again. Spamming a dead
	// key should read as a steady complaint, not one blink and then nothing.
	World->GetTimerManager().ClearTimer(RefusedTimer);

	World->GetTimerManager().SetTimer(RefusedTimer, FTimerDelegate::CreateWeakLambda(this,
		[this]()
		{
			bRefused = false;
			ApplyGlass();
		}),
		RefusedFlashSeconds, false);
}

void UCantripSlotWidget::OnApplyGlass()
{
	if (Bdr_Slot)
	{
		// Gold only while she is casting through this pane -- the same rule as the
		// selected Art roundel. An empty slot is an opening with no glass in it: came
		// and nothing else, so the row reads as ten sockets rather than ten buttons.
		//
		// A pane being dragged onto borrows the same lit treatment. It is the same
		// question either way -- "this one" -- and inventing a second highlight colour
		// would mean the player learning two things where one would do.
		const bool bLit = bHeld || bDropTarget;

		// Refusal outranks everything, because it is the answer to the thing the player
		// did most recently and it is gone again in a tenth of a second. Fire rather than
		// a fourth invented colour: the palette already has a red and it already means
		// heat and refusal, and the flash is far too brief to dilute what Fire says in
		// the roundels.
		//
		// Dimmer on an empty pane. "Nothing there" and "not now" are different answers
		// and the player can tell them apart without being told which is which.
		const FLinearColor Refusal = Faded(Glass.Fire, bFilled ? 1.0f : 0.45f);

		const FLinearColor Edge  = bRefused ? Refusal
		                         : bLit     ? Glass.Gold
		                                    : Glass.Came;

		const float        Width = (bLit || bRefused) ? 2.0f : Glass.CameWidth;

		const FLinearColor Fill  = bRefused    ? Faded(Glass.Fire, bFilled ? 0.16f : 0.06f)
		                         : bDropTarget ? Faded(Glass.Gold, 0.18f)
		                         : bFilled     ? Faded(Glass.Gold, 0.10f)
		                                       : FLinearColor::Transparent;

		Bdr_Slot->SetBrush(Pane(Edge, Width, Glass.PaneRadius, FLinearColor::White));
		Bdr_Slot->SetBrushColor(Fill);
		Bdr_Slot->SetContentColorAndOpacity(FLinearColor::White);
	}

	if (Btn_Slot)
	{
		// The border is the pane; the button is only the touch surface. Normal draws
		// nothing at all, or it fights the border it sits inside. Replaces the
		// template's black-quarter Normal and, more urgently, its YELLOW hover.
		FButtonStyle Style = Btn_Slot->GetStyle();

		Style.NormalPadding = FMargin(0.0f);
		Style.PressedPadding = FMargin(0.0f);

		Style.SetNormal  (Pane(FLinearColor::Transparent, 0.0f, Glass.PaneRadius,
			FLinearColor::Transparent));
		Style.SetHovered (Pane(FLinearColor::Transparent, 0.0f, Glass.PaneRadius,
			Faded(Glass.Parchment, 0.07f)));
		Style.SetPressed (Pane(FLinearColor::Transparent, 0.0f, Glass.PaneRadius,
			Faded(Glass.Parchment, 0.12f)));
		Style.SetDisabled(Pane(FLinearColor::Transparent, 0.0f, Glass.PaneRadius,
			FLinearColor::Transparent));

		Style.SetNormalForeground  (FSlateColor(Glass.Parchment));
		Style.SetHoveredForeground (FSlateColor(Glass.Parchment));
		Style.SetPressedForeground (FSlateColor(Glass.Parchment));
		Style.SetDisabledForeground(FSlateColor(Glass.Muted));

		Btn_Slot->SetStyle(Style);
	}

	if (Box_Icon)
	{
		Box_Icon->SetWidthOverride(IconSize);
		Box_Icon->SetHeightOverride(IconSize);
	}

	if (Img_Icon)
	{
		// White so the cantrip's own artwork comes through; an empty slot simply has
		// no texture, which is quieter than a dimmed placeholder.
		Img_Icon->SetColorAndOpacity(FLinearColor::White);

		if (!bFilled)
		{
			// Emptying has to be done HERE, because the obvious place cannot do it.
			// SetBrushFromSoftTexture on a null pointer never reaches SetBrushFromTexture
			// at all: UImage::RequestAsyncLoad only calls through immediately when the
			// soft pointer already resolves, and a null one falls into the streaming
			// path with an empty object path, where the callback never lands. So a slot
			// told to draw "nothing" silently keeps the last thing it drew -- and after
			// a swap, the cantrip appears to have been COPIED into the new key rather
			// than moved out of the old one.
			//
			// The hard overload does clear, and this class is the right one to call it:
			// bFilled is the same fact that decides every other part of the pane.
			Img_Icon->SetBrushFromTexture(nullptr);
		}
	}

	// Keeps its own face: at 10pt this is a numeral to be found, not read, and the
	// serif has no business here. Gold once the slot is bound, so the key and the
	// chip in the spellbook agree about what "laid out" looks like.
	PaintLabel(Txt_Key, bFilled ? Glass.Gold : Glass.Muted, 10.0f, 120);
}

// ---------------------------------------------------------------------------
// Rearranging
// ---------------------------------------------------------------------------

FReply UCantripSlotWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	// Preview, so this runs on the way DOWN the tree and beats the button to the
	// event. On the drag button that is the point; on any other the reply is Unhandled
	// and the press carries on to Btn_Slot exactly as before, which is what keeps
	// hold-to-cast working.
	if (bFilled && InMouseEvent.GetEffectingButton() == DragButton)
	{
		return FReply::Handled().DetectDrag(TakeWidget(), DragButton);
	}

	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

void UCantripSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	UCantripHotbarComponent* Hotbar = GetPlayerHotbar();

	if (!Hotbar)
	{
		return;
	}

	const FCantripSlotDisplay Display = Hotbar->GetSlot(SlotIndex);

	if (!Display.bFilled)
	{
		return;
	}

	UCantripDragDropOperation* Op = NewObject<UCantripDragDropOperation>(this);

	Op->SourceSlotIndex  = SlotIndex;
	Op->AbilityClass     = Display.AbilityClass;
	Op->Pivot            = EDragPivot::CenterCenter;
	Op->DefaultDragVisual = MakeDragVisual(Display);

	OutOperation = Op;
}

bool UCantripSlotWidget::NativeOnDrop(const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	SetDropTarget(false);

	UCantripDragDropOperation* Op = Cast<UCantripDragDropOperation>(InOperation);
	UCantripHotbarComponent* Hotbar = GetPlayerHotbar();

	if (!Op || !Hotbar || Op->SourceSlotIndex == SlotIndex)
	{
		return false;
	}

	// Two different operations wearing the same gesture. Off the bar it is a MOVE, and
	// SwapSlots is the only version that does not silently destroy whatever was already
	// here; out of the spellbook there is no source slot to give anything back to, so
	// it is an assignment and the displaced cantrip simply returns to the book.
	if (Op->SourceSlotIndex >= 0)
	{
		return Hotbar->SwapSlots(Op->SourceSlotIndex, SlotIndex);
	}

	return Hotbar->AssignSlot(SlotIndex, Op->AbilityClass);
}

void UCantripSlotWidget::NativeOnDragEnter(const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);

	if (const UCantripDragDropOperation* Op = Cast<UCantripDragDropOperation>(InOperation))
	{
		// Not the pane it came from: lighting the slot a cantrip is already sitting in
		// promises a move that would do nothing.
		SetDropTarget(Op->SourceSlotIndex != SlotIndex);
	}
}

void UCantripSlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	SetDropTarget(false);
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
}

void UCantripSlotWidget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	// Dropped on nothing. The bar keeps what it had -- a cantrip released over the
	// garden should not fall off the hotbar, because nobody has ever meant that.
	SetDropTarget(false);
	Super::NativeOnDragCancelled(InDragDropEvent, InOperation);
}

UImage* UCantripSlotWidget::MakeDragVisual(const FCantripSlotDisplay& Display) const
{
	UImage* Ghost = NewObject<UImage>(const_cast<UCantripSlotWidget*>(this));

	// The level emblem, matching what the pane itself shows -- what you pick up should
	// look like what you put down. Synchronous, and deliberately so: the ghost has to
	// exist this frame or the drag starts invisible. The slot it came from already has
	// the texture resident, so this resolves a pointer rather than reading from disk.
	//
	// Falls back to the cantrip's own icon rather than to nothing, because LevelSymbol
	// is a COMPOSED path: it is always non-null and only fails at load, so an Art whose
	// roundels have not been generated yet would otherwise drag an empty square.
	UTexture2D* Face = Display.LevelSymbol.LoadSynchronous();

	if (!Face)
	{
		Face = Display.Icon.LoadSynchronous();
	}

	if (Face)
	{
		Ghost->SetBrushFromTexture(Face);
	}

	Ghost->SetDesiredSizeOverride(FVector2D(IconSize, IconSize));

	// Translucent, because the ghost is a statement about where the cursor is, not a
	// second copy of the cantrip. Anything more solid reads as having already landed.
	Ghost->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.75f));

	return Ghost;
}
