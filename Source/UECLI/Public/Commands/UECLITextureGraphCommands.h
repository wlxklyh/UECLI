#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class FUECLIToolRegistry;

/**
 * Handler class for TextureGraph-related UECLI commands.
 * Handles programmatic creation and manipulation of Texture Graph assets.
 */
class UECLI_API FUECLITextureGraphCommands
{
public:
    FUECLITextureGraphCommands();
    static void RegisterTools(FUECLIToolRegistry& Registry);

    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    TSharedPtr<FJsonObject> HandleCreateTextureGraph(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddTGNode(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleConnectTGNodes(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetTGNodeProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetTGNodes(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetTGNodeInfo(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleListTGNodeTypes(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleExportTextureGraph(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSaveTextureGraph(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleApplyTGPatch(const TSharedPtr<FJsonObject>& Params);
};
