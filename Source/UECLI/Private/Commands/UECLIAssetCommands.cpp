#include "Commands/UECLIAssetCommands.h"
#include "Commands/UECLICommonUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "EditorAssetLibrary.h"
#include "FileHelpers.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Editor.h"
#include "Factories/Factory.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "ToolRegistry/UECLIToolRegistry.h"

namespace
{
	TSharedRef<FUECLIAssetCommands> GetAssetCommands()
	{
		static TSharedRef<FUECLIAssetCommands> Instance = MakeShared<FUECLIAssetCommands>();
		return Instance;
	}

	void RegisterAssetCommand(FUECLIToolRegistry& Registry, const TCHAR* PublicName, const TCHAR* InternalName = nullptr)
	{
		const TSharedRef<FUECLIAssetCommands> Handler = GetAssetCommands();
		const FString RoutedName = InternalName != nullptr ? InternalName : PublicName;
		Registry.Register(
			FUECLIToolSchema(PublicName, TEXT("Asset"), FString()),
			[Handler, RoutedName](const TSharedPtr<FJsonObject>& Params)
			{
				return Handler->HandleCommand(RoutedName, Params);
			});
	}

	void RegisterUnsupportedAssetCommand(FUECLIToolRegistry& Registry, const TCHAR* Name)
	{
		const FString ErrorMessage = FString::Printf(TEXT("%s is intentionally disabled in UECLI"), Name);
		Registry.Register(
			FUECLIToolSchema(Name, TEXT("Asset"), ErrorMessage),
			[ErrorMessage](const TSharedPtr<FJsonObject>&)
			{
				return FUECLICommonUtils::CreateErrorResponse(ErrorMessage);
			});
	}
}

FUECLIAssetCommands::FUECLIAssetCommands()
{
}

void FUECLIAssetCommands::RegisterTools(FUECLIToolRegistry& Registry)
{
	RegisterAssetCommand(Registry, TEXT("list_assets"));
	RegisterAssetCommand(Registry, TEXT("find_assets"));
	RegisterAssetCommand(Registry, TEXT("get_asset_info"));
	RegisterAssetCommand(Registry, TEXT("rename_asset"));
	RegisterAssetCommand(Registry, TEXT("duplicate_asset"));
	RegisterAssetCommand(Registry, TEXT("move_asset"));
	RegisterAssetCommand(Registry, TEXT("create_folder"));
	RegisterAssetCommand(Registry, TEXT("save_asset"));
	RegisterAssetCommand(Registry, TEXT("import_asset"));
	RegisterAssetCommand(Registry, TEXT("get_mesh_materials"));
	RegisterUnsupportedAssetCommand(Registry, TEXT("delete_asset"));
	RegisterUnsupportedAssetCommand(Registry, TEXT("save_all_assets"));
}

TSharedPtr<FJsonObject> FUECLIAssetCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("list_assets"))
    {
        return HandleListAssets(Params);
    }
    else if (CommandType == TEXT("find_assets"))
    {
        return HandleFindAssets(Params);
    }
    else if (CommandType == TEXT("get_asset_info"))
    {
        return HandleGetAssetInfo(Params);
    }
    else if (CommandType == TEXT("delete_asset"))
    {
        return HandleDeleteAsset(Params);
    }
    else if (CommandType == TEXT("rename_asset"))
    {
        return HandleRenameAsset(Params);
    }
    else if (CommandType == TEXT("duplicate_asset"))
    {
        return HandleDuplicateAsset(Params);
    }
    else if (CommandType == TEXT("move_asset"))
    {
        return HandleMoveAsset(Params);
    }
    else if (CommandType == TEXT("create_folder"))
    {
        return HandleCreateFolder(Params);
    }
    else if (CommandType == TEXT("save_asset"))
    {
        return HandleSaveAsset(Params);
    }
    else if (CommandType == TEXT("save_all_assets"))
    {
        return HandleSaveAllAssets(Params);
    }
    else if (CommandType == TEXT("import_asset"))
    {
        return HandleImportAsset(Params);
    }
    else if (CommandType == TEXT("get_mesh_materials"))
    {
        return HandleGetMeshMaterials(Params);
    }
    
    return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown asset command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUECLIAssetCommands::HandleListAssets(const TSharedPtr<FJsonObject>& Params)
{
    FString FolderPath;
    if (!Params->TryGetStringField(TEXT("path"), FolderPath))
    {
        FolderPath = TEXT("/Game");
    }

    bool bRecursive = false;
    Params->TryGetBoolField(TEXT("recursive"), bRecursive);

    FString ClassFilter;
    Params->TryGetStringField(TEXT("class_filter"), ClassFilter);

    // Get asset registry
    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

    // Build filter
    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*FolderPath));
    Filter.bRecursivePaths = bRecursive;

    // Add class filter if specified
    if (!ClassFilter.IsEmpty())
    {
        // Try to find the class
        UClass* FilterClass = FindObject<UClass>(nullptr, *ClassFilter);
        if (!FilterClass)
        {
            // Try with common prefixes
            FilterClass = FindObject<UClass>(nullptr, *(TEXT("U") + ClassFilter));
        }
        if (FilterClass)
        {
            Filter.ClassPaths.Add(FilterClass->GetClassPathName());
        }
    }

    // Get assets
    TArray<FAssetData> AssetList;
    AssetRegistry.GetAssets(Filter, AssetList);

    // Build response
    TArray<TSharedPtr<FJsonValue>> AssetsArray;
    for (const FAssetData& AssetData : AssetList)
    {
        AssetsArray.Add(MakeShared<FJsonValueObject>(AssetDataToJson(AssetData, false)));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("assets"), AssetsArray);
    ResultObj->SetNumberField(TEXT("count"), AssetList.Num());
    ResultObj->SetStringField(TEXT("path"), FolderPath);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIAssetCommands::HandleFindAssets(const TSharedPtr<FJsonObject>& Params)
{
    FString Pattern;
    if (!Params->TryGetStringField(TEXT("pattern"), Pattern))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'pattern' parameter"));
    }

    FString FolderPath;
    if (!Params->TryGetStringField(TEXT("path"), FolderPath))
    {
        FolderPath = TEXT("/Game");
    }

    FString ClassFilter;
    Params->TryGetStringField(TEXT("class_filter"), ClassFilter);

    // Get asset registry
    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

    // Build filter
    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*FolderPath));
    Filter.bRecursivePaths = true;

    // Add class filter if specified
    if (!ClassFilter.IsEmpty())
    {
        UClass* FilterClass = FindObject<UClass>(nullptr, *ClassFilter);
        if (!FilterClass)
        {
            FilterClass = FindObject<UClass>(nullptr, *(TEXT("U") + ClassFilter));
        }
        if (FilterClass)
        {
            Filter.ClassPaths.Add(FilterClass->GetClassPathName());
        }
    }

    // Get all assets
    TArray<FAssetData> AllAssets;
    AssetRegistry.GetAssets(Filter, AllAssets);

    // Filter by pattern
    TArray<TSharedPtr<FJsonValue>> MatchingAssets;
    for (const FAssetData& AssetData : AllAssets)
    {
        FString AssetName = AssetData.AssetName.ToString();
        if (AssetName.Contains(Pattern) || AssetName.MatchesWildcard(Pattern))
        {
            MatchingAssets.Add(MakeShared<FJsonValueObject>(AssetDataToJson(AssetData, false)));
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("assets"), MatchingAssets);
    ResultObj->SetNumberField(TEXT("count"), MatchingAssets.Num());
    ResultObj->SetStringField(TEXT("pattern"), Pattern);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIAssetCommands::HandleGetAssetInfo(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("path"), AssetPath))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'path' parameter"));
    }

    // Check if asset exists
    if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
    }

    // Get asset registry
    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
    
    FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(AssetPath));
    if (!AssetData.IsValid())
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Could not get asset data for: %s"), *AssetPath));
    }

    return AssetDataToJson(AssetData, true);
}

TSharedPtr<FJsonObject> FUECLIAssetCommands::HandleDeleteAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("path"), AssetPath))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'path' parameter"));
    }

    bool bForce = false;
    Params->TryGetBoolField(TEXT("force"), bForce);

    // Check if asset exists
    if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
    }

    // Check for references if not forcing
    if (!bForce)
    {
        TArray<FName> Referencers;
        IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
        AssetRegistry.GetReferencers(FName(*AssetPath), Referencers);
        
        if (Referencers.Num() > 0)
        {
            TSharedPtr<FJsonObject> ErrorObj = FUECLICommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Asset '%s' is referenced by %d other assets. Use 'force': true to delete anyway."), 
                *AssetPath, Referencers.Num()));
            
            TArray<TSharedPtr<FJsonValue>> RefsArray;
            for (const FName& Ref : Referencers)
            {
                RefsArray.Add(MakeShared<FJsonValueString>(Ref.ToString()));
            }
            ErrorObj->SetArrayField(TEXT("referencers"), RefsArray);
            return ErrorObj;
        }
    }

    // Delete the asset
    if (UEditorAssetLibrary::DeleteAsset(AssetPath))
    {
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("deleted_path"), AssetPath);
        ResultObj->SetBoolField(TEXT("success"), true);
        return ResultObj;
    }

    return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to delete asset: %s"), *AssetPath));
}

TSharedPtr<FJsonObject> FUECLIAssetCommands::HandleRenameAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("path"), AssetPath))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'path' parameter"));
    }

    FString NewName;
    if (!Params->TryGetStringField(TEXT("new_name"), NewName))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'new_name' parameter"));
    }

    // Check if asset exists
    if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
    }

    // Calculate new path
    FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
    FString NewPath = PackagePath / NewName;

    // Check if destination already exists
    if (UEditorAssetLibrary::DoesAssetExist(NewPath))
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Asset already exists at: %s"), *NewPath));
    }

    // Rename the asset
    if (UEditorAssetLibrary::RenameAsset(AssetPath, NewPath))
    {
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("old_path"), AssetPath);
        ResultObj->SetStringField(TEXT("new_path"), NewPath);
        ResultObj->SetStringField(TEXT("new_name"), NewName);
        ResultObj->SetBoolField(TEXT("success"), true);
        return ResultObj;
    }

    return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to rename asset: %s"), *AssetPath));
}

TSharedPtr<FJsonObject> FUECLIAssetCommands::HandleDuplicateAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString SourcePath;
    if (!Params->TryGetStringField(TEXT("source_path"), SourcePath))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'source_path' parameter"));
    }

    FString DestPath;
    if (!Params->TryGetStringField(TEXT("dest_path"), DestPath))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'dest_path' parameter"));
    }

    // Check if source asset exists
    if (!UEditorAssetLibrary::DoesAssetExist(SourcePath))
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Source asset not found: %s"), *SourcePath));
    }

    // Check if destination already exists
    if (UEditorAssetLibrary::DoesAssetExist(DestPath))
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Destination asset already exists: %s"), *DestPath));
    }

    // Duplicate the asset
    if (UEditorAssetLibrary::DuplicateAsset(SourcePath, DestPath))
    {
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("source_path"), SourcePath);
        ResultObj->SetStringField(TEXT("dest_path"), DestPath);
        ResultObj->SetBoolField(TEXT("success"), true);
        return ResultObj;
    }

    return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to duplicate asset: %s"), *SourcePath));
}

TSharedPtr<FJsonObject> FUECLIAssetCommands::HandleMoveAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString SourcePath;
    if (!Params->TryGetStringField(TEXT("source_path"), SourcePath))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'source_path' parameter"));
    }

    FString DestFolder;
    if (!Params->TryGetStringField(TEXT("dest_folder"), DestFolder))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'dest_folder' parameter"));
    }

    // Check if source asset exists
    if (!UEditorAssetLibrary::DoesAssetExist(SourcePath))
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Source asset not found: %s"), *SourcePath));
    }

    // Calculate new path
    FString AssetName = FPackageName::GetShortName(SourcePath);
    FString NewPath = DestFolder / AssetName;

    // Check if destination already exists
    if (UEditorAssetLibrary::DoesAssetExist(NewPath))
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Asset already exists at destination: %s"), *NewPath));
    }

    // Move the asset
    if (UEditorAssetLibrary::RenameAsset(SourcePath, NewPath))
    {
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("source_path"), SourcePath);
        ResultObj->SetStringField(TEXT("dest_path"), NewPath);
        ResultObj->SetBoolField(TEXT("success"), true);
        return ResultObj;
    }

    return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to move asset: %s"), *SourcePath));
}

TSharedPtr<FJsonObject> FUECLIAssetCommands::HandleCreateFolder(const TSharedPtr<FJsonObject>& Params)
{
    FString FolderPath;
    if (!Params->TryGetStringField(TEXT("path"), FolderPath))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'path' parameter"));
    }

    // Create the folder
    if (UEditorAssetLibrary::MakeDirectory(FolderPath))
    {
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("path"), FolderPath);
        ResultObj->SetBoolField(TEXT("success"), true);
        return ResultObj;
    }

    return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to create folder: %s"), *FolderPath));
}

TSharedPtr<FJsonObject> FUECLIAssetCommands::HandleSaveAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("path"), AssetPath))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'path' parameter"));
    }

    // Check if asset exists
    if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
    }

    // Save the asset
    if (UEditorAssetLibrary::SaveAsset(AssetPath))
    {
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("path"), AssetPath);
        ResultObj->SetBoolField(TEXT("success"), true);
        return ResultObj;
    }

    return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to save asset: %s"), *AssetPath));
}

TSharedPtr<FJsonObject> FUECLIAssetCommands::HandleSaveAllAssets(const TSharedPtr<FJsonObject>& Params)
{
    bool bPromptUser = false;
    Params->TryGetBoolField(TEXT("prompt_user"), bPromptUser);

    // Save all dirty packages
    bool bResult = FEditorFileUtils::SaveDirtyPackages(
        bPromptUser,  // bPromptUserToSave
        true,         // bSaveMapPackages
        true,         // bSaveContentPackages
        false,        // bFastSave
        false,        // bNotifyNoPackagesSaved
        false         // bCanBeDeclined
    );

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), bResult);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIAssetCommands::HandleImportAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString SourcePath;
    if (!Params->TryGetStringField(TEXT("source_path"), SourcePath))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'source_path' parameter"));
    }

    FString DestPath;
    if (!Params->TryGetStringField(TEXT("dest_path"), DestPath))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'dest_path' parameter"));
    }

    // Check if source file exists
    if (!FPaths::FileExists(SourcePath))
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Source file not found: %s"), *SourcePath));
    }

    // Get asset tools
    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
    IAssetTools& AssetTools = AssetToolsModule.Get();

    // Import the asset
    TArray<FString> FilesToImport;
    FilesToImport.Add(SourcePath);

    TArray<UObject*> ImportedAssets = AssetTools.ImportAssets(FilesToImport, DestPath);

    if (ImportedAssets.Num() > 0 && ImportedAssets[0] != nullptr)
    {
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("source_path"), SourcePath);
        ResultObj->SetStringField(TEXT("asset_path"), ImportedAssets[0]->GetPathName());
        ResultObj->SetStringField(TEXT("asset_name"), ImportedAssets[0]->GetName());
        ResultObj->SetStringField(TEXT("asset_class"), ImportedAssets[0]->GetClass()->GetName());
        ResultObj->SetBoolField(TEXT("success"), true);
        return ResultObj;
    }

    return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to import asset from: %s"), *SourcePath));
}

TSharedPtr<FJsonObject> FUECLIAssetCommands::HandleGetMeshMaterials(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("path"), AssetPath))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'path' parameter"));
    }

    UObject* AssetObject = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!AssetObject)
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
    }

    TArray<TSharedPtr<FJsonValue>> MaterialArray;
    FString MeshClassName = AssetObject->GetClass()->GetName();

    if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(AssetObject))
    {
        const TArray<FStaticMaterial>& StaticMaterials = StaticMesh->GetStaticMaterials();
        for (int32 Index = 0; Index < StaticMaterials.Num(); ++Index)
        {
            const FStaticMaterial& MaterialSlot = StaticMaterials[Index];
            TSharedPtr<FJsonObject> MaterialObj = MakeShared<FJsonObject>();
            MaterialObj->SetNumberField(TEXT("index"), Index);
            MaterialObj->SetStringField(TEXT("slot_name"), MaterialSlot.MaterialSlotName.ToString());
            MaterialObj->SetStringField(TEXT("imported_slot_name"), MaterialSlot.ImportedMaterialSlotName.ToString());
            MaterialObj->SetStringField(TEXT("material_path"), MaterialSlot.MaterialInterface ? MaterialSlot.MaterialInterface->GetPathName() : FString());
            MaterialObj->SetStringField(TEXT("material_name"), MaterialSlot.MaterialInterface ? MaterialSlot.MaterialInterface->GetName() : FString());
            MaterialArray.Add(MakeShared<FJsonValueObject>(MaterialObj));
        }
    }
    else if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(AssetObject))
    {
        const TArray<FSkeletalMaterial>& SkeletalMaterials = SkeletalMesh->GetMaterials();
        for (int32 Index = 0; Index < SkeletalMaterials.Num(); ++Index)
        {
            const FSkeletalMaterial& MaterialSlot = SkeletalMaterials[Index];
            TSharedPtr<FJsonObject> MaterialObj = MakeShared<FJsonObject>();
            MaterialObj->SetNumberField(TEXT("index"), Index);
            MaterialObj->SetStringField(TEXT("slot_name"), MaterialSlot.MaterialSlotName.ToString());
            MaterialObj->SetStringField(TEXT("imported_slot_name"), MaterialSlot.ImportedMaterialSlotName.ToString());
            MaterialObj->SetStringField(TEXT("material_path"), MaterialSlot.MaterialInterface ? MaterialSlot.MaterialInterface->GetPathName() : FString());
            MaterialObj->SetStringField(TEXT("material_name"), MaterialSlot.MaterialInterface ? MaterialSlot.MaterialInterface->GetName() : FString());
            MaterialArray.Add(MakeShared<FJsonValueObject>(MaterialObj));
        }
    }
    else
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Asset is not a supported mesh type: %s"), *MeshClassName));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("path"), AssetPath);
    ResultObj->SetStringField(TEXT("mesh_class"), MeshClassName);
    ResultObj->SetArrayField(TEXT("materials"), MaterialArray);
    ResultObj->SetNumberField(TEXT("count"), MaterialArray.Num());
    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIAssetCommands::AssetDataToJson(const FAssetData& AssetData, bool bDetailed)
{
    TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
    
    AssetObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
    AssetObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
    AssetObj->SetStringField(TEXT("package_path"), AssetData.PackagePath.ToString());
    AssetObj->SetStringField(TEXT("class"), AssetData.AssetClassPath.GetAssetName().ToString());

    if (bDetailed)
    {
        // Add more detailed info
        AssetObj->SetStringField(TEXT("package_name"), AssetData.PackageName.ToString());
        
        // Get disk size
        int64 DiskSize = -1;
        AssetData.GetTagValue(FName("Size"), DiskSize);
        if (DiskSize >= 0)
        {
            AssetObj->SetNumberField(TEXT("disk_size"), DiskSize);
        }

        // Check if asset is dirty (modified)
        UPackage* Package = FindPackage(nullptr, *AssetData.PackageName.ToString());
        if (Package)
        {
            AssetObj->SetBoolField(TEXT("is_dirty"), Package->IsDirty());
        }

        // Get all asset tags
        TSharedPtr<FJsonObject> TagsObj = MakeShared<FJsonObject>();
        FAssetDataTagMap TagsAndValues = AssetData.TagsAndValues.CopyMap();
        for (const TPair<FName, FString>& Tag : TagsAndValues)
        {
            TagsObj->SetStringField(Tag.Key.ToString(), Tag.Value);
        }
        AssetObj->SetObjectField(TEXT("tags"), TagsObj);
    }

    return AssetObj;
}
