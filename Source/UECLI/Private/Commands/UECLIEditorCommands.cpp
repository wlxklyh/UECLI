#include "Commands/UECLIEditorCommands.h"
#include "Commands/UECLICommonUtils.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "ImageUtils.h"
#include "HighResScreenshot.h"
#include "Engine/GameViewportClient.h"
#include "Misc/FileHelper.h"
#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "LevelEditorSubsystem.h"
#include "Misc/PackageName.h"
#include "ToolRegistry/UECLIToolRegistry.h"

namespace
{
	TSharedRef<FUECLIEditorCommands> GetEditorCommands()
	{
		static TSharedRef<FUECLIEditorCommands> Instance = MakeShared<FUECLIEditorCommands>();
		return Instance;
	}

	void RegisterEditorCommand(FUECLIToolRegistry& Registry, const TCHAR* PublicName, const TCHAR* InternalName = nullptr)
	{
		const TSharedRef<FUECLIEditorCommands> Handler = GetEditorCommands();
		const FString RoutedName = InternalName != nullptr ? InternalName : PublicName;
		Registry.Register(
			FUECLIToolSchema(PublicName, TEXT("Editor"), FString()),
			[Handler, RoutedName](const TSharedPtr<FJsonObject>& Params)
			{
				return Handler->HandleCommand(RoutedName, Params);
			});
	}

}

FUECLIEditorCommands::FUECLIEditorCommands()
{
}

void FUECLIEditorCommands::RegisterTools(FUECLIToolRegistry& Registry)
{
	RegisterEditorCommand(Registry, TEXT("get_actors_in_level"));
	RegisterEditorCommand(Registry, TEXT("find_actors_by_name"));
	RegisterEditorCommand(Registry, TEXT("spawn_actor"));
	RegisterEditorCommand(Registry, TEXT("delete_actor"));
	RegisterEditorCommand(Registry, TEXT("set_actor_transform"));
	RegisterEditorCommand(Registry, TEXT("get_actor_properties"));
	RegisterEditorCommand(Registry, TEXT("set_actor_property"));
	RegisterEditorCommand(Registry, TEXT("spawn_blueprint_actor"));
	RegisterEditorCommand(Registry, TEXT("focus_viewport"));
	RegisterEditorCommand(Registry, TEXT("take_screenshot"));
	RegisterEditorCommand(Registry, TEXT("select_actor"));
	RegisterEditorCommand(Registry, TEXT("duplicate_actor"));
	RegisterEditorCommand(Registry, TEXT("rename_actor"));
	RegisterEditorCommand(Registry, TEXT("get_viewport_camera"));
	RegisterEditorCommand(Registry, TEXT("set_viewport_camera"));
	RegisterEditorCommand(Registry, TEXT("execute_console_command"));
	RegisterEditorCommand(Registry, TEXT("play_in_editor"));
	RegisterEditorCommand(Registry, TEXT("stop_play_in_editor"));
	RegisterEditorCommand(Registry, TEXT("get_current_level"));
	RegisterEditorCommand(Registry, TEXT("open_level"));
	RegisterEditorCommand(Registry, TEXT("save_current_level"));
	RegisterEditorCommand(Registry, TEXT("create_new_level"));
}

TSharedPtr<FJsonObject> FUECLIEditorCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    // Actor manipulation commands
    if (CommandType == TEXT("get_actors_in_level"))
    {
        return HandleGetActorsInLevel(Params);
    }
    else if (CommandType == TEXT("find_actors_by_name"))
    {
        return HandleFindActorsByName(Params);
    }
    else if (CommandType == TEXT("spawn_actor") || CommandType == TEXT("create_actor"))
    {
        if (CommandType == TEXT("create_actor"))
        {
            UE_LOG(LogTemp, Warning, TEXT("'create_actor' command is deprecated and will be removed in a future version. Please use 'spawn_actor' instead."));
        }
        return HandleSpawnActor(Params);
    }
    else if (CommandType == TEXT("delete_actor"))
    {
        return HandleDeleteActor(Params);
    }
    else if (CommandType == TEXT("set_actor_transform"))
    {
        return HandleSetActorTransform(Params);
    }
    else if (CommandType == TEXT("get_actor_properties"))
    {
        return HandleGetActorProperties(Params);
    }
    else if (CommandType == TEXT("set_actor_property"))
    {
        return HandleSetActorProperty(Params);
    }
    else if (CommandType == TEXT("select_actor"))
    {
        return HandleSelectActor(Params);
    }
    else if (CommandType == TEXT("duplicate_actor"))
    {
        return HandleDuplicateActor(Params);
    }
    else if (CommandType == TEXT("rename_actor"))
    {
        return HandleRenameActor(Params);
    }
    // Blueprint actor spawning
    else if (CommandType == TEXT("spawn_blueprint_actor"))
    {
        return HandleSpawnBlueprintActor(Params);
    }
    // Editor viewport commands
    else if (CommandType == TEXT("focus_viewport"))
    {
        return HandleFocusViewport(Params);
    }
    else if (CommandType == TEXT("take_screenshot"))
    {
        return HandleTakeScreenshot(Params);
    }
    else if (CommandType == TEXT("get_viewport_camera"))
    {
        return HandleGetViewportCamera(Params);
    }
    else if (CommandType == TEXT("set_viewport_camera"))
    {
        return HandleSetViewportCamera(Params);
    }
    else if (CommandType == TEXT("execute_console_command"))
    {
        return HandleExecuteConsoleCommand(Params);
    }
    else if (CommandType == TEXT("play_in_editor"))
    {
        return HandlePlayInEditor(Params);
    }
    else if (CommandType == TEXT("stop_play_in_editor"))
    {
        return HandleStopPlayInEditor(Params);
    }
    else if (CommandType == TEXT("get_current_level"))
    {
        return HandleGetCurrentLevel(Params);
    }
    else if (CommandType == TEXT("open_level"))
    {
        return HandleOpenLevel(Params);
    }
    else if (CommandType == TEXT("save_current_level"))
    {
        return HandleSaveCurrentLevel(Params);
    }
    else if (CommandType == TEXT("create_new_level"))
    {
        return HandleCreateNewLevel(Params);
    }
    
    return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown editor command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUECLIEditorCommands::HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params)
{
    FString FilterClass;
    Params->TryGetStringField(TEXT("filter_class"), FilterClass);

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    TArray<TSharedPtr<FJsonValue>> ActorArray;
    for (AActor* Actor : AllActors)
    {
        if (Actor && (FilterClass.IsEmpty() || Actor->GetClass()->GetName() == FilterClass || Actor->GetClass()->GetPathName().Contains(FilterClass)))
        {
            ActorArray.Add(FUECLICommonUtils::ActorToJson(Actor));
        }
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), ActorArray);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIEditorCommands::HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params)
{
    FString Pattern;
    if (!Params->TryGetStringField(TEXT("pattern"), Pattern) && !Params->TryGetStringField(TEXT("name"), Pattern))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'pattern' or 'name' parameter"));
    }
    
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    TArray<TSharedPtr<FJsonValue>> MatchingActors;
    for (AActor* Actor : AllActors)
    {
        if (Actor && (Actor->GetName().Contains(Pattern) || Actor->GetActorLabel().Contains(Pattern)))
        {
            MatchingActors.Add(FUECLICommonUtils::ActorToJson(Actor));
        }
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), MatchingActors);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIEditorCommands::HandleSpawnActor(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString ActorType;
    if (!Params->TryGetStringField(TEXT("type"), ActorType) && !Params->TryGetStringField(TEXT("class"), ActorType))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'type' or 'class' parameter"));
    }

    // Get actor name (required parameter)
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Get optional transform parameters
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FUECLICommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUECLICommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }
    if (Params->HasField(TEXT("scale")))
    {
        Scale = FUECLICommonUtils::GetVectorFromJson(Params, TEXT("scale"));
    }

    // Create the actor based on type
    AActor* NewActor = nullptr;
    UWorld* World = GEditor->GetEditorWorldContext().World();

    if (!World)
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // Check if an actor with this name already exists
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor with name '%s' already exists"), *ActorName));
        }
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;

    if (ActorType == TEXT("StaticMeshActor"))
    {
        AStaticMeshActor* StaticMeshActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation, SpawnParams);
        NewActor = StaticMeshActor;
        if (StaticMeshActor)
        {
            // Set the default cube mesh so the actor is visible
            UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
            if (CubeMesh)
            {
                StaticMeshActor->GetStaticMeshComponent()->SetStaticMesh(CubeMesh);
            }
        }
    }
    else if (ActorType == TEXT("PointLight"))
    {
        NewActor = World->SpawnActor<APointLight>(APointLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("SpotLight"))
    {
        NewActor = World->SpawnActor<ASpotLight>(ASpotLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("DirectionalLight"))
    {
        NewActor = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("CameraActor"))
    {
        NewActor = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Location, Rotation, SpawnParams);
    }
    else
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown actor type: %s"), *ActorType));
    }

    if (NewActor)
    {
        // Set scale (since SpawnActor only takes location and rotation)
        FTransform Transform = NewActor->GetTransform();
        Transform.SetScale3D(Scale);
        NewActor->SetActorTransform(Transform);
        
        // Set the actor label so it shows correctly in World Outliner
        NewActor->SetActorLabel(ActorName);

        // Return the created actor's details
        return FUECLICommonUtils::ActorToJsonObject(NewActor, true);
    }

    return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to create actor"));
}

TSharedPtr<FJsonObject> FUECLIEditorCommands::HandleDeleteActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    AActor* Actor = FindActorByName(ActorName);
    if (Actor)
    {
        TSharedPtr<FJsonObject> ActorInfo = FUECLICommonUtils::ActorToJsonObject(Actor);
        UEditorActorSubsystem* ActorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UEditorActorSubsystem>() : nullptr;
        const bool bDeleted = ActorSubsystem ? ActorSubsystem->DestroyActor(Actor) : Actor->Destroy();
        if (!bDeleted)
        {
            return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to delete actor: %s"), *ActorName));
        }

        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetObjectField(TEXT("deleted_actor"), ActorInfo);
        return ResultObj;
    }
    
    return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

TSharedPtr<FJsonObject> FUECLIEditorCommands::HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    AActor* TargetActor = FindActorByName(ActorName);

    if (!TargetActor)
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Get transform parameters
    FTransform NewTransform = TargetActor->GetTransform();

    if (Params->HasField(TEXT("location")))
    {
        NewTransform.SetLocation(FUECLICommonUtils::GetVectorFromJson(Params, TEXT("location")));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        NewTransform.SetRotation(FQuat(FUECLICommonUtils::GetRotatorFromJson(Params, TEXT("rotation"))));
    }
    if (Params->HasField(TEXT("scale")))
    {
        NewTransform.SetScale3D(FUECLICommonUtils::GetVectorFromJson(Params, TEXT("scale")));
    }

    // Set the new transform
    TargetActor->SetActorTransform(NewTransform);

    // Return updated actor info
    return FUECLICommonUtils::ActorToJsonObject(TargetActor, true);
}

TSharedPtr<FJsonObject> FUECLIEditorCommands::HandleGetActorProperties(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Get optional max_depth parameter (default: 3)
    int32 MaxDepth = 3;
    if (Params->HasField(TEXT("max_depth")))
    {
        MaxDepth = static_cast<int32>(Params->GetNumberField(TEXT("max_depth")));
        MaxDepth = FMath::Clamp(MaxDepth, 1, 10); // Limit depth to prevent infinite recursion
    }

    AActor* TargetActor = FindActorByName(ActorName);

    if (!TargetActor)
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Use reflection-based serialization to get all properties
    return FUECLICommonUtils::SerializeObjectProperties(TargetActor, MaxDepth, true);
}

TSharedPtr<FJsonObject> FUECLIEditorCommands::HandleSetActorProperty(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    AActor* TargetActor = FindActorByName(ActorName);

    if (!TargetActor)
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Get property name
    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName) && !Params->TryGetStringField(TEXT("property"), PropertyName))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' or 'property' parameter"));
    }

    // Get property value
    if (!Params->HasField(TEXT("property_value")) && !Params->HasField(TEXT("value")))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'property_value' or 'value' parameter"));
    }
    
    TSharedPtr<FJsonValue> PropertyValue = Params->HasField(TEXT("property_value")) ? Params->Values.FindRef(TEXT("property_value")) : Params->Values.FindRef(TEXT("value"));
    
    // Set the property using our utility function
    FString ErrorMessage;
    if (FUECLICommonUtils::SetObjectProperty(TargetActor, PropertyName, PropertyValue, ErrorMessage))
    {
        // Property set successfully
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("actor"), ActorName);
        ResultObj->SetStringField(TEXT("property"), PropertyName);
        ResultObj->SetBoolField(TEXT("success"), true);
        
        // Also include the full actor details
        ResultObj->SetObjectField(TEXT("actor_details"), FUECLICommonUtils::ActorToJsonObject(TargetActor, true));
        return ResultObj;
    }
    else
    {
        return FUECLICommonUtils::CreateErrorResponse(ErrorMessage);
    }
}

TSharedPtr<FJsonObject> FUECLIEditorCommands::HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName) && !Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' or 'blueprint_path' parameter"));
    }

    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) && !Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' or 'name' parameter"));
    }

    // Find the blueprint
    if (BlueprintName.IsEmpty())
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Blueprint name is empty"));
    }

    FString AssetPath = BlueprintPath;
    if (AssetPath.IsEmpty())
    {
        const FString Root = TEXT("/Game/Blueprints/");
        AssetPath = Root + BlueprintName;
    }

    if (!FPackageName::DoesPackageExist(AssetPath))
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint '%s' not found 鈥?it must reside under /Game/Blueprints"), *BlueprintName));
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
    if (!Blueprint)
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get transform parameters
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FUECLICommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUECLICommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }
    if (Params->HasField(TEXT("scale")))
    {
        Scale = FUECLICommonUtils::GetVectorFromJson(Params, TEXT("scale"));
    }

    // Spawn the actor
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    FTransform SpawnTransform;
    SpawnTransform.SetLocation(Location);
    SpawnTransform.SetRotation(FQuat(Rotation));
    SpawnTransform.SetScale3D(Scale);

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;

    AActor* NewActor = World->SpawnActor<AActor>(Blueprint->GeneratedClass, SpawnTransform, SpawnParams);
    if (NewActor)
    {
        return FUECLICommonUtils::ActorToJsonObject(NewActor, true);
    }

    return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to spawn blueprint actor"));
}

TSharedPtr<FJsonObject> FUECLIEditorCommands::HandleFocusViewport(const TSharedPtr<FJsonObject>& Params)
{
    // Get target actor name if provided
    FString TargetActorName;
    bool HasTargetActor = Params->TryGetStringField(TEXT("target"), TargetActorName) || Params->TryGetStringField(TEXT("name"), TargetActorName);

    // Get location if provided
    FVector Location(0.0f, 0.0f, 0.0f);
    bool HasLocation = false;
    if (Params->HasField(TEXT("location")))
    {
        Location = FUECLICommonUtils::GetVectorFromJson(Params, TEXT("location"));
        HasLocation = true;
    }

    // Get distance
    float Distance = 1000.0f;
    if (Params->HasField(TEXT("distance")))
    {
        Distance = Params->GetNumberField(TEXT("distance"));
    }

    // Get orientation if provided
    FRotator Orientation(0.0f, 0.0f, 0.0f);
    bool HasOrientation = false;
    if (Params->HasField(TEXT("orientation")))
    {
        Orientation = FUECLICommonUtils::GetRotatorFromJson(Params, TEXT("orientation"));
        HasOrientation = true;
    }

    // Get the active viewport
    FLevelEditorViewportClient* ViewportClient = (FLevelEditorViewportClient*)GEditor->GetActiveViewport()->GetClient();
    if (!ViewportClient)
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to get active viewport"));
    }

    // If we have a target actor, focus on it
    if (HasTargetActor)
    {
        // Find the actor
        AActor* TargetActor = nullptr;
        TArray<AActor*> AllActors;
        UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
        
        for (AActor* Actor : AllActors)
        {
            if (Actor && Actor->GetName() == TargetActorName)
            {
                TargetActor = Actor;
                break;
            }
        }

        if (!TargetActor)
        {
            return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *TargetActorName));
        }

        // Focus on the actor
        ViewportClient->SetViewLocation(TargetActor->GetActorLocation() - FVector(Distance, 0.0f, 0.0f));
    }
    // Otherwise use the provided location
    else if (HasLocation)
    {
        ViewportClient->SetViewLocation(Location - FVector(Distance, 0.0f, 0.0f));
    }
    else
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Either 'target' or 'location' must be provided"));
    }

    // Set orientation if provided
    if (HasOrientation)
    {
        ViewportClient->SetViewRotation(Orientation);
    }

    // Force viewport to redraw
    ViewportClient->Invalidate();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIEditorCommands::HandleTakeScreenshot(const TSharedPtr<FJsonObject>& Params)
{
    // Get file path parameter
    FString FilePath;
    if (!Params->TryGetStringField(TEXT("filepath"), FilePath) && !Params->TryGetStringField(TEXT("filename"), FilePath))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'filepath' or 'filename' parameter"));
    }
    
    // Ensure the file path has a proper extension
    if (!FilePath.EndsWith(TEXT(".png")))
    {
        FilePath += TEXT(".png");
    }

    // Get the active viewport
    if (GEditor && GEditor->GetActiveViewport())
    {
        FViewport* Viewport = GEditor->GetActiveViewport();
        TArray<FColor> Bitmap;
        FIntRect ViewportRect(0, 0, Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y);
        
        if (Viewport->ReadPixels(Bitmap, FReadSurfaceDataFlags(), ViewportRect))
        {
            TArray64<uint8> CompressedBitmap;
            FImageUtils::PNGCompressImageArray(Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y, Bitmap, CompressedBitmap);

            if (FFileHelper::SaveArrayToFile(CompressedBitmap, *FilePath))
            {
                TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
                ResultObj->SetStringField(TEXT("filepath"), FilePath);
                return ResultObj;
            }
        }
    }
    
    return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to take screenshot"));
}

TSharedPtr<FJsonObject> FUECLIEditorCommands::HandleSelectActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    AActor* Actor = FindActorByName(ActorName);
    if (!Actor)
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    UEditorActorSubsystem* ActorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UEditorActorSubsystem>() : nullptr;
    if (!ActorSubsystem)
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to get EditorActorSubsystem"));
    }

    ActorSubsystem->SetSelectedLevelActors({Actor});

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("selected_actor"), Actor->GetName());
    ResultObj->SetStringField(TEXT("actor_label"), Actor->GetActorLabel());
    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIEditorCommands::HandleDuplicateActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    AActor* SourceActor = FindActorByName(ActorName);
    if (!SourceActor)
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    UEditorActorSubsystem* ActorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UEditorActorSubsystem>() : nullptr;
    if (!ActorSubsystem)
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to get EditorActorSubsystem"));
    }

    AActor* NewActor = ActorSubsystem->DuplicateActor(SourceActor, SourceActor->GetWorld(), FVector::ZeroVector);
    if (!NewActor)
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to duplicate actor"));
    }

    FString NewName;
    if (Params->TryGetStringField(TEXT("new_name"), NewName) && !NewName.IsEmpty())
    {
        NewActor->Modify();
        NewActor->SetActorLabel(NewName);
        NewActor->Rename(*NewName, nullptr, REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
    }

    return FUECLICommonUtils::ActorToJsonObject(NewActor, true);
}

TSharedPtr<FJsonObject> FUECLIEditorCommands::HandleRenameActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString NewName;
    if (!Params->TryGetStringField(TEXT("new_name"), NewName))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'new_name' parameter"));
    }

    AActor* Actor = FindActorByName(ActorName);
    if (!Actor)
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    const FString OldName = Actor->GetName();
    Actor->Modify();
    Actor->SetActorLabel(NewName);
    Actor->Rename(*NewName, nullptr, REN_DontCreateRedirectors | REN_ForceNoResetLoaders);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("old_name"), OldName);
    ResultObj->SetStringField(TEXT("new_name"), Actor->GetName());
    ResultObj->SetStringField(TEXT("actor_label"), Actor->GetActorLabel());
    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIEditorCommands::HandleGetViewportCamera(const TSharedPtr<FJsonObject>& Params)
{
    FViewport* Viewport = (GEditor != nullptr) ? GEditor->GetActiveViewport() : nullptr;
    FLevelEditorViewportClient* ViewportClient = (Viewport != nullptr) ? static_cast<FLevelEditorViewportClient*>(Viewport->GetClient()) : nullptr;
    if (!ViewportClient)
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to get active viewport"));
    }

    const FVector Location = ViewportClient->GetViewLocation();
    const FRotator Rotation = ViewportClient->GetViewRotation();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> LocationArray;
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.X));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Y));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Z));
    ResultObj->SetArrayField(TEXT("location"), LocationArray);

    TArray<TSharedPtr<FJsonValue>> RotationArray;
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
    ResultObj->SetArrayField(TEXT("rotation"), RotationArray);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIEditorCommands::HandleSetViewportCamera(const TSharedPtr<FJsonObject>& Params)
{
    FViewport* Viewport = (GEditor != nullptr) ? GEditor->GetActiveViewport() : nullptr;
    FLevelEditorViewportClient* ViewportClient = (Viewport != nullptr) ? static_cast<FLevelEditorViewportClient*>(Viewport->GetClient()) : nullptr;
    if (!ViewportClient)
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to get active viewport"));
    }

    if (Params->HasField(TEXT("location")))
    {
        ViewportClient->SetViewLocation(FUECLICommonUtils::GetVectorFromJson(Params, TEXT("location")));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        ViewportClient->SetViewRotation(FUECLICommonUtils::GetRotatorFromJson(Params, TEXT("rotation")));
    }

    ViewportClient->Invalidate();
    return HandleGetViewportCamera(Params);
}

TSharedPtr<FJsonObject> FUECLIEditorCommands::HandleExecuteConsoleCommand(const TSharedPtr<FJsonObject>& Params)
{
    FString Command;
    if (!Params->TryGetStringField(TEXT("command"), Command))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'command' parameter"));
    }

    UWorld* World = (GEditor != nullptr) ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!GEditor || !GEditor->Exec(World, *Command))
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to execute console command: %s"), *Command));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("command"), Command);
    ResultObj->SetStringField(TEXT("result"), TEXT("executed"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIEditorCommands::HandlePlayInEditor(const TSharedPtr<FJsonObject>& Params)
{
    ULevelEditorSubsystem* LevelEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<ULevelEditorSubsystem>() : nullptr;
    if (!LevelEditorSubsystem)
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to get LevelEditorSubsystem"));
    }

    if (!LevelEditorSubsystem->IsInPlayInEditor())
    {
        LevelEditorSubsystem->EditorRequestBeginPlay();
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("in_pie"), LevelEditorSubsystem->IsInPlayInEditor());
    ResultObj->SetStringField(TEXT("status"), TEXT("requested"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIEditorCommands::HandleStopPlayInEditor(const TSharedPtr<FJsonObject>& Params)
{
    ULevelEditorSubsystem* LevelEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<ULevelEditorSubsystem>() : nullptr;
    if (!LevelEditorSubsystem)
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to get LevelEditorSubsystem"));
    }

    if (LevelEditorSubsystem->IsInPlayInEditor())
    {
        LevelEditorSubsystem->EditorRequestEndPlay();
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("in_pie"), LevelEditorSubsystem->IsInPlayInEditor());
    ResultObj->SetStringField(TEXT("status"), TEXT("requested"));
    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIEditorCommands::HandleGetCurrentLevel(const TSharedPtr<FJsonObject>& Params)
{
    ULevelEditorSubsystem* LevelEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<ULevelEditorSubsystem>() : nullptr;
    ULevel* CurrentLevel = LevelEditorSubsystem ? LevelEditorSubsystem->GetCurrentLevel() : nullptr;
    if (!CurrentLevel)
    {
        UWorld* World = (GEditor != nullptr) ? GEditor->GetEditorWorldContext().World() : nullptr;
        CurrentLevel = World ? World->GetCurrentLevel() : nullptr;
    }

    if (!CurrentLevel)
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to get current level"));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("level_name"), CurrentLevel->GetName());
    ResultObj->SetStringField(TEXT("level_path"), CurrentLevel->GetPackage()->GetName());
    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIEditorCommands::HandleOpenLevel(const TSharedPtr<FJsonObject>& Params)
{
    FString LevelPath;
    if (!Params->TryGetStringField(TEXT("level_path"), LevelPath))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'level_path' parameter"));
    }

    ULevelEditorSubsystem* LevelEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<ULevelEditorSubsystem>() : nullptr;
    if (!LevelEditorSubsystem || !LevelEditorSubsystem->LoadLevel(LevelPath))
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to open level: %s"), *LevelPath));
    }

    return HandleGetCurrentLevel(Params);
}

TSharedPtr<FJsonObject> FUECLIEditorCommands::HandleSaveCurrentLevel(const TSharedPtr<FJsonObject>& Params)
{
    ULevelEditorSubsystem* LevelEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<ULevelEditorSubsystem>() : nullptr;
    if (!LevelEditorSubsystem || !LevelEditorSubsystem->SaveCurrentLevel())
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to save current level"));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("saved"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIEditorCommands::HandleCreateNewLevel(const TSharedPtr<FJsonObject>& Params)
{
    FString LevelName;
    if (!Params->TryGetStringField(TEXT("level_name"), LevelName))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'level_name' parameter"));
    }

    FString AssetPath = LevelName;
    if (!AssetPath.StartsWith(TEXT("/")))
    {
        AssetPath = FString::Printf(TEXT("/Game/Maps/%s"), *LevelName);
    }

    ULevelEditorSubsystem* LevelEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<ULevelEditorSubsystem>() : nullptr;
    if (!LevelEditorSubsystem || !LevelEditorSubsystem->NewLevel(AssetPath))
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to create level: %s"), *AssetPath));
    }

    return HandleGetCurrentLevel(Params);
}

AActor* FUECLIEditorCommands::FindActorByName(const FString& ActorName)
{
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);

    for (AActor* Actor : AllActors)
    {
        if (Actor && (Actor->GetName() == ActorName || Actor->GetActorLabel() == ActorName))
        {
            return Actor;
        }
    }

    return nullptr;
}
