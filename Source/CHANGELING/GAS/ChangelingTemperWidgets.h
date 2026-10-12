// ChangelingTemperWidgets.h
//
// The moving traits, as bars on the HUD.
//
// The four Tempers -- Glamour, Willpower, Banality, Nightmare -- plus Health and Stamina,
// which are not Tempers but move the same way and want the same bar. The nine Attributes
// do NOT belong here: a Strength of 3 is a fact about the character that changes across a
// chronicle, not across a fight, and a bar that never moves is a bar the player stops
// reading. Those go on the character sheet.
//
// One widget class for all six, chosen by an enum rather than six near-identical
// Blueprints. The differences between them are a colour, a label and where the maximum
// comes from, and none of that is worth a class.
//
// Everything is DELEGATE-DRIVEN. A HUD that polls attributes every frame is four
// attribute reads a tick forever, and worse, it is four reads that are almost always
// answering "no change" -- while still repainting. Binding means the bar is touched only
// when something actually moved it.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "CantripBookWidgets.h"
#include "ChangelingWoundTrack.h"
#include "ChangelingTemperWidgets.generated.h"

class UAbilitySystemComponent;
class UBorder;
class UProgressBar;
class UTextBlock;

struct FOnAttributeChangeData;

/**
 * What a bar shows.
 *
 * Four Tempers and two things that are not Tempers, and the distinction is kept in the
 * comments rather than in the type because it matters to the RULES and not to the widget.
 * Health and Stamina are the body; the four above them are what the Dreaming does to her.
 */
UENUM(BlueprintType)
enum class EChangelingTemper : uint8
{
	/** Faerie essence. Spent to work magic; the one Temper she wants full. */
	Glamour,

	/** Resolve. Spent, and recovered slowly. */
	Willpower,

	/**
	 * The weight of the mundane. RISES AGAINST HER -- it is never spent, only accrued,
	 * and a full bar is the end of the character rather than the peak of her powers.
	 */
	Banality,

	/** The Unseelie counterweight to Glamour. Also rises against her. */
	Nightmare,

	/**
	 * NOT a Temper, and kept here anyway.
	 *
	 * The comment above this enum used to say "these four and no more", on the grounds
	 * that the book counts health in named wound levels -- Bruised, Hurt, Wounded -- and
	 * a percentage throws that away. That reasoning depended on the levels being the
	 * unit, and the scale is continuous now, so the distinction it protected is already
	 * gone.
	 *
	 * It shares this widget rather than getting its own because everything that matters
	 * here is the same work: find the ability system, bind two attributes, survive the
	 * ability system arriving late, paint a gem-set track. Duplicating that for one more
	 * bar would mean two copies of the retry logic, and the copy nobody is looking at is
	 * the one that rots.
	 *
	 * What IS still lost: nothing tells the player which wound level she is on. If those
	 * come back, they want their own widget over the top of this one, not a fifth colour.
	 */
	Health,

	/**
	 * Wind, not the Stamina TRAIT.
	 *
	 * UChangelingAttributeSet has both, and they are different things: the trait is a
	 * rating of what she is and never moves during play; this is the pool it ought one day
	 * to size, drained by running and fighting. The attribute behind this is StaminaPool
	 * for exactly that reason -- two attributes in one set cannot share a name, and the
	 * book's trait has the better claim to "Stamina".
	 *
	 * The player never sees the distinction: this bar is labelled Stamina.
	 */
	Stamina
};

/**
 * WBP_TemperBar -- one bar.
 *
 * Place one per trait you want shown, set Temper on each. Bind any of Bar_Fill, Txt_Value, Txt_Label and
 * Bdr_Track that the design actually uses; all are optional, following the rule the rest
 * of these widgets follow -- a missing name should leave that element alone rather than
 * refuse to compile the Blueprint.
 */
UCLASS(Abstract)
class CHANGELING_API UChangelingTemperBar : public UCantripGlassWidget
{
	GENERATED_BODY()

public:
	/** Which Temper this bar is for. The only thing that differs between the four. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Temper")
	EChangelingTemper Temper = EChangelingTemper::Glamour;

	/**
	 * Ceiling for the two Tempers that have no Max attribute.
	 *
	 * Glamour and Willpower each carry their own maximum as a second attribute, because
	 * both can be permanently raised. Banality and Nightmare cannot -- the book caps them
	 * at ten flat, and UChangelingAttributeSet::PreAttributeChange already clamps them
	 * there. This is that same ten, and it is a property only so the two numbers can be
	 * kept in step from one place if the cap is ever retuned.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Temper",
		meta = (ClampMin = "1.0"))
	float FlatMaximum = 10.0f;

	/** Current value. Valid once the ability system has been found. */
	UPROPERTY(BlueprintReadOnly, Category = "Temper")
	float Current = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Temper")
	float Maximum = 0.0f;

	/** Current over Maximum, clamped 0-1. What a progress bar wants. */
	UPROPERTY(BlueprintReadOnly, Category = "Temper")
	float Percent = 0.0f;

	/**
	 * True when a FULL bar is bad news -- Banality and Nightmare.
	 *
	 * Published so the Blueprint can say so visually without restating which Tempers are
	 * which. The two that rise against her should not read like the two she spends, or
	 * the player learns to want the wrong bars full.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Temper")
	bool bRisingIsHarm = false;

	/**
	 * True for the Temper that is alive rather than merely stored -- Glamour.
	 *
	 * Derived from Temper the same way bRisingIsHarm is, rather than ticked on by hand in
	 * four places. If another Temper should ever breathe, change it here and every bar
	 * agrees at once.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Temper")
	bool bBreathes = false;

	/**
	 * How far the breathing brightens and dims the fill, as a fraction.
	 *
	 * Small. This should be felt rather than watched: at 0.1 the bar is never obviously
	 * animating, it simply refuses to look inert. Anything past about 0.2 stops reading as
	 * breath and starts reading as a warning light.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Temper|Style",
		meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float BreathAmplitude = 0.10f;

	/** Seconds for one full breath. Four is roughly a calm human one, which is the
	 *  rhythm the eye reads as alive rather than as a machine blinking. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Temper|Style",
		meta = (ClampMin = "0.5"))
	float BreathSeconds = 4.0f;

	/**
	 * Scale the breathing by how full the bar is.
	 *
	 * On by default, and it is the half that makes this mean something: an empty Glamour
	 * bar lies perfectly still and a full one breathes deepest, so how alive it looks IS
	 * the reading. Off, it breathes the same whether she is brimming or spent, which is
	 * decoration.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Temper|Style")
	bool bBreathScalesWithFill = true;

	/**
	 * Wound level, for the Health bar only. Empty Name on the other four.
	 *
	 * This is what makes a continuous health bar honest. The book's named levels carried
	 * the dice penalty, and dropping them for a percentage would have quietly deleted the
	 * mechanic; instead the same table that costs her dice names the band, so "Wounded"
	 * on the bar and the two dice missing from her pool are one fact shown twice.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Temper")
	FChangelingWoundLevel WoundLevel;

	/** "Wounded (-2)", or empty text on a non-Health bar. Bind Txt_Label to it, or set
	 *  bLabelShowsWoundLevel and let the widget write it. */
	UFUNCTION(BlueprintPure, Category = "Temper")
	FText GetWoundLabel() const;

	/**
	 * Replace the Health bar's "Health" label with its wound level.
	 *
	 * On by default, because the level is the more useful of the two: the player already
	 * knows the red bar is health, and what she cannot see is how much it is costing her.
	 * Ignored entirely by the four Tempers.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Temper|Style")
	bool bLabelShowsWoundLevel = true;

	/** Fires whenever the value or its maximum moves. Animate the bar from here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Temper")
	void OnTemperUpdated(float InCurrent, float InMaximum, float InPercent);

	/** Re-read and repaint now. Called for you; exposed for a sheet that force-refreshes. */
	UFUNCTION(BlueprintCallable, Category = "Temper")
	void RefreshTemper();

	/** The Temper's own colour, for anything the Blueprint paints itself. */
	UFUNCTION(BlueprintPure, Category = "Temper")
	FLinearColor GetTemperColour() const;

	UFUNCTION(BlueprintPure, Category = "Temper")
	FText GetTemperLabel() const;

	/** The gem the track is set in. Distinct from the fill colour -- see the palette. */
	UFUNCTION(BlueprintPure, Category = "Temper")
	FLinearColor GetTemperBorderColour() const;

	// ── Palette ─────────────────────────────────────────────────────────────
	//
	// Editable rather than hardcoded for the reason the glass palette is: UI colour is
	// judged by eye against the scene it sits on, and a colour needing a recompile is a
	// colour nobody tunes.

	// Given as LINEAR values, converted from the sRGB hex each one was chosen as. That
	// conversion is not optional: FLinearColor is linear, and typing the hex's 0-1
	// fractions straight in produces a colour visibly paler and flatter than the one
	// picked. The hex in each comment is the intent; the numbers are that hex converted.

	/**
	 * Lavender with life in it -- #C1A6EC.
	 *
	 * Brighter and more saturated than a resting lavender, because this is the one Temper
	 * that is not a reserve but a living thing she carries. It also BREATHES; see
	 * BreathAmplitude below. Colour alone can look luminous but it cannot look alive --
	 * that takes movement, and the movement is what sells it.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Temper|Style")
	FLinearColor GlamourColour = FLinearColor(0.533f, 0.382f, 0.839f, 1.0f);

	/**
	 * Steely blue with a light in it -- #6AA4CC.
	 *
	 * Lifted and leaned very slightly toward cyan, so it reads as steel that is faintly
	 * lit rather than as plain metal. Cyan rather than violet on purpose: violet is the
	 * obvious way to make something look magical and it would put this a few degrees from
	 * Glamour's lavender, which is the one bar it must never be mistaken for. Moonlight
	 * and ice are magical too, and they sit on the other side of the wheel.
	 *
	 * Willpower is HER magic, not the Dreaming's -- so it glows, but coldly.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Temper|Style")
	FLinearColor WillpowerColour = FLinearColor(0.144f, 0.371f, 0.604f, 1.0f);

	/** Dull grey -- #6E6E72. Faintly cool, and drained of colour on purpose: Banality is
	 *  the absence of the Dreaming and should look like something being taken away. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Temper|Style")
	FLinearColor BanalityColour = FLinearColor(0.156f, 0.156f, 0.168f, 1.0f);

	/**
	 * Very dark red-black -- #350A11.
	 *
	 * Nearly black by design, which is also its one risk: at this value the fill has very
	 * little separation from a dark track, so a rising Nightmare can be hard to SEE
	 * rising. If it disappears against the glass, lift this rather than the track -- the
	 * track carries the page ground and is shared with every other bar.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Temper|Style")
	FLinearColor NightmareColour = FLinearColor(0.036f, 0.003f, 0.006f, 1.0f);

	/**
	 * Living green -- #3E9B4F.
	 *
	 * Green rather than the obvious red, and it costs nothing to be clear about why: a red
	 * fill draining down a bar reads as damage arriving, which is the same signal the
	 * player gets from Nightmare and from the wound label. Green reads as what is LEFT.
	 * It also keeps the one bar that means "still alive" out of the red family entirely,
	 * so a glance never confuses health with harm.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Temper|Style")
	FLinearColor HealthColour = FLinearColor(0.048f, 0.328f, 0.078f, 1.0f);

	/**
	 * Amber -- #D9A036. Exertion: warm, and the colour of effort rather than of magic.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Temper|Style")
	FLinearColor StaminaColour = FLinearColor(0.694f, 0.352f, 0.037f, 1.0f);

	// ── Gem settings ────────────────────────────────────────────────────────
	//
	// The BORDER, not the fill, and the pairings are deliberately unmatched: a ruby
	// setting around a lavender light, a topaz one around a near-black stain. Each bar
	// reads as a jewel the Temper is mounted in rather than as one colour twice, and the
	// frame stays legible even when the fill inside it is nearly invisible -- which for
	// Nightmare is most of the time.
	//
	// Linear, converted from the sRGB hex named in each comment.

	/** Ruby -- #C41E3A. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Temper|Style|Gems")
	FLinearColor GlamourBorderColour = FLinearColor(0.552f, 0.013f, 0.042f, 1.0f);

	/** Emerald -- #189A63. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Temper|Style|Gems")
	FLinearColor BanalityBorderColour = FLinearColor(0.009f, 0.323f, 0.125f, 1.0f);

	/** Sapphire -- #3A50C8, leaned violet to match the project's hero sapphire. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Temper|Style|Gems")
	FLinearColor WillpowerBorderColour = FLinearColor(0.042f, 0.080f, 0.578f, 1.0f);

	/** Topaz -- #E8A317. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Temper|Style|Gems")
	FLinearColor NightmareBorderColour = FLinearColor(0.807f, 0.366f, 0.009f, 1.0f);

	/**
	 * Garnet -- #9B2334.
	 *
	 * A red gem, but not ruby: Glamour already holds that, and the two sit far enough
	 * apart in depth to tell at a glance. Health being the odd one out among four Tempers
	 * is fair -- it is the only bar that is about the body.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Temper|Style|Gems")
	FLinearColor HealthBorderColour = FLinearColor(0.328f, 0.017f, 0.034f, 1.0f);

	/**
	 * Aquamarine -- #5FC9C3.
	 *
	 * The one cool gem left once ruby, emerald, sapphire, topaz and garnet are spoken for,
	 * and the furthest from the amber it frames -- so the setting reads as a setting
	 * rather than as a slightly darker edge of the fill.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Temper|Style|Gems")
	FLinearColor StaminaBorderColour = FLinearColor(0.114f, 0.584f, 0.546f, 1.0f);

	/**
	 * Thickness of the gem setting. Zero inherits the glass palette's came width.
	 *
	 * Its own setting because a came is meant to disappear and a gem setting is not: at
	 * the book's hairline the tone is there but too thin to read as a colour at all.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Temper|Style|Gems",
		meta = (ClampMin = "0.0"))
	float BorderWidth = 2.0f;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void OnApplyGlass() override;

	/** Drives the breath. Returns immediately for the three Tempers that do not. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Category = "Temper", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> Bar_Fill;

	UPROPERTY(BlueprintReadOnly, Category = "Temper", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Bdr_Track;

	/** Size Box around Bar_Fill. Optional; only needed if BarThickness is used. */
	UPROPERTY(BlueprintReadOnly, Category = "Temper", meta = (BindWidgetOptional))
	TObjectPtr<class USizeBox> Box_Bar;

	/** Size Box at the root. Optional; only needed if WidgetHeight is used. */
	UPROPERTY(BlueprintReadOnly, Category = "Temper", meta = (BindWidgetOptional))
	TObjectPtr<class USizeBox> Box_Root;

	/** "4 / 10". Set bShowMaximum false for a bar that shows the current alone. */
	UPROPERTY(BlueprintReadOnly, Category = "Temper", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Value;

	UPROPERTY(BlueprintReadOnly, Category = "Temper", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Label;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Temper|Style")
	bool bShowMaximum = true;

	/**
	 * Decimal places on the printed value.
	 *
	 * Zero by default, because a HUD read at a glance wants "4 / 10" rather than
	 * "3.7 / 10". But the Tempers here are NOT the book's integer dots -- the scale is
	 * continuous -- so rounding is a display choice and not a fact about the value, and
	 * anywhere the fraction matters can turn it up.
	 *
	 * The BAR is unaffected either way: Percent is computed from the true float, so the
	 * fill is always exact however the number beside it is written.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Temper|Style",
		meta = (ClampMin = "0", ClampMax = "3"))
	int32 ValueDecimals = 0;

	// Both sizes were hardcoded in OnApplyGlass, chosen for type sitting ON the bar.
	// Text set BELOW the bar is read at a different distance and usually wants to be
	// smaller, and the whole point of the glass palette is that a number judged by eye
	// should not need a recompile to change.

	/** The Temper's name. An eyebrow: small, tracked out, quiet. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Temper|Style",
		meta = (ClampMin = "4.0"))
	float LabelTextSize = 9.0f;

	/** The "4 / 10". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Temper|Style",
		meta = (ClampMin = "4.0"))
	float ValueTextSize = 11.0f;

	// ── Size ────────────────────────────────────────────────────────────────
	//
	// Both drive a bound Size Box's Height Override, and both are ABSOLUTE rather than a
	// multiplier of whatever the Designer set. A multiplier would have to read the current
	// height, scale it and write it back -- and OnApplyGlass runs on every construct and
	// every property change, so the bar would grow by half again each time anyone touched
	// it. Absolute is the only version that is idempotent.
	//
	// Zero on either means "leave that Size Box alone", so a design that would rather set
	// its heights in the Designer simply does not fill these in.

	/**
	 * Height of the coloured bar itself, in slate units. Needs a Size Box named Box_Bar
	 * wrapped around Bar_Fill.
	 *
	 * A ProgressBar has no height of its own -- it fills whatever slot it is given -- which
	 * is why this cannot be a property on the bar and has to be a box around it.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Temper|Style|Size",
		meta = (ClampMin = "0.0"))
	float BarThickness = 0.0f;

	/** Height of the whole widget -- bar plus the text row beneath it. Needs a Size Box
	 *  named Box_Root at the top of the hierarchy. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Temper|Style|Size",
		meta = (ClampMin = "0.0"))
	float WidgetHeight = 0.0f;

private:
	/** The attribute this bar watches, and the one holding its ceiling (may be invalid). */
	FGameplayAttribute ValueAttribute() const;
	FGameplayAttribute MaximumAttribute() const;

	/**
	 * Find the ability system and bind. Returns whether it succeeded.
	 *
	 * Separate from NativeConstruct because it usually FAILS there: a HUD is commonly
	 * built before the PlayerState has replicated, so the pawn has no ability system yet
	 * and there is nothing to bind to. The retry below is what makes that a non-issue
	 * rather than a bar that is empty for the whole session.
	 */
	bool BindToAbilitySystem();

	void UnbindFromAbilitySystem();

	/** GAS's callback. One function for both attributes -- either moving means repaint. */
	void HandleAttributeChanged(const FOnAttributeChangeData& Data);

	/** Pull the numbers out of the ability system and push them at the widgets. */
	void UpdateFromAttributes();

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;

	FDelegateHandle ValueChangedHandle;
	FDelegateHandle MaximumChangedHandle;

	/** Retries the bind until the ability system exists. Cleared the moment it does. */
	FTimerHandle BindRetryTimer;

	/** Where in the breath cycle we are, in seconds. Wrapped rather than left to grow,
	 *  so the sine keeps its precision in a session left running for hours. */
	float BreathTime = 0.0f;
};
