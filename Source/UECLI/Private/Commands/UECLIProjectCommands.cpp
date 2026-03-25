#include "Commands/UECLIProjectCommands.h"
#include "Commands/UECLICommonUtils.h"
#include "GameFramework/InputSettings.h"
#include "GeneralProjectSettings.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"
#include "ToolRegistry/UECLIToolRegistry.h"

namespace
{
	TSharedRef<FUECLIProjectCommands> GetProjectCommands()
	{
		static TSharedRef<FUECLIProjectCommands> Instance = MakeShared<FUECLIProjectCommands>();
		return Instance;
	}

	void RegisterProjectCommand(FUECLIToolRegistry& Registry, const TCHAR* PublicName, const TCHAR* InternalName = nullptr)
	{
		const TSharedRef<FUECLIProjectCommands> Handler = GetProjectCommands();
		const FString RoutedName = InternalName != nullptr ? InternalName : PublicName;
		Registry.Register(
			FUECLIToolSchema(PublicName, TEXT("Project"), FString()),
			[Handler, RoutedName](const TSharedPtr<FJsonObject>& Params)
			{
				return Handler->HandleCommand(RoutedName, Params);
			});
	}
}

FUECLIProjectCommands::FUECLIProjectCommands()
{
}

void FUECLIProjectCommands::RegisterTools(FUECLIToolRegistry& Registry)
{
	RegisterProjectCommand(Registry, TEXT("create_input_mapping"));
	RegisterProjectCommand(Registry, TEXT("get_input_mappings"));
	RegisterProjectCommand(Registry, TEXT("delete_input_mapping"));
	RegisterProjectCommand(Registry, TEXT("create_axis_mapping"));
	RegisterProjectCommand(Registry, TEXT("get_project_info"));
	RegisterProjectCommand(Registry, TEXT("get_project_settings"));
	RegisterProjectCommand(Registry, TEXT("set_project_setting"));
	RegisterProjectCommand(Registry, TEXT("get_build_configuration"));
}

TSharedPtr<FJsonObject> FUECLIProjectCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("create_input_mapping"))
    {
        return HandleCreateInputMapping(Params);
    }
    else if (CommandType == TEXT("get_input_mappings"))
    {
        return HandleGetInputMappings(Params);
    }
    else if (CommandType == TEXT("delete_input_mapping"))
    {
        return HandleDeleteInputMapping(Params);
    }
    else if (CommandType == TEXT("create_axis_mapping"))
    {
        return HandleCreateAxisMapping(Params);
    }
    else if (CommandType == TEXT("get_project_info"))
    {
        return HandleGetProjectInfo(Params);
    }
    else if (CommandType == TEXT("get_project_settings"))
    {
        return HandleGetProjectSettings(Params);
    }
    else if (CommandType == TEXT("set_project_setting"))
    {
        return HandleSetProjectSetting(Params);
    }
    else if (CommandType == TEXT("get_build_configuration"))
    {
        return HandleGetBuildConfiguration(Params);
    }
    
    return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown project command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUECLIProjectCommands::HandleCreateInputMapping(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString ActionName;
    if (!Params->TryGetStringField(TEXT("action_name"), ActionName))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'action_name' parameter"));
    }

    FString Key;
    if (!Params->TryGetStringField(TEXT("key"), Key))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'key' parameter"));
    }

    // Get the input settings
    UInputSettings* InputSettings = GetMutableDefault<UInputSettings>();
    if (!InputSettings)
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to get input settings"));
    }

    // Create the input action mapping
    FInputActionKeyMapping ActionMapping;
    ActionMapping.ActionName = FName(*ActionName);
    ActionMapping.Key = FKey(*Key);

    // Add modifiers if provided
    if (Params->HasField(TEXT("shift")))
    {
        ActionMapping.bShift = Params->GetBoolField(TEXT("shift"));
    }
    if (Params->HasField(TEXT("ctrl")))
    {
        ActionMapping.bCtrl = Params->GetBoolField(TEXT("ctrl"));
    }
    if (Params->HasField(TEXT("alt")))
    {
        ActionMapping.bAlt = Params->GetBoolField(TEXT("alt"));
    }
    if (Params->HasField(TEXT("cmd")))
    {
        ActionMapping.bCmd = Params->GetBoolField(TEXT("cmd"));
    }

    // Add the mapping
    InputSettings->AddActionMapping(ActionMapping);
    InputSettings->SaveConfig();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("action_name"), ActionName);
    ResultObj->SetStringField(TEXT("key"), Key);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIProjectCommands::HandleGetInputMappings(const TSharedPtr<FJsonObject>& Params)
{
    UInputSettings* InputSettings = GetMutableDefault<UInputSettings>();
    if (!InputSettings)
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to get input settings"));
    }

    // Get action mappings
    TArray<TSharedPtr<FJsonValue>> ActionMappingsArray;
    const TArray<FInputActionKeyMapping>& ActionMappings = InputSettings->GetActionMappings();
    for (const FInputActionKeyMapping& Mapping : ActionMappings)
    {
        TSharedPtr<FJsonObject> MappingObj = MakeShared<FJsonObject>();
        MappingObj->SetStringField(TEXT("action_name"), Mapping.ActionName.ToString());
        MappingObj->SetStringField(TEXT("key"), Mapping.Key.GetFName().ToString());
        MappingObj->SetBoolField(TEXT("shift"), Mapping.bShift);
        MappingObj->SetBoolField(TEXT("ctrl"), Mapping.bCtrl);
        MappingObj->SetBoolField(TEXT("alt"), Mapping.bAlt);
        MappingObj->SetBoolField(TEXT("cmd"), Mapping.bCmd);
        ActionMappingsArray.Add(MakeShared<FJsonValueObject>(MappingObj));
    }

    // Get axis mappings
    TArray<TSharedPtr<FJsonValue>> AxisMappingsArray;
    const TArray<FInputAxisKeyMapping>& AxisMappings = InputSettings->GetAxisMappings();
    for (const FInputAxisKeyMapping& Mapping : AxisMappings)
    {
        TSharedPtr<FJsonObject> MappingObj = MakeShared<FJsonObject>();
        MappingObj->SetStringField(TEXT("axis_name"), Mapping.AxisName.ToString());
        MappingObj->SetStringField(TEXT("key"), Mapping.Key.GetFName().ToString());
        MappingObj->SetNumberField(TEXT("scale"), Mapping.Scale);
        AxisMappingsArray.Add(MakeShared<FJsonValueObject>(MappingObj));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("action_mappings"), ActionMappingsArray);
    ResultObj->SetNumberField(TEXT("action_count"), ActionMappingsArray.Num());
    ResultObj->SetArrayField(TEXT("axis_mappings"), AxisMappingsArray);
    ResultObj->SetNumberField(TEXT("axis_count"), AxisMappingsArray.Num());
    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIProjectCommands::HandleDeleteInputMapping(const TSharedPtr<FJsonObject>& Params)
{
    FString ActionName;
    if (!Params->TryGetStringField(TEXT("action_name"), ActionName))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'action_name' parameter"));
    }

    UInputSettings* InputSettings = GetMutableDefault<UInputSettings>();
    if (!InputSettings)
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to get input settings"));
    }

    // Find and remove the mapping
    FString Key;
    Params->TryGetStringField(TEXT("key"), Key);

    bool bRemoved = false;
    TArray<FInputActionKeyMapping> Mappings = InputSettings->GetActionMappings();
    for (int32 i = Mappings.Num() - 1; i >= 0; i--)
    {
        if (Mappings[i].ActionName.ToString() == ActionName)
        {
            if (Key.IsEmpty() || Mappings[i].Key.GetFName().ToString() == Key)
            {
                InputSettings->RemoveActionMapping(Mappings[i]);
                bRemoved = true;
            }
        }
    }

    if (bRemoved)
    {
        InputSettings->SaveConfig();
        
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("action_name"), ActionName);
        ResultObj->SetBoolField(TEXT("success"), true);
        return ResultObj;
    }

    return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Action mapping not found: %s"), *ActionName));
}

TSharedPtr<FJsonObject> FUECLIProjectCommands::HandleCreateAxisMapping(const TSharedPtr<FJsonObject>& Params)
{
    FString AxisName;
    if (!Params->TryGetStringField(TEXT("axis_name"), AxisName))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'axis_name' parameter"));
    }

    FString Key;
    if (!Params->TryGetStringField(TEXT("key"), Key))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'key' parameter"));
    }

    float Scale = 1.0f;
    Params->TryGetNumberField(TEXT("scale"), Scale);

    UInputSettings* InputSettings = GetMutableDefault<UInputSettings>();
    if (!InputSettings)
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to get input settings"));
    }

    FInputAxisKeyMapping AxisMapping;
    AxisMapping.AxisName = FName(*AxisName);
    AxisMapping.Key = FKey(*Key);
    AxisMapping.Scale = Scale;

    InputSettings->AddAxisMapping(AxisMapping);
    InputSettings->SaveConfig();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("axis_name"), AxisName);
    ResultObj->SetStringField(TEXT("key"), Key);
    ResultObj->SetNumberField(TEXT("scale"), Scale);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIProjectCommands::HandleGetProjectInfo(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    
    // Project paths
    ResultObj->SetStringField(TEXT("project_name"), FApp::GetProjectName());
    ResultObj->SetStringField(TEXT("project_dir"), FPaths::ProjectDir());
    ResultObj->SetStringField(TEXT("content_dir"), FPaths::ProjectContentDir());
    ResultObj->SetStringField(TEXT("config_dir"), FPaths::ProjectConfigDir());
    
    // Engine info
    ResultObj->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());
    ResultObj->SetStringField(TEXT("engine_dir"), FPaths::EngineDir());
    
    // Platform info
    ResultObj->SetStringField(TEXT("platform"), FPlatformMisc::GetUBTPlatform());
    
    // Build configuration
#if UE_BUILD_DEBUG
    ResultObj->SetStringField(TEXT("build_config"), TEXT("Debug"));
#elif UE_BUILD_DEVELOPMENT
    ResultObj->SetStringField(TEXT("build_config"), TEXT("Development"));
#elif UE_BUILD_SHIPPING
    ResultObj->SetStringField(TEXT("build_config"), TEXT("Shipping"));
#else
    ResultObj->SetStringField(TEXT("build_config"), TEXT("Unknown"));
#endif

    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIProjectCommands::HandleGetProjectSettings(const TSharedPtr<FJsonObject>& Params)
{
    const UGeneralProjectSettings* ProjectSettings = GetDefault<UGeneralProjectSettings>();
    if (!ProjectSettings)
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to get project settings"));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("project_name"), ProjectSettings->ProjectName);
    ResultObj->SetStringField(TEXT("company_name"), ProjectSettings->CompanyName);
    ResultObj->SetStringField(TEXT("company_distinguished_name"), ProjectSettings->CompanyDistinguishedName);
    ResultObj->SetStringField(TEXT("project_version"), ProjectSettings->ProjectVersion);
    ResultObj->SetStringField(TEXT("homepage"), ProjectSettings->Homepage);
    ResultObj->SetStringField(TEXT("support_contact"), ProjectSettings->SupportContact);
    ResultObj->SetStringField(TEXT("copyright_notice"), ProjectSettings->CopyrightNotice);
    ResultObj->SetStringField(TEXT("project_id"), ProjectSettings->ProjectID.ToString());
    ResultObj->SetBoolField(TEXT("should_window_preserve_aspect_ratio"), ProjectSettings->bShouldWindowPreserveAspectRatio);
    ResultObj->SetBoolField(TEXT("use_borderless_window"), ProjectSettings->bUseBorderlessWindow);
    ResultObj->SetBoolField(TEXT("start_in_vr"), ProjectSettings->bStartInVR);
    // Note: bStartInAR was removed in newer UE5 versions
    ResultObj->SetBoolField(TEXT("allow_window_resize"), ProjectSettings->bAllowWindowResize);
    ResultObj->SetBoolField(TEXT("allow_close"), ProjectSettings->bAllowClose);
    ResultObj->SetBoolField(TEXT("allow_maximize"), ProjectSettings->bAllowMaximize);
    ResultObj->SetBoolField(TEXT("allow_minimize"), ProjectSettings->bAllowMinimize);

    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIProjectCommands::HandleSetProjectSetting(const TSharedPtr<FJsonObject>& Params)
{
    FString SettingName;
    if (!Params->TryGetStringField(TEXT("setting_name"), SettingName))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'setting_name' parameter"));
    }

    if (!Params->HasField(TEXT("value")))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'value' parameter"));
    }

    UGeneralProjectSettings* ProjectSettings = GetMutableDefault<UGeneralProjectSettings>();
    if (!ProjectSettings)
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to get project settings"));
    }

    bool bSuccess = false;

    // Handle string settings
    if (SettingName == TEXT("ProjectName"))
    {
        ProjectSettings->ProjectName = Params->GetStringField(TEXT("value"));
        bSuccess = true;
    }
    else if (SettingName == TEXT("CompanyName"))
    {
        ProjectSettings->CompanyName = Params->GetStringField(TEXT("value"));
        bSuccess = true;
    }
    else if (SettingName == TEXT("ProjectVersion"))
    {
        ProjectSettings->ProjectVersion = Params->GetStringField(TEXT("value"));
        bSuccess = true;
    }
    else if (SettingName == TEXT("CopyrightNotice"))
    {
        ProjectSettings->CopyrightNotice = Params->GetStringField(TEXT("value"));
        bSuccess = true;
    }
    else if (SettingName == TEXT("Homepage"))
    {
        ProjectSettings->Homepage = Params->GetStringField(TEXT("value"));
        bSuccess = true;
    }
    else if (SettingName == TEXT("SupportContact"))
    {
        ProjectSettings->SupportContact = Params->GetStringField(TEXT("value"));
        bSuccess = true;
    }

    if (bSuccess)
    {
        ProjectSettings->SaveConfig();
        
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("setting_name"), SettingName);
        ResultObj->SetBoolField(TEXT("success"), true);
        return ResultObj;
    }

    return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown or read-only setting: %s"), *SettingName));
}

TSharedPtr<FJsonObject> FUECLIProjectCommands::HandleGetBuildConfiguration(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();

#if UE_BUILD_DEBUG
    ResultObj->SetStringField(TEXT("configuration"), TEXT("Debug"));
    ResultObj->SetBoolField(TEXT("is_debug"), true);
    ResultObj->SetBoolField(TEXT("is_development"), false);
    ResultObj->SetBoolField(TEXT("is_shipping"), false);
#elif UE_BUILD_DEVELOPMENT
    ResultObj->SetStringField(TEXT("configuration"), TEXT("Development"));
    ResultObj->SetBoolField(TEXT("is_debug"), false);
    ResultObj->SetBoolField(TEXT("is_development"), true);
    ResultObj->SetBoolField(TEXT("is_shipping"), false);
#elif UE_BUILD_SHIPPING
    ResultObj->SetStringField(TEXT("configuration"), TEXT("Shipping"));
    ResultObj->SetBoolField(TEXT("is_debug"), false);
    ResultObj->SetBoolField(TEXT("is_development"), false);
    ResultObj->SetBoolField(TEXT("is_shipping"), true);
#else
    ResultObj->SetStringField(TEXT("configuration"), TEXT("Unknown"));
#endif

#if WITH_EDITOR
    ResultObj->SetBoolField(TEXT("with_editor"), true);
#else
    ResultObj->SetBoolField(TEXT("with_editor"), false);
#endif

    ResultObj->SetStringField(TEXT("platform"), FPlatformMisc::GetUBTPlatform());

    return ResultObj;
} 
