// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "UndoPing.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEditorUndoRedo);

UCLASS()
class PANORAMAKE_API UUndoPing : public UEditorSubsystem
{
	GENERATED_BODY()


public:

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UPROPERTY(BlueprintAssignable)
    FOnEditorUndoRedo OnUndoRedo;

private:
    void HandleUndoRedo();

};
