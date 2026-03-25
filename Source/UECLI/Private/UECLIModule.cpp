#include "UECLIModule.h"
#include "Server/UECLIServer.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FUECLIModule"

void FUECLIModule::StartupModule()
{
	EnsureToolsRegistered();
	UE_LOG(LogTemp, Display, TEXT("UECLI module started"));
}

void FUECLIModule::ShutdownModule()
{
	UE_LOG(LogTemp, Display, TEXT("UECLI module shut down"));
}

void FUECLIModule::EnsureToolsRegistered()
{
	UUECLIServer::EnsureToolsRegistered();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUECLIModule, UECLI) 
