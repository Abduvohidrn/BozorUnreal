// Copyright Ryan Roberts 2026. All Rights Reserved.

#include "RyanBakeToRoot.h"
#include "RyanBakeToRootMenuExtensions.h"

#define LOCTEXT_NAMESPACE "FRyanBakeToRootModule"

void FRyanBakeToRootModule::StartupModule()
{
	FRyanBakeToRootMenuExtensions::RegisterMenus();
}

void FRyanBakeToRootModule::ShutdownModule()
{
	FRyanBakeToRootMenuExtensions::UnregisterMenus();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FRyanBakeToRootModule, RyanBakeToRoot)