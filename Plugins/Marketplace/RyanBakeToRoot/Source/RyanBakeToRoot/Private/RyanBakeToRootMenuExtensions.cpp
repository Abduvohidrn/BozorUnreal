// Copyright Ryan Roberts 2026. All Rights Reserved.

#include "RyanBakeToRootMenuExtensions.h"

#include "Editor.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "FRyanBakeToRootModule"

namespace
{
	struct FRyanToolEntry
	{
		FName Name;
		FText Label;
		FText Tooltip;
		FSoftObjectPath WidgetPath;
	};

	// Extensibility point: add one entry per future tool. Each opens an
	// Editor Utility Widget; no other code needs to change for that case.
	const TArray<FRyanToolEntry>& GetRyanTools()
	{
		static const TArray<FRyanToolEntry> Tools = {
			{
				TEXT("BakeToRoot"),
				LOCTEXT("BakeToRootLabel", "Bake To Root"),
				LOCTEXT("BakeToRootTooltip", "Open the Bake To Root editor utility widget."),
				FSoftObjectPath(TEXT("/RyanBakeToRoot/RyanBakeToRoot/Blueprints/EUW_RyanBakeToRoot.EUW_RyanBakeToRoot"))
			},
			// { TEXT("SomeOtherTool"), LOCTEXT(...), LOCTEXT(...), FSoftObjectPath(TEXT("/RyanBakeToRoot/.../EUW_SomeOtherTool.EUW_SomeOtherTool")) },
		};
		return Tools;
	}
}

void FRyanBakeToRootMenuExtensions::RegisterMenus()
{
	// RegisterStartupCallback defers registration until UToolMenus/the main
	// menu bar actually exist, regardless of this module's own load phase.
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
	{
		FToolMenuOwnerScoped OwnerScoped(TEXT("RyanBakeToRoot"));

		UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
		FToolMenuSection& Section = ToolsMenu->FindOrAddSection("RyanTools", LOCTEXT("RyanToolsSection", "Ryan Tools"));

		Section.AddSubMenu(
			"RyanTools",
			LOCTEXT("RyanToolsLabel", "Ryan Tools"),
			LOCTEXT("RyanToolsTooltip", "Custom tools by Ryan Roberts Studio"),
			FNewToolMenuDelegate::CreateStatic(&FRyanBakeToRootMenuExtensions::PopulateRyanToolsMenu)
		);
	}));
}

void FRyanBakeToRootMenuExtensions::UnregisterMenus()
{
	if (UObjectInitialized() && !IsRunningCommandlet())
	{
		UToolMenus::UnregisterOwner(TEXT("RyanBakeToRoot"));
	}
}

void FRyanBakeToRootMenuExtensions::PopulateRyanToolsMenu(UToolMenu* Menu)
{
	FToolMenuSection& Section = Menu->FindOrAddSection("RyanToolsEntries");

	for (const FRyanToolEntry& Tool : GetRyanTools())
	{
		FSoftObjectPath WidgetPath = Tool.WidgetPath;
		Section.AddMenuEntry(
			Tool.Name,
			Tool.Label,
			Tool.Tooltip,
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&FRyanBakeToRootMenuExtensions::OpenEditorUtilityWidget, WidgetPath))
		);
	}
}

void FRyanBakeToRootMenuExtensions::OpenEditorUtilityWidget(FSoftObjectPath WidgetPath)
{
	UEditorUtilityWidgetBlueprint* WidgetBlueprint = Cast<UEditorUtilityWidgetBlueprint>(WidgetPath.TryLoad());
	if (!WidgetBlueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[RyanTools] Could not load Editor Utility Widget at '%s'."), *WidgetPath.ToString());
		return;
	}

	if (UEditorUtilitySubsystem* Subsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>())
	{
		Subsystem->SpawnAndRegisterTab(WidgetBlueprint);
	}
}

#undef LOCTEXT_NAMESPACE