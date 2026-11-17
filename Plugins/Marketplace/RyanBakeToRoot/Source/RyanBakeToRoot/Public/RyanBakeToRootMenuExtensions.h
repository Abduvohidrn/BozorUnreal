// Copyright Ryan Roberts 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UToolMenu;

/**
 * Registers the "Ryan Tools" submenu under the editor's main Tools menu.
 * To add a new tool, add one entry to the array in RegisterMenus() (in the .cpp).
 */
class FRyanBakeToRootMenuExtensions
{
public:
	static void RegisterMenus();
	static void UnregisterMenus();

private:
	static void PopulateRyanToolsMenu(UToolMenu* Menu);
	static void OpenEditorUtilityWidget(FSoftObjectPath WidgetPath);
};