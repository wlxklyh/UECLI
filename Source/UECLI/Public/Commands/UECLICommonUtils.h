#pragma once

#include "CoreMinimal.h"
#include "Json.h"

// Forward declarations
class AActor;

/**
 * Common utilities for UECLI commands
 */
class UECLI_API FUECLICommonUtils
{
public:
    // JSON utilities
    static TSharedPtr<FJsonObject> CreateErrorResponse(const FString& Message);
    static TSharedPtr<FJsonObject> CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data = nullptr);
    static void GetIntArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<int32>& OutArray);
    static void GetFloatArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<float>& OutArray);
    static FVector2D GetVector2DFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName);
    static FVector GetVectorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName);
    static FRotator GetRotatorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName);

    // Actor utilities
    static TSharedPtr<FJsonValue> ActorToJson(AActor* Actor);
    static TSharedPtr<FJsonObject> ActorToJsonObject(AActor* Actor, bool bDetailed = false);

    // Property utilities
    static bool SetObjectProperty(UObject* Object, const FString& PropertyName,
                                 const TSharedPtr<FJsonValue>& Value, FString& OutErrorMessage);

    // Reflection-based property serialization
    static TSharedPtr<FJsonObject> SerializeObjectProperties(UObject* Object, int32 MaxDepth = 3, bool bIncludeInherited = true);
    static TSharedPtr<FJsonValue> SerializePropertyValue(FProperty* Property, const void* ValuePtr, int32 CurrentDepth, int32 MaxDepth);
    static TSharedPtr<FJsonObject> SerializeStructProperties(UScriptStruct* Struct, const void* StructPtr, int32 CurrentDepth, int32 MaxDepth);
}; 