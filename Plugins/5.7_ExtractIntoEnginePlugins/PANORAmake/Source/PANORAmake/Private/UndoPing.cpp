// Fill out your copyright notice in the Description page of Project Settings.

#include "UndoPing.h"
#include "Editor.h"

void UUndoPing::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    FEditorDelegates::PostUndoRedo.AddUObject(this, &UUndoPing::HandleUndoRedo);
}

void UUndoPing::Deinitialize()
{
    FEditorDelegates::PostUndoRedo.RemoveAll(this);

    Super::Deinitialize();
}

void UUndoPing::HandleUndoRedo()
{
    OnUndoRedo.Broadcast();
}