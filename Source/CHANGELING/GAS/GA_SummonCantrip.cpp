// GA_SummonCantrip.cpp

#include "GA_SummonCantrip.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogSummonCantrip, Log, All);

void UGA_SummonCantrip::PostResolve(const FCantripOutcome& Outcome, const FCantripSpec& Spec)
{
	// Up front, always. The parent resolves where the cantrip landed, plays the montage
	// and spawns the impact effect -- the summons need that location, so they cannot go
	// first. Ending the ability along the way is harmless: the ability is
	// InstancedPerActor, so the object outlives the activation that spawned these.
	Super::PostResolve(Outcome, Spec);

	UWorld* World = GetWorld();
	AActor* Avatar = GetAvatarActorFromActorInfo();

	if (!Outcome.bSucceeded || !SummonClass || !World || !Avatar)
	{
		return;
	}

	const FVector Origin = GetImpactLocation();

	const int32 Extra = FMath::Max(0, Outcome.Successes - 1);
	const int32 Count = FMath::Clamp(BaseCount + (CountPerSuccess * Extra), 1, MaxCount);

	// Zero on both means it stays until something removes it. Expressed by never setting
	// a lifespan rather than by a sentinel, so nothing has to remember what -1 meant.
	const float Lifetime = BaseLifetime + (LifetimePerSuccess * Extra);

	Summoned.Reset();

	for (int32 Index = 0; Index < Count; ++Index)
	{
		FActorSpawnParameters Params;
		Params.Owner = Avatar;
		Params.Instigator = Cast<APawn>(Avatar);

		// Always spawn. A summon refused for overlapping a rock is a cantrip that
		// silently did nothing, which reads as a broken key rather than as a bad roll.
		Params.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* Summon = World->SpawnActor<AActor>(
			SummonClass, PlacementFor(Index, Count, Origin), Params);

		if (!Summon)
		{
			continue;
		}

		if (Lifetime > 0.0f)
		{
			Summon->SetLifeSpan(Lifetime);
		}

		Summoned.Add(Summon);
		OnSummoned(Summon, Index, Outcome);
	}

	UE_LOG(LogSummonCantrip, Log,
		TEXT("SUMMON %s: %d x %s for %s at %s."), *GetName(), Summoned.Num(),
		*GetNameSafe(SummonClass),
		Lifetime > 0.0f ? *FString::Printf(TEXT("%.0fs"), Lifetime) : TEXT("as long as they last"),
		*Origin.ToCompactString());
}

FTransform UGA_SummonCantrip::PlacementFor(int32 Index, int32 Count,
	const FVector& Origin) const
{
	FVector Location = Origin;

	switch (Placement)
	{
	case ECantripSummonPlacement::Ring:
	{
		// Offset by half a step so an even count never puts two summons on the same axis
		// as the caster's facing, which reads as a gap rather than as a ring.
		const float Angle = (2.0f * PI * Index) / FMath::Max(1, Count);
		Location += FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * PlacementRadius;
		break;
	}
	case ECantripSummonPlacement::Scattered:
	{
		// Square-rooted so points spread evenly over the AREA. A plain random radius
		// crowds them all toward the middle, which does not look scattered.
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const float Dist = PlacementRadius * FMath::Sqrt(FMath::FRand());
		Location += FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Dist;
		break;
	}
	case ECantripSummonPlacement::Stacked:
	default:
		break;
	}

	if (bDropToGround && GroundTraceDistance > 0.0f)
	{
		if (const UWorld* World = GetWorld())
		{
			FCollisionQueryParams Params(SCENE_QUERY_STAT(SummonGround), /*bTraceComplex*/ false,
				GetAvatarActorFromActorInfo());

			FHitResult Hit;
			const FVector Start = Location + FVector(0.0f, 0.0f, GroundTraceDistance * 0.5f);
			const FVector End = Location - FVector(0.0f, 0.0f, GroundTraceDistance);

			if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
			{
				Location = Hit.ImpactPoint;
			}
		}
	}

	FRotator Rotation = FRotator::ZeroRotator;

	if (bFaceOutward)
	{
		const FVector Out = (Location - Origin).GetSafeNormal2D();

		// A summon standing exactly on the origin has no outward to face; it keeps the
		// caster's own, which is the only meaningful direction left.
		if (Out.IsNearlyZero())
		{
			if (const AActor* Avatar = GetAvatarActorFromActorInfo())
			{
				Rotation = Avatar->GetActorRotation();
			}
		}
		else
		{
			Rotation = Out.Rotation();
		}
	}

	return FTransform(Rotation, Location);
}

TArray<AActor*> UGA_SummonCantrip::GetSummoned() const
{
	TArray<AActor*> Alive;

	for (const TWeakObjectPtr<AActor>& Summon : Summoned)
	{
		if (AActor* Valid = Summon.Get())
		{
			Alive.Add(Valid);
		}
	}

	return Alive;
}

int32 UGA_SummonCantrip::DismissSummons(AActor* Avatar,
	TSubclassOf<UGA_SummonCantrip> CantripClass)
{
	const IAbilitySystemInterface* Interface = Cast<IAbilitySystemInterface>(Avatar);
	UAbilitySystemComponent* ASC = Interface ? Interface->GetAbilitySystemComponent() : nullptr;

	if (!ASC)
	{
		return 0;
	}

	int32 Banished = 0;

	// Read off the granted specs' instances rather than searching the level for actors of
	// the summon's class: another changeling's wisps are not hers to dismiss, and an
	// actor carries no back-pointer to the cantrip that made it.
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		for (UGameplayAbility* Instance : Spec.GetAbilityInstances())
		{
			UGA_SummonCantrip* Cantrip = Cast<UGA_SummonCantrip>(Instance);

			if (!Cantrip || (CantripClass && !Cantrip->IsA(CantripClass)))
			{
				continue;
			}

			for (const TWeakObjectPtr<AActor>& Summon : Cantrip->Summoned)
			{
				if (AActor* Alive = Summon.Get())
				{
					Alive->Destroy();
					++Banished;
				}
			}

			Cantrip->Summoned.Reset();
		}
	}

	return Banished;
}
