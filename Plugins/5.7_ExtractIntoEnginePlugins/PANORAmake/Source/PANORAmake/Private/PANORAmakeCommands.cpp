// Copyright Epic Games, Inc. All Rights Reserved.

#include "PANORAmakeCommands.h"

#define LOCTEXT_NAMESPACE "FPANORAmakeModule"

void FPANORAmakeCommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "PANORAmake", "Open PANORA|make Builder", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
