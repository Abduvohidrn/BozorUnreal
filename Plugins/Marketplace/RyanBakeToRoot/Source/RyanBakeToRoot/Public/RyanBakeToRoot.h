// Copyright Ryan Roberts 2026. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FRyanBakeToRootModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
