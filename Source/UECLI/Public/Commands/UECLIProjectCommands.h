#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class FUECLIToolRegistry;

/**
 * Handler class for project-wide UECLI commands.
 * Handles project settings, input mappings, and general project configuration.
 */
class UECLI_API FUECLIProjectCommands
{
public:
    FUECLIProjectCommands();
    static void RegisterTools(FUECLIToolRegistry& Registry);

    // Handle project commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Input mapping commands
    TSharedPtr<FJsonObject> HandleCreateInputMapping(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetInputMappings(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteInputMapping(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateAxisMapping(const TSharedPtr<FJsonObject>& Params);

    // Project info commands
    TSharedPtr<FJsonObject> HandleGetProjectInfo(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetProjectSettings(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetProjectSetting(const TSharedPtr<FJsonObject>& Params);

    // Build/packaging commands
    TSharedPtr<FJsonObject> HandleGetBuildConfiguration(const TSharedPtr<FJsonObject>& Params);
}; 
