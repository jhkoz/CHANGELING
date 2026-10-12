// ItemPickup.h
//
// An object lying in the world, waiting to be walked into.
//
// The pickup deliberately knows almost nothing about what it holds. Its job is the world
// side of the transaction -- notice someone, ask whether they will take it, go dark when
// they do -- and the item system sits behind IItemReceiver. That boundary is the point:
// what an item IS (a DataTable row, an instanced object with charges and durability, an
// actor attached to a socket) is still open, and none of those answers change the sphere,
// the bloom, or the going-dark.
//
// So this holds a row handle, which names an item under every one of those answers, and
// hands it to whatever implements the interface. Nothing here needs revisiting when the
// inventory arrives.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "UObject/Interface.h"
#include "ItemPickup.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class USphereComponent;

/**
 * Implemented by anything that can carry objects.
 *
 * Two calls rather than one, because "may I" and "here, take it" are different questions
 * and only the second one is allowed to change anything. A pickup that asked only the
 * second would have to be told after the fact whether it had been taken, which is how you
 * get an object that vanishes into a full bag.
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UItemReceiver : public UInterface
{
	GENERATED_BODY()
};

class CHANGELING_API IItemReceiver
{
	GENERATED_BODY()

public:
	/** Whether this actor would take the item. Must not change anything. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Item|Pickup")
	bool CanTakeItem(const FDataTableRowHandle& Item) const;

	/**
	 * Take it. Returns false if it was refused after all -- a bag that filled up between
	 * the two calls, an item she already has and cannot hold twice.
	 *
	 * Defaults to false, not true. An actor that has not implemented this has no
	 * inventory to put anything in, and a pickup that went dark on a silent default would
	 * delete the object rather than leave it lying there.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Item|Pickup")
	bool ReceiveItem(const FDataTableRowHandle& Item);

	// Declared so UHT does not generate its own defaults -- a BlueprintNativeEvent on an
	// interface gets a stub body unless the class provides one, and the stub is what the
	// two definitions in the .cpp would collide with.
	virtual bool CanTakeItem_Implementation(const FDataTableRowHandle& Item) const;
	virtual bool ReceiveItem_Implementation(const FDataTableRowHandle& Item);
};

/**
 * The pickup. A trigger sphere, a bloom of Niagara, and one object.
 */
UCLASS()
class CHANGELING_API AItemPickup : public AActor
{
	GENERATED_BODY()

public:
	AItemPickup();

	/**
	 * The object lying here.
	 *
	 * A row handle rather than a class or an instance, so that authoring an object is the
	 * same act as authoring a cantrip -- a row in a table -- and so this actor does not
	 * have to be rebuilt when the item system settles on how it stores state.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Pickup")
	FDataTableRowHandle Item;

	/**
	 * The bloom over the pickup.
	 *
	 * Per instance so one pickup class can stand for anything lying in the grass -- a
	 * treasure should not glow like a handful of dross, and the alternative is a
	 * Blueprint per object.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Pickup")
	TObjectPtr<UNiagaraSystem> Bloom;

	/**
	 * Whether the pickup goes inert instead of being destroyed.
	 *
	 * Inert by default so a spawner can move it and light it again. Destroying reads more
	 * simply, but throws away an actor that a garden full of respawning objects will only
	 * have to build again.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Pickup")
	bool bRecycle = true;

	/** Put a spent pickup back in play, holding something new. */
	UFUNCTION(BlueprintCallable, Category = "Item|Pickup")
	void Rearm(const FDataTableRowHandle& NewItem);

	/** Fired after the object is taken, so a spawner can move this one on. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FItemTaken,
		AItemPickup*, Pickup, AActor*, Taker);

	UPROPERTY(BlueprintAssignable, Category = "Item|Pickup")
	FItemTaken OnItemTaken;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* Other,
		UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep,
		const FHitResult& Sweep);

	/** Dark and untouchable, but still here. */
	void GoInert();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item|Pickup")
	TObjectPtr<USphereComponent> Trigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item|Pickup")
	TObjectPtr<UNiagaraComponent> Effect;
};
