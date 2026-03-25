#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class FUECLIToolRegistry;

/**
 * Handler class for asset-related UECLI commands.
 * Handles asset management operations like listing, finding, importing,
 * deleting, renaming, and moving assets in the content browser.
 */
class UECLI_API FUECLIAssetCommands
{
public:
    FUECLIAssetCommands();
    static void RegisterTools(FUECLIToolRegistry& Registry);

    // Handle asset commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    /**
     * List assets in a specified folder
     * @param Params - Must include:
     *                "path" - Content folder path (e.g., "/Game/Blueprints")
     *                "recursive" - Optional, whether to search recursively (default: false)
     *                "class_filter" - Optional, filter by asset class (e.g., "Blueprint", "Material")
     * @return JSON array of asset information
     */
    TSharedPtr<FJsonObject> HandleListAssets(const TSharedPtr<FJsonObject>& Params);

    /**
     * Find assets by name pattern or class type
     * @param Params - Must include:
     *                "pattern" - Name pattern to search for (supports wildcards)
     *                "class_filter" - Optional, filter by asset class
     *                "path" - Optional, folder to search in (default: "/Game")
     * @return JSON array of matching assets
     */
    TSharedPtr<FJsonObject> HandleFindAssets(const TSharedPtr<FJsonObject>& Params);

    /**
     * Get detailed information about a specific asset
     * @param Params - Must include:
     *                "path" - Full asset path
     * @return JSON object with asset details
     */
    TSharedPtr<FJsonObject> HandleGetAssetInfo(const TSharedPtr<FJsonObject>& Params);

    /**
     * Delete an asset from the project
     * @param Params - Must include:
     *                "path" - Full asset path to delete
     *                "force" - Optional, force delete even if referenced (default: false)
     * @return JSON response with deletion result
     */
    TSharedPtr<FJsonObject> HandleDeleteAsset(const TSharedPtr<FJsonObject>& Params);

    /**
     * Rename an asset
     * @param Params - Must include:
     *                "path" - Current asset path
     *                "new_name" - New name for the asset
     * @return JSON response with the new asset path
     */
    TSharedPtr<FJsonObject> HandleRenameAsset(const TSharedPtr<FJsonObject>& Params);

    /**
     * Duplicate an asset
     * @param Params - Must include:
     *                "source_path" - Source asset path
     *                "dest_path" - Destination path (including new name)
     * @return JSON response with the duplicated asset path
     */
    TSharedPtr<FJsonObject> HandleDuplicateAsset(const TSharedPtr<FJsonObject>& Params);

    /**
     * Move an asset to a different folder
     * @param Params - Must include:
     *                "source_path" - Current asset path
     *                "dest_folder" - Destination folder path
     * @return JSON response with the new asset path
     */
    TSharedPtr<FJsonObject> HandleMoveAsset(const TSharedPtr<FJsonObject>& Params);

    /**
     * Create a content folder
     * @param Params - Must include:
     *                "path" - Folder path to create
     * @return JSON response with creation result
     */
    TSharedPtr<FJsonObject> HandleCreateFolder(const TSharedPtr<FJsonObject>& Params);

    /**
     * Save a specific asset
     * @param Params - Must include:
     *                "path" - Asset path to save
     * @return JSON response with save result
     */
    TSharedPtr<FJsonObject> HandleSaveAsset(const TSharedPtr<FJsonObject>& Params);

    /**
     * Save all dirty (modified) assets
     * @param Params - Optional:
     *                "prompt_user" - Whether to prompt user for confirmation (default: false)
     * @return JSON response with number of assets saved
     */
    TSharedPtr<FJsonObject> HandleSaveAllAssets(const TSharedPtr<FJsonObject>& Params);

    /**
     * Import an asset from a file path
     * @param Params - Must include:
     *                "source_path" - File system path to import from
     *                "dest_path" - Content folder destination
     *                "asset_name" - Optional name for the imported asset
     * @return JSON response with imported asset path
     */
    TSharedPtr<FJsonObject> HandleImportAsset(const TSharedPtr<FJsonObject>& Params);

    /**
     * Get assigned materials for a mesh asset
     * @param Params - Must include:
     *                "path" - Mesh asset path
     * @return JSON response with material slots
     */
    TSharedPtr<FJsonObject> HandleGetMeshMaterials(const TSharedPtr<FJsonObject>& Params);

    // Helper functions
    TSharedPtr<FJsonObject> AssetDataToJson(const struct FAssetData& AssetData, bool bDetailed = false);
};
