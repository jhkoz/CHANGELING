// River ambience that follows the water instead of sitting at the actor origin.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "RiverAudioFollowComponent.generated.h"

class UAudioComponent;
class USplineComponent;
class USoundBase;
class USoundAttenuation;

/**
 * Keeps one looping river sound parked at the point on the water spline nearest
 * the listener, so attenuation fades with distance from the BANK rather than
 * from the water body's origin.
 *
 * The Water plugin gives each river a single "RiverAudio" component at the actor
 * origin. On a 30,000+ unit spline that means the sound is a blob at one end and
 * silent along the rest, and no attenuation shape can fix it -- the emitter is in
 * the wrong place. This moves the emitter instead.
 *
 * Add to each WaterBodyRiver actor, set RiverSound + Attenuation.
 * Deliberately has no dependency on the Water module: it looks for any
 * USplineComponent on the owner, and UWaterSplineComponent derives from it.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent),
	HideCategories = (Object, LOD, Physics, Collision, Activation))
class CHANGELING_API URiverAudioFollowComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	URiverAudioFollowComponent();

	//~ Begin UActorComponent
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent

	/** Looping river ambience. Assign StreamSoundCombined (or its own cue). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "River Audio")
	TObjectPtr<USoundBase> RiverSound;

	/**
	 * Overrides whatever the cue carries. Assign Atten_RiverBank -- with a moving
	 * emitter you want a TIGHT shape, since it is always near the water now.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "River Audio")
	TObjectPtr<USoundAttenuation> Attenuation;

	/** Seconds between repositions. 0.15 is inaudible as stepping and costs nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "River Audio",
		meta = (ClampMin = "0.02", ClampMax = "1.0"))
	float UpdateInterval = 0.15f;

	/**
	 * Stop the Water plugin's own RiverAudio component on this actor, so the
	 * river is not emitting from two places at once.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "River Audio")
	bool bSilencePluginRiverAudio = true;

	/** Skip repositioning when the listener is further than this from the spline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "River Audio",
		meta = (ClampMin = "0.0"))
	float MaxUpdateDistance = 12000.0f;

	/** Draws the emitter position and the listener link. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "River Audio|Debug")
	bool bDrawDebug = false;

	/** Current emitter position — the closest point on the spline to the listener. */
	UFUNCTION(BlueprintPure, Category = "River Audio")
	FVector GetEmitterLocation() const;

private:
	/** Listener position: audio listener if available, else the player camera. */
	bool GetListenerLocation(FVector& OutLocation) const;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> AudioComp;

	UPROPERTY(Transient)
	TObjectPtr<USplineComponent> Spline;
};
