// ChangelingTemperWidgets.cpp

#include "ChangelingTemperWidgets.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "ChangelingAttributeSet.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogTemperBar, Log, All);

namespace
{
	/** How often to look again for an ability system that was not there yet. */
	constexpr float BindRetrySeconds = 0.25f;
}

FGameplayAttribute UChangelingTemperBar::ValueAttribute() const
{
	switch (Temper)
	{
	case EChangelingTemper::Willpower: return UChangelingAttributeSet::GetWillpowerAttribute();
	case EChangelingTemper::Banality:  return UChangelingAttributeSet::GetBanalityAttribute();
	case EChangelingTemper::Nightmare: return UChangelingAttributeSet::GetNightmareAttribute();
	case EChangelingTemper::Health:    return UChangelingAttributeSet::GetHealthAttribute();
	case EChangelingTemper::Stamina:   return UChangelingAttributeSet::GetStaminaPoolAttribute();
	case EChangelingTemper::Glamour:
	default:                           return UChangelingAttributeSet::GetGlamourAttribute();
	}
}

FGameplayAttribute UChangelingTemperBar::MaximumAttribute() const
{
	// Three of the five have one. Banality and Nightmare do not, and that is deliberate:
	// both are hard-capped at ten by PreAttributeChange, so the cap is a rule rather than
	// a property of the character and there is nothing per-character to store. They return
	// an invalid attribute, which UpdateFromAttributes reads as "use FlatMaximum".
	switch (Temper)
	{
	case EChangelingTemper::Glamour:   return UChangelingAttributeSet::GetGlamourMaxAttribute();
	case EChangelingTemper::Willpower: return UChangelingAttributeSet::GetWillpowerMaxAttribute();
	case EChangelingTemper::Health:    return UChangelingAttributeSet::GetHealthMaxAttribute();
	case EChangelingTemper::Stamina:   return UChangelingAttributeSet::GetStaminaPoolMaxAttribute();
	default:                           return FGameplayAttribute();
	}
}

FLinearColor UChangelingTemperBar::GetTemperColour() const
{
	switch (Temper)
	{
	case EChangelingTemper::Willpower: return WillpowerColour;
	case EChangelingTemper::Banality:  return BanalityColour;
	case EChangelingTemper::Nightmare: return NightmareColour;
	case EChangelingTemper::Health:    return HealthColour;
	case EChangelingTemper::Stamina:   return StaminaColour;
	case EChangelingTemper::Glamour:
	default:                           return GlamourColour;
	}
}

FLinearColor UChangelingTemperBar::GetTemperBorderColour() const
{
	switch (Temper)
	{
	case EChangelingTemper::Willpower: return WillpowerBorderColour;
	case EChangelingTemper::Banality:  return BanalityBorderColour;
	case EChangelingTemper::Nightmare: return NightmareBorderColour;
	case EChangelingTemper::Health:    return HealthBorderColour;
	case EChangelingTemper::Stamina:   return StaminaBorderColour;
	case EChangelingTemper::Glamour:
	default:                           return GlamourBorderColour;
	}
}

FText UChangelingTemperBar::GetTemperLabel() const
{
	switch (Temper)
	{
	case EChangelingTemper::Willpower: return NSLOCTEXT("Changeling", "Willpower", "Willpower");
	case EChangelingTemper::Banality:  return NSLOCTEXT("Changeling", "Banality",  "Banality");
	case EChangelingTemper::Nightmare: return NSLOCTEXT("Changeling", "Nightmare", "Nightmare");
	case EChangelingTemper::Health:    return NSLOCTEXT("Changeling", "Health",    "Health");
	case EChangelingTemper::Stamina:   return NSLOCTEXT("Changeling", "Stamina",   "Stamina");
	case EChangelingTemper::Glamour:
	default:                           return NSLOCTEXT("Changeling", "Glamour",   "Glamour");
	}
}

FText UChangelingTemperBar::GetWoundLabel() const
{
	if (Temper != EChangelingTemper::Health || WoundLevel.Name.IsEmpty())
	{
		return FText::GetEmpty();
	}

	// The penalty is only worth printing when there is one. "Healthy (-0)" is noise, and
	// worse, it trains the player to read a number that is usually zero and therefore to
	// stop reading it at the moment it stops being zero.
	if (WoundLevel.DicePenalty <= 0)
	{
		return WoundLevel.Name;
	}

	return FText::Format(NSLOCTEXT("Changeling", "WoundLevelWithPenalty", "{0} (-{1})"),
		WoundLevel.Name, FText::AsNumber(WoundLevel.DicePenalty));
}

void UChangelingTemperBar::NativeConstruct()
{
	Super::NativeConstruct();

	bRisingIsHarm = (Temper == EChangelingTemper::Banality
		|| Temper == EChangelingTemper::Nightmare);

	// Glamour alone. Willpower is resolve and Banality is the absence of magic; neither
	// should look like it has a pulse. Derived here rather than set on four instances,
	// for the same reason bRisingIsHarm is.
	bBreathes = (Temper == EChangelingTemper::Glamour);

	if (!BindToAbilitySystem())
	{
		// Almost always this path on the first construct. The HUD is built before the
		// PlayerState has replicated, so there is no ability system to bind to yet and
		// there is no event on the widget that says when there is. Retrying quietly beats
		// every alternative: a bar that is empty for the session, or a Blueprint that has
		// to remember to call Refresh from somewhere.
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(BindRetryTimer,
				FTimerDelegate::CreateWeakLambda(this, [this]()
				{
					if (BindToAbilitySystem())
					{
						if (UWorld* Inner = GetWorld())
						{
							Inner->GetTimerManager().ClearTimer(BindRetryTimer);
						}
					}
				}),
				BindRetrySeconds, /*bLoop*/ true);
		}
	}
}

void UChangelingTemperBar::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryTimer);
	}

	UnbindFromAbilitySystem();

	Super::NativeDestruct();
}

bool UChangelingTemperBar::BindToAbilitySystem()
{
	if (AbilitySystem.IsValid())
	{
		return true;
	}

	// Through the interface off the owning pawn rather than FindComponentByClass, so this
	// keeps working the day the ability system moves to the PlayerState -- which is where
	// it usually ends up once there is more than one kind of character.
	const APlayerController* Controller = GetOwningPlayer();
	const AActor* Owner = Controller ? Controller->GetPawn() : nullptr;

	UAbilitySystemComponent* Found = nullptr;
	if (const IAbilitySystemInterface* Interface = Cast<IAbilitySystemInterface>(Owner))
	{
		Found = Interface->GetAbilitySystemComponent();
	}

	if (!Found)
	{
		return false;
	}

	AbilitySystem = Found;

	ValueChangedHandle = Found->GetGameplayAttributeValueChangeDelegate(ValueAttribute())
		.AddUObject(this, &UChangelingTemperBar::HandleAttributeChanged);

	// Only if there is one. Binding an invalid attribute would register a delegate that
	// can never fire, which is harmless but leaves a handle that looks meaningful.
	const FGameplayAttribute Max = MaximumAttribute();
	if (Max.IsValid())
	{
		MaximumChangedHandle = Found->GetGameplayAttributeValueChangeDelegate(Max)
			.AddUObject(this, &UChangelingTemperBar::HandleAttributeChanged);
	}

	// Once immediately. The delegates only fire on CHANGE, so without this the bar shows
	// nothing until the first time the player spends something -- which for Banality could
	// be most of a session.
	UpdateFromAttributes();

	return true;
}

void UChangelingTemperBar::UnbindFromAbilitySystem()
{
	UAbilitySystemComponent* Bound = AbilitySystem.Get();
	if (!Bound)
	{
		AbilitySystem.Reset();
		return;
	}

	if (ValueChangedHandle.IsValid())
	{
		Bound->GetGameplayAttributeValueChangeDelegate(ValueAttribute())
			.Remove(ValueChangedHandle);
		ValueChangedHandle.Reset();
	}

	const FGameplayAttribute Max = MaximumAttribute();
	if (MaximumChangedHandle.IsValid() && Max.IsValid())
	{
		Bound->GetGameplayAttributeValueChangeDelegate(Max).Remove(MaximumChangedHandle);
		MaximumChangedHandle.Reset();
	}

	AbilitySystem.Reset();
}

void UChangelingTemperBar::HandleAttributeChanged(const FOnAttributeChangeData& /*Data*/)
{
	// The payload is ignored on purpose. Either attribute moving means the same work, and
	// the pair has to be read together anyway -- a Glamour of 4 means nothing without the
	// maximum it is 4 of.
	UpdateFromAttributes();
}

void UChangelingTemperBar::RefreshTemper()
{
	if (!AbilitySystem.IsValid())
	{
		BindToAbilitySystem();
		return;
	}

	UpdateFromAttributes();
}

void UChangelingTemperBar::UpdateFromAttributes()
{
	UAbilitySystemComponent* Bound = AbilitySystem.Get();
	if (!Bound)
	{
		return;
	}

	bool bFound = false;
	Current = Bound->GetGameplayAttributeValue(ValueAttribute(), bFound);

	const FGameplayAttribute Max = MaximumAttribute();
	if (Max.IsValid())
	{
		bool bMaxFound = false;
		Maximum = Bound->GetGameplayAttributeValue(Max, bMaxFound);
	}
	else
	{
		Maximum = FlatMaximum;
	}

	// A maximum of zero is survivable rather than fatal: it means the attribute set has
	// not been initialised yet, and dividing by it would put a NaN into the progress bar,
	// which Slate renders as a bar that is either empty or completely full at random.
	Percent = (Maximum > KINDA_SMALL_NUMBER)
		? FMath::Clamp(Current / Maximum, 0.0f, 1.0f)
		: 0.0f;

	// Only Health has a wound track. The other four leave WoundLevel at its default, so
	// a Blueprint reading it on a Temper bar gets empty text rather than a wrong one.
	if (Temper == EChangelingTemper::Health)
	{
		WoundLevel = UChangelingWoundLibrary::GetWoundLevel(Percent);

		if (bLabelShowsWoundLevel && Txt_Label)
		{
			Txt_Label->SetText(GetWoundLabel());
		}
	}

	if (Bar_Fill)
	{
		Bar_Fill->SetPercent(Percent);
		Bar_Fill->SetFillColorAndOpacity(GetTemperColour());
	}

	if (Txt_Value)
	{
		// Formatted rather than rounded. These Tempers move continuously rather than in
		// the book's whole dots, so how many decimals to show is a decision about the
		// HUD, not about the value -- and printing an int would throw away a distinction
		// the rest of the system is keeping.
		FNumberFormattingOptions Format;
		Format.MinimumFractionalDigits = ValueDecimals;
		Format.MaximumFractionalDigits = ValueDecimals;

		Txt_Value->SetText(bShowMaximum
			? FText::Format(NSLOCTEXT("Changeling", "TemperOverMax", "{0} / {1}"),
				FText::AsNumber(Current, &Format),
				FText::AsNumber(Maximum, &Format))
			: FText::AsNumber(Current, &Format));
	}

	OnTemperUpdated(Current, Maximum, Percent);
}

void UChangelingTemperBar::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Three of the four leave immediately, before touching anything. A HUD element that
	// ticks is a cost paid every frame forever, and only Glamour is buying anything.
	if (!bBreathes || !Bar_Fill || BreathAmplitude <= 0.0f || BreathSeconds <= 0.0f)
	{
		return;
	}

	// Wrapped rather than accumulated without limit: a float seconds counter left running
	// for hours loses the resolution that makes the sine smooth, and the breath develops
	// a visible stutter late in a long session.
	BreathTime = FMath::Fmod(BreathTime + InDeltaTime, BreathSeconds);

	// Sine rather than a triangle or a saw. Breath has no corners -- it slows at the top
	// and the bottom, and any ramp with a constant rate reads as a machine.
	const float Phase = (BreathTime / BreathSeconds) * 2.0f * PI;

	float Depth = BreathAmplitude;
	if (bBreathScalesWithFill)
	{
		// An empty Glamour bar lies still; a full one breathes deepest. This is what makes
		// the motion a READING rather than an ornament -- how alive it looks is how much
		// she is carrying.
		Depth *= Percent;
	}

	const float Breath = 1.0f + (Depth * FMath::Sin(Phase));

	// RGB only. Scaling alpha as well would fade the bar toward its background at the
	// bottom of each breath, which reads as the widget flickering rather than as the
	// Glamour pulsing.
	FLinearColor Lit = GetTemperColour();
	Lit.R *= Breath;
	Lit.G *= Breath;
	Lit.B *= Breath;

	Bar_Fill->SetFillColorAndOpacity(Lit);
}

void UChangelingTemperBar::OnApplyGlass()
{
	Super::OnApplyGlass();

	// The track's GROUND stays the page colour -- a tinted ground makes an empty bar look
	// partly full -- but its OUTLINE is the Temper's gem.
	//
	// Built here rather than through PaintPane because that helper takes the outline from
	// Glass.Came and gives every pane in the game the same leading, which is exactly what
	// it is for everywhere else. This is the one place that wants a different ring per
	// widget.
	if (Bdr_Track)
	{
		FSlateBrush Setting;
		Setting.DrawAs = ESlateBrushDrawType::RoundedBox;

		// WHITE, and this is the trap PaintPane's own comment warns about: SBorder paints
		// the box as BrushColor * Brush.TintColor, so putting the ground colour here as
		// well would multiply it into itself and darken the track for no visible reason.
		Setting.TintColor = FSlateColor(FLinearColor::White);

		Setting.OutlineSettings.Color = FSlateColor(GetTemperBorderColour());
		Setting.OutlineSettings.Width = (BorderWidth > 0.0f) ? BorderWidth : Glass.CameWidth;
		Setting.OutlineSettings.CornerRadii =
			FVector4(Glass.PaneRadius, Glass.PaneRadius, Glass.PaneRadius, Glass.PaneRadius);
		Setting.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;

		Bdr_Track->SetBrush(Setting);
		Bdr_Track->SetBrushColor(Glass.Page);

		// White, so the pane tints its backing and not the progress bar inside it.
		Bdr_Track->SetContentColorAndOpacity(FLinearColor::White);
	}

	// Heights first: the text below is sized against the room it ends up with, and a
	// label painted before the box grew would be measured against the old height.
	if (Box_Bar && BarThickness > 0.0f)
	{
		Box_Bar->SetHeightOverride(BarThickness);
	}

	if (Box_Root && WidgetHeight > 0.0f)
	{
		Box_Root->SetHeightOverride(WidgetHeight);
	}

	PaintLabel(Txt_Label, Glass.Muted, LabelTextSize);
	PaintText(Txt_Value, Glass.Parchment, ValueTextSize, BodyFont);

	// The name follows the dropdown rather than being typed into four Blueprints. Set
	// here rather than in UpdateFromAttributes because it depends only on Temper, not on
	// anything the ability system knows -- so it is correct in the Designer, before there
	// is a character, which is exactly when the layout is being judged.
	//
	// A design that wants its own wording simply does not name its text block Txt_Label.
	if (Txt_Label)
	{
		Txt_Label->SetText(GetTemperLabel());
	}

	if (Bar_Fill)
	{
		Bar_Fill->SetFillColorAndOpacity(GetTemperColour());
	}

	// Runs in the Designer too, where there is no ability system and every bar would sit
	// at zero. Half-full shows the fill colour and the type against a real value while
	// the layout is being built.
	if (!AbilitySystem.IsValid() && IsDesignTime() && Bar_Fill)
	{
		Bar_Fill->SetPercent(0.5f);
	}
}
