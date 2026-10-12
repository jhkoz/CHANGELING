// CantripHudWidgets.h
//
// The hotbar, in the same glass the spellbook is set in.
//
// Ten small leaded panes in a row. The vocabulary is deliberately the book's: came
// around every pane, gold only where light is actually coming through, and the key
// numerals on the same quiet label treatment as the eyebrows. A hotbar that invents
// its own look is a second design to maintain and a seam the player can see.
//
// Deriving from UCantripGlassWidget rather than restating the palette, so retuning
// FCantripGlass moves the book and the bar together.

#pragma once

#include "CoreMinimal.h"
#include "CantripBookWidgets.h"
#include "CantripHudWidgets.generated.h"

class UBorder;
class UButton;
class UCantripHotbarComponent;
class UImage;
class USizeBox;
class UTextBlock;
class UUniformGridPanel;

struct FCantripSlotDisplay;

/**
 * WBP_AbilityHotbar -- the row itself.
 */
UCLASS(Abstract)
class CHANGELING_API UCantripHotbarWidget : public UCantripGlassWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Style")
	float BarWidth = 770.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Style")
	float BarHeight = 70.0f;

	/**
	 * How present the strip behind the slots is. 0 leaves the panes floating.
	 *
	 * Lower than the book's grounds by default: this is on screen the whole time she
	 * is playing, and a HUD that competes with the garden is a HUD she stops seeing.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Style",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BackingOpacity = 0.45f;

protected:
	virtual void OnApplyGlass() override;

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|HUD", meta = (BindWidgetOptional)) TObjectPtr<UBorder> Bdr_Hotbar;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|HUD", meta = (BindWidgetOptional)) TObjectPtr<USizeBox> Box_Hotbar;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|HUD", meta = (BindWidgetOptional)) TObjectPtr<UUniformGridPanel> Grid_Slots;
};

/**
 * WBP_HotbarSlot -- one pane.
 *
 * The two state setters exist because C++ cannot see what the slot holds: Refresh
 * reads the hotbar and knows, so it tells the pane. Everything else is fixed.
 */
UCLASS(Abstract)
class CHANGELING_API UCantripSlotWidget : public UCantripGlassWidget
{
	GENERATED_BODY()

public:
	UCantripSlotWidget(const FObjectInitializer& ObjectInitializer);

	/** Call from Refresh: true when the slot has a cantrip laid out in it. */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|HUD")
	void SetFilled(bool bInFilled);

	/** Call from PressSlot / ReleaseSlot: the pane she is currently casting through. */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|HUD")
	void SetHeld(bool bInHeld);

	UFUNCTION(BlueprintPure, Category = "Cantrip|HUD")
	bool IsFilled() const { return bFilled; }

	UFUNCTION(BlueprintPure, Category = "Cantrip|HUD")
	bool IsHeld() const { return bHeld; }

	/**
	 * Flash the pane to say the press went nowhere. Bound in C++; nothing to wire.
	 *
	 * Short on purpose. Long enough to be seen when you are looking at the bar, short
	 * enough not to be read as a state the slot is in -- a refusal is an answer to a
	 * question just asked, not a condition.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Style")
	float RefusedFlashSeconds = 0.12f;

	UFUNCTION(BlueprintCallable, Category = "Cantrip|HUD")
	void FlashRefused();

	virtual void NativeConstruct() override;

	/** The icon inside the pane. The pane itself is sized by the grid. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Style")
	float IconSize = 56.0f;

	/**
	 * Which key on the number row this pane answers to, and its index in the bar.
	 *
	 * Lives in C++ because the drag handling below needs it, and a Blueprint variable of
	 * the same name would shadow this one. Delete the Blueprint's SlotIndex when you
	 * reparent; everything that read it will find this instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cantrip|HUD")
	int32 SlotIndex = 0;

	// ── Rearranging ─────────────────────────────────────────────────────────

	/**
	 * Which button picks a cantrip up off the bar.
	 *
	 * The MIDDLE button, which is what is left once the other two are spoken for.
	 *
	 * Left cannot have it: casting is a HOLD -- the length of the press buys the
	 * difficulty reduction -- so a left-drag and a left-cast are the same gesture, and
	 * DetectDrag captures the mouse so the button never sees the release that would
	 * have fired the cantrip. Right cannot have it either, though only the spellbook
	 * proves that: STableViewBase claims the right button for drag-to-scroll, so a
	 * right-drag inside the entry list scrolls the list instead. The bar has no list
	 * above it and would have been fine, but one gesture has to mean one thing.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|HUD")
	FKey DragButton;

	/** The pane a cantrip is hovering over, lit so the player can see where it lands. */
	void SetDropTarget(bool bInDropTarget);

protected:
	virtual void OnApplyGlass() override;

	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;

	virtual void NativeOnDragDetected(const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;

	virtual bool NativeOnDrop(const FGeometry& InGeometry,
		const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	virtual void NativeOnDragEnter(const FGeometry& InGeometry,
		const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;

	virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|HUD", meta = (BindWidgetOptional)) TObjectPtr<UBorder> Bdr_Slot;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|HUD", meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Slot;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|HUD", meta = (BindWidgetOptional)) TObjectPtr<USizeBox> Box_Icon;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|HUD", meta = (BindWidgetOptional)) TObjectPtr<UImage> Img_Icon;
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|HUD", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Key;

private:
	/** The ghost that follows the cursor. A bare UImage -- DefaultDragVisual takes any
	 *  UWidget, so a whole Blueprint for one icon would be a Blueprint to maintain. */
	UImage* MakeDragVisual(const FCantripSlotDisplay& Display) const;

	bool bFilled = false;
	bool bHeld = false;
	bool bRefused = false;

	FTimerHandle RefusedTimer;

	/** Bound to the component's OnSlotRefused; flashes only when the index is ours. */
	UFUNCTION()
	void HandleSlotRefused(int32 RefusedIndex);

	/**
	 * Clicking a pane casts it, exactly as its key does.
	 *
	 * Pressed and Released rather than Clicked, because casting is a HOLD -- the length
	 * of the press buys the difficulty reduction. OnClicked would collapse that to a
	 * single instant and every mouse-cast would resolve at the lowest tier, which reads
	 * as the mouse being worse at magic than the keyboard.
	 */
	UFUNCTION()
	void HandleButtonPressed();

	UFUNCTION()
	void HandleButtonReleased();
	bool bDropTarget = false;
};
