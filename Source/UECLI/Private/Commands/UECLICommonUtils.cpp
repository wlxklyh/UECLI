#include "Commands/UECLICommonUtils.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "UObject/UObjectIterator.h"
#include "Engine/Selection.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "JsonObjectConverter.h"

// JSON Utilities
TSharedPtr<FJsonObject> FUECLICommonUtils::CreateErrorResponse(const FString& Message)
{
    TSharedPtr<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
    ResponseObject->SetBoolField(TEXT("success"), false);
    ResponseObject->SetStringField(TEXT("error"), Message);
    return ResponseObject;
}

TSharedPtr<FJsonObject> FUECLICommonUtils::CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data)
{
    TSharedPtr<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
    ResponseObject->SetBoolField(TEXT("success"), true);
    
    if (Data.IsValid())
    {
        ResponseObject->SetObjectField(TEXT("data"), Data);
    }
    
    return ResponseObject;
}

void FUECLICommonUtils::GetIntArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<int32>& OutArray)
{
    OutArray.Reset();
    
    if (!JsonObject->HasField(FieldName))
    {
        return;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray))
    {
        for (const TSharedPtr<FJsonValue>& Value : *JsonArray)
        {
            OutArray.Add((int32)Value->AsNumber());
        }
    }
}

void FUECLICommonUtils::GetFloatArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<float>& OutArray)
{
    OutArray.Reset();
    
    if (!JsonObject->HasField(FieldName))
    {
        return;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray))
    {
        for (const TSharedPtr<FJsonValue>& Value : *JsonArray)
        {
            OutArray.Add((float)Value->AsNumber());
        }
    }
}

FVector2D FUECLICommonUtils::GetVector2DFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FVector2D Result(0.0f, 0.0f);
    
    if (!JsonObject->HasField(FieldName))
    {
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 2)
    {
        Result.X = (float)(*JsonArray)[0]->AsNumber();
        Result.Y = (float)(*JsonArray)[1]->AsNumber();
    }
    
    return Result;
}

FVector FUECLICommonUtils::GetVectorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FVector Result(0.0f, 0.0f, 0.0f);
    
    if (!JsonObject->HasField(FieldName))
    {
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 3)
    {
        Result.X = (float)(*JsonArray)[0]->AsNumber();
        Result.Y = (float)(*JsonArray)[1]->AsNumber();
        Result.Z = (float)(*JsonArray)[2]->AsNumber();
    }
    
    return Result;
}

FRotator FUECLICommonUtils::GetRotatorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FRotator Result(0.0f, 0.0f, 0.0f);
    
    if (!JsonObject->HasField(FieldName))
    {
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 3)
    {
        Result.Pitch = (float)(*JsonArray)[0]->AsNumber();
        Result.Yaw = (float)(*JsonArray)[1]->AsNumber();
        Result.Roll = (float)(*JsonArray)[2]->AsNumber();
    }
    
    return Result;
}

// Actor utilities
TSharedPtr<FJsonValue> FUECLICommonUtils::ActorToJson(AActor* Actor)
{
    if (!Actor)
    {
        return MakeShared<FJsonValueNull>();
    }
    
    TSharedPtr<FJsonObject> ActorObject = MakeShared<FJsonObject>();
    ActorObject->SetStringField(TEXT("name"), Actor->GetName());
    ActorObject->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
    
    FVector Location = Actor->GetActorLocation();
    TArray<TSharedPtr<FJsonValue>> LocationArray;
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.X));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Y));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Z));
    ActorObject->SetArrayField(TEXT("location"), LocationArray);
    
    FRotator Rotation = Actor->GetActorRotation();
    TArray<TSharedPtr<FJsonValue>> RotationArray;
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
    ActorObject->SetArrayField(TEXT("rotation"), RotationArray);
    
    FVector Scale = Actor->GetActorScale3D();
    TArray<TSharedPtr<FJsonValue>> ScaleArray;
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.X));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Y));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Z));
    ActorObject->SetArrayField(TEXT("scale"), ScaleArray);
    
    return MakeShared<FJsonValueObject>(ActorObject);
}

TSharedPtr<FJsonObject> FUECLICommonUtils::ActorToJsonObject(AActor* Actor, bool bDetailed)
{
    if (!Actor)
    {
        return nullptr;
    }
    
    TSharedPtr<FJsonObject> ActorObject = MakeShared<FJsonObject>();
    ActorObject->SetStringField(TEXT("name"), Actor->GetName());
    ActorObject->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
    
    FVector Location = Actor->GetActorLocation();
    TArray<TSharedPtr<FJsonValue>> LocationArray;
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.X));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Y));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Z));
    ActorObject->SetArrayField(TEXT("location"), LocationArray);
    
    FRotator Rotation = Actor->GetActorRotation();
    TArray<TSharedPtr<FJsonValue>> RotationArray;
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
    ActorObject->SetArrayField(TEXT("rotation"), RotationArray);
    
    FVector Scale = Actor->GetActorScale3D();
    TArray<TSharedPtr<FJsonValue>> ScaleArray;
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.X));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Y));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Z));
    ActorObject->SetArrayField(TEXT("scale"), ScaleArray);
    
    return ActorObject;
}

bool FUECLICommonUtils::SetObjectProperty(UObject* Object, const FString& PropertyName, 
                                     const TSharedPtr<FJsonValue>& Value, FString& OutErrorMessage)
{
    if (!Object)
    {
        OutErrorMessage = TEXT("Invalid object");
        return false;
    }

    // Handle dot-path properties (e.g., "Settings.WeightedBlendables.Array")
    if (PropertyName.Contains(TEXT(".")))
    {
        TArray<FString> PathParts;
        PropertyName.ParseIntoArray(PathParts, TEXT("."));

        UStruct* CurrentStruct = Object->GetClass();
        void* CurrentContainer = Object;

        for (int32 i = 0; i < PathParts.Num() - 1; ++i)
        {
            FProperty* SubProp = CurrentStruct->FindPropertyByName(*PathParts[i]);
            if (!SubProp)
            {
                OutErrorMessage = FString::Printf(TEXT("Property not found in path: %s (at segment '%s')"), *PropertyName, *PathParts[i]);
                return false;
            }

            FStructProperty* StructProp = CastField<FStructProperty>(SubProp);
            if (!StructProp)
            {
                OutErrorMessage = FString::Printf(TEXT("Property '%s' is not a struct, cannot traverse further"), *PathParts[i]);
                return false;
            }

            CurrentContainer = SubProp->ContainerPtrToValuePtr<void>(CurrentContainer);
            CurrentStruct = StructProp->Struct;
        }

        // Now find and set the final property
        const FString& FinalPropName = PathParts.Last();
        FProperty* FinalProp = CurrentStruct->FindPropertyByName(*FinalPropName);
        if (!FinalProp)
        {
            OutErrorMessage = FString::Printf(TEXT("Final property not found: %s"), *FinalPropName);
            return false;
        }

        void* FinalAddr = FinalProp->ContainerPtrToValuePtr<void>(CurrentContainer);
        FText ImportError;
        if (FJsonObjectConverter::JsonValueToUProperty(Value, FinalProp, FinalAddr, 0, 0, false, &ImportError))
        {
            return true;
        }
        OutErrorMessage = FString::Printf(TEXT("Failed to set dot-path property '%s': %s"), *PropertyName, *ImportError.ToString());
        return false;
    }

    FProperty* Property = Object->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property)
    {
        OutErrorMessage = FString::Printf(TEXT("Property not found: %s"), *PropertyName);
        return false;
    }

    void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(Object);

    // Prefer the engine JSON->property importer first; it supports
    // structs/arrays/maps/object refs/gameplay tags and most reflected types.
    FText ImportError;
    if (FJsonObjectConverter::JsonValueToUProperty(Value, Property, PropertyAddr, 0, 0, false, &ImportError))
    {
        return true;
    }
    
    // Handle different property types
    if (Property->IsA<FBoolProperty>())
    {
        ((FBoolProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsBool());
        return true;
    }
    else if (Property->IsA<FIntProperty>())
    {
        int32 IntValue = static_cast<int32>(Value->AsNumber());
        FIntProperty* IntProperty = CastField<FIntProperty>(Property);
        if (IntProperty)
        {
            IntProperty->SetPropertyValue_InContainer(Object, IntValue);
            return true;
        }
    }
    else if (Property->IsA<FFloatProperty>())
    {
        ((FFloatProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsNumber());
        return true;
    }
    else if (Property->IsA<FStrProperty>())
    {
        ((FStrProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsString());
        return true;
    }
    else if (Property->IsA<FByteProperty>())
    {
        FByteProperty* ByteProp = CastField<FByteProperty>(Property);
        UEnum* EnumDef = ByteProp ? ByteProp->GetIntPropertyEnum() : nullptr;
        
        // If this is a TEnumAsByte property (has associated enum)
        if (EnumDef)
        {
            // Handle numeric value
            if (Value->Type == EJson::Number)
            {
                uint8 ByteValue = static_cast<uint8>(Value->AsNumber());
                ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
                
                UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric value: %d"), 
                      *PropertyName, ByteValue);
                return true;
            }
            // Handle string enum value
            else if (Value->Type == EJson::String)
            {
                FString EnumValueName = Value->AsString();
                
                // Try to convert numeric string to number first
                if (EnumValueName.IsNumeric())
                {
                    uint8 ByteValue = FCString::Atoi(*EnumValueName);
                    ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric string value: %s -> %d"), 
                          *PropertyName, *EnumValueName, ByteValue);
                    return true;
                }
                
                // Handle qualified enum names (e.g., "Player0" or "EAutoReceiveInput::Player0")
                if (EnumValueName.Contains(TEXT("::")))
                {
                    EnumValueName.Split(TEXT("::"), nullptr, &EnumValueName);
                }
                
                int64 EnumValue = EnumDef->GetValueByNameString(EnumValueName);
                if (EnumValue == INDEX_NONE)
                {
                    // Try with full name as fallback
                    EnumValue = EnumDef->GetValueByNameString(Value->AsString());
                }
                
                if (EnumValue != INDEX_NONE)
                {
                    ByteProp->SetPropertyValue(PropertyAddr, static_cast<uint8>(EnumValue));
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to name value: %s -> %lld"), 
                          *PropertyName, *EnumValueName, EnumValue);
                    return true;
                }
                else
                {
                    // Log all possible enum values for debugging
                    UE_LOG(LogTemp, Warning, TEXT("Could not find enum value for '%s'. Available options:"), *EnumValueName);
                    for (int32 i = 0; i < EnumDef->NumEnums(); i++)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("  - %s (value: %d)"), 
                               *EnumDef->GetNameStringByIndex(i), EnumDef->GetValueByIndex(i));
                    }
                    
                    OutErrorMessage = FString::Printf(TEXT("Could not find enum value for '%s'"), *EnumValueName);
                    return false;
                }
            }
        }
        else
        {
            // Regular byte property
            uint8 ByteValue = static_cast<uint8>(Value->AsNumber());
            ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
            return true;
        }
    }
    else if (Property->IsA<FEnumProperty>())
    {
        FEnumProperty* EnumProp = CastField<FEnumProperty>(Property);
        UEnum* EnumDef = EnumProp ? EnumProp->GetEnum() : nullptr;
        FNumericProperty* UnderlyingNumericProp = EnumProp ? EnumProp->GetUnderlyingProperty() : nullptr;
        
        if (EnumDef && UnderlyingNumericProp)
        {
            // Handle numeric value
            if (Value->Type == EJson::Number)
            {
                int64 EnumValue = static_cast<int64>(Value->AsNumber());
                UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
                
                UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric value: %lld"), 
                      *PropertyName, EnumValue);
                return true;
            }
            // Handle string enum value
            else if (Value->Type == EJson::String)
            {
                FString EnumValueName = Value->AsString();
                
                // Try to convert numeric string to number first
                if (EnumValueName.IsNumeric())
                {
                    int64 EnumValue = FCString::Atoi64(*EnumValueName);
                    UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric string value: %s -> %lld"), 
                          *PropertyName, *EnumValueName, EnumValue);
                    return true;
                }
                
                // Handle qualified enum names
                if (EnumValueName.Contains(TEXT("::")))
                {
                    EnumValueName.Split(TEXT("::"), nullptr, &EnumValueName);
                }
                
                int64 EnumValue = EnumDef->GetValueByNameString(EnumValueName);
                if (EnumValue == INDEX_NONE)
                {
                    // Try with full name as fallback
                    EnumValue = EnumDef->GetValueByNameString(Value->AsString());
                }
                
                if (EnumValue != INDEX_NONE)
                {
                    UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to name value: %s -> %lld"), 
                          *PropertyName, *EnumValueName, EnumValue);
                    return true;
                }
                else
                {
                    // Log all possible enum values for debugging
                    UE_LOG(LogTemp, Warning, TEXT("Could not find enum value for '%s'. Available options:"), *EnumValueName);
                    for (int32 i = 0; i < EnumDef->NumEnums(); i++)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("  - %s (value: %d)"), 
                               *EnumDef->GetNameStringByIndex(i), EnumDef->GetValueByIndex(i));
                    }
                    
                    OutErrorMessage = FString::Printf(TEXT("Could not find enum value for '%s'"), *EnumValueName);
                    return false;
                }
            }
        }
    }
    
    if (!ImportError.IsEmpty())
    {
        OutErrorMessage = FString::Printf(TEXT("Failed to import property '%s': %s"), *PropertyName, *ImportError.ToString());
    }
    else
    {
        OutErrorMessage = FString::Printf(TEXT("Unsupported property type: %s for property %s"),
            *Property->GetClass()->GetName(), *PropertyName);
    }
    return false;
}

TSharedPtr<FJsonValue> FUECLICommonUtils::SerializePropertyValue(FProperty* Property, const void* ValuePtr, int32 CurrentDepth, int32 MaxDepth)
{
    if (!Property || !ValuePtr)
    {
        return MakeShared<FJsonValueNull>();
    }

    // Bool
    if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
    {
        return MakeShared<FJsonValueBoolean>(BoolProp->GetPropertyValue(ValuePtr));
    }
    // Int
    else if (const FIntProperty* IntProp = CastField<FIntProperty>(Property))
    {
        return MakeShared<FJsonValueNumber>(IntProp->GetPropertyValue(ValuePtr));
    }
    // Int64
    else if (const FInt64Property* Int64Prop = CastField<FInt64Property>(Property))
    {
        return MakeShared<FJsonValueNumber>(static_cast<double>(Int64Prop->GetPropertyValue(ValuePtr)));
    }
    // Float
    else if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
    {
        return MakeShared<FJsonValueNumber>(FloatProp->GetPropertyValue(ValuePtr));
    }
    // Double
    else if (const FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
    {
        return MakeShared<FJsonValueNumber>(DoubleProp->GetPropertyValue(ValuePtr));
    }
    // String
    else if (const FStrProperty* StrProp = CastField<FStrProperty>(Property))
    {
        return MakeShared<FJsonValueString>(StrProp->GetPropertyValue(ValuePtr));
    }
    // Name
    else if (const FNameProperty* NameProp = CastField<FNameProperty>(Property))
    {
        return MakeShared<FJsonValueString>(NameProp->GetPropertyValue(ValuePtr).ToString());
    }
    // Text
    else if (const FTextProperty* TextProp = CastField<FTextProperty>(Property))
    {
        return MakeShared<FJsonValueString>(TextProp->GetPropertyValue(ValuePtr).ToString());
    }
    // Byte (including TEnumAsByte)
    else if (const FByteProperty* ByteProp = CastField<FByteProperty>(Property))
    {
        UEnum* EnumDef = ByteProp->GetIntPropertyEnum();
        uint8 ByteValue = ByteProp->GetPropertyValue(ValuePtr);
        if (EnumDef)
        {
            FString EnumName = EnumDef->GetNameStringByValue(ByteValue);
            return MakeShared<FJsonValueString>(EnumName);
        }
        return MakeShared<FJsonValueNumber>(ByteValue);
    }
    // Enum
    else if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
    {
        UEnum* EnumDef = EnumProp->GetEnum();
        FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
        if (EnumDef && UnderlyingProp)
        {
            int64 EnumValue = UnderlyingProp->GetSignedIntPropertyValue(ValuePtr);
            FString EnumName = EnumDef->GetNameStringByValue(EnumValue);
            return MakeShared<FJsonValueString>(EnumName);
        }
        return MakeShared<FJsonValueNull>();
    }
    // Struct
    else if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
    {
        if (CurrentDepth >= MaxDepth)
        {
            return MakeShared<FJsonValueString>(FString::Printf(TEXT("[Struct: %s]"), *StructProp->Struct->GetName()));
        }
        
        UScriptStruct* Struct = StructProp->Struct;
        
        // Handle common struct types specially
        if (Struct == TBaseStructure<FVector>::Get())
        {
            const FVector* Vec = static_cast<const FVector*>(ValuePtr);
            TArray<TSharedPtr<FJsonValue>> Arr;
            Arr.Add(MakeShared<FJsonValueNumber>(Vec->X));
            Arr.Add(MakeShared<FJsonValueNumber>(Vec->Y));
            Arr.Add(MakeShared<FJsonValueNumber>(Vec->Z));
            return MakeShared<FJsonValueArray>(Arr);
        }
        else if (Struct == TBaseStructure<FVector2D>::Get())
        {
            const FVector2D* Vec = static_cast<const FVector2D*>(ValuePtr);
            TArray<TSharedPtr<FJsonValue>> Arr;
            Arr.Add(MakeShared<FJsonValueNumber>(Vec->X));
            Arr.Add(MakeShared<FJsonValueNumber>(Vec->Y));
            return MakeShared<FJsonValueArray>(Arr);
        }
        else if (Struct == TBaseStructure<FRotator>::Get())
        {
            const FRotator* Rot = static_cast<const FRotator*>(ValuePtr);
            TArray<TSharedPtr<FJsonValue>> Arr;
            Arr.Add(MakeShared<FJsonValueNumber>(Rot->Pitch));
            Arr.Add(MakeShared<FJsonValueNumber>(Rot->Yaw));
            Arr.Add(MakeShared<FJsonValueNumber>(Rot->Roll));
            return MakeShared<FJsonValueArray>(Arr);
        }
        else if (Struct == TBaseStructure<FLinearColor>::Get())
        {
            const FLinearColor* Color = static_cast<const FLinearColor*>(ValuePtr);
            TSharedPtr<FJsonObject> ColorObj = MakeShared<FJsonObject>();
            ColorObj->SetNumberField(TEXT("R"), Color->R);
            ColorObj->SetNumberField(TEXT("G"), Color->G);
            ColorObj->SetNumberField(TEXT("B"), Color->B);
            ColorObj->SetNumberField(TEXT("A"), Color->A);
            return MakeShared<FJsonValueObject>(ColorObj);
        }
        else if (Struct == TBaseStructure<FColor>::Get())
        {
            const FColor* Color = static_cast<const FColor*>(ValuePtr);
            TSharedPtr<FJsonObject> ColorObj = MakeShared<FJsonObject>();
            ColorObj->SetNumberField(TEXT("R"), Color->R);
            ColorObj->SetNumberField(TEXT("G"), Color->G);
            ColorObj->SetNumberField(TEXT("B"), Color->B);
            ColorObj->SetNumberField(TEXT("A"), Color->A);
            return MakeShared<FJsonValueObject>(ColorObj);
        }
        else if (Struct == TBaseStructure<FTransform>::Get())
        {
            const FTransform* Transform = static_cast<const FTransform*>(ValuePtr);
            TSharedPtr<FJsonObject> TransformObj = MakeShared<FJsonObject>();
            
            FVector Location = Transform->GetLocation();
            TArray<TSharedPtr<FJsonValue>> LocArr;
            LocArr.Add(MakeShared<FJsonValueNumber>(Location.X));
            LocArr.Add(MakeShared<FJsonValueNumber>(Location.Y));
            LocArr.Add(MakeShared<FJsonValueNumber>(Location.Z));
            TransformObj->SetArrayField(TEXT("location"), LocArr);
            
            FRotator Rotation = Transform->Rotator();
            TArray<TSharedPtr<FJsonValue>> RotArr;
            RotArr.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
            RotArr.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
            RotArr.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
            TransformObj->SetArrayField(TEXT("rotation"), RotArr);
            
            FVector Scale = Transform->GetScale3D();
            TArray<TSharedPtr<FJsonValue>> ScaleArr;
            ScaleArr.Add(MakeShared<FJsonValueNumber>(Scale.X));
            ScaleArr.Add(MakeShared<FJsonValueNumber>(Scale.Y));
            ScaleArr.Add(MakeShared<FJsonValueNumber>(Scale.Z));
            TransformObj->SetArrayField(TEXT("scale"), ScaleArr);
            
            return MakeShared<FJsonValueObject>(TransformObj);
        }
        
        // Generic struct serialization
        TSharedPtr<FJsonObject> StructObj = SerializeStructProperties(Struct, ValuePtr, CurrentDepth + 1, MaxDepth);
        return MakeShared<FJsonValueObject>(StructObj);
    }
    // Array
    else if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
    {
        if (CurrentDepth >= MaxDepth)
        {
            return MakeShared<FJsonValueString>(TEXT("[Array]"));
        }
        
        FScriptArrayHelper ArrayHelper(ArrayProp, ValuePtr);
        TArray<TSharedPtr<FJsonValue>> JsonArray;
        
        for (int32 i = 0; i < ArrayHelper.Num(); ++i)
        {
            TSharedPtr<FJsonValue> ElementValue = SerializePropertyValue(
                ArrayProp->Inner, 
                ArrayHelper.GetRawPtr(i), 
                CurrentDepth + 1, 
                MaxDepth
            );
            JsonArray.Add(ElementValue);
        }
        
        return MakeShared<FJsonValueArray>(JsonArray);
    }
    // Object reference
    else if (const FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
    {
        UObject* Obj = ObjProp->GetObjectPropertyValue(ValuePtr);
        if (Obj)
        {
            return MakeShared<FJsonValueString>(Obj->GetPathName());
        }
        return MakeShared<FJsonValueNull>();
    }
    // Soft object reference
    else if (const FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(Property))
    {
        const FSoftObjectPtr* SoftPtr = static_cast<const FSoftObjectPtr*>(ValuePtr);
        if (SoftPtr)
        {
            return MakeShared<FJsonValueString>(SoftPtr->ToString());
        }
        return MakeShared<FJsonValueNull>();
    }
    // Class reference
    else if (const FClassProperty* ClassProp = CastField<FClassProperty>(Property))
    {
        UClass* ClassValue = Cast<UClass>(ClassProp->GetObjectPropertyValue(ValuePtr));
        if (ClassValue)
        {
            return MakeShared<FJsonValueString>(ClassValue->GetPathName());
        }
        return MakeShared<FJsonValueNull>();
    }
    
    // Unsupported type - return type name
    return MakeShared<FJsonValueString>(FString::Printf(TEXT("[%s]"), *Property->GetCPPType()));
}

TSharedPtr<FJsonObject> FUECLICommonUtils::SerializeStructProperties(UScriptStruct* Struct, const void* StructPtr, int32 CurrentDepth, int32 MaxDepth)
{
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    
    if (!Struct || !StructPtr)
    {
        return ResultObj;
    }
    
    for (TFieldIterator<FProperty> PropIt(Struct); PropIt; ++PropIt)
    {
        FProperty* Property = *PropIt;
        
        // Skip deprecated and transient properties
        if (Property->HasAnyPropertyFlags(CPF_Deprecated | CPF_Transient))
        {
            continue;
        }
        
        const void* PropertyValuePtr = Property->ContainerPtrToValuePtr<void>(StructPtr);
        TSharedPtr<FJsonValue> JsonValue = SerializePropertyValue(Property, PropertyValuePtr, CurrentDepth, MaxDepth);
        
        ResultObj->SetField(Property->GetName(), JsonValue);
    }
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLICommonUtils::SerializeObjectProperties(UObject* Object, int32 MaxDepth, bool bIncludeInherited)
{
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    
    if (!Object)
    {
        return ResultObj;
    }
    
    ResultObj->SetStringField(TEXT("name"), Object->GetName());
    ResultObj->SetStringField(TEXT("class"), Object->GetClass()->GetName());
    
    TSharedPtr<FJsonObject> PropertiesObj = MakeShared<FJsonObject>();
    
    // Iterate through all properties
    for (TFieldIterator<FProperty> PropIt(Object->GetClass(), bIncludeInherited ? EFieldIteratorFlags::IncludeSuper : EFieldIteratorFlags::ExcludeSuper); PropIt; ++PropIt)
    {
        FProperty* Property = *PropIt;
        
        // Only include editable/visible properties to avoid internal engine properties
        if (!Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
        {
            continue;
        }
        
        // Skip deprecated properties
        if (Property->HasAnyPropertyFlags(CPF_Deprecated))
        {
            continue;
        }
        
        const void* PropertyValuePtr = Property->ContainerPtrToValuePtr<void>(Object);
        TSharedPtr<FJsonValue> JsonValue = SerializePropertyValue(Property, PropertyValuePtr, 0, MaxDepth);
        
        PropertiesObj->SetField(Property->GetName(), JsonValue);
    }
    
    ResultObj->SetObjectField(TEXT("properties"), PropertiesObj);
    
    // Check serialized size and add warning if too large
    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
    FJsonSerializer::Serialize(ResultObj.ToSharedRef(), Writer);
    
    int32 DataSize = JsonString.Len();
    ResultObj->SetNumberField(TEXT("_serialized_size_bytes"), DataSize);
    
    // Warn if data is larger than 1MB
    const int32 WarnThreshold = 1024 * 1024;  // 1MB
    if (DataSize > WarnThreshold)
    {
        UE_LOG(LogTemp, Warning, TEXT("SerializeObjectProperties: Large data size detected: %d bytes (%.2f MB) for object %s"), 
               DataSize, DataSize / (1024.0f * 1024.0f), *Object->GetName());
        ResultObj->SetStringField(TEXT("_warning"), FString::Printf(TEXT("Large data size: %.2f MB"), DataSize / (1024.0f * 1024.0f)));
    }
    
    // Hard limit at 50MB
    const int32 MaxDataSize = 50 * 1024 * 1024;  // 50MB
    if (DataSize > MaxDataSize)
    {
        UE_LOG(LogTemp, Error, TEXT("SerializeObjectProperties: Data size exceeds limit: %d bytes for object %s"), 
               DataSize, *Object->GetName());
        
        // Return error response instead
        TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
        ErrorObj->SetStringField(TEXT("error"), FString::Printf(TEXT("Data too large: %.2f MB exceeds 50MB limit. Try reducing max_depth or using property_filter."), DataSize / (1024.0f * 1024.0f)));
        ErrorObj->SetStringField(TEXT("name"), Object->GetName());
        ErrorObj->SetStringField(TEXT("class"), Object->GetClass()->GetName());
        return ErrorObj;
    }
    
    return ResultObj;
} 
