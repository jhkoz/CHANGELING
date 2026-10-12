// GA_SummonCantrip.h
//
// A cantrip that leaves something behind: a wisp, a swarm of flicker-flies, gremlins in
// the wiring, an effigy of someone who is not there.
//
// The distinction from a sustained cantrip is worth being exact about, because it decides
// which class a Blueprint derives from. A sustained cantrip IS the effect -- end the
// ability and the flame goes out. A summon is over the moment it finishes casting; what
// persists is an ACTOR, with its own lifetime, its own collision, and every right to
// outlive the caster's concentration. So this derives from the instant kind, not the
// sustained one, and hands the duration to the thing it made.
//
// Six of C20's ninety are this. None of them says "summon" in its System line, which is
// why they were classified by hand rather than by pattern.

#pragma once

#include "CoreMinimal.h"
#include "GA_InstantCantrip.h"
#include "GA_SummonCantrip.generated.h"

/** How the summoned actors are arranged when more than one appears. */
UENUM(BlueprintType)
enum class ECantripSummonPlacement : uint8
{
	/** All at the impact point. For a swarm that sorts itself out. */
	Stacked UMETA(DisplayName = "Stacked"),

	/** Evenly spaced on a circle around it. */
	Ring UMETA(DisplayName = "Ring"),

	/** Random points inside the radius. */
	Scattered UMETA(DisplayName = "Scattered")
};

UCLASS(Abstract, Blueprintable)
class CHANGELING_API UGA_SummonCantrip : public UGA_InstantCantrip
{
	GENERATED_BODY()

public:
	/**
	 * Dismiss everything this actor has summoned, early.
	 *
	 * Returns how many were still standing, so a caller can react to having actually
	 * banished something rather than guessing. The hook for a dispel, a Banality crisis,
	 * or the caster simply calling them back -- ordinary expiry needs nothing, since each
	 * summon carries its own lifespan.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Summon", meta = (DefaultToSelf = "Avatar"))
	static int32 DismissSummons(AActor* Avatar, TSubclassOf<UGA_SummonCantrip> CantripClass);

	/** What this casting brought into being, minus anything already gone. */
	UFUNCTION(BlueprintPure, Category = "Cantrip|Summon")
	TArray<AActor*> GetSummoned() const;

protected:
	virtual void PostResolve(const FCantripOutcome& Outcome, const FCantripSpec& Spec) override;

	/**
	 * Fires once per summoned actor, as it appears.
	 *
	 * Where a Blueprint gives the thing its orders -- a target to harry, a colour, a name.
	 * Per actor rather than once for the batch, because a swarm whose members all received
	 * the same instruction at the same instant moves like one object.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Cantrip|Summon")
	void OnSummoned(AActor* Summon, int32 Index, const FCantripOutcome& Outcome);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Summon")
	TSubclassOf<AActor> SummonClass;

	/** How many appear on a bare success, before successes are counted. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Summon",
		meta = (ClampMin = "1"))
	int32 BaseCount = 1;

	/** Added per success beyond the first. Zero for a summon that is always one thing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Summon",
		meta = (ClampMin = "0"))
	int32 CountPerSuccess = 0;

	/** Hard ceiling, whatever the roll. A wisp is a wisp; forty of them is a framerate. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Summon",
		meta = (ClampMin = "1"))
	int32 MaxCount = 12;

	// ── Lifetime ────────────────────────────────────────────────────────────

	/**
	 * Seconds it stands at one success, and what each further success buys.
	 *
	 * The book grades most summons by the roll exactly this way. Set both to zero for a
	 * summon that stays until something else removes it -- an effigy left in a room, a
	 * portal that waits.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Summon|Lifetime",
		meta = (ClampMin = "0.0"))
	float BaseLifetime = 30.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Summon|Lifetime",
		meta = (ClampMin = "0.0"))
	float LifetimePerSuccess = 30.0f;

	// ── Placement ───────────────────────────────────────────────────────────

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Summon|Placement")
	ECantripSummonPlacement Placement = ECantripSummonPlacement::Ring;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Summon|Placement",
		meta = (ClampMin = "0.0"))
	float PlacementRadius = 150.0f;

	/**
	 * Drop each summon onto whatever is beneath its spawn point.
	 *
	 * On for anything that walks; off for anything that flies or hangs in the air. A ring
	 * placed on a slope without this leaves half the summons buried and half in mid-air.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Summon|Placement")
	bool bDropToGround = true;

	/** How far down to look for that ground. */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Cantrip|Summon|Placement",
		meta = (ClampMin = "0.0", EditCondition = "bDropToGround"))
	float GroundTraceDistance = 400.0f;

	/** Face away from the caster rather than keeping the world's rotation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Summon|Placement")
	bool bFaceOutward = true;

private:
	/** Spawn point for the Index'th of Count, around the impact location. */
	FTransform PlacementFor(int32 Index, int32 Count, const FVector& Origin) const;

	/** What this instance summoned. Weak, because a summon can be destroyed by anything
	 *  from a sword to its own lifespan, and none of them tell the ability. */
	UPROPERTY() TArray<TWeakObjectPtr<AActor>> Summoned;
};
