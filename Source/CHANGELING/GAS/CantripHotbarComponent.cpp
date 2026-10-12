// CantripHotbarComponent.cpp

#include "CantripHotbarComponent.h"

#include "AbilitySystemComponent.h"
#include "CHANGELINGCharacter.h"
#include "CantripInternal.h"
#include "ChangelingMagicSet.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "CantripResolver.h"
#include "GA_Cantrip.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogCantripHotbar, Log, All);

UCantripHotbarComponent::UCantripHotbarComponent()
{
	// Nothing here ticks. Everything is event-driven off the ability system and the
	// widgets' own input -- a hotbar that polls is a hotbar costing frame time to tell
	// you nothing has changed.
	PrimaryComponentTick.bCanEverTick = false;
}

void UCantripHotbarComponent::BeginPlay()
{
	Super::BeginPlay();

	Slots.SetNum(FMath::Max(1, SlotCount));

	if (!GetOwner<APlayerController>())
	{
		// A copy of this component on the pawn is the failure that does not look like
		// one. It binds nothing, so it never learns what she knows; every query then
		// answers truthfully and emptily, and a widget that found this one instead of
		// the controller's draws blank rows with no error anywhere. Say so out loud --
		// the alternative is another evening spent looking at the DataTables.
		UE_LOG(LogCantripHotbar, Error,
			TEXT("HOTBAR: CantripHotbarComponent is on %s, which is not a PlayerController. ")
			TEXT("It will stay empty and any widget reading it will draw nothing. ")
			TEXT("The hotbar belongs on the controller -- remove this one."),
			*GetNameSafe(GetOwner()));

		return;
	}

	if (APlayerController* PC = Cast<APlayerController>(GetOwner()))
	{
		// Bound rather than read once. The controller can be given a new body -- a
		// respawn, a possession, a shapechange -- and the bar has to start listening to
		// the new one's ability system without losing what the player laid out.
		PC->OnPossessedPawnChanged.AddDynamic(
			this, &UCantripHotbarComponent::HandlePossessedPawnChanged);

		HandlePossessedPawnChanged(nullptr, PC->GetPawn());
	}
}

void UCantripHotbarComponent::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	if (ACHANGELINGCharacter* Previous = BoundCharacter.Get())
	{
		Previous->OnAbilitiesChanged.RemoveDynamic(
			this, &UCantripHotbarComponent::HandleAbilitiesChanged);
	}

	BoundCharacter = Cast<ACHANGELINGCharacter>(NewPawn);

	if (ACHANGELINGCharacter* Changeling = BoundCharacter.Get())
	{
		Changeling->OnAbilitiesChanged.AddDynamic(
			this, &UCantripHotbarComponent::HandleAbilitiesChanged);
	}

	BindToASC(GetASC());

	// Possession and granting race each other -- BeginPlay and PossessedBy can run in
	// either order -- so the bar reconciles on both, and the reconcile is idempotent.
	RefreshFromKnown();
}

void UCantripHotbarComponent::BindToASC(UAbilitySystemComponent* ASC)
{
	if (BoundASC.Get() == ASC)
	{
		return;
	}

	if (UAbilitySystemComponent* Previous = BoundASC.Get())
	{
		Previous->AbilityActivatedCallbacks.Remove(AbilityActivatedHandle);
		Previous->AbilityEndedCallbacks.Remove(AbilityEndedHandle);
	}

	AbilityActivatedHandle.Reset();
	AbilityEndedHandle.Reset();
	BoundASC = ASC;

	if (ASC)
	{
		// Both callbacks, not just activation. A cantrip can stop being channelled for
		// reasons no one presses a key for -- an interrupt, a fizzle, running out of
		// Glamour, dying mid-hold -- and a bar that lit on activation and waited for a
		// release to unlight would leave a gold pane burning over a cast that is long
		// over.
		AbilityActivatedHandle = ASC->AbilityActivatedCallbacks.AddUObject(
			this, &UCantripHotbarComponent::HandleAbilityActiveChanged);

		AbilityEndedHandle = ASC->AbilityEndedCallbacks.AddUObject(
			this, &UCantripHotbarComponent::HandleAbilityActiveChanged);
	}
}

void UCantripHotbarComponent::HandleAbilityActiveChanged(UGameplayAbility* Ability)
{
	if (!Ability)
	{
		return;
	}

	// Every ability on the character comes through here, not just cantrips. Jumping
	// should not repaint the spell bar, so this is gated on the ability actually being
	// laid out somewhere -- which is also exactly the condition under which any pane's
	// appearance could have changed.
	const UClass* Started = Ability->GetClass();

	const bool bOnBar = Slots.ContainsByPredicate(
		[Started](const TSubclassOf<UGameplayAbility>& Slot)
		{
			return Slot.Get() == Started;
		});

	if (bOnBar)
	{
		RequestRefresh();
	}

	// A held press gets its retry here, on the next tick rather than now. The blocking
	// tag is taken off as the ability ends, and this callback fires close enough to that
	// moment that trying immediately can find the tag still standing -- so the retry
	// would be refused by the working it was waiting for. Next tick, everything has
	// settled. Deliberately not gated on bOnBar: what blocked her may have been anything
	// that owns the casting tag, not necessarily something laid out on the bar.
	if (BufferedSlot != INDEX_NONE)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(this,
				&UCantripHotbarComponent::FlushBufferedInput);
		}
	}
}

void UCantripHotbarComponent::RequestRefresh()
{
	if (bRefreshQueued)
	{
		return;
	}

	UWorld* World = GetWorld();

	if (!World)
	{
		OnHotbarChanged.Broadcast();
		return;
	}

	bRefreshQueued = true;
	World->GetTimerManager().SetTimerForNextTick(
		this, &UCantripHotbarComponent::FlushRefresh);
}

void UCantripHotbarComponent::FlushRefresh()
{
	bRefreshQueued = false;
	OnHotbarChanged.Broadcast();
}

void UCantripHotbarComponent::HandleAbilitiesChanged()
{
	// Granting can bring the ability system itself into being, so this is the other
	// moment the cast delegates might need attaching -- possession alone is not enough
	// when the ASC arrives after the pawn does.
	BindToASC(GetASC());

	RefreshFromKnown();
}

ACHANGELINGCharacter* UCantripHotbarComponent::GetChangeling() const
{
	if (ACHANGELINGCharacter* Bound = BoundCharacter.Get())
	{
		return Bound;
	}

	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	return PC ? Cast<ACHANGELINGCharacter>(PC->GetPawn()) : nullptr;
}

UAbilitySystemComponent* UCantripHotbarComponent::GetASC() const
{
	const ACHANGELINGCharacter* Changeling = GetChangeling();
	return Changeling ? Changeling->GetAbilitySystemComponent() : nullptr;
}

bool UCantripHotbarComponent::IsKnown(TSubclassOf<UGameplayAbility> AbilityClass) const
{
	const UAbilitySystemComponent* ASC = GetASC();
	if (!ASC || !AbilityClass)
	{
		return false;
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->GetClass() == AbilityClass)
		{
			return true;
		}
	}

	return false;
}

bool UCantripHotbarComponent::BuildDisplay(TSubclassOf<UGameplayAbility> AbilityClass,
	FCantripSlotDisplay& Out) const
{
	if (!AbilityClass)
	{
		return false;
	}

	// Read off the class default object. The row a cantrip belongs to is a property of
	// the ABILITY CLASS, not of any instance -- so the bar can describe a cantrip that
	// has never once been cast, which is exactly the case that matters for a spellbook.
	const UGA_Cantrip* CDO = Cast<UGA_Cantrip>(AbilityClass->GetDefaultObject());
	if (!CDO)
	{
		return false;
	}

	Out.bFilled = true;
	Out.AbilityClass = AbilityClass;
	Out.CantripRow = CDO->GetCantripRowName();

	if (const FCantripSpec* Spec = CDO->GetCantripSpec())
	{
		Out.DisplayName = Spec->DisplayName;
		Out.Description = Spec->Description;
		Out.Icon = Spec->Icon;
		Out.Element = Spec->Element;
		Out.ArtLevel = Spec->ArtLevel;
		Out.ArtTag = Spec->ArtTag;
		Out.ArtDisplayName = ArtNameFor(Spec->ArtTag);
		Out.ChosenRealms = GetChosenRealms(Out.CantripRow);
		Out.Manifestation = Spec->Manifestation;
		Out.CastTierThresholds = Spec->CastTiers.TierThresholds;
		Out.Ease = EaseFor(Out.CantripRow, GetArtRating(Spec->ArtTag));

		// Composed here rather than stored on the row, exactly as the book does it --
		// ninety hand-entered references to ninety generated textures is ninety chances
		// to point Pyretics 3 at Wayfare's roundel.
		Out.LevelSymbol = SymbolFor(LevelSymbolPattern,
			CantripInternal::ArtPropertyNameFromTag(Spec->ArtTag), Spec->ArtLevel);
	}
	else
	{
		// A missing row is a data error, not a reason to draw nothing. Falling back to
		// the row name gives the player a label and gives whoever filled the table a
		// visible clue about which entry they missed.
		Out.DisplayName = FText::FromName(Out.CantripRow);
	}

	return true;
}

FCantripSlotDisplay UCantripHotbarComponent::GetSlot(int32 Index) const
{
	FCantripSlotDisplay Display;

	if (!Slots.IsValidIndex(Index))
	{
		return Display;
	}

	if (BuildDisplay(Slots[Index], Display))
	{
		Display.bChannelling = IsSlotChannelling(Index);
	}

	return Display;
}

TArray<FCantripSlotDisplay> UCantripHotbarComponent::GetKnownCantrips() const
{
	TArray<FCantripSlotDisplay> Known;

	const UAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		return Known;
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		// Cantrips only. The ASC also carries passives and whatever internal abilities
		// the system grants itself, and none of those belong on a spell bar.
		if (!Spec.Ability || !Spec.Ability->IsA<UGA_Cantrip>())
		{
			continue;
		}

		FCantripSlotDisplay Display;
		if (BuildDisplay(Spec.Ability->GetClass(), Display))
		{
			Display.bChannelling = Spec.IsActive();
			Known.Add(Display);
		}
	}

	return Known;
}

int32 UCantripHotbarComponent::FindSlotFor(TSubclassOf<UGameplayAbility> AbilityClass) const
{
	return AbilityClass ? Slots.IndexOfByKey(AbilityClass) : INDEX_NONE;
}

int32 UCantripHotbarComponent::FindEmptySlot() const
{
	return Slots.IndexOfByKey(nullptr);
}

bool UCantripHotbarComponent::GrantCantrip(TSubclassOf<UGameplayAbility> AbilityClass)
{
	UAbilitySystemComponent* ASC = GetASC();

	if (!ASC || !AbilityClass)
	{
		return false;
	}

	// Already hers. Not a failure worth logging as one -- walking back over a pickup she
	// already emptied should do nothing, quietly.
	if (IsKnown(AbilityClass))
	{
		return false;
	}

	// Level 1 and no source object: a cantrip's power comes from her Art rating and the
	// Realms she aims it through, both read at cast time, so there is nothing to bake in
	// at the moment she learns it.
	ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));

	UE_LOG(LogCantripHotbar, Log, TEXT("HOTBAR: learned %s."), *GetNameSafe(AbilityClass));

	// The bar does not watch the ASC's ability list -- activation and ending have
	// delegates, granting does not -- so nothing would repaint without this.
	RefreshFromKnown();

	return true;
}

bool UCantripHotbarComponent::AssignSlot(int32 Index,
	TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (!Slots.IsValidIndex(Index))
	{
		return false;
	}

	// Refused rather than allowed and hidden. A slot holding a cantrip she does not know
	// would look castable and silently do nothing, which reads as a broken key.
	if (AbilityClass && !IsKnown(AbilityClass))
	{
		UE_LOG(LogCantripHotbar, Warning,
			TEXT("HOTBAR: refused %s in slot %d -- not granted to this character."),
			*GetNameSafe(AbilityClass), Index);
		return false;
	}

	// Moved, not copied. Dragging a spell onto a new key means it leaves the old one.
	const int32 Existing = FindSlotFor(AbilityClass);
	if (Existing != INDEX_NONE && Existing != Index)
	{
		Slots[Existing] = nullptr;
	}

	Slots[Index] = AbilityClass;

	OnHotbarChanged.Broadcast();
	return true;
}

bool UCantripHotbarComponent::ClearSlot(int32 Index)
{
	if (!Slots.IsValidIndex(Index) || !Slots[Index])
	{
		return false;
	}

	Slots[Index] = nullptr;

	OnHotbarChanged.Broadcast();
	return true;
}

bool UCantripHotbarComponent::SwapSlots(int32 A, int32 B)
{
	if (!Slots.IsValidIndex(A) || !Slots.IsValidIndex(B) || A == B)
	{
		return false;
	}

	Slots.Swap(A, B);

	OnHotbarChanged.Broadcast();
	return true;
}

bool UCantripHotbarComponent::TryPressSlot(int32 Index)
{
	ACHANGELINGCharacter* Changeling = GetChangeling();

	if (!Changeling || !Slots.IsValidIndex(Index) || !Slots[Index])
	{
		return false;
	}

	return Changeling->PressCantrip(Slots[Index]);
}

void UCantripHotbarComponent::PressSlot(int32 Index)
{
	if (!Slots.IsValidIndex(Index))
	{
		return;
	}

	// An empty pane is refused outright. There is nothing coming that would make it
	// castable, so holding the press would only delay the answer.
	if (!Slots[Index])
	{
		OnSlotRefused.Broadcast(Index);
		return;
	}

	if (TryPressSlot(Index))
	{
		BufferedSlot = INDEX_NONE;
		return;
	}

	// Refused, but she is probably just mid-working -- hold it and try again the moment
	// the current one ends. The alternative is a bar that eats any key pressed a frame
	// early, which reads as the input being broken rather than as her being busy.
	BufferedSlot = Index;
	BufferedAt = FPlatformTime::Seconds();

	// A timer as well as the ability-ended retry, because nothing may ever end: a press
	// blocked by cost or by a cooldown has no event coming, and without this the held
	// press would sit there silently instead of admitting it failed.
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(BufferTimer, this,
			&UCantripHotbarComponent::FlushBufferedInput, InputBufferSeconds, false);
	}
}

void UCantripHotbarComponent::FlushBufferedInput()
{
	if (BufferedSlot == INDEX_NONE)
	{
		return;
	}

	const int32 Index = BufferedSlot;

	// Measured from the ORIGINAL press, not from this attempt -- a retry that reset the
	// clock would keep a press alive indefinitely as long as things kept ending.
	if (FPlatformTime::Seconds() - BufferedAt > InputBufferSeconds)
	{
		BufferedSlot = INDEX_NONE;
		OnSlotRefused.Broadcast(Index);
		return;
	}

	if (TryPressSlot(Index))
	{
		BufferedSlot = INDEX_NONE;
	}
}

void UCantripHotbarComponent::ReleaseSlot(int32 Index)
{
	ACHANGELINGCharacter* Changeling = GetChangeling();

	if (!Changeling || !Slots.IsValidIndex(Index) || !Slots[Index])
	{
		return;
	}

	Changeling->ReleaseCantrip(Slots[Index]);
}

bool UCantripHotbarComponent::IsSlotChannelling(int32 Index) const
{
	const UAbilitySystemComponent* ASC = GetASC();

	if (!ASC || !Slots.IsValidIndex(Index) || !Slots[Index])
	{
		return false;
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->GetClass() == Slots[Index])
		{
			return Spec.IsActive();
		}
	}

	return false;
}

void UCantripHotbarComponent::RefreshFromKnown()
{
	if (Slots.Num() != FMath::Max(1, SlotCount))
	{
		Slots.SetNum(FMath::Max(1, SlotCount));
	}

	bool bChanged = false;

	// Anything she no longer has comes off the bar. Forgetting a cantrip -- a Banality
	// crisis, a lost Art -- should empty its key rather than leave a dead one behind.
	for (TSubclassOf<UGameplayAbility>& Slot : Slots)
	{
		if (Slot && !IsKnown(Slot))
		{
			Slot = nullptr;
			bChanged = true;
		}
	}

	if (bAutoFillOnLearn)
	{
		for (const FCantripSlotDisplay& Known : GetKnownCantrips())
		{
			if (FindSlotFor(Known.AbilityClass) != INDEX_NONE)
			{
				continue;
			}

			const int32 Free = Slots.IndexOfByKey(nullptr);
			if (Free == INDEX_NONE)
			{
				// A full bar is not an error. She knows more than ten cantrips and the
				// rest live in the spellbook until the player makes room.
				break;
			}

			Slots[Free] = Known.AbilityClass;
			bChanged = true;

			UE_LOG(LogCantripHotbar, Log, TEXT("HOTBAR: %s -> slot %d (learned)."),
				*GetNameSafe(Known.AbilityClass), Free);
		}
	}

	if (bChanged)
	{
		OnHotbarChanged.Broadcast();
	}
}

// ── Spellbook ───────────────────────────────────────────────────────────────

TSubclassOf<UGameplayAbility> UCantripHotbarComponent::KnownClassForRow(FName RowName) const
{
	const UAbilitySystemComponent* ASC = GetASC();
	if (!ASC || RowName.IsNone())
	{
		return nullptr;
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		const UGA_Cantrip* Cantrip = Cast<UGA_Cantrip>(Spec.Ability);
		if (Cantrip && Cantrip->GetCantripRowName() == RowName)
		{
			return Spec.Ability->GetClass();
		}
	}

	return nullptr;
}

TSoftObjectPtr<UTexture2D> UCantripHotbarComponent::SymbolFor(const FString& Pattern,
	FName ArtName, int32 Level) const
{
	if (ArtSymbolPath.IsEmpty() || Pattern.IsEmpty() || ArtName.IsNone())
	{
		return nullptr;
	}

	FString Asset = Pattern;
	Asset = Asset.Replace(TEXT("{Art}"), *ArtName.ToString());
	Asset = Asset.Replace(TEXT("{Level}"), *FString::FromInt(Level));

	// Package path plus object name after the dot -- a soft reference to a texture is
	// "/Game/Path/T_Foo.T_Foo", and one written without the object half silently
	// resolves to nothing.
	const FString Full = FString::Printf(TEXT("%s/%s.%s"), *ArtSymbolPath, *Asset, *Asset);
	return TSoftObjectPtr<UTexture2D>(FSoftObjectPath(Full));
}

FText UCantripHotbarComponent::DescribeRealms(const TArray<ECantripRealm>& Realms) const
{
	if (Realms.Num() == 0)
	{
		return FText::GetEmpty();
	}

	const UEnum* RealmEnum = StaticEnum<ECantripRealm>();
	TArray<FString> Parts;

	for (const ECantripRealm Realm : Realms)
	{
		// Display names off the UENUM rather than a switch. The enum already carries
		// the player-facing spelling in its UMETA, and a second list of the same six
		// words is a second list to keep in step.
		FString Name = RealmEnum
			? RealmEnum->GetDisplayNameTextByValue(static_cast<int64>(Realm)).ToString()
			: FString();

		if (Name.IsEmpty())
		{
			continue;
		}

		// A Realm she has no dots in still aims the cantrip -- it just aims it badly,
		// and cheating a rating-0 Realm costs an extra Glamour. Marking it is the
		// whole point of printing the list.
		if (GetRealmRating(Realm) <= 0)
		{
			Name += TEXT(" (unlearned)");
		}

		Parts.Add(Name);
	}

	return FText::FromString(FString::Join(Parts, TEXT(", ")));
}

TSoftObjectPtr<UTexture2D> UCantripHotbarComponent::RealmSymbol(ECantripRealm Realm,
	int32 Ranks) const
{
	const UEnum* RealmEnum = StaticEnum<ECantripRealm>();

	if (!RealmEnum || RealmSymbolPath.IsEmpty() || RealmSymbolPattern.IsEmpty()
		|| Realm == ECantripRealm::None)
	{
		return nullptr;
	}

	// The entry NAME, not the display name: the files were named from the enum and the
	// two only happen to agree today. A UMETA DisplayName added later for the player's
	// benefit should not silently unhook every roundel.
	FString Leaf = RealmEnum->GetNameStringByValue(static_cast<int64>(Realm));

	if (Leaf.IsEmpty())
	{
		return nullptr;
	}

	// Dotted above zero, plain at zero. Clamped rather than trusted: a rating that has
	// climbed past five would otherwise ask for a file nobody drew, and a Realm chip
	// that silently loses its roundel is a worse bug than one dot too few.
	const int32 Dots = FMath::Clamp(Ranks, 0, 5);

	FString Asset = Dots > 0 && !RealmLevelSymbolPattern.IsEmpty()
		? RealmLevelSymbolPattern.Replace(TEXT("{Rank}"), *FString::FromInt(Dots))
		: RealmSymbolPattern;

	Asset = Asset.Replace(TEXT("{Realm}"), *Leaf);

	// Package path plus object name after the dot, as SymbolFor does -- a soft
	// reference written without the object half silently resolves to nothing.
	return TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		FString::Printf(TEXT("%s/%s.%s"), *RealmSymbolPath, *Asset, *Asset)));
}

TArray<ECantripRealm> UCantripHotbarComponent::GetChosenRealms(FName CantripRow) const
{
	if (const FCantripRealmChoice* Saved = RealmChoices.Find(CantripRow))
	{
		// Filtered on the way out. AimableRealms is a setting, and settings get changed
		// -- a saved choice naming a Realm no longer offered would otherwise aim the
		// cantrip through something the player can no longer see or undo.
		TArray<ECantripRealm> Kept;

		for (const ECantripRealm Realm : Saved->Realms)
		{
			if (AimableRealms.Contains(Realm))
			{
				Kept.Add(Realm);
			}
		}

		if (Kept.Num() > 0)
		{
			return Kept;
		}
	}

	// The default: her strongest single PRIMARY Realm. See the header for why one strong
	// Realm rather than all of them -- and primary specifically, because a cantrip
	// opened on Time alone would contribute no dice at all and read as beyond her reach
	// before she had touched anything.
	ECantripRealm Best = ECantripRealm::None;
	int32 BestRating = 0;

	for (const ECantripRealm Realm : AimableRealms)
	{
		if (!UCantripResolver::IsPrimaryRealm(Realm))
		{
			continue;
		}

		const int32 Rating = GetRealmRating(Realm);

		// Strictly greater than zero, not merely the best of a bad lot. A changeling
		// with no primary Realm at all gets an empty aim and an honest "beyond her
		// reach" -- defaulting her onto a Realm she cannot use would light a chip she
		// is not allowed to press.
		if (Rating > BestRating)
		{
			BestRating = Rating;
			Best = Realm;
		}
	}

	TArray<ECantripRealm> Default;

	if (Best != ECantripRealm::None)
	{
		Default.Add(Best);
	}

	return Default;
}

bool UCantripHotbarComponent::IsRealmChosen(FName CantripRow, ECantripRealm Realm) const
{
	return GetChosenRealms(CantripRow).Contains(Realm);
}

bool UCantripHotbarComponent::ToggleRealm(FName CantripRow, ECantripRealm Realm)
{
	if (CantripRow.IsNone() || !AimableRealms.Contains(Realm))
	{
		return false;
	}

	// A Realm she has no dots in cannot be put into a casting (jhkoz, 2026-08-29).
	// Note this is stricter than the tabletop, which lets a rating-0 Realm be "cheated"
	// for an extra Glamour -- deliberately, so the chips never offer a choice the UI
	// then has to explain away. Enforced here and not only in the widget, because a
	// rule that lives only in the thing that draws it is a rule waiting to be bypassed.
	if (GetRealmRating(Realm) <= 0)
	{
		return false;
	}

	TArray<ECantripRealm> Chosen = GetChosenRealms(CantripRow);

	if (Chosen.Contains(Realm))
	{
		// The last PRIMARY one stays. Counting all Realms here would have let her strip
		// a casting down to Time and Scene, which contribute no dice -- a working aimed
		// only at a duration has nothing to aim THROUGH, and would read as beyond her
		// reach for a reason nothing on the page explains. Modifiers come off freely.
		if (UCantripResolver::IsPrimaryRealm(Realm))
		{
			int32 Primaries = 0;

			for (const ECantripRealm Held : Chosen)
			{
				Primaries += UCantripResolver::IsPrimaryRealm(Held) ? 1 : 0;
			}

			if (Primaries <= 1)
			{
				return false;
			}
		}

		Chosen.Remove(Realm);
	}
	else
	{
		Chosen.Add(Realm);
	}

	// Stored in the Realm order the chips are drawn in rather than click order, so the
	// row reads the same way every time the page is opened.
	TArray<ECantripRealm> Ordered;

	for (const ECantripRealm Candidate : AimableRealms)
	{
		if (Chosen.Contains(Candidate))
		{
			Ordered.Add(Candidate);
		}
	}

	RealmChoices.FindOrAdd(CantripRow).Realms = Ordered;

	OnRealmsChanged.Broadcast(CantripRow);
	return true;
}

void UCantripHotbarComponent::ClearChosenRealms(FName CantripRow)
{
	if (RealmChoices.Remove(CantripRow) > 0)
	{
		OnRealmsChanged.Broadcast(CantripRow);
	}
}

int32 UCantripHotbarComponent::GetArtRating(FGameplayTag ArtTag) const
{
	const UAbilitySystemComponent* ASC = GetASC();
	const FName PropertyName = CantripInternal::ArtPropertyNameFromTag(ArtTag);

	if (!ASC || PropertyName.IsNone())
	{
		return 0;
	}

	FProperty* Property =
		FindFProperty<FProperty>(UChangelingMagicSet::StaticClass(), PropertyName);

	if (!Property)
	{
		return 0;
	}

	bool bFound = false;
	const float Value = ASC->GetGameplayAttributeValue(FGameplayAttribute(Property), bFound);
	return bFound ? FMath::RoundToInt(Value) : 0;
}

TArray<FCantripArtPage> UCantripHotbarComponent::GetSpellbook() const
{
	TArray<FCantripArtPage> Pages;

	if (!CantripTable)
	{
		UE_LOG(LogCantripHotbar, Warning,
			TEXT("SPELLBOOK: no CantripTable set on %s -- the book has no catalogue to "
				 "read and will come back empty."), *GetName());
		return Pages;
	}

	// Grouped by tag as the rows are walked, rather than by asking the table once per Art.
	// The table is the only thing that knows which Arts exist at all; hard-coding the
	// eighteen here would mean a nineteenth never appears in the book.
	TMap<FGameplayTag, int32> PageForTag;

	for (const TPair<FName, uint8*>& Row : CantripTable->GetRowMap())
	{
		const FCantripSpec* Spec = reinterpret_cast<const FCantripSpec*>(Row.Value);
		if (!Spec || !Spec->ArtTag.IsValid())
		{
			continue;
		}

		int32* Existing = PageForTag.Find(Spec->ArtTag);
		if (!Existing)
		{
			FCantripArtPage Page;
			Page.ArtTag = Spec->ArtTag;
			Page.ArtName = CantripInternal::ArtPropertyNameFromTag(Spec->ArtTag);
			// The tag leaf is the fallback, not the answer: it reads "DragonsIre".
			Page.ArtDisplayName = FText::FromName(Page.ArtName);

			if (ArtTable)
			{
				static const FString Context(TEXT("Spellbook Art lookup"));

				if (const FCantripArtInfo* Info =
					ArtTable->FindRow<FCantripArtInfo>(Page.ArtName, Context, false))
				{
					if (!Info->DisplayName.IsEmpty())
					{
						Page.ArtDisplayName = Info->DisplayName;
					}

					Page.ArtDescription = Info->Description;

					// A hand-made icon wins over the composed one, when there is one.
					if (!Info->Symbol.IsNull())
					{
						Page.ArtSymbol = Info->Symbol;
					}
				}
			}
			Page.ArtRating = GetArtRating(Spec->ArtTag);

			// Composed only when the Art table did not supply one. This line used to
			// be unconditional, which made the hand-made-icon branch above dead code:
			// every roundel came from the pattern no matter what DT_Arts said.
			if (Page.ArtSymbol.IsNull())
			{
				Page.ArtSymbol = SymbolFor(ArtSymbolPattern, Page.ArtName, 0);
			}

			// Resolved once here rather than per row in a widget's Refresh. After the
			// first load this is a pointer fetch, and it means no widget ever has to
			// Load-then-Cast -- which is where a missing roundel used to abandon the
			// rest of the function without saying so.
			Page.ArtSymbolTexture = Page.ArtSymbol.LoadSynchronous();

			Existing = &PageForTag.Add(Spec->ArtTag, Pages.Add(Page));
		}

		FCantripArtPage& Page = Pages[*Existing];

		FCantripBookEntry Entry;
		Entry.Display.bFilled = true;
		Entry.Display.CantripRow = Row.Key;
		Entry.Display.DisplayName = Spec->DisplayName.IsEmpty()
			? FText::FromName(Row.Key)
			: Spec->DisplayName;
		Entry.Display.Description = Spec->Description;
		Entry.Display.Icon = Spec->Icon;
		Entry.Display.Element = Spec->Element;
		Entry.Display.ArtLevel = Spec->ArtLevel;
		Entry.Display.ArtTag = Spec->ArtTag;
		Entry.Display.ArtDisplayName = Page.ArtDisplayName;
		Entry.Display.ChosenRealms = GetChosenRealms(Entry.Display.CantripRow);
		Entry.Display.Manifestation = Spec->Manifestation;
		Entry.Display.CastTierThresholds = Spec->CastTiers.TierThresholds;
		Entry.Display.Ease = EaseFor(Entry.Display.CantripRow, Page.ArtRating);

		// Joined by row name rather than merged into the cantrip row, so reimporting the
		// book's text can never disturb the hand-tuned play data.
		if (LoreTable)
		{
			static const FString Context(TEXT("Spellbook lore lookup"));

			if (const FCantripLore* Lore =
				LoreTable->FindRow<FCantripLore>(Row.Key, Context, false))
			{
				Entry.Display.FullDescription = Lore->FullDescription;
				Entry.Display.SystemText = Lore->SystemText;
			}
		}

		// Null for a cantrip she cannot cast -- which is what stops the book handing the
		// hotbar something AssignSlot would only refuse. Note this is granted-ness, while
		// bKnown is the rating: they disagree only when the two have fallen out of step,
		// and the book should still read as the character sheet does.
		Entry.Display.AbilityClass = KnownClassForRow(Row.Key);
		Entry.bKnown = Spec->ArtLevel <= Page.ArtRating;
		Entry.bNextToLearn = Spec->ArtLevel == Page.ArtRating + 1;
		Entry.SlotIndex = Entry.Display.AbilityClass
			? FindSlotFor(Entry.Display.AbilityClass) : INDEX_NONE;
		Entry.LevelSymbol = SymbolFor(LevelSymbolPattern, Page.ArtName, Spec->ArtLevel);
		Entry.LevelSymbolTexture = Entry.LevelSymbol.LoadSynchronous();

		if (Entry.bKnown)
		{
			++Page.KnownCount;
		}

		Page.Entries.Add(Entry);
	}

	for (FCantripArtPage& Page : Pages)
	{
		Page.Entries.Sort([](const FCantripBookEntry& A, const FCantripBookEntry& B)
		{
			return A.Display.ArtLevel < B.Display.ArtLevel;
		});

	}

	// Alphabetical, because a DataTable's row order is whatever it was authored in and
	// the tabs should not reshuffle when somebody adds a row in the middle.
	Pages.Sort([](const FCantripArtPage& A, const FCantripArtPage& B)
	{
		return A.ArtName.LexicalLess(B.ArtName);
	});

	return Pages;
}

FCantripArtPage UCantripHotbarComponent::GetArtPage(FGameplayTag ArtTag) const
{
	for (const FCantripArtPage& Page : GetSpellbook())
	{
		if (Page.ArtTag == ArtTag)
		{
			return Page;
		}
	}

	return FCantripArtPage();
}

FCantripArtPage UCantripHotbarComponent::GetOpeningPage() const
{
	const TArray<FCantripArtPage> Pages = GetSpellbook();

	if (Pages.Num() == 0)
	{
		return FCantripArtPage();
	}

	// Pages already arrive sorted by name, so a strict > keeps the first of any tie
	// and the fallback for a character who has learned nothing is simply Pages[0].
	const FCantripArtPage* Best = &Pages[0];

	for (const FCantripArtPage& Page : Pages)
	{
		if (Page.ArtRating > Best->ArtRating)
		{
			Best = &Page;
		}
	}

	return *Best;
}

bool UCantripHotbarComponent::GetOpeningEntry(const FCantripArtPage& Page,
	FCantripBookEntry& OutEntry)
{
	if (Page.Entries.Num() == 0)
	{
		return false;
	}

	for (const FCantripBookEntry& Entry : Page.Entries)
	{
		if (Entry.bNextToLearn)
		{
			OutEntry = Entry;
			return true;
		}
	}

	// Every cantrip in the Art is hers already, so there is no next rung -- show the
	// first rather than nothing.
	OutEntry = Page.Entries[0];
	return true;
}

int32 UCantripHotbarComponent::GetRealmRating(ECantripRealm Realm) const
{
	const FName PropertyName = CantripInternal::RealmPropertyName(Realm);
	const UAbilitySystemComponent* ASC = GetASC();

	if (!ASC || PropertyName.IsNone())
	{
		return 0;
	}

	FProperty* Property =
		FindFProperty<FProperty>(UChangelingMagicSet::StaticClass(), PropertyName);

	if (!Property)
	{
		return 0;
	}

	bool bFound = false;
	const float Value =
		ASC->GetGameplayAttributeValue(FGameplayAttribute(Property), bFound);

	return bFound ? FMath::Clamp(FMath::RoundToInt(Value), 0, 5) : 0;
}

ECantripStanding UCantripHotbarComponent::StandingFor(const FCantripBookEntry& Entry)
{
	// Reading the bools the book already worked out, rather than recomputing from the
	// rating, so the standing cannot disagree with the entry it describes.
	if (Entry.bKnown)       { return ECantripStanding::Known; }
	if (Entry.bNextToLearn) { return ECantripStanding::NextToLearn; }
	return ECantripStanding::Locked;
}

FCantripRowStyle UCantripHotbarComponent::StyleFor(const FCantripBookEntry& Entry) const
{
	FCantripRowStyle Style;
	Style.Standing = StandingFor(Entry);

	switch (Style.Standing)
	{
	case ECantripStanding::Known:
		Style.RowTint   = Palette.RowKnown;
		Style.NameColor = Palette.NameKnown;
		Style.IconTint  = Palette.IconLit;
		break;

	case ECantripStanding::NextToLearn:
		Style.RowTint   = Palette.RowNextToLearn;
		Style.NameColor = Palette.NameNextToLearn;
		Style.IconTint  = Palette.IconLit;
		break;

	default:
		Style.RowTint   = Palette.RowLocked;
		Style.NameColor = Palette.NameLocked;
		Style.IconTint  = Palette.IconLocked;
		break;
	}

	// The chip says the most specific true thing about the row. A cantrip already on
	// the bar says which key casts it -- that outranks "known", which she can read
	// from the row's colour anyway.
	if (Entry.SlotIndex >= 0)
	{
		// The tenth slot is the 0 key, because that is where it sits on a keyboard.
		Style.ChipText      = FText::AsNumber((Entry.SlotIndex + 1) % 10);
		Style.ChipTint      = Palette.ChipBound;
		Style.ChipTextColor = Palette.ChipTextLit;
	}
	else if (Style.Standing == ECantripStanding::Known)
	{
		Style.ChipText      = NSLOCTEXT("Cantrip", "ChipKnown", "KNOWN");
		Style.ChipTint      = Palette.ChipKnown;
		Style.ChipTextColor = Palette.ChipTextLit;
	}
	else
	{
		// Unreached cantrips name the rung instead, so the page reads as a ladder.
		Style.ChipText = FText::Format(
			NSLOCTEXT("Cantrip", "ChipArtLevel", "ART {0}"),
			FText::AsNumber(Entry.Display.ArtLevel));

		const bool bNext    = Style.Standing == ECantripStanding::NextToLearn;
		Style.ChipTint      = bNext ? Palette.ChipNextToLearn : Palette.ChipLocked;
		Style.ChipTextColor = bNext ? Palette.ChipTextLit : Palette.ChipTextLocked;
	}

	return Style;
}

ECantripEase UCantripHotbarComponent::EaseFor(FName CantripRow, int32 ArtRating) const
{
	// The pool takes the LOWEST primary Realm in play, so a strong Art reached through a
	// Realm she barely knows is dragged down to that Realm. Modifier Realms extend reach
	// and never contribute dice, so they are skipped entirely.
	int32 Lowest = TNumericLimits<int32>::Max();

	for (const ECantripRealm Realm : GetChosenRealms(CantripRow))
	{
		if (!UCantripResolver::IsPrimaryRealm(Realm))
		{
			continue;
		}

		Lowest = FMath::Min(Lowest, GetRealmRating(Realm));
	}

	// No primary Realm at all, or none she has a single dot in. Not a hard casting --
	// simply nothing to aim it through, which is a different thing and worth saying so.
	if (Lowest == TNumericLimits<int32>::Max() || Lowest <= 0 || ArtRating <= 0)
	{
		return ECantripEase::BeyondReach;
	}

	const int32 Pool = FMath::Min(ArtRating, Lowest);

	if (Pool >= 4) { return ECantripEase::Effortless; }
	if (Pool == 3) { return ECantripEase::Comfortable; }
	return ECantripEase::Demanding;
}

FText UCantripHotbarComponent::ArtNameFor(FGameplayTag ArtTag) const
{
	const FName Leaf = CantripInternal::ArtPropertyNameFromTag(ArtTag);

	// The Art table when there is one, the tag's leaf when there is not. The leaf reads
	// "DragonsIre", which is a fallback rather than an answer.
	if (ArtTable)
	{
		static const FString Context(TEXT("Art name lookup"));

		if (const FCantripArtInfo* Info = ArtTable->FindRow<FCantripArtInfo>(Leaf, Context, false))
		{
			if (!Info->DisplayName.IsEmpty())
			{
				return Info->DisplayName;
			}
		}
	}

	return FText::FromName(Leaf);
}
