#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class FUECLIToolRegistry;

/**
 * Handler class for Editor-related UECLI commands.
 * Handles viewport control, actor manipulation, and level management
 */
class UECLI_API FUECLIEditorCommands
{
public:
    FUECLIEditorCommands();
    static void RegisterTools(FUECLIToolRegistry& Registry);

    // Handle editor commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Actor manipulation commands
    TSharedPtr<FJsonObject> HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSpawnActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetActorProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetActorProperty(const TSharedPtr<FJsonObject>& Params);

    // New actor commands
    TSharedPtr<FJsonObject> HandleSelectActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetSelectedActors(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDuplicateActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRenameActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetActorComponents(const TSharedPtr<FJsonObject>& Params);

    // Blueprint actor spawning
    TSharedPtr<FJsonObject> HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params);

    // Editor viewport commands
    TSharedPtr<FJsonObject> HandleFocusViewport(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleTakeScreenshot(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetViewportCamera(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetViewportCamera(const TSharedPtr<FJsonObject>& Params);

    // Console and editor control commands
    TSharedPtr<FJsonObject> HandleExecuteConsoleCommand(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandlePlayInEditor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleStopPlayInEditor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetEditorState(const TSharedPtr<FJsonObject>& Params);

    // Level management commands
    TSharedPtr<FJsonObject> HandleGetCurrentLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleOpenLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSaveCurrentLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateNewLevel(const TSharedPtr<FJsonObject>& Params);

    // PostProcessVolume commands
    TSharedPtr<FJsonObject> HandleSetPPVMaterial(const TSharedPtr<FJsonObject>& Params);

    // Helper to find actor by name
    AActor* FindActorByName(const FString& ActorName);
}; 
