#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FUECLIModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static inline FUECLIModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FUECLIModule>("UECLI");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("UECLI");
	}

	static void EnsureToolsRegistered();
}; 
