// CantripLibrary.cpp

#include "CantripLibrary.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "CantripResolver.h"
#include "ChangelingMagicSet.h"
#include "Engine/DataTable.h"
#include "GA_Cantrip.h"

namespace
{
	UAbilitySystemComponent* ResolveASC(AActor* Avatar)
	{
		if (!Avatar)
		{
			return nullptr;
		}

		if (UAbilitySystemComponent* Found = Avatar->FindComponentByClass<UAbilitySystemComponent>())
		{
			return Found;
		}

		const IAbilitySystemInterface* Interface = Cast<IAbilitySystemInterface>(Avatar);
		return Interface ? Interface->GetAbilitySystemComponent() : nullptr;
	}

	/** Property name on UChangelingMagicSet backing a Realm. Matches GA_Cantrip's
	 *  switch deliberately -- enum DISPLAY names contain spaces and never resolve. */
	FName RealmPropertyName(ECantripRealm Realm)
	{
		switch (Realm)
		{
		case ECantripRealm::Fae:    return TEXT("Fae");
		case ECantripRealm::Actor:  return TEXT("Actor");
		case ECantripRealm::Nature: return TEXT("Nature");
		case ECantripRealm::Prop:   return TEXT("Prop");
		case ECantripRealm::Time:   return TEXT("Time");
		case ECantripRealm::Scene:  return TEXT("Scene");
		default:                    return NAME_None;
		}
	}

	/** "Art.Wayfare" -> the Wayfare attribute's current value, or 0. */
	int32 ReadArtRating(const UAbilitySystemComponent* ASC, const FGameplayTag& ArtTag)
	{
		if (!ASC || !ArtTag.IsValid())
		{
			return 0;
		}

		FString Path = ArtTag.GetTagName().ToString();
		FString Leaf;
		if (!Path.Split(TEXT("."), nullptr, &Leaf, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
		{
			Leaf = Path;
		}

		FProperty* Property = FindFProperty<FProperty>(UChangelingMagicSet::StaticClass(), *Leaf);
		if (!Property)
		{
			return 0;
		}

		bool bFound = false;
		const float Value = ASC->GetGameplayAttributeValue(FGameplayAttribute(Property), bFound);
		return bFound ? FMath::RoundToInt(Value) : 0;
	}
}

TArray<FCantripEntry> UCantripLibrary::GetGrantedCantrips(AActor* Avatar)
{
	TArray<FCantripEntry> Entries;

	const UAbilitySystemComponent* ASC = ResolveASC(Avatar);
	if (!ASC)
	{
		return Entries;
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		const UGA_Cantrip* Cantrip = Cast<UGA_Cantrip>(Spec.Ability);
		if (!Cantrip)
		{
			// Not every granted ability is a cantrip; skip anything else quietly.
			continue;
		}

		const FCantripSpec* Row = Cantrip->GetCantripSpec();
		if (!Row)
		{
			// A misconfigured row would produce a nameless slot that looks like a
			// cantrip you own and cannot cast. Better absent than blank.
			continue;
		}

		FCantripEntry& Entry = Entries.AddDefaulted_GetRef();
		Entry.AbilityClass = Spec.Ability->GetClass();
		Entry.Spec         = *Row;
		Entry.RowName      = Cantrip->GetCantripRowName();
		Entry.bActive      = Spec.IsActive();
		Entry.ArtRating    = ReadArtRating(ASC, Row->ArtTag);
		Entry.RequiredArtLevel = Row->ArtLevel;
		Entry.bCastable    = (Row->ArtLevel <= Entry.ArtRating);
		Entry.bPreview     = !Entry.bCastable;
	}

	return Entries;
}

TArray<FCantripEntry> UCantripLibrary::GetKnownCantrips(AActor* Avatar,
	UDataTable* CantripTable, int32 PreviewLevels)
{
	TArray<FCantripEntry> Entries;

	const UAbilitySystemComponent* ASC = ResolveASC(Avatar);
	if (!ASC || !CantripTable)
	{
		return Entries;
	}

	// Row name -> the ability class that implements it, for the rows that HAVE been
	// granted. Preview rows have no ability yet and come back with a null class,
	// which is correct: there is nothing to activate.
	TMap<FName, TSubclassOf<UGameplayAbility>> Granted;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (const UGA_Cantrip* Cantrip = Cast<UGA_Cantrip>(Spec.Ability))
		{
			Granted.Add(Cantrip->GetCantripRowName(), Spec.Ability->GetClass());
		}
	}

	const int32 Preview = FMath::Max(0, PreviewLevels);

	CantripTable->ForeachRow<FCantripSpec>(TEXT("KnownCantrips"),
		[&](const FName& RowName, const FCantripSpec& Row)
		{
			const int32 Rating = ReadArtRating(ASC, Row.ArtTag);

			// Never studied: contributes nothing. Without this every character would
			// see the first cantrip of all eighteen Arts on day one.
			if (Rating <= 0)
			{
				return;
			}

			if (Row.ArtLevel > Rating + Preview)
			{
				return;
			}

			FCantripEntry& Entry = Entries.AddDefaulted_GetRef();
			Entry.Spec             = Row;
			Entry.RowName          = RowName;
			Entry.ArtRating        = Rating;
			Entry.RequiredArtLevel = Row.ArtLevel;
			Entry.bCastable        = (Row.ArtLevel <= Rating);
			Entry.bPreview         = !Entry.bCastable;

			if (const TSubclassOf<UGameplayAbility>* Found = Granted.Find(RowName))
			{
				Entry.AbilityClass = *Found;
			}
		});

	// Art first, then level: a spellbook reads as progressions, not a flat list.
	Entries.Sort([](const FCantripEntry& A, const FCantripEntry& B)
	{
		if (A.Spec.ArtTag != B.Spec.ArtTag)
		{
			return A.Spec.ArtTag.ToString() < B.Spec.ArtTag.ToString();
		}
		return A.RequiredArtLevel < B.RequiredArtLevel;
	});

	return Entries;
}

TArray<FCantripEntry> UCantripLibrary::GetGrantedCantripsByElement(AActor* Avatar,
	ECantripElement Element)
{
	TArray<FCantripEntry> All = GetGrantedCantrips(Avatar);
	All.RemoveAll([Element](const FCantripEntry& Entry)
	{
		return Entry.Spec.Element != Element;
	});
	return All;
}

int32 UCantripLibrary::GetCantripPoolSize(AActor* Avatar, TSubclassOf<UGA_Cantrip> CantripClass)
{
	const UAbilitySystemComponent* ASC = ResolveASC(Avatar);
	if (!ASC || !CantripClass)
	{
		return 0;
	}

	const UGA_Cantrip* Defaults = CantripClass->GetDefaultObject<UGA_Cantrip>();
	const FCantripSpec* Row = Defaults ? Defaults->GetCantripSpec() : nullptr;
	if (!Row)
	{
		return 0;
	}

	const int32 ArtRating = ReadArtRating(ASC, Row->ArtTag);

	// Same rule the resolver uses: the LOWEST primary Realm in play, because a strong
	// Art reaching through a Realm you barely know is dragged down to that Realm.
	int32 Lowest = TNumericLimits<int32>::Max();
	for (const ECantripRealm Realm : Defaults->GetRealmsUsed())
	{
		if (!UCantripResolver::IsPrimaryRealm(Realm))
		{
			continue;
		}

		FProperty* Property = FindFProperty<FProperty>(
			UChangelingMagicSet::StaticClass(), RealmPropertyName(Realm));

		bool bFound = false;
		const float Value = Property
			? ASC->GetGameplayAttributeValue(FGameplayAttribute(Property), bFound)
			: 0.0f;

		Lowest = FMath::Min(Lowest, bFound ? FMath::RoundToInt(Value) : 0);
	}

	if (Lowest == TNumericLimits<int32>::Max())
	{
		return 0;
	}

	return FMath::Max(0, ArtRating + Lowest);
}
