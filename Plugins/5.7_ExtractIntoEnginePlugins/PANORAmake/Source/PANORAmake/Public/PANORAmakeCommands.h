// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "PANORAmakeStyle.h"

class FPANORAmakeCommands : public TCommands<FPANORAmakeCommands>
{
public:

	FPANORAmakeCommands()
		: TCommands<FPANORAmakeCommands>(TEXT("PANORAmake"), NSLOCTEXT("Contexts", "PANORAmake", "PANORAmake Plugin"), NAME_None, FPANORAmakeStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > PluginAction;
};
