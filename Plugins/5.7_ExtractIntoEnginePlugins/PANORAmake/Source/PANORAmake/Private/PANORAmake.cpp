// Copyright Epic Games, Inc. All Rights Reserved.

#include "PANORAmake.h"
#include "PANORAmakeStyle.h"
#include "PANORAmakeCommands.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"
#include "EditorUtilityWidget.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "EditorUtilitySubsystem.h"

static const FName PANORAmakeTabName("PANORAmake");

#define LOCTEXT_NAMESPACE "FPANORAmakeModule"

void FPANORAmakeModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FPANORAmakeStyle::Initialize();
	FPANORAmakeStyle::ReloadTextures();

	FPANORAmakeCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FPANORAmakeCommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FPANORAmakeModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FPANORAmakeModule::RegisterMenus));
}

void FPANORAmakeModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FPANORAmakeStyle::Shutdown();

	FPANORAmakeCommands::Unregister();
}

void FPANORAmakeModule::PluginButtonClicked()
{
	// Put your "OnButtonClicked" stuff here
	const FSoftObjectPath widgetAssetPath("/PANORAmake/PanoraBuilder.PanoraBuilder");

	UObject* widgetAssetLoaded = widgetAssetPath.TryLoad();
	if (widgetAssetLoaded == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("PANORA|make Builder was not found. Please Reinstall the plugin."));
		return;
	}

	UEditorUtilityWidgetBlueprint* widget = Cast<UEditorUtilityWidgetBlueprint>(widgetAssetLoaded);
	if (widget == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("PANORA|make Builder was not found. Please Reinstall the plugin."));
		return;
	}

	UEditorUtilitySubsystem* EditorUtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();
	EditorUtilitySubsystem->SpawnAndRegisterTab(widget);
}

void FPANORAmakeModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FPANORAmakeCommands::Get().PluginAction, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FPANORAmakeCommands::Get().PluginAction));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FPANORAmakeModule, PANORAmake)