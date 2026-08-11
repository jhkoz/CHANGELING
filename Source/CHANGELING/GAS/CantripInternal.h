// CantripInternal.h
//
// Small helpers shared by the cantrip .cpp files.
//
// These lived as private copies in three separate translation units, which worked only
// for as long as the build kept those files out of unity compilation -- and the build
// does that based on which files are currently modified in git, so committing them was
// enough to collide every copy. Shared once here instead: no duplicates to collide, and
// no three places to fix when the ASC moves off the pawn.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "CantripTypes.h"

namespace CantripInternal
{
	/** The ASC may hang off the actor as a component or be reached through the
	 *  interface -- in a game where it moves to the PlayerState, only this changes. */
	inline UAbilitySystemComponent* ResolveASC(AActor* Avatar)
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

	/** Property name on UChangelingMagicSet backing a Realm. Spelled out rather than
	 *  derived from the enum, because DISPLAY names contain spaces and never resolve. */
	inline FName RealmPropertyName(ECantripRealm Realm)
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

	/**
	 * "Art.Wayfare" -> "Wayfare".
	 *
	 * Derived from the tag rather than kept in a parallel lookup table: a table is one
	 * more thing to forget when the nineteenth Art arrives, and it fails silently when
	 * it drifts.
	 */
	inline FName ArtPropertyNameFromTag(const FGameplayTag& ArtTag)
	{
		if (!ArtTag.IsValid())
		{
			return NAME_None;
		}

		FString Path = ArtTag.GetTagName().ToString();
		FString Leaf;
		return Path.Split(TEXT("."), nullptr, &Leaf, ESearchCase::CaseSensitive,
			ESearchDir::FromEnd) ? FName(*Leaf) : FName(*Path);
	}
}
