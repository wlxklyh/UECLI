#include "Commands/UECLIMaterialCommands.h"
#include "Commands/UECLICommonUtils.h"
#include "AssetCompilingManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialExpressionStaticBool.h"
#include "Materials/MaterialExpressionStaticSwitch.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionReroute.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialExpressionBreakMaterialAttributes.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionIf.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionRotator.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionCosine.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionCameraPositionWS.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionObjectPositionWS.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionPixelNormalWS.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialFunctionFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "EditorAssetLibrary.h"
#include "MaterialEditingLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "ShaderCompiler.h"
#include "Components/PrimitiveComponent.h"
#include "Components/MeshComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"
#include "ToolRegistry/UECLIToolRegistry.h"

namespace
{
	TSharedRef<FUECLIMaterialCommands> GetMaterialCommands()
	{
		static TSharedRef<FUECLIMaterialCommands> Instance = MakeShared<FUECLIMaterialCommands>();
		return Instance;
	}

	void RegisterMaterialCommand(FUECLIToolRegistry& Registry, const TCHAR* PublicName, const TCHAR* InternalName = nullptr)
	{
		const TSharedRef<FUECLIMaterialCommands> Handler = GetMaterialCommands();
		const FString RoutedName = InternalName != nullptr ? InternalName : PublicName;
		Registry.Register(
			FUECLIToolSchema(PublicName, TEXT("Material"), FString()),
			[Handler, RoutedName](const TSharedPtr<FJsonObject>& Params)
			{
				return Handler->HandleCommand(RoutedName, Params);
			});
	}

	void RegisterUnsupportedMaterialCommand(FUECLIToolRegistry& Registry, const TCHAR* Name)
	{
		const FString ErrorMessage = FString::Printf(TEXT("%s is not implemented in UECLI yet"), Name);
		Registry.Register(
			FUECLIToolSchema(Name, TEXT("Material"), ErrorMessage),
			[ErrorMessage](const TSharedPtr<FJsonObject>&)
			{
				return FUECLICommonUtils::CreateErrorResponse(ErrorMessage);
			});
	}

	bool TryGetMaterialPath(const TSharedPtr<FJsonObject>& Params, FString& OutMaterialPath)
	{
		return Params->TryGetStringField(TEXT("material_path"), OutMaterialPath)
			|| Params->TryGetStringField(TEXT("path"), OutMaterialPath);
	}

	bool TryGetFunctionPath(const TSharedPtr<FJsonObject>& Params, FString& OutFunctionPath)
	{
		return Params->TryGetStringField(TEXT("function_path"), OutFunctionPath)
			|| Params->TryGetStringField(TEXT("path"), OutFunctionPath);
	}

	UMaterial* LoadMaterialForEditing(const FString& MaterialPath)
	{
		return Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
	}

	UMaterialFunction* LoadMaterialFunctionForEditing(const FString& FunctionPath)
	{
		return Cast<UMaterialFunction>(UEditorAssetLibrary::LoadAsset(FunctionPath));
	}

	FString GetAssetPackagePath(const UObject* Asset)
	{
		return Asset != nullptr && Asset->GetOutermost() != nullptr
			? Asset->GetOutermost()->GetName()
			: FString();
	}

	UMaterialExpression* FindMaterialExpressionByName(UMaterial* Material, const FString& NodeName)
	{
		if (!Material || NodeName.IsEmpty())
		{
			return nullptr;
		}

		for (UMaterialExpression* Expression : Material->GetExpressions())
		{
			if (Expression && Expression->GetName().Equals(NodeName, ESearchCase::CaseSensitive))
			{
				return Expression;
			}
		}

		for (UMaterialExpression* Expression : Material->GetExpressions())
		{
			if (Expression && Expression->GetName().Equals(NodeName, ESearchCase::IgnoreCase))
			{
				return Expression;
			}
		}

		return nullptr;
	}

	UMaterialExpression* FindMaterialExpressionByReference(UMaterial* Material, const FString& NodeReference)
	{
		if (!Material || NodeReference.IsEmpty())
		{
			return nullptr;
		}

		for (UMaterialExpression* Expression : Material->GetExpressions())
		{
			if (Expression != nullptr && Expression->GetMaterialExpressionId().ToString(EGuidFormats::Digits).Equals(NodeReference, ESearchCase::IgnoreCase))
			{
				return Expression;
			}
		}

		return FindMaterialExpressionByName(Material, NodeReference);
	}

	UMaterialExpression* FindMaterialFunctionExpressionByReference(UMaterialFunction* MaterialFunction, const FString& NodeReference)
	{
		if (!MaterialFunction || NodeReference.IsEmpty())
		{
			return nullptr;
		}

		for (UMaterialExpression* Expression : MaterialFunction->GetExpressions())
		{
			if (Expression != nullptr && Expression->GetMaterialExpressionId().ToString(EGuidFormats::Digits).Equals(NodeReference, ESearchCase::IgnoreCase))
			{
				return Expression;
			}
		}

		for (UMaterialExpression* Expression : MaterialFunction->GetExpressions())
		{
			if (Expression != nullptr && Expression->GetName().Equals(NodeReference, ESearchCase::CaseSensitive))
			{
				return Expression;
			}
		}

		for (UMaterialExpression* Expression : MaterialFunction->GetExpressions())
		{
			if (Expression != nullptr && Expression->GetName().Equals(NodeReference, ESearchCase::IgnoreCase))
			{
				return Expression;
			}
		}

		return nullptr;
	}

	bool ResetExpressionInput(FExpressionInput* Input)
	{
		if (Input == nullptr || Input->Expression == nullptr)
		{
			return false;
		}

		Input->Expression = nullptr;
		Input->OutputIndex = 0;
		Input->Mask = 0;
		Input->MaskR = 0;
		Input->MaskG = 0;
		Input->MaskB = 0;
		Input->MaskA = 0;
		return true;
	}

	bool TryResolveExpressionInput(UMaterialExpression* Expression, const FString& InputName, int32& OutInputIndex, FExpressionInput*& OutInput)
	{
		OutInputIndex = INDEX_NONE;
		OutInput = nullptr;
		if (Expression == nullptr)
		{
			return false;
		}

		if (InputName.IsEmpty())
		{
			OutInputIndex = 0;
			OutInput = Expression->GetInput(0);
			return OutInput != nullptr;
		}

		for (int32 InputIndex = 0; ; ++InputIndex)
		{
			FExpressionInput* Input = Expression->GetInput(InputIndex);
			if (Input == nullptr)
			{
				break;
			}

			const FName Name = Expression->GetInputName(InputIndex);
			if (Name.ToString().Equals(InputName, ESearchCase::IgnoreCase))
			{
				OutInputIndex = InputIndex;
				OutInput = Input;
				return true;
			}
		}

		return false;
	}

	bool TryGetNumberField(const TSharedPtr<FJsonObject>& Params, const TCHAR* FieldName, float& OutValue)
	{
		double Value = 0.0;
		if (!Params->TryGetNumberField(FieldName, Value))
		{
			return false;
		}

		OutValue = static_cast<float>(Value);
		return true;
	}

	bool TryGetLinearColorField(const TSharedPtr<FJsonObject>& Params, const TCHAR* FieldName, FLinearColor& OutColor)
	{
		const TArray<TSharedPtr<FJsonValue>>* ArrayValue = nullptr;
		if (!Params->TryGetArrayField(FieldName, ArrayValue) || ArrayValue == nullptr || ArrayValue->Num() < 3)
		{
			return false;
		}

		OutColor = FLinearColor(
			static_cast<float>((*ArrayValue)[0]->AsNumber()),
			static_cast<float>((*ArrayValue)[1]->AsNumber()),
			static_cast<float>((*ArrayValue)[2]->AsNumber()),
			ArrayValue->Num() >= 4 ? static_cast<float>((*ArrayValue)[3]->AsNumber()) : 1.0f);
		return true;
	}

	bool TryResolveMaterialProperty(const FString& PropertyName, EMaterialProperty& OutProperty)
	{
		static const TMap<FString, EMaterialProperty> PropertyMap =
		{
			{TEXT("basecolor"), MP_BaseColor},
			{TEXT("metallic"), MP_Metallic},
			{TEXT("specular"), MP_Specular},
			{TEXT("roughness"), MP_Roughness},
			{TEXT("anisotropy"), MP_Anisotropy},
			{TEXT("emissivecolor"), MP_EmissiveColor},
			{TEXT("emissive"), MP_EmissiveColor},
			{TEXT("opacity"), MP_Opacity},
			{TEXT("opacitymask"), MP_OpacityMask},
			{TEXT("normal"), MP_Normal},
			{TEXT("tangent"), MP_Tangent},
			{TEXT("worldpositionoffset"), MP_WorldPositionOffset},
			{TEXT("ambientocclusion"), MP_AmbientOcclusion},
			{TEXT("refraction"), MP_Refraction},
			{TEXT("pixeldepthoffset"), MP_PixelDepthOffset},
			{TEXT("subsurfacecolor"), MP_SubsurfaceColor},
			{TEXT("materialattributes"), MP_MaterialAttributes}
		};

		if (const EMaterialProperty* Found = PropertyMap.Find(PropertyName.ToLower()))
		{
			OutProperty = *Found;
			return true;
		}

		return false;
	}

	bool TryResolveBlendMode(const FString& BlendModeName, EBlendMode& OutBlendMode)
	{
		static const TMap<FString, EBlendMode> BlendModeMap =
		{
			{TEXT("opaque"), BLEND_Opaque},
			{TEXT("masked"), BLEND_Masked},
			{TEXT("translucent"), BLEND_Translucent},
			{TEXT("additive"), BLEND_Additive},
			{TEXT("modulate"), BLEND_Modulate},
			{TEXT("alphacomposite"), BLEND_AlphaComposite},
			{TEXT("alphaholdout"), BLEND_AlphaHoldout}
		};

		if (const EBlendMode* Found = BlendModeMap.Find(BlendModeName.ToLower()))
		{
			OutBlendMode = *Found;
			return true;
		}

		return false;
	}

	FString BlendModeToString(const EBlendMode BlendMode)
	{
		switch (BlendMode)
		{
		case BLEND_Opaque: return TEXT("Opaque");
		case BLEND_Masked: return TEXT("Masked");
		case BLEND_Translucent: return TEXT("Translucent");
		case BLEND_Additive: return TEXT("Additive");
		case BLEND_Modulate: return TEXT("Modulate");
		case BLEND_AlphaComposite: return TEXT("AlphaComposite");
		case BLEND_AlphaHoldout: return TEXT("AlphaHoldout");
		default: return TEXT("Unknown");
		}
	}

	bool TryResolveShadingModel(const FString& ShadingModelName, EMaterialShadingModel& OutShadingModel)
	{
		static const TMap<FString, EMaterialShadingModel> ShadingModelMap =
		{
			{TEXT("defaultlit"), MSM_DefaultLit},
			{TEXT("unlit"), MSM_Unlit},
			{TEXT("subsurface"), MSM_Subsurface},
			{TEXT("preintegratedskin"), MSM_PreintegratedSkin},
			{TEXT("clearcoat"), MSM_ClearCoat},
			{TEXT("subsurfaceprofile"), MSM_SubsurfaceProfile},
			{TEXT("twosidedfoliage"), MSM_TwoSidedFoliage},
			{TEXT("hair"), MSM_Hair},
			{TEXT("cloth"), MSM_Cloth},
			{TEXT("eye"), MSM_Eye},
			{TEXT("singlelayerwater"), MSM_SingleLayerWater},
			{TEXT("thintranslucent"), MSM_ThinTranslucent}
		};

		if (const EMaterialShadingModel* Found = ShadingModelMap.Find(ShadingModelName.ToLower()))
		{
			OutShadingModel = *Found;
			return true;
		}

		return false;
	}

	FString ShadingModelToString(const EMaterialShadingModel ShadingModel)
	{
		switch (ShadingModel)
		{
		case MSM_DefaultLit: return TEXT("DefaultLit");
		case MSM_Unlit: return TEXT("Unlit");
		case MSM_Subsurface: return TEXT("Subsurface");
		case MSM_PreintegratedSkin: return TEXT("PreintegratedSkin");
		case MSM_ClearCoat: return TEXT("ClearCoat");
		case MSM_SubsurfaceProfile: return TEXT("SubsurfaceProfile");
		case MSM_TwoSidedFoliage: return TEXT("TwoSidedFoliage");
		case MSM_Hair: return TEXT("Hair");
		case MSM_Cloth: return TEXT("Cloth");
		case MSM_Eye: return TEXT("Eye");
		case MSM_SingleLayerWater: return TEXT("SingleLayerWater");
		case MSM_ThinTranslucent: return TEXT("ThinTranslucent");
		default: return TEXT("Custom");
		}
	}

	bool TryResolveMaterialDomain(const FString& MaterialDomainName, EMaterialDomain& OutMaterialDomain)
	{
		static const TMap<FString, EMaterialDomain> DomainMap =
		{
			{TEXT("surface"), MD_Surface},
			{TEXT("deferreddecal"), MD_DeferredDecal},
			{TEXT("lightfunction"), MD_LightFunction},
			{TEXT("volume"), MD_Volume},
			{TEXT("postprocess"), MD_PostProcess},
			{TEXT("ui"), MD_UI},
			{TEXT("runtimevirtualtexture"), MD_RuntimeVirtualTexture}
		};

		if (const EMaterialDomain* Found = DomainMap.Find(MaterialDomainName.ToLower()))
		{
			OutMaterialDomain = *Found;
			return true;
		}

		return false;
	}

	FString MaterialDomainToString(const EMaterialDomain MaterialDomain)
	{
		switch (MaterialDomain)
		{
		case MD_Surface: return TEXT("Surface");
		case MD_DeferredDecal: return TEXT("DeferredDecal");
		case MD_LightFunction: return TEXT("LightFunction");
		case MD_Volume: return TEXT("Volume");
		case MD_PostProcess: return TEXT("PostProcess");
		case MD_UI: return TEXT("UI");
		case MD_RuntimeVirtualTexture: return TEXT("RuntimeVirtualTexture");
		default: return TEXT("Unknown");
		}
	}

	bool TryResolvePositionOrigin(const FString& OriginName, EPositionOrigin& OutOrigin)
	{
		static const TMap<FString, EPositionOrigin> OriginMap =
		{
			{TEXT("absolute"), EPositionOrigin::Absolute},
			{TEXT("absoluteworldposition"), EPositionOrigin::Absolute},
			{TEXT("camerarelative"), EPositionOrigin::CameraRelative},
			{TEXT("camerarelativeworldposition"), EPositionOrigin::CameraRelative}
		};

		if (const EPositionOrigin* Found = OriginMap.Find(OriginName.ToLower()))
		{
			OutOrigin = *Found;
			return true;
		}

		return false;
	}

	FString PositionOriginToString(const EPositionOrigin Origin)
	{
		switch (Origin)
		{
		case EPositionOrigin::Absolute: return TEXT("Absolute");
		case EPositionOrigin::CameraRelative: return TEXT("CameraRelative");
		default: return TEXT("Unknown");
		}
	}

	bool TryResolveFunctionInputType(const FString& InputTypeName, EFunctionInputType& OutInputType)
	{
		static const TMap<FString, EFunctionInputType> InputTypeMap =
		{
			{TEXT("scalar"), FunctionInput_Scalar},
			{TEXT("vector2"), FunctionInput_Vector2},
			{TEXT("vector3"), FunctionInput_Vector3},
			{TEXT("vector4"), FunctionInput_Vector4},
			{TEXT("texture2d"), FunctionInput_Texture2D},
			{TEXT("texturecube"), FunctionInput_TextureCube},
			{TEXT("texture2darray"), FunctionInput_Texture2DArray},
			{TEXT("volumetexture"), FunctionInput_VolumeTexture},
			{TEXT("staticbool"), FunctionInput_StaticBool},
			{TEXT("materialattributes"), FunctionInput_MaterialAttributes},
			{TEXT("textureexternal"), FunctionInput_TextureExternal},
			{TEXT("bool"), FunctionInput_Bool},
			{TEXT("substrate"), FunctionInput_Substrate}
		};

		if (const EFunctionInputType* Found = InputTypeMap.Find(InputTypeName.ToLower()))
		{
			OutInputType = *Found;
			return true;
		}

		return false;
	}

	TSubclassOf<UMaterialExpression> ResolveExpressionClass(const FString& NodeType)
	{
		static const TMap<FString, TSubclassOf<UMaterialExpression>> NodeTypeMap =
		{
			{TEXT("constant"), UMaterialExpressionConstant::StaticClass()},
			{TEXT("constant2vector"), UMaterialExpressionConstant2Vector::StaticClass()},
			{TEXT("constant3vector"), UMaterialExpressionConstant3Vector::StaticClass()},
			{TEXT("constant4vector"), UMaterialExpressionConstant4Vector::StaticClass()},
			{TEXT("scalarparameter"), UMaterialExpressionScalarParameter::StaticClass()},
			{TEXT("vectorparameter"), UMaterialExpressionVectorParameter::StaticClass()},
			{TEXT("texturesample"), UMaterialExpressionTextureSample::StaticClass()},
			{TEXT("texturecoordinate"), UMaterialExpressionTextureCoordinate::StaticClass()},
			{TEXT("add"), UMaterialExpressionAdd::StaticClass()},
			{TEXT("subtract"), UMaterialExpressionSubtract::StaticClass()},
			{TEXT("multiply"), UMaterialExpressionMultiply::StaticClass()},
			{TEXT("divide"), UMaterialExpressionDivide::StaticClass()},
			{TEXT("linearinterpolate"), UMaterialExpressionLinearInterpolate::StaticClass()},
			{TEXT("lerp"), UMaterialExpressionLinearInterpolate::StaticClass()},
			{TEXT("materialfunctioncall"), UMaterialExpressionMaterialFunctionCall::StaticClass()},
			{TEXT("staticbool"), UMaterialExpressionStaticBool::StaticClass()},
			{TEXT("staticswitch"), UMaterialExpressionStaticSwitch::StaticClass()},
			{TEXT("staticboolparameter"), UMaterialExpressionStaticBoolParameter::StaticClass()},
			{TEXT("reroute"), UMaterialExpressionReroute::StaticClass()},
			{TEXT("makematerialattributes"), UMaterialExpressionMakeMaterialAttributes::StaticClass()},
			{TEXT("breakmaterialattributes"), UMaterialExpressionBreakMaterialAttributes::StaticClass()},
			{TEXT("textureobjectparameter"), UMaterialExpressionTextureObjectParameter::StaticClass()},
			{TEXT("texturesampleparameter2d"), UMaterialExpressionTextureSampleParameter2D::StaticClass()},
			{TEXT("comment"), UMaterialExpressionComment::StaticClass()},
			{TEXT("functioninput"), UMaterialExpressionFunctionInput::StaticClass()},
			{TEXT("functionoutput"), UMaterialExpressionFunctionOutput::StaticClass()},
			{TEXT("appendvector"), UMaterialExpressionAppendVector::StaticClass()},
			{TEXT("componentmask"), UMaterialExpressionComponentMask::StaticClass()},
			{TEXT("if"), UMaterialExpressionIf::StaticClass()},
			{TEXT("fresnel"), UMaterialExpressionFresnel::StaticClass()},
			{TEXT("power"), UMaterialExpressionPower::StaticClass()},
			{TEXT("clamp"), UMaterialExpressionClamp::StaticClass()},
			{TEXT("oneminus"), UMaterialExpressionOneMinus::StaticClass()},
			{TEXT("time"), UMaterialExpressionTime::StaticClass()},
			{TEXT("panner"), UMaterialExpressionPanner::StaticClass()},
			{TEXT("rotator"), UMaterialExpressionRotator::StaticClass()},
			{TEXT("sine"), UMaterialExpressionSine::StaticClass()},
			{TEXT("cosine"), UMaterialExpressionCosine::StaticClass()},
			{TEXT("dotproduct"), UMaterialExpressionDotProduct::StaticClass()},
			{TEXT("normalize"), UMaterialExpressionNormalize::StaticClass()},
			{TEXT("worldposition"), UMaterialExpressionWorldPosition::StaticClass()},
			{TEXT("camerapositionws"), UMaterialExpressionCameraPositionWS::StaticClass()},
			{TEXT("cameravectorws"), UMaterialExpressionCameraVectorWS::StaticClass()},
			{TEXT("objectpositionws"), UMaterialExpressionObjectPositionWS::StaticClass()},
			{TEXT("vertexnormalws"), UMaterialExpressionVertexNormalWS::StaticClass()},
			{TEXT("pixelnormalws"), UMaterialExpressionPixelNormalWS::StaticClass()},
			{TEXT("vertexcolor"), UMaterialExpressionVertexColor::StaticClass()}
		};

		FString Normalized = NodeType;
		Normalized.RemoveFromStart(TEXT("MaterialExpression"));
		Normalized = Normalized.ToLower();

		if (const TSubclassOf<UMaterialExpression>* Found = NodeTypeMap.Find(Normalized))
		{
			return *Found;
		}

		return nullptr;
	}

	bool InitializeMaterialExpressionFromParams(UMaterialExpression* Expression, const TSharedPtr<FJsonObject>& Params, FString& OutError)
	{
		if (!Expression || !Params.IsValid())
		{
			OutError = TEXT("Invalid material expression");
			return false;
		}

		if (UMaterialExpressionConstant* Constant = Cast<UMaterialExpressionConstant>(Expression))
		{
			float Value = 0.0f;
			if (TryGetNumberField(Params, TEXT("value"), Value))
			{
				Constant->R = Value;
			}
		}
		else if (UMaterialExpressionConstant2Vector* Constant2 = Cast<UMaterialExpressionConstant2Vector>(Expression))
		{
			const TArray<TSharedPtr<FJsonValue>>* ValueArray = nullptr;
			if (Params->TryGetArrayField(TEXT("value"), ValueArray) && ValueArray != nullptr && ValueArray->Num() >= 2)
			{
				Constant2->R = static_cast<float>((*ValueArray)[0]->AsNumber());
				Constant2->G = static_cast<float>((*ValueArray)[1]->AsNumber());
			}
		}
		else if (UMaterialExpressionConstant3Vector* Constant3 = Cast<UMaterialExpressionConstant3Vector>(Expression))
		{
			FLinearColor Color;
			if (TryGetLinearColorField(Params, TEXT("value"), Color))
			{
				Constant3->Constant = Color;
			}
		}
		else if (UMaterialExpressionConstant4Vector* Constant4 = Cast<UMaterialExpressionConstant4Vector>(Expression))
		{
			FLinearColor Color;
			if (TryGetLinearColorField(Params, TEXT("value"), Color))
			{
				Constant4->Constant = Color;
			}
		}
		else if (UMaterialExpressionScalarParameter* ScalarParameter = Cast<UMaterialExpressionScalarParameter>(Expression))
		{
			FString ParameterName;
			if (Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
			{
				ScalarParameter->ParameterName = FName(*ParameterName);
			}

			float Value = 0.0f;
			if (TryGetNumberField(Params, TEXT("value"), Value))
			{
				ScalarParameter->DefaultValue = Value;
			}
		}
		else if (UMaterialExpressionVectorParameter* VectorParameter = Cast<UMaterialExpressionVectorParameter>(Expression))
		{
			FString ParameterName;
			if (Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
			{
				VectorParameter->ParameterName = FName(*ParameterName);
			}

			FLinearColor Color;
			if (TryGetLinearColorField(Params, TEXT("value"), Color))
			{
				VectorParameter->DefaultValue = Color;
			}
		}
		else if (UMaterialExpressionTextureSample* TextureSample = Cast<UMaterialExpressionTextureSample>(Expression))
		{
			FString TexturePath;
			if (Params->TryGetStringField(TEXT("texture_path"), TexturePath))
			{
				UTexture* Texture = Cast<UTexture>(UEditorAssetLibrary::LoadAsset(TexturePath));
				if (!Texture)
				{
					OutError = FString::Printf(TEXT("Texture not found: %s"), *TexturePath);
					return false;
				}

				TextureSample->Texture = Texture;
				TextureSample->AutoSetSampleType();
			}
		}
		else if (UMaterialExpressionTextureCoordinate* TextureCoordinate = Cast<UMaterialExpressionTextureCoordinate>(Expression))
		{
			int32 CoordinateIndex = 0;
			if (Params->TryGetNumberField(TEXT("coordinate_index"), CoordinateIndex))
			{
				TextureCoordinate->CoordinateIndex = CoordinateIndex;
			}

			float UTiling = 0.0f;
			if (TryGetNumberField(Params, TEXT("u_tiling"), UTiling))
			{
				TextureCoordinate->UTiling = UTiling;
			}

			float VTiling = 0.0f;
			if (TryGetNumberField(Params, TEXT("v_tiling"), VTiling))
			{
				TextureCoordinate->VTiling = VTiling;
			}
		}
		else if (UMaterialExpressionPanner* Panner = Cast<UMaterialExpressionPanner>(Expression))
		{
			float SpeedX = 0.0f;
			if (TryGetNumberField(Params, TEXT("speed_x"), SpeedX))
			{
				Panner->SpeedX = SpeedX;
			}

			float SpeedY = 0.0f;
			if (TryGetNumberField(Params, TEXT("speed_y"), SpeedY))
			{
				Panner->SpeedY = SpeedY;
			}

			int32 ConstCoordinate = 0;
			if (Params->TryGetNumberField(TEXT("const_coordinate"), ConstCoordinate))
			{
				Panner->ConstCoordinate = static_cast<uint32>(ConstCoordinate);
			}

			bool bFractionalPart = false;
			if (Params->TryGetBoolField(TEXT("fractional_part"), bFractionalPart))
			{
				Panner->bFractionalPart = bFractionalPart;
			}
		}
		else if (UMaterialExpressionRotator* Rotator = Cast<UMaterialExpressionRotator>(Expression))
		{
			float CenterX = 0.0f;
			if (TryGetNumberField(Params, TEXT("center_x"), CenterX))
			{
				Rotator->CenterX = CenterX;
			}

			float CenterY = 0.0f;
			if (TryGetNumberField(Params, TEXT("center_y"), CenterY))
			{
				Rotator->CenterY = CenterY;
			}

			float Speed = 0.0f;
			if (TryGetNumberField(Params, TEXT("speed"), Speed))
			{
				Rotator->Speed = Speed;
			}

			int32 ConstCoordinate = 0;
			if (Params->TryGetNumberField(TEXT("const_coordinate"), ConstCoordinate))
			{
				Rotator->ConstCoordinate = static_cast<uint32>(ConstCoordinate);
			}
		}
		else if (UMaterialExpressionSine* Sine = Cast<UMaterialExpressionSine>(Expression))
		{
			float Period = 0.0f;
			if (TryGetNumberField(Params, TEXT("period"), Period))
			{
				Sine->Period = Period;
			}
		}
		else if (UMaterialExpressionCosine* Cosine = Cast<UMaterialExpressionCosine>(Expression))
		{
			float Period = 0.0f;
			if (TryGetNumberField(Params, TEXT("period"), Period))
			{
				Cosine->Period = Period;
			}
		}
		else if (UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
		{
			FString FunctionPath;
			if (Params->TryGetStringField(TEXT("material_function_path"), FunctionPath))
			{
				UMaterialFunctionInterface* Function = Cast<UMaterialFunctionInterface>(UEditorAssetLibrary::LoadAsset(FunctionPath));
				if (!Function)
				{
					OutError = FString::Printf(TEXT("Material function not found: %s"), *FunctionPath);
					return false;
				}

				if (!FunctionCall->SetMaterialFunction(Function))
				{
					OutError = FString::Printf(TEXT("Failed to assign material function: %s"), *FunctionPath);
					return false;
				}
			}
		}
		else if (UMaterialExpressionFunctionInput* FunctionInput = Cast<UMaterialExpressionFunctionInput>(Expression))
		{
			FString InputName;
			if (Params->TryGetStringField(TEXT("input_name"), InputName) || Params->TryGetStringField(TEXT("parameter_name"), InputName))
			{
				FunctionInput->InputName = FName(*InputName);
			}

			FString InputDescription;
			if (Params->TryGetStringField(TEXT("description"), InputDescription))
			{
				FunctionInput->Description = InputDescription;
			}

			FString InputTypeName;
			if (Params->TryGetStringField(TEXT("input_type"), InputTypeName))
			{
				EFunctionInputType InputType = FunctionInput_Scalar;
				if (!TryResolveFunctionInputType(InputTypeName, InputType))
				{
					OutError = FString::Printf(TEXT("Unsupported function input type: %s"), *InputTypeName);
					return false;
				}
				FunctionInput->InputType = InputType;
			}

			int32 SortPriority = 0;
			if (Params->TryGetNumberField(TEXT("sort_priority"), SortPriority))
			{
				FunctionInput->SortPriority = SortPriority;
			}

			bool bUsePreviewValueAsDefault = false;
			if (Params->TryGetBoolField(TEXT("use_preview_value_as_default"), bUsePreviewValueAsDefault))
			{
				FunctionInput->bUsePreviewValueAsDefault = bUsePreviewValueAsDefault;
			}

			const TArray<TSharedPtr<FJsonValue>>* ValueArray = nullptr;
			if (Params->TryGetArrayField(TEXT("value"), ValueArray) && ValueArray != nullptr && ValueArray->Num() >= 1)
			{
				FunctionInput->PreviewValue = FVector4f(
					static_cast<float>((*ValueArray)[0]->AsNumber()),
					ValueArray->Num() >= 2 ? static_cast<float>((*ValueArray)[1]->AsNumber()) : 0.0f,
					ValueArray->Num() >= 3 ? static_cast<float>((*ValueArray)[2]->AsNumber()) : 0.0f,
					ValueArray->Num() >= 4 ? static_cast<float>((*ValueArray)[3]->AsNumber()) : 0.0f);
			}

			FunctionInput->ConditionallyGenerateId(true);
			FunctionInput->ValidateName();
		}
		else if (UMaterialExpressionFunctionOutput* FunctionOutput = Cast<UMaterialExpressionFunctionOutput>(Expression))
		{
			FString OutputName;
			if (Params->TryGetStringField(TEXT("output_name"), OutputName) || Params->TryGetStringField(TEXT("parameter_name"), OutputName))
			{
				FunctionOutput->OutputName = FName(*OutputName);
			}

			FString OutputDescription;
			if (Params->TryGetStringField(TEXT("description"), OutputDescription))
			{
				FunctionOutput->Description = OutputDescription;
			}

			int32 SortPriority = 0;
			if (Params->TryGetNumberField(TEXT("sort_priority"), SortPriority))
			{
				FunctionOutput->SortPriority = SortPriority;
			}

			FunctionOutput->ConditionallyGenerateId(true);
			FunctionOutput->ValidateName();
		}
		else if (UMaterialExpressionObjectPositionWS* ObjectPosition = Cast<UMaterialExpressionObjectPositionWS>(Expression))
		{
			FString OriginName;
			if (Params->TryGetStringField(TEXT("origin_type"), OriginName))
			{
				EPositionOrigin Origin = EPositionOrigin::Absolute;
				if (!TryResolvePositionOrigin(OriginName, Origin))
				{
					OutError = FString::Printf(TEXT("Unsupported object position origin type: %s"), *OriginName);
					return false;
				}

				ObjectPosition->OriginType = Origin;
			}
		}
		else if (UMaterialExpressionStaticBool* StaticBool = Cast<UMaterialExpressionStaticBool>(Expression))
		{
			bool bValue = false;
			if (Params->TryGetBoolField(TEXT("value"), bValue))
			{
				StaticBool->Value = bValue;
			}
		}
		else if (UMaterialExpressionStaticBoolParameter* StaticBoolParameter = Cast<UMaterialExpressionStaticBoolParameter>(Expression))
		{
			FString ParameterName;
			if (Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
			{
				StaticBoolParameter->ParameterName = FName(*ParameterName);
			}

			bool bValue = false;
			if (Params->TryGetBoolField(TEXT("value"), bValue))
			{
				StaticBoolParameter->DefaultValue = bValue;
			}
		}
		else if (UMaterialExpressionTextureObjectParameter* TextureObjectParameter = Cast<UMaterialExpressionTextureObjectParameter>(Expression))
		{
			FString ParameterName;
			if (Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
			{
				TextureObjectParameter->SetParameterName(FName(*ParameterName));
				TextureObjectParameter->UpdateParameterGuid(true, false);
				TextureObjectParameter->ValidateParameterName(false);
			}

			FString TexturePath;
			if (Params->TryGetStringField(TEXT("texture_path"), TexturePath))
			{
				UTexture* Texture = Cast<UTexture>(UEditorAssetLibrary::LoadAsset(TexturePath));
				if (!Texture)
				{
					OutError = FString::Printf(TEXT("Texture not found: %s"), *TexturePath);
					return false;
				}

				TextureObjectParameter->Texture = Texture;
			}
		}
		else if (UMaterialExpressionTextureSampleParameter2D* TextureParameter = Cast<UMaterialExpressionTextureSampleParameter2D>(Expression))
		{
			FString ParameterName;
			if (Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
			{
				TextureParameter->SetParameterName(FName(*ParameterName));
				TextureParameter->UpdateParameterGuid(true, false);
				TextureParameter->ValidateParameterName(false);
			}

			FString TexturePath;
			if (Params->TryGetStringField(TEXT("texture_path"), TexturePath))
			{
				UTexture* Texture = Cast<UTexture>(UEditorAssetLibrary::LoadAsset(TexturePath));
				if (!Texture)
				{
					OutError = FString::Printf(TEXT("Texture not found: %s"), *TexturePath);
					return false;
				}

				TextureParameter->Texture = Texture;
				TextureParameter->AutoSetSampleType();
			}
		}
		else if (UMaterialExpressionComment* Comment = Cast<UMaterialExpressionComment>(Expression))
		{
			FString CommentText;
			if (Params->TryGetStringField(TEXT("text"), CommentText))
			{
				Comment->Text = CommentText;
			}

			int32 SizeX = 0;
			if (Params->TryGetNumberField(TEXT("size_x"), SizeX))
			{
				Comment->SizeX = SizeX;
			}

			int32 SizeY = 0;
			if (Params->TryGetNumberField(TEXT("size_y"), SizeY))
			{
				Comment->SizeY = SizeY;
			}
		}
		else if (UMaterialExpressionComponentMask* ComponentMask = Cast<UMaterialExpressionComponentMask>(Expression))
		{
			FString Mask;
			if (Params->TryGetStringField(TEXT("mask"), Mask))
			{
				const FString UpperMask = Mask.ToUpper();
				ComponentMask->R = UpperMask.Contains(TEXT("R"));
				ComponentMask->G = UpperMask.Contains(TEXT("G"));
				ComponentMask->B = UpperMask.Contains(TEXT("B"));
				ComponentMask->A = UpperMask.Contains(TEXT("A"));
			}
		}
		else if (UMaterialExpressionIf* IfNode = Cast<UMaterialExpressionIf>(Expression))
		{
			float EqualsThreshold = 0.0f;
			if (TryGetNumberField(Params, TEXT("equals_threshold"), EqualsThreshold))
			{
				IfNode->EqualsThreshold = EqualsThreshold;
			}
		}
		else if (UMaterialExpressionFresnel* Fresnel = Cast<UMaterialExpressionFresnel>(Expression))
		{
			float Exponent = 0.0f;
			if (TryGetNumberField(Params, TEXT("exponent"), Exponent))
			{
				Fresnel->Exponent = Exponent;
			}

			float BaseReflectFraction = 0.0f;
			if (TryGetNumberField(Params, TEXT("base_reflect_fraction"), BaseReflectFraction))
			{
				Fresnel->BaseReflectFraction = BaseReflectFraction;
			}
		}
		else if (UMaterialExpressionPower* Power = Cast<UMaterialExpressionPower>(Expression))
		{
			float ConstExponent = 0.0f;
			if (TryGetNumberField(Params, TEXT("const_exponent"), ConstExponent))
			{
				Power->ConstExponent = ConstExponent;
			}
		}
		else if (UMaterialExpressionClamp* Clamp = Cast<UMaterialExpressionClamp>(Expression))
		{
			float MinDefault = 0.0f;
			if (TryGetNumberField(Params, TEXT("min_default"), MinDefault))
			{
				Clamp->MinDefault = MinDefault;
			}

			float MaxDefault = 0.0f;
			if (TryGetNumberField(Params, TEXT("max_default"), MaxDefault))
			{
				Clamp->MaxDefault = MaxDefault;
			}
		}

		Expression->PostEditChange();
		return true;
	}
}

FUECLIMaterialCommands::FUECLIMaterialCommands()
{
}

void FUECLIMaterialCommands::RegisterTools(FUECLIToolRegistry& Registry)
{
	RegisterMaterialCommand(Registry, TEXT("create_material"));
	RegisterMaterialCommand(Registry, TEXT("create_material_function"));
	RegisterMaterialCommand(Registry, TEXT("create_material_instance"));
	RegisterMaterialCommand(Registry, TEXT("get_material_parameters"));
	RegisterMaterialCommand(Registry, TEXT("set_material_parameter"));
	RegisterMaterialCommand(Registry, TEXT("list_materials"));
	RegisterMaterialCommand(Registry, TEXT("get_material_info"));
	RegisterMaterialCommand(Registry, TEXT("set_material_asset_properties"));
	RegisterMaterialCommand(Registry, TEXT("create_material_scaffold"));
	RegisterMaterialCommand(Registry, TEXT("apply_material_to_actor"));
	RegisterMaterialCommand(Registry, TEXT("get_opened_material"));
	RegisterMaterialCommand(Registry, TEXT("get_material_nodes"));
	RegisterMaterialCommand(Registry, TEXT("get_material_connections"));
	RegisterMaterialCommand(Registry, TEXT("create_material_node"));
	RegisterMaterialCommand(Registry, TEXT("connect_material_nodes"));
	RegisterMaterialCommand(Registry, TEXT("set_material_output"));
	RegisterMaterialCommand(Registry, TEXT("delete_material_node"));
	RegisterMaterialCommand(Registry, TEXT("disconnect_material_nodes"));
	RegisterMaterialCommand(Registry, TEXT("clear_material_output"));
	RegisterMaterialCommand(Registry, TEXT("move_material_node"));
	RegisterMaterialCommand(Registry, TEXT("set_material_node_property"));
	RegisterMaterialCommand(Registry, TEXT("layout_material_nodes"));
	RegisterMaterialCommand(Registry, TEXT("compile_material"));
	RegisterMaterialCommand(Registry, TEXT("save_material"));
	RegisterMaterialCommand(Registry, TEXT("apply_material_graph_patch"));
	RegisterMaterialCommand(Registry, TEXT("create_material_function_node"));
	RegisterMaterialCommand(Registry, TEXT("create_material_function_scaffold"));
	RegisterMaterialCommand(Registry, TEXT("connect_material_function_nodes"));
	RegisterMaterialCommand(Registry, TEXT("disconnect_material_function_nodes"));
	RegisterMaterialCommand(Registry, TEXT("delete_material_function_node"));
	RegisterMaterialCommand(Registry, TEXT("move_material_function_node"));
	RegisterMaterialCommand(Registry, TEXT("layout_material_function_nodes"));
	RegisterMaterialCommand(Registry, TEXT("set_material_function_node_property"));
	RegisterMaterialCommand(Registry, TEXT("compile_material_function"));
	RegisterMaterialCommand(Registry, TEXT("save_material_function"));
	RegisterMaterialCommand(Registry, TEXT("apply_material_function_patch"));
	RegisterMaterialCommand(Registry, TEXT("analyze_material"));
	RegisterMaterialCommand(Registry, TEXT("get_material_function_info"));
	RegisterMaterialCommand(Registry, TEXT("get_material_flow_graph"));
	RegisterMaterialCommand(Registry, TEXT("get_material_full_graph"));
	RegisterMaterialCommand(Registry, TEXT("get_material_shader_info"));
	RegisterMaterialCommand(Registry, TEXT("explain_material_node"));
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("create_material"))
    {
        return HandleCreateMaterial(Params);
    }
    else if (CommandType == TEXT("create_material_function"))
    {
        return HandleCreateMaterialFunction(Params);
    }
    else if (CommandType == TEXT("create_material_instance"))
    {
        return HandleCreateMaterialInstance(Params);
    }
    else if (CommandType == TEXT("get_material_parameters"))
    {
        return HandleGetMaterialParameters(Params);
    }
    else if (CommandType == TEXT("set_material_parameter"))
    {
        return HandleSetMaterialParameter(Params);
    }
    else if (CommandType == TEXT("list_materials"))
    {
        return HandleListMaterials(Params);
    }
    else if (CommandType == TEXT("get_material_info"))
    {
        return HandleGetMaterialInfo(Params);
    }
    else if (CommandType == TEXT("set_material_asset_properties"))
    {
        return HandleSetMaterialAssetProperties(Params);
    }
    else if (CommandType == TEXT("create_material_scaffold"))
    {
        return HandleCreateMaterialScaffold(Params);
    }
    else if (CommandType == TEXT("apply_material_to_actor"))
    {
        return HandleApplyMaterialToActor(Params);
    }
    else if (CommandType == TEXT("get_opened_material"))
    {
        return HandleGetOpenedMaterial(Params);
    }
    else if (CommandType == TEXT("get_material_nodes"))
    {
        return HandleGetMaterialNodes(Params);
    }
    else if (CommandType == TEXT("get_material_connections"))
    {
        return HandleGetMaterialConnections(Params);
    }
    else if (CommandType == TEXT("create_material_node"))
    {
        return HandleCreateMaterialNode(Params);
    }
    else if (CommandType == TEXT("connect_material_nodes"))
    {
        return HandleConnectMaterialNodes(Params);
    }
    else if (CommandType == TEXT("set_material_output"))
    {
        return HandleSetMaterialOutput(Params);
    }
    else if (CommandType == TEXT("delete_material_node"))
    {
        return HandleDeleteMaterialNode(Params);
    }
    else if (CommandType == TEXT("disconnect_material_nodes"))
    {
        return HandleDisconnectMaterialNodes(Params);
    }
    else if (CommandType == TEXT("clear_material_output"))
    {
        return HandleClearMaterialOutput(Params);
    }
    else if (CommandType == TEXT("move_material_node"))
    {
        return HandleMoveMaterialNode(Params);
    }
    else if (CommandType == TEXT("set_material_node_property"))
    {
        return HandleSetMaterialNodeProperty(Params);
    }
    else if (CommandType == TEXT("layout_material_nodes"))
    {
        return HandleLayoutMaterialNodes(Params);
    }
    else if (CommandType == TEXT("compile_material"))
    {
        return HandleCompileMaterial(Params);
    }
    else if (CommandType == TEXT("save_material"))
    {
        return HandleSaveMaterial(Params);
    }
    else if (CommandType == TEXT("apply_material_graph_patch"))
    {
        return HandleApplyMaterialGraphPatch(Params);
    }
    else if (CommandType == TEXT("create_material_function_node"))
    {
        return HandleCreateMaterialFunctionNode(Params);
    }
    else if (CommandType == TEXT("create_material_function_scaffold"))
    {
        return HandleCreateMaterialFunctionScaffold(Params);
    }
    else if (CommandType == TEXT("connect_material_function_nodes"))
    {
        return HandleConnectMaterialFunctionNodes(Params);
    }
    else if (CommandType == TEXT("disconnect_material_function_nodes"))
    {
        return HandleDisconnectMaterialFunctionNodes(Params);
    }
    else if (CommandType == TEXT("delete_material_function_node"))
    {
        return HandleDeleteMaterialFunctionNode(Params);
    }
    else if (CommandType == TEXT("move_material_function_node"))
    {
        return HandleMoveMaterialFunctionNode(Params);
    }
    else if (CommandType == TEXT("layout_material_function_nodes"))
    {
        return HandleLayoutMaterialFunctionNodes(Params);
    }
    else if (CommandType == TEXT("set_material_function_node_property"))
    {
        return HandleSetMaterialFunctionNodeProperty(Params);
    }
    else if (CommandType == TEXT("compile_material_function"))
    {
        return HandleCompileMaterialFunction(Params);
    }
    else if (CommandType == TEXT("save_material_function"))
    {
        return HandleSaveMaterialFunction(Params);
    }
    else if (CommandType == TEXT("apply_material_function_patch"))
    {
        return HandleApplyMaterialFunctionPatch(Params);
    }
    else if (CommandType == TEXT("analyze_material"))
    {
        return HandleAnalyzeMaterial(Params);
    }
    else if (CommandType == TEXT("get_material_function_info"))
    {
        return HandleGetMaterialFunctionInfo(Params);
    }
    else if (CommandType == TEXT("get_material_flow_graph"))
    {
        return HandleGetMaterialFlowGraph(Params);
    }
    else if (CommandType == TEXT("get_material_full_graph"))
    {
        return HandleGetMaterialFullGraph(Params);
    }
    else if (CommandType == TEXT("get_material_shader_info"))
    {
        return HandleGetMaterialShaderInfo(Params);
    }
    else if (CommandType == TEXT("explain_material_node"))
    {
        return HandleExplainMaterialNode(Params);
    }
    
    return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown material command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialName;
    if (!Params->TryGetStringField(TEXT("name"), MaterialName))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString FolderPath;
    if (!Params->TryGetStringField(TEXT("path"), FolderPath))
    {
        FolderPath = TEXT("/Game/Materials");
    }

    FString FullPath = FolderPath / MaterialName;

    // Check if already exists
    if (UEditorAssetLibrary::DoesAssetExist(FullPath))
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material already exists: %s"), *FullPath));
    }

    // Create the material factory
    UMaterialFactoryNew* MaterialFactory = NewObject<UMaterialFactoryNew>();
    
    // Create package
    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to create package"));
    }

    // Create the material
    UMaterial* NewMaterial = Cast<UMaterial>(MaterialFactory->FactoryCreateNew(
        UMaterial::StaticClass(),
        Package,
        *MaterialName,
        RF_Standalone | RF_Public,
        nullptr,
        GWarn
    ));

    if (NewMaterial)
    {
        FAssetRegistryModule::AssetCreated(NewMaterial);
        Package->MarkPackageDirty();

        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("name"), MaterialName);
        ResultObj->SetStringField(TEXT("path"), FullPath);
        ResultObj->SetStringField(TEXT("material_path"), FullPath);
        ResultObj->SetBoolField(TEXT("success"), true);
        return ResultObj;
    }

    return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to create material"));
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleCreateMaterialFunction(const TSharedPtr<FJsonObject>& Params)
{
	FString FunctionName;
	if (!Params->TryGetStringField(TEXT("name"), FunctionName))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	FString FolderPath;
	if (!Params->TryGetStringField(TEXT("path"), FolderPath))
	{
		FolderPath = TEXT("/Game/MaterialFunctions");
	}

	const FString FullPath = FolderPath / FunctionName;
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material function already exists: %s"), *FullPath));
	}

	UMaterialFunctionFactoryNew* Factory = NewObject<UMaterialFunctionFactoryNew>();
	UPackage* Package = CreatePackage(*FullPath);
	if (!Package)
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to create package"));
	}

	UMaterialFunction* NewFunction = Cast<UMaterialFunction>(Factory->FactoryCreateNew(
		UMaterialFunction::StaticClass(),
		Package,
		*FunctionName,
		RF_Standalone | RF_Public,
		nullptr,
		GWarn));
	if (!NewFunction)
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to create material function"));
	}

	FString Description;
	if (Params->TryGetStringField(TEXT("description"), Description))
	{
		NewFunction->Description = Description;
	}

	FString UserExposedCaption;
	if (Params->TryGetStringField(TEXT("user_exposed_caption"), UserExposedCaption))
	{
		NewFunction->UserExposedCaption = UserExposedCaption;
	}

	FAssetRegistryModule::AssetCreated(NewFunction);
	Package->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("name"), FunctionName);
	ResultObj->SetStringField(TEXT("path"), FullPath);
	ResultObj->SetStringField(TEXT("function_path"), FullPath);
	ResultObj->SetBoolField(TEXT("success"), true);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleCreateMaterialInstance(const TSharedPtr<FJsonObject>& Params)
{
    FString InstanceName;
    if (!Params->TryGetStringField(TEXT("name"), InstanceName))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString ParentPath;
    if (!Params->TryGetStringField(TEXT("parent_material"), ParentPath))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'parent_material' parameter"));
    }

    FString FolderPath;
    if (!Params->TryGetStringField(TEXT("path"), FolderPath))
    {
        FolderPath = TEXT("/Game/Materials");
    }

    // Load parent material
    UMaterialInterface* ParentMaterial = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(ParentPath));
    if (!ParentMaterial)
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Parent material not found: %s"), *ParentPath));
    }

    FString FullPath = FolderPath / InstanceName;

    // Check if already exists
    if (UEditorAssetLibrary::DoesAssetExist(FullPath))
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material instance already exists: %s"), *FullPath));
    }

    // Create the factory
    UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
    Factory->InitialParent = ParentMaterial;

    // Create package
    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to create package"));
    }

    // Create the material instance
    UMaterialInstanceConstant* NewInstance = Cast<UMaterialInstanceConstant>(Factory->FactoryCreateNew(
        UMaterialInstanceConstant::StaticClass(),
        Package,
        *InstanceName,
        RF_Standalone | RF_Public,
        nullptr,
        GWarn
    ));

    if (NewInstance)
    {
        FAssetRegistryModule::AssetCreated(NewInstance);
        Package->MarkPackageDirty();

        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("name"), InstanceName);
        ResultObj->SetStringField(TEXT("path"), FullPath);
        ResultObj->SetStringField(TEXT("parent"), ParentPath);
        ResultObj->SetBoolField(TEXT("success"), true);
        return ResultObj;
    }

    return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to create material instance"));
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleGetMaterialParameters(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
    }

    UMaterialInterface* Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
    if (!Material)
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
    ResultObj->SetStringField(TEXT("material_name"), Material->GetName());

    // Get scalar parameters
    TArray<TSharedPtr<FJsonValue>> ScalarParams;
    TArray<FMaterialParameterInfo> ScalarParamInfos;
    TArray<FGuid> ScalarParamIds;
    Material->GetAllScalarParameterInfo(ScalarParamInfos, ScalarParamIds);
    
    for (int32 i = 0; i < ScalarParamInfos.Num(); i++)
    {
        TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
        ParamObj->SetStringField(TEXT("name"), ScalarParamInfos[i].Name.ToString());
        
        float Value;
        if (Material->GetScalarParameterValue(ScalarParamInfos[i], Value))
        {
            ParamObj->SetNumberField(TEXT("value"), Value);
        }
        ScalarParams.Add(MakeShared<FJsonValueObject>(ParamObj));
    }
    ResultObj->SetArrayField(TEXT("scalar_parameters"), ScalarParams);

    // Get vector parameters
    TArray<TSharedPtr<FJsonValue>> VectorParams;
    TArray<FMaterialParameterInfo> VectorParamInfos;
    TArray<FGuid> VectorParamIds;
    Material->GetAllVectorParameterInfo(VectorParamInfos, VectorParamIds);
    
    for (int32 i = 0; i < VectorParamInfos.Num(); i++)
    {
        TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
        ParamObj->SetStringField(TEXT("name"), VectorParamInfos[i].Name.ToString());
        
        FLinearColor Value;
        if (Material->GetVectorParameterValue(VectorParamInfos[i], Value))
        {
            TArray<TSharedPtr<FJsonValue>> ColorArray;
            ColorArray.Add(MakeShared<FJsonValueNumber>(Value.R));
            ColorArray.Add(MakeShared<FJsonValueNumber>(Value.G));
            ColorArray.Add(MakeShared<FJsonValueNumber>(Value.B));
            ColorArray.Add(MakeShared<FJsonValueNumber>(Value.A));
            ParamObj->SetArrayField(TEXT("value"), ColorArray);
        }
        VectorParams.Add(MakeShared<FJsonValueObject>(ParamObj));
    }
    ResultObj->SetArrayField(TEXT("vector_parameters"), VectorParams);

    // Get texture parameters
    TArray<TSharedPtr<FJsonValue>> TextureParams;
    TArray<FMaterialParameterInfo> TextureParamInfos;
    TArray<FGuid> TextureParamIds;
    Material->GetAllTextureParameterInfo(TextureParamInfos, TextureParamIds);
    
    for (int32 i = 0; i < TextureParamInfos.Num(); i++)
    {
        TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
        ParamObj->SetStringField(TEXT("name"), TextureParamInfos[i].Name.ToString());
        
        UTexture* Texture;
        if (Material->GetTextureParameterValue(TextureParamInfos[i], Texture) && Texture)
        {
            ParamObj->SetStringField(TEXT("texture_path"), Texture->GetPathName());
        }
        TextureParams.Add(MakeShared<FJsonValueObject>(ParamObj));
    }
    ResultObj->SetArrayField(TEXT("texture_parameters"), TextureParams);

    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleSetMaterialParameter(const TSharedPtr<FJsonObject>& Params)
{
    FString InstancePath;
    if (!Params->TryGetStringField(TEXT("instance_path"), InstancePath))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'instance_path' parameter"));
    }

    FString ParameterName;
    if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'parameter_name' parameter"));
    }

    FString ParameterType;
    if (!Params->TryGetStringField(TEXT("parameter_type"), ParameterType))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'parameter_type' parameter (Scalar, Vector, Texture)"));
    }

    UMaterialInstanceConstant* MaterialInstance = Cast<UMaterialInstanceConstant>(UEditorAssetLibrary::LoadAsset(InstancePath));
    if (!MaterialInstance)
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material instance not found: %s"), *InstancePath));
    }

    bool bSuccess = false;

    if (ParameterType == TEXT("Scalar"))
    {
        double Value;
        if (Params->TryGetNumberField(TEXT("value"), Value))
        {
            MaterialInstance->SetScalarParameterValueEditorOnly(FName(*ParameterName), Value);
            bSuccess = true;
        }
    }
    else if (ParameterType == TEXT("Vector"))
    {
        const TArray<TSharedPtr<FJsonValue>>* ColorArray;
        if (Params->TryGetArrayField(TEXT("value"), ColorArray) && ColorArray->Num() >= 3)
        {
            FLinearColor Color(
                (*ColorArray)[0]->AsNumber(),
                (*ColorArray)[1]->AsNumber(),
                (*ColorArray)[2]->AsNumber(),
                ColorArray->Num() >= 4 ? (*ColorArray)[3]->AsNumber() : 1.0f
            );
            MaterialInstance->SetVectorParameterValueEditorOnly(FName(*ParameterName), Color);
            bSuccess = true;
        }
    }
    else if (ParameterType == TEXT("Texture"))
    {
        FString TexturePath;
        if (Params->TryGetStringField(TEXT("value"), TexturePath))
        {
            UTexture* Texture = Cast<UTexture>(UEditorAssetLibrary::LoadAsset(TexturePath));
            if (Texture)
            {
                MaterialInstance->SetTextureParameterValueEditorOnly(FName(*ParameterName), Texture);
                bSuccess = true;
            }
        }
    }

    if (bSuccess)
    {
        MaterialInstance->MarkPackageDirty();

        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("instance_path"), InstancePath);
        ResultObj->SetStringField(TEXT("parameter_name"), ParameterName);
        ResultObj->SetStringField(TEXT("parameter_type"), ParameterType);
        ResultObj->SetBoolField(TEXT("success"), true);
        return ResultObj;
    }

    return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to set parameter"));
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleListMaterials(const TSharedPtr<FJsonObject>& Params)
{
    FString FolderPath;
    if (!Params->TryGetStringField(TEXT("path"), FolderPath))
    {
        FolderPath = TEXT("/Game/Materials");
    }

    bool bRecursive = false;
    Params->TryGetBoolField(TEXT("recursive"), bRecursive);

    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*FolderPath));
    Filter.bRecursivePaths = bRecursive;
    Filter.ClassPaths.Add(UMaterial::StaticClass()->GetClassPathName());
    Filter.ClassPaths.Add(UMaterialInstance::StaticClass()->GetClassPathName());
    Filter.ClassPaths.Add(UMaterialInstanceConstant::StaticClass()->GetClassPathName());

    TArray<FAssetData> MaterialList;
    AssetRegistry.GetAssets(Filter, MaterialList);

    TArray<TSharedPtr<FJsonValue>> MaterialsArray;
    for (const FAssetData& AssetData : MaterialList)
    {
        TSharedPtr<FJsonObject> MatObj = MakeShared<FJsonObject>();
        MatObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
        MatObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
        MatObj->SetStringField(TEXT("class"), AssetData.AssetClassPath.GetAssetName().ToString());
        MaterialsArray.Add(MakeShared<FJsonValueObject>(MatObj));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("materials"), MaterialsArray);
    ResultObj->SetNumberField(TEXT("count"), MaterialsArray.Num());
    ResultObj->SetStringField(TEXT("search_path"), FolderPath);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleGetMaterialInfo(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath;
    if (!TryGetMaterialPath(Params, MaterialPath))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
    }

    UMaterialInterface* Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
    if (!Material)
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("name"), Material->GetName());
    ResultObj->SetStringField(TEXT("path"), MaterialPath);
    ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
    ResultObj->SetStringField(TEXT("class"), Material->GetClass()->GetName());

	UMaterial* BaseMaterial = Material->GetMaterial();
	if (BaseMaterial != nullptr)
	{
		ResultObj->SetStringField(TEXT("material_domain"), MaterialDomainToString(BaseMaterial->MaterialDomain));
		ResultObj->SetStringField(TEXT("blend_mode"), BlendModeToString(Material->GetBlendMode()));
		ResultObj->SetStringField(TEXT("shading_model"), ShadingModelToString(Material->GetShadingModels().GetFirstShadingModel()));
		ResultObj->SetBoolField(TEXT("two_sided"), Material->IsTwoSided());
		ResultObj->SetBoolField(TEXT("dithered_lod_transition"), Material->IsDitheredLODTransition());
		ResultObj->SetNumberField(TEXT("opacity_mask_clip_value"), Material->GetOpacityMaskClipValue());
		ResultObj->SetBoolField(TEXT("use_material_attributes"), BaseMaterial->bUseMaterialAttributes);
	}
    
    // Check if it's an instance
    UMaterialInstance* Instance = Cast<UMaterialInstance>(Material);
    if (Instance)
    {
        ResultObj->SetBoolField(TEXT("is_instance"), true);
        if (Instance->Parent)
        {
            ResultObj->SetStringField(TEXT("parent_material"), Instance->Parent->GetPathName());
        }
    }
    else
    {
        ResultObj->SetBoolField(TEXT("is_instance"), false);
    }

    // Count parameters
    TArray<FMaterialParameterInfo> ParamInfos;
    TArray<FGuid> ParamIds;
    
    Material->GetAllScalarParameterInfo(ParamInfos, ParamIds);
    ResultObj->SetNumberField(TEXT("scalar_parameter_count"), ParamInfos.Num());
    
    ParamInfos.Empty();
    ParamIds.Empty();
    Material->GetAllVectorParameterInfo(ParamInfos, ParamIds);
    ResultObj->SetNumberField(TEXT("vector_parameter_count"), ParamInfos.Num());
    
    ParamInfos.Empty();
    ParamIds.Empty();
    Material->GetAllTextureParameterInfo(ParamInfos, ParamIds);
    ResultObj->SetNumberField(TEXT("texture_parameter_count"), ParamInfos.Num());

    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleSetMaterialAssetProperties(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!TryGetMaterialPath(Params, MaterialPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
	}

	UMaterial* Material = LoadMaterialForEditing(MaterialPath);
	if (!Material)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Base material not found: %s"), *MaterialPath));
	}

	FString MaterialDomainName;
	EMaterialDomain NewMaterialDomain = MD_Surface;
	const bool bSetMaterialDomain = Params->TryGetStringField(TEXT("material_domain"), MaterialDomainName);
	if (bSetMaterialDomain && !TryResolveMaterialDomain(MaterialDomainName, NewMaterialDomain))
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unsupported material domain: %s"), *MaterialDomainName));
	}

	FString BlendModeName;
	EBlendMode NewBlendMode = BLEND_Opaque;
	const bool bSetBlendMode = Params->TryGetStringField(TEXT("blend_mode"), BlendModeName);
	if (bSetBlendMode && !TryResolveBlendMode(BlendModeName, NewBlendMode))
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unsupported blend mode: %s"), *BlendModeName));
	}

	FString ShadingModelName;
	EMaterialShadingModel NewShadingModel = MSM_DefaultLit;
	const bool bSetShadingModel = Params->TryGetStringField(TEXT("shading_model"), ShadingModelName);
	if (bSetShadingModel && !TryResolveShadingModel(ShadingModelName, NewShadingModel))
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unsupported shading model: %s"), *ShadingModelName));
	}

	bool bTwoSided = false;
	const bool bSetTwoSided = Params->TryGetBoolField(TEXT("two_sided"), bTwoSided);

	bool bDitheredLODTransition = false;
	const bool bSetDitheredLODTransition = Params->TryGetBoolField(TEXT("dithered_lod_transition"), bDitheredLODTransition);

	bool bUseMaterialAttributes = false;
	const bool bSetUseMaterialAttributes = Params->TryGetBoolField(TEXT("use_material_attributes"), bUseMaterialAttributes);

	double OpacityMaskClipValue = 0.0;
	const bool bSetOpacityMaskClipValue = Params->TryGetNumberField(TEXT("opacity_mask_clip_value"), OpacityMaskClipValue);

	const bool bHasAnyProperty =
		bSetMaterialDomain ||
		bSetBlendMode ||
		bSetShadingModel ||
		bSetTwoSided ||
		bSetDitheredLODTransition ||
		bSetUseMaterialAttributes ||
		bSetOpacityMaskClipValue;

	if (!bHasAnyProperty)
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("No editable material asset properties were provided"));
	}

	Material->Modify();
	Material->PreEditChange(nullptr);

	if (bSetMaterialDomain)
	{
		Material->MaterialDomain = NewMaterialDomain;
	}

	if (bSetBlendMode)
	{
		Material->BlendMode = NewBlendMode;
	}

	if (bSetShadingModel)
	{
		Material->SetShadingModel(NewShadingModel);
	}

	if (bSetTwoSided)
	{
		Material->TwoSided = bTwoSided;
	}

	if (bSetDitheredLODTransition)
	{
		Material->DitheredLODTransition = bDitheredLODTransition;
	}

	if (bSetUseMaterialAttributes)
	{
		Material->bUseMaterialAttributes = bUseMaterialAttributes;
	}

	if (bSetOpacityMaskClipValue)
	{
		Material->OpacityMaskClipValue = static_cast<float>(OpacityMaskClipValue);
	}

	Material->PostEditChange();
	Material->MarkPackageDirty();

	bool bCompile = false;
	Params->TryGetBoolField(TEXT("compile"), bCompile);
	if (bCompile)
	{
		UMaterialEditingLibrary::RecompileMaterial(Material);

		UObject* MaterialObject = Material;
		FAssetCompilingManager::Get().FinishCompilationForObjects(MakeArrayView(&MaterialObject, 1));
		if (GShaderCompilingManager != nullptr)
		{
			GShaderCompilingManager->FinishAllCompilation();
		}
		FAssetCompilingManager::Get().ProcessAsyncTasks(true);
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
	ResultObj->SetStringField(TEXT("material_domain"), MaterialDomainToString(Material->MaterialDomain));
	ResultObj->SetStringField(TEXT("blend_mode"), BlendModeToString(Material->BlendMode));
	ResultObj->SetStringField(TEXT("shading_model"), ShadingModelToString(Material->GetShadingModels().GetFirstShadingModel()));
	ResultObj->SetBoolField(TEXT("two_sided"), Material->IsTwoSided());
	ResultObj->SetBoolField(TEXT("dithered_lod_transition"), Material->IsDitheredLODTransition());
	ResultObj->SetNumberField(TEXT("opacity_mask_clip_value"), Material->GetOpacityMaskClipValue());
	ResultObj->SetBoolField(TEXT("use_material_attributes"), Material->bUseMaterialAttributes);
	ResultObj->SetBoolField(TEXT("compiled"), bCompile);
	ResultObj->SetBoolField(TEXT("success"), true);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleCreateMaterialScaffold(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!TryGetMaterialPath(Params, MaterialPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
	}

	FString ScaffoldType;
	if (!Params->TryGetStringField(TEXT("scaffold_type"), ScaffoldType))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'scaffold_type' parameter"));
	}

	const FString NormalizedScaffoldType = ScaffoldType.ToLower();
	if (NormalizedScaffoldType != TEXT("feather_foliage"))
	{
		return FUECLICommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unsupported material scaffold type: %s"), *ScaffoldType));
	}

	bool bLayout = true;
	Params->TryGetBoolField(TEXT("layout"), bLayout);

	bool bCompile = false;
	Params->TryGetBoolField(TEXT("compile"), bCompile);

	bool bSave = false;
	Params->TryGetBoolField(TEXT("save"), bSave);

	float CoveragePower = 1.35f;
	TryGetNumberField(Params, TEXT("coverage_power"), CoveragePower);

	float EdgeIntensity = 0.12f;
	TryGetNumberField(Params, TEXT("edge_intensity"), EdgeIntensity);

	float OpacityMaskClipValue = 0.25f;
	TryGetNumberField(Params, TEXT("opacity_mask_clip_value"), OpacityMaskClipValue);

	FLinearColor BaseColor(0.72f, 0.65f, 0.50f, 1.0f);
	TryGetLinearColorField(Params, TEXT("base_color"), BaseColor);

	FLinearColor SubsurfaceColor(0.95f, 0.72f, 0.45f, 1.0f);
	TryGetLinearColorField(Params, TEXT("subsurface_color"), SubsurfaceColor);

	float RoughnessValue = 0.68f;
	TryGetNumberField(Params, TEXT("roughness_value"), RoughnessValue);

	float SwayIntensity = 0.8f;
	TryGetNumberField(Params, TEXT("sway_intensity"), SwayIntensity);

	float SwayFrequency = 0.75f;
	TryGetNumberField(Params, TEXT("sway_frequency"), SwayFrequency);

	bool bEnableWPO = true;
	Params->TryGetBoolField(TEXT("enable_wpo"), bEnableWPO);

	FString AlbedoTexturePath;
	Params->TryGetStringField(TEXT("albedo_texture_path"), AlbedoTexturePath);

	FString CoverageTexturePath;
	Params->TryGetStringField(TEXT("coverage_texture_path"), CoverageTexturePath);

	FString NormalTexturePath;
	Params->TryGetStringField(TEXT("normal_texture_path"), NormalTexturePath);

	FString CoverageFunctionPath;
	Params->TryGetStringField(TEXT("coverage_function_path"), CoverageFunctionPath);

	FString WpoFunctionPath;
	Params->TryGetStringField(TEXT("wpo_function_path"), WpoFunctionPath);

	auto MakeNumberArray = [](std::initializer_list<double> Values) -> TArray<TSharedPtr<FJsonValue>>
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		for (double Value : Values)
		{
			Result.Add(MakeShared<FJsonValueNumber>(Value));
		}
		return Result;
	};

	auto MakeColorArray = [&MakeNumberArray](const FLinearColor& Color) -> TArray<TSharedPtr<FJsonValue>>
	{
		return MakeNumberArray({ Color.R, Color.G, Color.B, Color.A });
	};

	TArray<TSharedPtr<FJsonValue>> Operations;
	auto AddOperation = [&Operations](const TSharedPtr<FJsonObject>& Operation)
	{
		Operations.Add(MakeShared<FJsonValueObject>(Operation));
	};

	auto AddCreateNodeOp = [&AddOperation](const FString& Alias, const FString& NodeType, int32 PosX, int32 PosY)
	{
		TSharedPtr<FJsonObject> Operation = MakeShared<FJsonObject>();
		Operation->SetStringField(TEXT("action"), TEXT("create_node"));
		Operation->SetStringField(TEXT("alias"), Alias);
		Operation->SetStringField(TEXT("node_type"), NodeType);
		Operation->SetNumberField(TEXT("node_pos_x"), PosX);
		Operation->SetNumberField(TEXT("node_pos_y"), PosY);
		AddOperation(Operation);
		return Operation;
	};

	auto AddConnectOp = [&AddOperation](const FString& FromAlias, const FString& ToAlias, const FString& ToInput, const FString& FromOutput = FString())
	{
		TSharedPtr<FJsonObject> Operation = MakeShared<FJsonObject>();
		Operation->SetStringField(TEXT("action"), TEXT("connect_nodes"));
		Operation->SetStringField(TEXT("from_node"), FromAlias);
		Operation->SetStringField(TEXT("to_node"), ToAlias);
		if (!FromOutput.IsEmpty())
		{
			Operation->SetStringField(TEXT("from_output"), FromOutput);
		}
		if (!ToInput.IsEmpty())
		{
			Operation->SetStringField(TEXT("to_input"), ToInput);
		}
		AddOperation(Operation);
	};

	auto AddOutputOp = [&AddOperation](const FString& FromAlias, const FString& PropertyName)
	{
		TSharedPtr<FJsonObject> Operation = MakeShared<FJsonObject>();
		Operation->SetStringField(TEXT("action"), TEXT("set_output"));
		Operation->SetStringField(TEXT("from_node"), FromAlias);
		Operation->SetStringField(TEXT("property"), PropertyName);
		AddOperation(Operation);
	};

	auto AddSetNodePropertyOp = [&AddOperation](const FString& NodeAlias, const FString& PropertyName, const FString& Value)
	{
		TSharedPtr<FJsonObject> Operation = MakeShared<FJsonObject>();
		Operation->SetStringField(TEXT("action"), TEXT("set_node_property"));
		Operation->SetStringField(TEXT("node_name"), NodeAlias);
		Operation->SetStringField(TEXT("property_name"), PropertyName);
		Operation->SetStringField(TEXT("value"), Value);
		AddOperation(Operation);
	};

	{
		TSharedPtr<FJsonObject> Operation = MakeShared<FJsonObject>();
		Operation->SetStringField(TEXT("action"), TEXT("set_material_asset_properties"));
		Operation->SetStringField(TEXT("blend_mode"), TEXT("Masked"));
		Operation->SetStringField(TEXT("shading_model"), TEXT("TwoSidedFoliage"));
		Operation->SetBoolField(TEXT("two_sided"), true);
		Operation->SetBoolField(TEXT("dithered_lod_transition"), true);
		Operation->SetBoolField(TEXT("use_material_attributes"), false);
		Operation->SetNumberField(TEXT("opacity_mask_clip_value"), OpacityMaskClipValue);
		AddOperation(Operation);
	}

	{
		TSharedPtr<FJsonObject> Comment = AddCreateNodeOp(TEXT("coverage_comment"), TEXT("Comment"), -1480, -380);
		Comment->SetStringField(TEXT("text"), TEXT("Coverage / silhouette"));
		Comment->SetNumberField(TEXT("size_x"), 760);
		Comment->SetNumberField(TEXT("size_y"), 760);
	}

	{
		TSharedPtr<FJsonObject> Comment = AddCreateNodeOp(TEXT("shading_comment"), TEXT("Comment"), -520, -380);
		Comment->SetStringField(TEXT("text"), TEXT("Color / subsurface / edge"));
		Comment->SetNumberField(TEXT("size_x"), 860);
		Comment->SetNumberField(TEXT("size_y"), 760);
	}

	{
		TSharedPtr<FJsonObject> Comment = AddCreateNodeOp(TEXT("uv_comment"), TEXT("Comment"), -1480, 470);
		Comment->SetStringField(TEXT("text"), TEXT("UV flow staging"));
		Comment->SetNumberField(TEXT("size_x"), 760);
		Comment->SetNumberField(TEXT("size_y"), 420);
	}

	{
		TSharedPtr<FJsonObject> Comment = AddCreateNodeOp(TEXT("detail_comment"), TEXT("Comment"), 520, -380);
		Comment->SetStringField(TEXT("text"), TEXT("Normals / roughness / motion"));
		Comment->SetNumberField(TEXT("size_x"), 920);
		Comment->SetNumberField(TEXT("size_y"), 900);
	}

	AddCreateNodeOp(TEXT("vertex_color"), TEXT("VertexColor"), -1260, -40);

	TSharedPtr<FJsonObject> AlphaMask = AddCreateNodeOp(TEXT("alpha_mask"), TEXT("ComponentMask"), -1020, 190);
	AlphaMask->SetStringField(TEXT("mask"), TEXT("A"));

	TSharedPtr<FJsonObject> CoveragePowerParam = AddCreateNodeOp(TEXT("coverage_power"), TEXT("ScalarParameter"), -1260, 220);
	CoveragePowerParam->SetStringField(TEXT("parameter_name"), TEXT("CoveragePower"));
	CoveragePowerParam->SetNumberField(TEXT("value"), CoveragePower);

	const bool bUseCoverageFunction = !CoverageFunctionPath.IsEmpty();
	FString CoverageOutputAlias;
	if (bUseCoverageFunction)
	{
		TSharedPtr<FJsonObject> CoverageFunctionCall = AddCreateNodeOp(TEXT("coverage_function_call"), TEXT("MaterialFunctionCall"), -760, 180);
		CoverageFunctionCall->SetStringField(TEXT("material_function_path"), CoverageFunctionPath);
		AddConnectOp(TEXT("coverage_power"), TEXT("coverage_function_call"), TEXT("Power"));
		CoverageOutputAlias = TEXT("coverage_function_call");
	}
	else
	{
		TSharedPtr<FJsonObject> CoverageCurve = AddCreateNodeOp(TEXT("coverage_curve"), TEXT("Power"), -760, 180);
		CoverageCurve->SetNumberField(TEXT("const_exponent"), CoveragePower);
		AddConnectOp(TEXT("coverage_power"), TEXT("coverage_curve"), TEXT("Exp"));
		CoverageOutputAlias = TEXT("coverage_curve");
	}

	TSharedPtr<FJsonObject> FeatherTint = AddCreateNodeOp(TEXT("feather_tint"), TEXT("VectorParameter"), -300, -180);
	FeatherTint->SetStringField(TEXT("parameter_name"), TEXT("FeatherTint"));
	FeatherTint->SetArrayField(TEXT("value"), MakeColorArray(BaseColor));

	TSharedPtr<FJsonObject> SubsurfaceTint = AddCreateNodeOp(TEXT("subsurface_tint"), TEXT("VectorParameter"), -300, 20);
	SubsurfaceTint->SetStringField(TEXT("parameter_name"), TEXT("SubsurfaceTint"));
	SubsurfaceTint->SetArrayField(TEXT("value"), MakeColorArray(SubsurfaceColor));

	TSharedPtr<FJsonObject> EdgeFresnel = AddCreateNodeOp(TEXT("edge_fresnel"), TEXT("Fresnel"), -300, 240);
	EdgeFresnel->SetNumberField(TEXT("exponent"), 4.0);
	EdgeFresnel->SetNumberField(TEXT("base_reflect_fraction"), 0.03);

	TSharedPtr<FJsonObject> EdgeIntensityParam = AddCreateNodeOp(TEXT("edge_intensity"), TEXT("ScalarParameter"), -300, 420);
	EdgeIntensityParam->SetStringField(TEXT("parameter_name"), TEXT("EdgeIntensity"));
	EdgeIntensityParam->SetNumberField(TEXT("value"), EdgeIntensity);

	AddCreateNodeOp(TEXT("edge_strength"), TEXT("Multiply"), -20, 320);
	AddCreateNodeOp(TEXT("edge_color"), TEXT("Multiply"), 220, 200);
	TSharedPtr<FJsonObject> RoughnessParam = AddCreateNodeOp(TEXT("roughness_value"), TEXT("ScalarParameter"), 680, -80);
	RoughnessParam->SetStringField(TEXT("parameter_name"), TEXT("RoughnessValue"));
	RoughnessParam->SetNumberField(TEXT("value"), RoughnessValue);

	AddCreateNodeOp(TEXT("uv0"), TEXT("TextureCoordinate"), -1260, 620);

	TSharedPtr<FJsonObject> Panner = AddCreateNodeOp(TEXT("barb_panner"), TEXT("Panner"), -1020, 620);
	Panner->SetNumberField(TEXT("speed_x"), 0.02);
	Panner->SetNumberField(TEXT("speed_y"), 0.0);
	Panner->SetBoolField(TEXT("fractional_part"), true);

	TSharedPtr<FJsonObject> Rotator = AddCreateNodeOp(TEXT("twist_rotator"), TEXT("Rotator"), -760, 620);
	Rotator->SetNumberField(TEXT("center_x"), 0.5);
	Rotator->SetNumberField(TEXT("center_y"), 0.5);
	Rotator->SetNumberField(TEXT("speed"), 0.01);

	AddCreateNodeOp(TEXT("camera_vector"), TEXT("CameraVectorWS"), -1260, 860);

	TSharedPtr<FJsonObject> ObjectPosition = AddCreateNodeOp(TEXT("object_position"), TEXT("ObjectPositionWS"), -1020, 860);
	ObjectPosition->SetStringField(TEXT("origin_type"), TEXT("CameraRelative"));

	TSharedPtr<FJsonObject> FlatNormal = AddCreateNodeOp(TEXT("flat_normal"), TEXT("Constant3Vector"), 680, 120);
	FlatNormal->SetArrayField(TEXT("value"), MakeNumberArray({0.5, 0.5, 1.0, 1.0}));

	AddConnectOp(TEXT("edge_fresnel"), TEXT("edge_strength"), TEXT("A"));
	AddConnectOp(TEXT("edge_intensity"), TEXT("edge_strength"), TEXT("B"));
	AddConnectOp(TEXT("feather_tint"), TEXT("edge_color"), TEXT("A"));
	AddConnectOp(TEXT("edge_strength"), TEXT("edge_color"), TEXT("B"));
	AddConnectOp(TEXT("uv0"), TEXT("barb_panner"), TEXT("Coordinate"));
	AddConnectOp(TEXT("barb_panner"), TEXT("twist_rotator"), TEXT("Coordinate"));

	FString BaseColorOutputAlias = TEXT("feather_tint");
	FString CoverageInputAlias = TEXT("vertex_color");
	FString CoverageInputOutput = TEXT("A");

	if (!AlbedoTexturePath.IsEmpty())
	{
		TSharedPtr<FJsonObject> AlbedoTexture = AddCreateNodeOp(TEXT("albedo_tex"), TEXT("TextureSampleParameter2D"), 60, -180);
		AlbedoTexture->SetStringField(TEXT("parameter_name"), TEXT("FeatherAlbedoTex"));
		AlbedoTexture->SetStringField(TEXT("texture_path"), AlbedoTexturePath);
		AddSetNodePropertyOp(TEXT("albedo_tex"), TEXT("ParameterName"), TEXT("FeatherAlbedoTex"));

		AddCreateNodeOp(TEXT("albedo_tint_mul"), TEXT("Multiply"), 320, -180);
		AddConnectOp(TEXT("twist_rotator"), TEXT("albedo_tex"), TEXT("UVs"));
		AddConnectOp(TEXT("albedo_tex"), TEXT("albedo_tint_mul"), TEXT("A"));
		AddConnectOp(TEXT("feather_tint"), TEXT("albedo_tint_mul"), TEXT("B"));
		BaseColorOutputAlias = TEXT("albedo_tint_mul");
	}

	if (!CoverageTexturePath.IsEmpty())
	{
		TSharedPtr<FJsonObject> CoverageTexture = AddCreateNodeOp(TEXT("coverage_tex"), TEXT("TextureSampleParameter2D"), -520, 40);
		CoverageTexture->SetStringField(TEXT("parameter_name"), TEXT("FeatherCoverageTex"));
		CoverageTexture->SetStringField(TEXT("texture_path"), CoverageTexturePath);
		AddSetNodePropertyOp(TEXT("coverage_tex"), TEXT("ParameterName"), TEXT("FeatherCoverageTex"));

		AddCreateNodeOp(TEXT("coverage_mul"), TEXT("Multiply"), -260, 110);
		AddConnectOp(TEXT("twist_rotator"), TEXT("coverage_tex"), TEXT("UVs"));
		AddConnectOp(TEXT("vertex_color"), TEXT("coverage_mul"), TEXT("A"), TEXT("A"));
		AddConnectOp(TEXT("coverage_tex"), TEXT("coverage_mul"), TEXT("B"), TEXT("A"));
		CoverageInputAlias = TEXT("coverage_mul");
		CoverageInputOutput.Empty();
	}

	AddOutputOp(BaseColorOutputAlias, TEXT("BaseColor"));
	AddOutputOp(TEXT("subsurface_tint"), TEXT("SubsurfaceColor"));
	AddOutputOp(CoverageOutputAlias, TEXT("OpacityMask"));
	AddOutputOp(TEXT("edge_color"), TEXT("EmissiveColor"));
	AddOutputOp(TEXT("roughness_value"), TEXT("Roughness"));
	AddOutputOp(TEXT("flat_normal"), TEXT("Normal"));

	if (!NormalTexturePath.IsEmpty())
	{
		TSharedPtr<FJsonObject> NormalTexture = AddCreateNodeOp(TEXT("normal_tex"), TEXT("TextureSampleParameter2D"), 960, 120);
		NormalTexture->SetStringField(TEXT("parameter_name"), TEXT("FeatherNormalTex"));
		NormalTexture->SetStringField(TEXT("texture_path"), NormalTexturePath);
		AddSetNodePropertyOp(TEXT("normal_tex"), TEXT("ParameterName"), TEXT("FeatherNormalTex"));
		AddConnectOp(TEXT("twist_rotator"), TEXT("normal_tex"), TEXT("UVs"));
		AddOutputOp(TEXT("normal_tex"), TEXT("Normal"));
	}

	if (bEnableWPO)
	{
		AddCreateNodeOp(TEXT("time"), TEXT("Time"), 680, 420);

		if (!WpoFunctionPath.IsEmpty())
		{
			TSharedPtr<FJsonObject> FunctionCall = AddCreateNodeOp(TEXT("wpo_function_call"), TEXT("MaterialFunctionCall"), 1120, 520);
			FunctionCall->SetStringField(TEXT("material_function_path"), WpoFunctionPath);
			AddConnectOp(TEXT("vertex_color"), TEXT("wpo_function_call"), TEXT("Mask"), TEXT("A"));
			AddConnectOp(TEXT("time"), TEXT("wpo_function_call"), TEXT("Time"));
			AddOutputOp(TEXT("wpo_function_call"), TEXT("WorldPositionOffset"));
		}
		else
		{
			TSharedPtr<FJsonObject> SwayFrequencyParam = AddCreateNodeOp(TEXT("sway_frequency"), TEXT("ScalarParameter"), 680, 560);
			SwayFrequencyParam->SetStringField(TEXT("parameter_name"), TEXT("SwayFrequency"));
			SwayFrequencyParam->SetNumberField(TEXT("value"), SwayFrequency);

			TSharedPtr<FJsonObject> SwayIntensityParam = AddCreateNodeOp(TEXT("sway_intensity"), TEXT("ScalarParameter"), 680, 700);
			SwayIntensityParam->SetStringField(TEXT("parameter_name"), TEXT("SwayIntensity"));
			SwayIntensityParam->SetNumberField(TEXT("value"), SwayIntensity);

			TSharedPtr<FJsonObject> ZeroScalar = AddCreateNodeOp(TEXT("zero_scalar"), TEXT("Constant"), 680, 840);
			ZeroScalar->SetNumberField(TEXT("value"), 0.0);

			AddCreateNodeOp(TEXT("time_freq_mul"), TEXT("Multiply"), 940, 500);
			AddCreateNodeOp(TEXT("sway_sine"), TEXT("Sine"), 1180, 500);
			AddCreateNodeOp(TEXT("sway_mask_mul"), TEXT("Multiply"), 940, 700);
			AddCreateNodeOp(TEXT("sway_intensity_mul"), TEXT("Multiply"), 1180, 700);
			AddCreateNodeOp(TEXT("wpo_xy"), TEXT("AppendVector"), 1420, 760);
			AddCreateNodeOp(TEXT("wpo_xyz"), TEXT("AppendVector"), 1660, 680);

			AddConnectOp(TEXT("time"), TEXT("time_freq_mul"), TEXT("A"));
			AddConnectOp(TEXT("sway_frequency"), TEXT("time_freq_mul"), TEXT("B"));
			AddConnectOp(TEXT("time_freq_mul"), TEXT("sway_sine"), FString());
			AddConnectOp(TEXT("sway_sine"), TEXT("sway_mask_mul"), TEXT("A"));
			AddConnectOp(TEXT("vertex_color"), TEXT("sway_mask_mul"), TEXT("B"), TEXT("A"));
			AddConnectOp(TEXT("sway_mask_mul"), TEXT("sway_intensity_mul"), TEXT("A"));
			AddConnectOp(TEXT("sway_intensity"), TEXT("sway_intensity_mul"), TEXT("B"));
			AddConnectOp(TEXT("zero_scalar"), TEXT("wpo_xy"), TEXT("A"));
			AddConnectOp(TEXT("zero_scalar"), TEXT("wpo_xy"), TEXT("B"));
			AddConnectOp(TEXT("wpo_xy"), TEXT("wpo_xyz"), TEXT("A"));
			AddConnectOp(TEXT("sway_intensity_mul"), TEXT("wpo_xyz"), TEXT("B"));
			AddOutputOp(TEXT("wpo_xyz"), TEXT("WorldPositionOffset"));
		}
	}

	AddConnectOp(
		CoverageInputAlias,
		CoverageOutputAlias,
		bUseCoverageFunction ? TEXT("Mask") : TEXT("Base"),
		CoverageInputOutput);

	if (bLayout)
	{
		TSharedPtr<FJsonObject> Operation = MakeShared<FJsonObject>();
		Operation->SetStringField(TEXT("action"), TEXT("layout_nodes"));
		AddOperation(Operation);
	}

	if (bCompile)
	{
		TSharedPtr<FJsonObject> Operation = MakeShared<FJsonObject>();
		Operation->SetStringField(TEXT("action"), TEXT("compile_material"));
		AddOperation(Operation);
	}

	if (bSave)
	{
		TSharedPtr<FJsonObject> Operation = MakeShared<FJsonObject>();
		Operation->SetStringField(TEXT("action"), TEXT("save_material"));
		AddOperation(Operation);
	}

	TSharedPtr<FJsonObject> PatchParams = MakeShared<FJsonObject>();
	PatchParams->SetStringField(TEXT("material_path"), MaterialPath);
	PatchParams->SetArrayField(TEXT("operations"), Operations);
	const TSharedPtr<FJsonObject> PatchResult = HandleApplyMaterialGraphPatch(PatchParams);
	if (PatchResult.IsValid())
	{
		PatchResult->SetStringField(TEXT("scaffold_type"), TEXT("feather_foliage"));
	}
	return PatchResult;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleApplyMaterialToActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    FString MaterialPath;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
    }

    int32 SlotIndex = 0;
    Params->TryGetNumberField(TEXT("slot_index"), SlotIndex);

    // Find the actor
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    AActor* TargetActor = nullptr;
    for (AActor* Actor : AllActors)
    {
        if (Actor && (Actor->GetName() == ActorName || Actor->GetActorLabel() == ActorName))
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Load the material
    UMaterialInterface* Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
    if (!Material)
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
    }

    // Find a mesh component and apply the material
    TArray<UMeshComponent*> MeshComponents;
    TargetActor->GetComponents<UMeshComponent>(MeshComponents);

    if (MeshComponents.Num() == 0)
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Actor has no mesh components"));
    }

    MeshComponents[0]->SetMaterial(SlotIndex, Material);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor_name"), ActorName);
    ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
    ResultObj->SetNumberField(TEXT("slot_index"), SlotIndex);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleGetOpenedMaterial(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> OpenedMaterials;

    // Get the Asset Editor Subsystem to find opened editors
    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (!AssetEditorSubsystem)
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to get AssetEditorSubsystem"));
    }

    // Get all edited assets
    TArray<UObject*> EditedAssets = AssetEditorSubsystem->GetAllEditedAssets();
    
    for (UObject* Asset : EditedAssets)
    {
        UMaterial* Material = Cast<UMaterial>(Asset);
        UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(Asset);
        
        if (Material || MaterialInstance)
        {
            TSharedPtr<FJsonObject> MatObj = MakeShared<FJsonObject>();
            MatObj->SetStringField(TEXT("name"), Asset->GetName());
            MatObj->SetStringField(TEXT("path"), Asset->GetPathName());
            MatObj->SetStringField(TEXT("class"), Asset->GetClass()->GetName());
            MatObj->SetBoolField(TEXT("is_instance"), MaterialInstance != nullptr);
            OpenedMaterials.Add(MakeShared<FJsonValueObject>(MatObj));
        }
    }

    ResultObj->SetArrayField(TEXT("opened_materials"), OpenedMaterials);
    ResultObj->SetNumberField(TEXT("count"), OpenedMaterials.Num());
    
    // If there's at least one opened material, set it as the "current" one
    if (OpenedMaterials.Num() > 0)
    {
        ResultObj->SetStringField(TEXT("current_material_path"), 
            OpenedMaterials[0]->AsObject()->GetStringField(TEXT("path")));
    }

    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleGetMaterialNodes(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath;
    
    // Try to get path from params, or use currently opened material
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        // Try to get the currently opened material
        UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
        if (AssetEditorSubsystem)
        {
            TArray<UObject*> EditedAssets = AssetEditorSubsystem->GetAllEditedAssets();
            for (UObject* Asset : EditedAssets)
            {
                if (Cast<UMaterial>(Asset))
                {
                    MaterialPath = Asset->GetPathName();
                    break;
                }
            }
        }
    }

    if (MaterialPath.IsEmpty())
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("No material path provided and no material is currently opened"));
    }

    UMaterial* Material = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
    if (!Material)
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
    ResultObj->SetStringField(TEXT("material_name"), Material->GetName());

    TArray<TSharedPtr<FJsonValue>> NodesArray;
    
    // Iterate through all expressions in the material
    auto Expressions = Material->GetExpressions();
    
    for (int32 i = 0; i < Expressions.Num(); i++)
    {
        UMaterialExpression* Expression = Expressions[i].Get();
        if (!Expression) continue;

        TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
        NodeObj->SetNumberField(TEXT("index"), i);
        NodeObj->SetStringField(TEXT("name"), Expression->GetName());
        NodeObj->SetStringField(TEXT("guid"), Expression->GetMaterialExpressionId().ToString(EGuidFormats::Digits));
        NodeObj->SetStringField(TEXT("class"), Expression->GetClass()->GetName());
        NodeObj->SetStringField(TEXT("description"), Expression->Desc);
        
        // Position in graph
        NodeObj->SetNumberField(TEXT("pos_x"), Expression->MaterialExpressionEditorX);
        NodeObj->SetNumberField(TEXT("pos_y"), Expression->MaterialExpressionEditorY);

        // Get type-specific info
        FString NodeType = Expression->GetClass()->GetName();
        NodeType.RemoveFromStart(TEXT("MaterialExpression"));
        NodeObj->SetStringField(TEXT("type"), NodeType);

        // Add specific values for common node types
        if (UMaterialExpressionConstant* Const = Cast<UMaterialExpressionConstant>(Expression))
        {
            NodeObj->SetNumberField(TEXT("value"), Const->R);
        }
        else if (UMaterialExpressionConstant3Vector* Const3 = Cast<UMaterialExpressionConstant3Vector>(Expression))
        {
            TArray<TSharedPtr<FJsonValue>> ColorArray;
            ColorArray.Add(MakeShared<FJsonValueNumber>(Const3->Constant.R));
            ColorArray.Add(MakeShared<FJsonValueNumber>(Const3->Constant.G));
            ColorArray.Add(MakeShared<FJsonValueNumber>(Const3->Constant.B));
            NodeObj->SetArrayField(TEXT("color"), ColorArray);
        }
        else if (UMaterialExpressionConstant4Vector* Const4 = Cast<UMaterialExpressionConstant4Vector>(Expression))
        {
            TArray<TSharedPtr<FJsonValue>> ColorArray;
            ColorArray.Add(MakeShared<FJsonValueNumber>(Const4->Constant.R));
            ColorArray.Add(MakeShared<FJsonValueNumber>(Const4->Constant.G));
            ColorArray.Add(MakeShared<FJsonValueNumber>(Const4->Constant.B));
            ColorArray.Add(MakeShared<FJsonValueNumber>(Const4->Constant.A));
            NodeObj->SetArrayField(TEXT("color"), ColorArray);
        }
        else if (UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expression))
        {
            NodeObj->SetStringField(TEXT("parameter_name"), ScalarParam->ParameterName.ToString());
            NodeObj->SetNumberField(TEXT("default_value"), ScalarParam->DefaultValue);
        }
        else if (UMaterialExpressionVectorParameter* VectorParam = Cast<UMaterialExpressionVectorParameter>(Expression))
        {
            NodeObj->SetStringField(TEXT("parameter_name"), VectorParam->ParameterName.ToString());
            TArray<TSharedPtr<FJsonValue>> DefaultArray;
            DefaultArray.Add(MakeShared<FJsonValueNumber>(VectorParam->DefaultValue.R));
            DefaultArray.Add(MakeShared<FJsonValueNumber>(VectorParam->DefaultValue.G));
            DefaultArray.Add(MakeShared<FJsonValueNumber>(VectorParam->DefaultValue.B));
            DefaultArray.Add(MakeShared<FJsonValueNumber>(VectorParam->DefaultValue.A));
            NodeObj->SetArrayField(TEXT("default_value"), DefaultArray);
        }
        else if (UMaterialExpressionTextureSampleParameter2D* TexParam = Cast<UMaterialExpressionTextureSampleParameter2D>(Expression))
        {
            NodeObj->SetStringField(TEXT("parameter_name"), TexParam->ParameterName.ToString());
            if (TexParam->Texture)
            {
                NodeObj->SetStringField(TEXT("texture_path"), TexParam->Texture->GetPathName());
                NodeObj->SetStringField(TEXT("texture_name"), TexParam->Texture->GetName());
            }
        }
        else if (UMaterialExpressionTextureSample* TexSample = Cast<UMaterialExpressionTextureSample>(Expression))
        {
            if (TexSample->Texture)
            {
                NodeObj->SetStringField(TEXT("texture_path"), TexSample->Texture->GetPathName());
                NodeObj->SetStringField(TEXT("texture_name"), TexSample->Texture->GetName());
            }
        }
        else if (UMaterialExpressionTextureCoordinate* TexCoord = Cast<UMaterialExpressionTextureCoordinate>(Expression))
        {
            NodeObj->SetNumberField(TEXT("coordinate_index"), TexCoord->CoordinateIndex);
            NodeObj->SetNumberField(TEXT("u_tiling"), TexCoord->UTiling);
            NodeObj->SetNumberField(TEXT("v_tiling"), TexCoord->VTiling);
        }
        else if (UMaterialExpressionPanner* Panner = Cast<UMaterialExpressionPanner>(Expression))
        {
            NodeObj->SetNumberField(TEXT("speed_x"), Panner->SpeedX);
            NodeObj->SetNumberField(TEXT("speed_y"), Panner->SpeedY);
            NodeObj->SetNumberField(TEXT("const_coordinate"), Panner->ConstCoordinate);
            NodeObj->SetBoolField(TEXT("fractional_part"), Panner->bFractionalPart);
        }
        else if (UMaterialExpressionRotator* Rotator = Cast<UMaterialExpressionRotator>(Expression))
        {
            NodeObj->SetNumberField(TEXT("center_x"), Rotator->CenterX);
            NodeObj->SetNumberField(TEXT("center_y"), Rotator->CenterY);
            NodeObj->SetNumberField(TEXT("speed"), Rotator->Speed);
            NodeObj->SetNumberField(TEXT("const_coordinate"), Rotator->ConstCoordinate);
        }
        else if (UMaterialExpressionSine* Sine = Cast<UMaterialExpressionSine>(Expression))
        {
            NodeObj->SetNumberField(TEXT("period"), Sine->Period);
        }
        else if (UMaterialExpressionCosine* Cosine = Cast<UMaterialExpressionCosine>(Expression))
        {
            NodeObj->SetNumberField(TEXT("period"), Cosine->Period);
        }
        else if (UMaterialExpressionObjectPositionWS* ObjectPosition = Cast<UMaterialExpressionObjectPositionWS>(Expression))
        {
            NodeObj->SetStringField(TEXT("origin_type"), PositionOriginToString(ObjectPosition->OriginType));
        }
        // MaterialFunctionCall - Extract function details
        else if (UMaterialExpressionMaterialFunctionCall* FuncCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
        {
            if (FuncCall->MaterialFunction)
            {
                UMaterialFunction* MatFunc = Cast<UMaterialFunction>(FuncCall->MaterialFunction.Get());
                NodeObj->SetStringField(TEXT("function_path"), GetAssetPackagePath(MatFunc));
                NodeObj->SetStringField(TEXT("function_name"), MatFunc->GetName());
                NodeObj->SetStringField(TEXT("function_description"), MatFunc->GetDescription());
                
                // Get function inputs
                TArray<TSharedPtr<FJsonValue>> FuncInputsArray;
                for (const FFunctionExpressionInput& FuncInput : FuncCall->FunctionInputs)
                {
                    TSharedPtr<FJsonObject> InputObj = MakeShared<FJsonObject>();
                    InputObj->SetStringField(TEXT("name"), FuncInput.ExpressionInput ? FuncInput.ExpressionInput->InputName.ToString() : TEXT(""));
                    InputObj->SetBoolField(TEXT("is_connected"), FuncInput.Input.Expression != nullptr);
                    FuncInputsArray.Add(MakeShared<FJsonValueObject>(InputObj));
                }
                NodeObj->SetArrayField(TEXT("function_inputs"), FuncInputsArray);
                
                // Get function outputs
                TArray<TSharedPtr<FJsonValue>> FuncOutputsArray;
                for (const FFunctionExpressionOutput& FuncOutput : FuncCall->FunctionOutputs)
                {
                    TSharedPtr<FJsonObject> OutputObj = MakeShared<FJsonObject>();
                    OutputObj->SetStringField(TEXT("name"), FuncOutput.ExpressionOutput ? FuncOutput.ExpressionOutput->OutputName.ToString() : TEXT(""));
                    FuncOutputsArray.Add(MakeShared<FJsonValueObject>(OutputObj));
                }
                NodeObj->SetArrayField(TEXT("function_outputs"), FuncOutputsArray);
            }
        }
        // StaticBool
        else if (UMaterialExpressionStaticBool* StaticBool = Cast<UMaterialExpressionStaticBool>(Expression))
        {
            NodeObj->SetBoolField(TEXT("value"), StaticBool->Value);
        }
        // StaticBoolParameter
        else if (UMaterialExpressionStaticBoolParameter* StaticBoolParam = Cast<UMaterialExpressionStaticBoolParameter>(Expression))
        {
            NodeObj->SetStringField(TEXT("parameter_name"), StaticBoolParam->ParameterName.ToString());
            NodeObj->SetBoolField(TEXT("default_value"), StaticBoolParam->DefaultValue);
        }
        // StaticSwitch
        else if (UMaterialExpressionStaticSwitch* StaticSwitch = Cast<UMaterialExpressionStaticSwitch>(Expression))
        {
            NodeObj->SetBoolField(TEXT("default_value"), StaticSwitch->DefaultValue);
        }
        // Comment node
        else if (UMaterialExpressionComment* Comment = Cast<UMaterialExpressionComment>(Expression))
        {
            NodeObj->SetStringField(TEXT("comment_text"), Comment->Text);
            NodeObj->SetNumberField(TEXT("size_x"), Comment->SizeX);
            NodeObj->SetNumberField(TEXT("size_y"), Comment->SizeY);
        }
        // TextureObjectParameter
        else if (UMaterialExpressionTextureObjectParameter* TexObjParam = Cast<UMaterialExpressionTextureObjectParameter>(Expression))
        {
            NodeObj->SetStringField(TEXT("parameter_name"), TexObjParam->ParameterName.ToString());
            if (TexObjParam->Texture)
            {
                NodeObj->SetStringField(TEXT("texture_path"), TexObjParam->Texture->GetPathName());
            }
        }
        // AppendVector
        else if (UMaterialExpressionAppendVector* Append = Cast<UMaterialExpressionAppendVector>(Expression))
        {
            NodeObj->SetStringField(TEXT("operation"), TEXT("Append"));
        }
        // ComponentMask
        else if (UMaterialExpressionComponentMask* Mask = Cast<UMaterialExpressionComponentMask>(Expression))
        {
            FString MaskStr;
            if (Mask->R) MaskStr += TEXT("R");
            if (Mask->G) MaskStr += TEXT("G");
            if (Mask->B) MaskStr += TEXT("B");
            if (Mask->A) MaskStr += TEXT("A");
            NodeObj->SetStringField(TEXT("mask"), MaskStr);
        }
        // If node
        else if (UMaterialExpressionIf* IfNode = Cast<UMaterialExpressionIf>(Expression))
        {
            NodeObj->SetNumberField(TEXT("equals_threshold"), IfNode->EqualsThreshold);
        }
        // Power
        else if (UMaterialExpressionPower* Power = Cast<UMaterialExpressionPower>(Expression))
        {
            NodeObj->SetNumberField(TEXT("const_exponent"), Power->ConstExponent);
        }
        // Clamp
        else if (UMaterialExpressionClamp* Clamp = Cast<UMaterialExpressionClamp>(Expression))
        {
            NodeObj->SetNumberField(TEXT("min_default"), Clamp->MinDefault);
            NodeObj->SetNumberField(TEXT("max_default"), Clamp->MaxDefault);
        }
        // FunctionInput
        else if (UMaterialExpressionFunctionInput* FuncInput = Cast<UMaterialExpressionFunctionInput>(Expression))
        {
            NodeObj->SetStringField(TEXT("input_name"), FuncInput->InputName.ToString());
            NodeObj->SetStringField(TEXT("description"), FuncInput->Description);
        }
        // FunctionOutput
        else if (UMaterialExpressionFunctionOutput* FuncOutput = Cast<UMaterialExpressionFunctionOutput>(Expression))
        {
            NodeObj->SetStringField(TEXT("output_name"), FuncOutput->OutputName.ToString());
            NodeObj->SetStringField(TEXT("description"), FuncOutput->Description);
        }
        // Fresnel
        else if (UMaterialExpressionFresnel* Fresnel = Cast<UMaterialExpressionFresnel>(Expression))
        {
            NodeObj->SetNumberField(TEXT("exponent"), Fresnel->Exponent);
            NodeObj->SetNumberField(TEXT("base_reflect_fraction"), Fresnel->BaseReflectFraction);
        }
        // Time
        else if (UMaterialExpressionTime* TimeNode = Cast<UMaterialExpressionTime>(Expression))
        {
            NodeObj->SetBoolField(TEXT("ignore_pause"), TimeNode->bIgnorePause);
            NodeObj->SetBoolField(TEXT("override_period"), TimeNode->bOverride_Period);
        }
        // WorldPosition
        else if (UMaterialExpressionWorldPosition* WorldPos = Cast<UMaterialExpressionWorldPosition>(Expression))
        {
            NodeObj->SetStringField(TEXT("world_position_type"), TEXT("WorldPosition"));
        }

        // Get outputs info
        TArray<FExpressionOutput>& Outputs = Expression->GetOutputs();
        TArray<TSharedPtr<FJsonValue>> OutputsArray;
        for (int32 j = 0; j < Outputs.Num(); j++)
        {
            TSharedPtr<FJsonObject> OutputObj = MakeShared<FJsonObject>();
            OutputObj->SetNumberField(TEXT("index"), j);
            OutputObj->SetStringField(TEXT("name"), Outputs[j].OutputName.ToString());
            OutputsArray.Add(MakeShared<FJsonValueObject>(OutputObj));
        }
        NodeObj->SetArrayField(TEXT("outputs"), OutputsArray);

        NodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
    }

    ResultObj->SetArrayField(TEXT("nodes"), NodesArray);
    ResultObj->SetNumberField(TEXT("node_count"), NodesArray.Num());

    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleGetMaterialConnections(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath;
    
    // Try to get path from params, or use currently opened material
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
        if (AssetEditorSubsystem)
        {
            TArray<UObject*> EditedAssets = AssetEditorSubsystem->GetAllEditedAssets();
            for (UObject* Asset : EditedAssets)
            {
                if (Cast<UMaterial>(Asset))
                {
                    MaterialPath = Asset->GetPathName();
                    break;
                }
            }
        }
    }

    if (MaterialPath.IsEmpty())
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("No material path provided and no material is currently opened"));
    }

    UMaterial* Material = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
    if (!Material)
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
    
    TArray<TSharedPtr<FJsonValue>> ConnectionsArray;
    auto Expressions = Material->GetExpressions();

    // Helper to find expression index
    auto FindExpressionIndex = [&Expressions](UMaterialExpression* Expr) -> int32
    {
        for (int32 i = 0; i < Expressions.Num(); i++)
        {
            if (Expressions[i].Get() == Expr) return i;
        }
        return INDEX_NONE;
    };

    // Get connections to material outputs using GetExpressionInputForProperty
    auto AddOutputConnection = [&](EMaterialProperty Property, const FString& OutputName)
    {
        const FExpressionInput* Input = Material->GetExpressionInputForProperty(Property);
        if (Input && Input->Expression)
        {
            int32 SourceIndex = FindExpressionIndex(Input->Expression);
            TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
            ConnObj->SetNumberField(TEXT("source_node_index"), SourceIndex);
            ConnObj->SetStringField(TEXT("source_node_name"), Input->Expression->GetName());
            ConnObj->SetNumberField(TEXT("source_output_index"), Input->OutputIndex);
            ConnObj->SetStringField(TEXT("target"), TEXT("MaterialOutput"));
            ConnObj->SetStringField(TEXT("target_input"), OutputName);
            ConnectionsArray.Add(MakeShared<FJsonValueObject>(ConnObj));
        }
    };

    AddOutputConnection(MP_BaseColor, TEXT("BaseColor"));
    AddOutputConnection(MP_Metallic, TEXT("Metallic"));
    AddOutputConnection(MP_Specular, TEXT("Specular"));
    AddOutputConnection(MP_Roughness, TEXT("Roughness"));
    AddOutputConnection(MP_Anisotropy, TEXT("Anisotropy"));
    AddOutputConnection(MP_EmissiveColor, TEXT("EmissiveColor"));
    AddOutputConnection(MP_Opacity, TEXT("Opacity"));
    AddOutputConnection(MP_OpacityMask, TEXT("OpacityMask"));
    AddOutputConnection(MP_Normal, TEXT("Normal"));
    AddOutputConnection(MP_Tangent, TEXT("Tangent"));
    AddOutputConnection(MP_WorldPositionOffset, TEXT("WorldPositionOffset"));
    AddOutputConnection(MP_SubsurfaceColor, TEXT("SubsurfaceColor"));
    AddOutputConnection(MP_AmbientOcclusion, TEXT("AmbientOcclusion"));
    AddOutputConnection(MP_Refraction, TEXT("Refraction"));
    AddOutputConnection(MP_PixelDepthOffset, TEXT("PixelDepthOffset"));

    // Get connections between nodes by iterating through each expression's inputs
    for (int32 TargetIndex = 0; TargetIndex < Expressions.Num(); TargetIndex++)
    {
        UMaterialExpression* TargetExpr = Expressions[TargetIndex].Get();
        if (!TargetExpr) continue;

        // Iterate through inputs using GetInput() method
        for (int32 InputIndex = 0; ; InputIndex++)
        {
            FExpressionInput* Input = TargetExpr->GetInput(InputIndex);
            if (!Input) break; // No more inputs
            
            if (Input->Expression)
            {
                int32 SourceIndex = FindExpressionIndex(Input->Expression);
                
                TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
                ConnObj->SetNumberField(TEXT("source_node_index"), SourceIndex);
                ConnObj->SetStringField(TEXT("source_node_name"), Input->Expression->GetName());
                ConnObj->SetNumberField(TEXT("source_output_index"), Input->OutputIndex);
                ConnObj->SetNumberField(TEXT("target_node_index"), TargetIndex);
                ConnObj->SetStringField(TEXT("target_node_name"), TargetExpr->GetName());
                ConnObj->SetNumberField(TEXT("target_input_index"), InputIndex);
                
                FName InputName = TargetExpr->GetInputName(InputIndex);
                if (!InputName.IsNone())
                {
                    ConnObj->SetStringField(TEXT("target_input_name"), InputName.ToString());
                }
                
                ConnectionsArray.Add(MakeShared<FJsonValueObject>(ConnObj));
            }
        }
    }

    ResultObj->SetArrayField(TEXT("connections"), ConnectionsArray);
    ResultObj->SetNumberField(TEXT("connection_count"), ConnectionsArray.Num());

    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleCreateMaterialNode(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!TryGetMaterialPath(Params, MaterialPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
	}

	FString NodeType;
	if (!Params->TryGetStringField(TEXT("node_type"), NodeType))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'node_type' parameter"));
	}

	UMaterial* Material = LoadMaterialForEditing(MaterialPath);
	if (!Material)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	TSubclassOf<UMaterialExpression> ExpressionClass = ResolveExpressionClass(NodeType);
	if (!ExpressionClass)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unsupported material node type: %s"), *NodeType));
	}

	int32 NodePosX = 0;
	int32 NodePosY = 0;
	Params->TryGetNumberField(TEXT("node_pos_x"), NodePosX);
	Params->TryGetNumberField(TEXT("node_pos_y"), NodePosY);

	UMaterialExpression* Expression = UMaterialEditingLibrary::CreateMaterialExpression(Material, ExpressionClass, NodePosX, NodePosY);
	if (!Expression)
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to create material node"));
	}

	FString ErrorMessage;
	if (!InitializeMaterialExpressionFromParams(Expression, Params, ErrorMessage))
	{
		UMaterialEditingLibrary::DeleteMaterialExpression(Material, Expression);
		return FUECLICommonUtils::CreateErrorResponse(ErrorMessage);
	}

	Material->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
	ResultObj->SetStringField(TEXT("node_name"), Expression->GetName());
	ResultObj->SetStringField(TEXT("node_guid"), Expression->GetMaterialExpressionId().ToString(EGuidFormats::Digits));
	ResultObj->SetStringField(TEXT("node_type"), Expression->GetClass()->GetName().Replace(TEXT("MaterialExpression"), TEXT("")));
	ResultObj->SetNumberField(TEXT("node_pos_x"), Expression->MaterialExpressionEditorX);
	ResultObj->SetNumberField(TEXT("node_pos_y"), Expression->MaterialExpressionEditorY);
	ResultObj->SetBoolField(TEXT("success"), true);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleConnectMaterialNodes(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!TryGetMaterialPath(Params, MaterialPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
	}

	FString FromNodeName;
	if (!Params->TryGetStringField(TEXT("from_node"), FromNodeName))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'from_node' parameter"));
	}

	FString ToNodeName;
	if (!Params->TryGetStringField(TEXT("to_node"), ToNodeName))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'to_node' parameter"));
	}

	UMaterial* Material = LoadMaterialForEditing(MaterialPath);
	if (!Material)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	UMaterialExpression* FromNode = FindMaterialExpressionByReference(Material, FromNodeName);
	if (!FromNode)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Source node not found: %s"), *FromNodeName));
	}

	UMaterialExpression* ToNode = FindMaterialExpressionByReference(Material, ToNodeName);
	if (!ToNode)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Target node not found: %s"), *ToNodeName));
	}

	FString FromOutputName;
	Params->TryGetStringField(TEXT("from_output"), FromOutputName);

	FString ToInputName;
	Params->TryGetStringField(TEXT("to_input"), ToInputName);

	if (!UMaterialEditingLibrary::ConnectMaterialExpressions(FromNode, FromOutputName, ToNode, ToInputName))
	{
		return FUECLICommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to connect '%s' to '%s'"), *FromNodeName, *ToNodeName));
	}

	Material->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
	ResultObj->SetStringField(TEXT("from_node"), FromNodeName);
	ResultObj->SetStringField(TEXT("to_node"), ToNodeName);
	ResultObj->SetStringField(TEXT("from_output"), FromOutputName);
	ResultObj->SetStringField(TEXT("to_input"), ToInputName);
	ResultObj->SetBoolField(TEXT("success"), true);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleSetMaterialOutput(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!TryGetMaterialPath(Params, MaterialPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
	}

	FString FromNodeName;
	if (!Params->TryGetStringField(TEXT("from_node"), FromNodeName))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'from_node' parameter"));
	}

	FString PropertyName;
	if (!Params->TryGetStringField(TEXT("property"), PropertyName))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'property' parameter"));
	}

	UMaterial* Material = LoadMaterialForEditing(MaterialPath);
	if (!Material)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	UMaterialExpression* FromNode = FindMaterialExpressionByReference(Material, FromNodeName);
	if (!FromNode)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Source node not found: %s"), *FromNodeName));
	}

	EMaterialProperty Property = MP_MAX;
	if (!TryResolveMaterialProperty(PropertyName, Property))
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unsupported material property: %s"), *PropertyName));
	}

	FString FromOutputName;
	Params->TryGetStringField(TEXT("from_output"), FromOutputName);

	if (!UMaterialEditingLibrary::ConnectMaterialProperty(FromNode, FromOutputName, Property))
	{
		return FUECLICommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to connect node '%s' to material property '%s'"), *FromNodeName, *PropertyName));
	}

	Material->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
	ResultObj->SetStringField(TEXT("from_node"), FromNodeName);
	ResultObj->SetStringField(TEXT("property"), PropertyName);
	ResultObj->SetStringField(TEXT("from_output"), FromOutputName);
	ResultObj->SetBoolField(TEXT("success"), true);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleDeleteMaterialNode(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!TryGetMaterialPath(Params, MaterialPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
	}

	FString NodeName;
	if (!Params->TryGetStringField(TEXT("node_name"), NodeName))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'node_name' parameter"));
	}

	UMaterial* Material = LoadMaterialForEditing(MaterialPath);
	if (!Material)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	UMaterialExpression* Node = FindMaterialExpressionByReference(Material, NodeName);
	if (!Node)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Node not found: %s"), *NodeName));
	}

	const FString ResolvedName = Node->GetName();
	UMaterialEditingLibrary::DeleteMaterialExpression(Material, Node);
	Material->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
	ResultObj->SetStringField(TEXT("node_name"), ResolvedName);
	ResultObj->SetBoolField(TEXT("success"), true);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleDisconnectMaterialNodes(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!TryGetMaterialPath(Params, MaterialPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
	}

	FString ToNodeName;
	if (!Params->TryGetStringField(TEXT("to_node"), ToNodeName))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'to_node' parameter"));
	}

	UMaterial* Material = LoadMaterialForEditing(MaterialPath);
	if (!Material)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	UMaterialExpression* ToNode = FindMaterialExpressionByReference(Material, ToNodeName);
	if (!ToNode)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Target node not found: %s"), *ToNodeName));
	}

	FString FromNodeName;
	Params->TryGetStringField(TEXT("from_node"), FromNodeName);
	UMaterialExpression* ExpectedFromNode = FromNodeName.IsEmpty() ? nullptr : FindMaterialExpressionByReference(Material, FromNodeName);
	if (!FromNodeName.IsEmpty() && ExpectedFromNode == nullptr)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Source node not found: %s"), *FromNodeName));
	}

	FString ToInputName;
	Params->TryGetStringField(TEXT("to_input"), ToInputName);

	int32 InputIndex = INDEX_NONE;
	FExpressionInput* Input = nullptr;
	if (!TryResolveExpressionInput(ToNode, ToInputName, InputIndex, Input))
	{
		return FUECLICommonUtils::CreateErrorResponse(
			ToInputName.IsEmpty()
				? FString::Printf(TEXT("Node '%s' has no disconnectable inputs"), *ToNodeName)
				: FString::Printf(TEXT("Input '%s' not found on node '%s'"), *ToInputName, *ToNodeName));
	}

	if (Input->Expression == nullptr)
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Target input is already disconnected"));
	}

	if (ExpectedFromNode != nullptr && Input->Expression != ExpectedFromNode)
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Target input is connected to a different source node"));
	}

	const FString ResolvedInputName = ToNode->GetInputName(InputIndex).ToString();
	const FString ResolvedFromNodeName = Input->Expression->GetName();
	ResetExpressionInput(Input);
	Material->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
	ResultObj->SetStringField(TEXT("from_node"), ResolvedFromNodeName);
	ResultObj->SetStringField(TEXT("to_node"), ToNode->GetName());
	ResultObj->SetStringField(TEXT("to_input"), ResolvedInputName);
	ResultObj->SetBoolField(TEXT("success"), true);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleClearMaterialOutput(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!TryGetMaterialPath(Params, MaterialPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
	}

	FString PropertyName;
	if (!Params->TryGetStringField(TEXT("property"), PropertyName))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'property' parameter"));
	}

	UMaterial* Material = LoadMaterialForEditing(MaterialPath);
	if (!Material)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	EMaterialProperty Property = MP_MAX;
	if (!TryResolveMaterialProperty(PropertyName, Property))
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unsupported material property: %s"), *PropertyName));
	}

	FExpressionInput* Input = Material->GetExpressionInputForProperty(Property);
	if (!Input)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material property is not editable: %s"), *PropertyName));
	}

	const bool bWasConnected = ResetExpressionInput(Input);
	Material->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
	ResultObj->SetStringField(TEXT("property"), PropertyName);
	ResultObj->SetBoolField(TEXT("was_connected"), bWasConnected);
	ResultObj->SetBoolField(TEXT("success"), true);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleMoveMaterialNode(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!TryGetMaterialPath(Params, MaterialPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
	}

	FString NodeName;
	if (!Params->TryGetStringField(TEXT("node_name"), NodeName))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'node_name' parameter"));
	}

	int32 NodePosX = 0;
	int32 NodePosY = 0;
	if (!Params->TryGetNumberField(TEXT("node_pos_x"), NodePosX) || !Params->TryGetNumberField(TEXT("node_pos_y"), NodePosY))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'node_pos_x' or 'node_pos_y' parameter"));
	}

	UMaterial* Material = LoadMaterialForEditing(MaterialPath);
	if (!Material)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	UMaterialExpression* Node = FindMaterialExpressionByReference(Material, NodeName);
	if (!Node)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Node not found: %s"), *NodeName));
	}

	Node->MaterialExpressionEditorX = NodePosX;
	Node->MaterialExpressionEditorY = NodePosY;
	Material->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
	ResultObj->SetStringField(TEXT("node_name"), Node->GetName());
	ResultObj->SetNumberField(TEXT("node_pos_x"), NodePosX);
	ResultObj->SetNumberField(TEXT("node_pos_y"), NodePosY);
	ResultObj->SetBoolField(TEXT("success"), true);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleSetMaterialNodeProperty(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!TryGetMaterialPath(Params, MaterialPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
	}

	FString NodeName;
	if (!Params->TryGetStringField(TEXT("node_name"), NodeName))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'node_name' parameter"));
	}

	FString PropertyName;
	if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
	}

	const TSharedPtr<FJsonValue>* ValuePtr = Params->Values.Find(TEXT("value"));
	if (ValuePtr == nullptr || !ValuePtr->IsValid())
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'value' parameter"));
	}

	UMaterial* Material = LoadMaterialForEditing(MaterialPath);
	if (!Material)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	UMaterialExpression* Node = FindMaterialExpressionByReference(Material, NodeName);
	if (!Node)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Node not found: %s"), *NodeName));
	}

	FString ErrorMessage;
	if (!FUECLICommonUtils::SetObjectProperty(Node, PropertyName, *ValuePtr, ErrorMessage))
	{
		return FUECLICommonUtils::CreateErrorResponse(ErrorMessage);
	}

	if (Node->HasAParameterName() && PropertyName.Equals(TEXT("ParameterName"), ESearchCase::IgnoreCase))
	{
		Node->UpdateParameterGuid(true, false);
		Node->ValidateParameterName(false);
	}

	Node->PostEditChange();
	Material->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
	ResultObj->SetStringField(TEXT("node_name"), Node->GetName());
	ResultObj->SetStringField(TEXT("property_name"), PropertyName);
	ResultObj->SetBoolField(TEXT("success"), true);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleLayoutMaterialNodes(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!TryGetMaterialPath(Params, MaterialPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
	}

	UMaterial* Material = LoadMaterialForEditing(MaterialPath);
	if (!Material)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	UMaterialEditingLibrary::LayoutMaterialExpressions(Material);
	Material->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
	ResultObj->SetBoolField(TEXT("success"), true);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleCompileMaterial(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!TryGetMaterialPath(Params, MaterialPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
	}

	UMaterial* Material = LoadMaterialForEditing(MaterialPath);
	if (!Material)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	bool bLayout = false;
	Params->TryGetBoolField(TEXT("layout"), bLayout);
	if (bLayout)
	{
		UMaterialEditingLibrary::LayoutMaterialExpressions(Material);
	}

	UMaterialEditingLibrary::RecompileMaterial(Material);

	UObject* MaterialObject = Material;
	FAssetCompilingManager::Get().FinishCompilationForObjects(MakeArrayView(&MaterialObject, 1));
	if (GShaderCompilingManager != nullptr)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}
	FAssetCompilingManager::Get().ProcessAsyncTasks(true);

	bool bHadCompileError = false;
	TArray<TSharedPtr<FJsonValue>> CompileErrors;
	if (const FMaterialResource* MaterialResource = Material->GetMaterialResource(ERHIFeatureLevel::SM5))
	{
		for (const FString& CompileError : MaterialResource->GetCompileErrors())
		{
			if (!CompileError.IsEmpty())
			{
				bHadCompileError = true;
				CompileErrors.Add(MakeShared<FJsonValueString>(CompileError));
			}
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
	ResultObj->SetBoolField(TEXT("layout_applied"), bLayout);
	ResultObj->SetBoolField(TEXT("had_compile_error"), bHadCompileError);
	ResultObj->SetArrayField(TEXT("compile_errors"), CompileErrors);
	ResultObj->SetBoolField(TEXT("success"), true);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleSaveMaterial(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!TryGetMaterialPath(Params, MaterialPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
	}

	if (!UEditorAssetLibrary::DoesAssetExist(MaterialPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	if (!UEditorAssetLibrary::SaveAsset(MaterialPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to save material: %s"), *MaterialPath));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
	ResultObj->SetBoolField(TEXT("success"), true);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleCreateMaterialFunctionNode(const TSharedPtr<FJsonObject>& Params)
{
	FString FunctionPath;
	if (!TryGetFunctionPath(Params, FunctionPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'function_path' parameter"));
	}

	FString NodeType;
	if (!Params->TryGetStringField(TEXT("node_type"), NodeType))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'node_type' parameter"));
	}

	UMaterialFunction* MaterialFunction = LoadMaterialFunctionForEditing(FunctionPath);
	if (!MaterialFunction)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material function not found: %s"), *FunctionPath));
	}

	TSubclassOf<UMaterialExpression> ExpressionClass = ResolveExpressionClass(NodeType);
	if (!ExpressionClass)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unsupported material function node type: %s"), *NodeType));
	}

	int32 NodePosX = 0;
	int32 NodePosY = 0;
	Params->TryGetNumberField(TEXT("node_pos_x"), NodePosX);
	Params->TryGetNumberField(TEXT("node_pos_y"), NodePosY);

	UMaterialExpression* Expression = UMaterialEditingLibrary::CreateMaterialExpressionInFunction(MaterialFunction, ExpressionClass, NodePosX, NodePosY);
	if (!Expression)
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to create material function node"));
	}

	FString ErrorMessage;
	if (!InitializeMaterialExpressionFromParams(Expression, Params, ErrorMessage))
	{
		UMaterialEditingLibrary::DeleteMaterialExpressionInFunction(MaterialFunction, Expression);
		return FUECLICommonUtils::CreateErrorResponse(ErrorMessage);
	}

	MaterialFunction->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("function_path"), FunctionPath);
	ResultObj->SetStringField(TEXT("node_name"), Expression->GetName());
	ResultObj->SetStringField(TEXT("node_guid"), Expression->GetMaterialExpressionId().ToString(EGuidFormats::Digits));
	ResultObj->SetStringField(TEXT("node_type"), Expression->GetClass()->GetName().Replace(TEXT("MaterialExpression"), TEXT("")));
	ResultObj->SetNumberField(TEXT("node_pos_x"), Expression->MaterialExpressionEditorX);
	ResultObj->SetNumberField(TEXT("node_pos_y"), Expression->MaterialExpressionEditorY);
	ResultObj->SetBoolField(TEXT("success"), true);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleCreateMaterialFunctionScaffold(const TSharedPtr<FJsonObject>& Params)
{
	FString FunctionPath;
	if (!TryGetFunctionPath(Params, FunctionPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'function_path' parameter"));
	}

	FString ScaffoldType;
	if (!Params->TryGetStringField(TEXT("scaffold_type"), ScaffoldType))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'scaffold_type' parameter"));
	}

	const FString NormalizedScaffoldType = ScaffoldType.ToLower();
	if (NormalizedScaffoldType != TEXT("feather_wpo") && NormalizedScaffoldType != TEXT("feather_coverage"))
	{
		return FUECLICommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unsupported material function scaffold type: %s"), *ScaffoldType));
	}

	bool bLayout = true;
	Params->TryGetBoolField(TEXT("layout"), bLayout);

	bool bCompile = false;
	Params->TryGetBoolField(TEXT("compile"), bCompile);

	bool bSave = false;
	Params->TryGetBoolField(TEXT("save"), bSave);

	float SwayFrequency = 0.75f;
	TryGetNumberField(Params, TEXT("sway_frequency"), SwayFrequency);

	float SwayIntensity = 0.8f;
	TryGetNumberField(Params, TEXT("sway_intensity"), SwayIntensity);

	float CoveragePower = 1.35f;
	TryGetNumberField(Params, TEXT("coverage_power"), CoveragePower);

	auto MakeNumberArray = [](std::initializer_list<double> Values) -> TArray<TSharedPtr<FJsonValue>>
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		for (double Value : Values)
		{
			Result.Add(MakeShared<FJsonValueNumber>(Value));
		}
		return Result;
	};

	TArray<TSharedPtr<FJsonValue>> Operations;
	auto AddOperation = [&Operations](const TSharedPtr<FJsonObject>& Operation)
	{
		Operations.Add(MakeShared<FJsonValueObject>(Operation));
	};

	auto AddCreateNodeOp = [&AddOperation](const FString& Alias, const FString& NodeType, int32 PosX, int32 PosY)
	{
		TSharedPtr<FJsonObject> Operation = MakeShared<FJsonObject>();
		Operation->SetStringField(TEXT("action"), TEXT("create_node"));
		Operation->SetStringField(TEXT("alias"), Alias);
		Operation->SetStringField(TEXT("node_type"), NodeType);
		Operation->SetNumberField(TEXT("node_pos_x"), PosX);
		Operation->SetNumberField(TEXT("node_pos_y"), PosY);
		AddOperation(Operation);
		return Operation;
	};

	auto AddConnectOp = [&AddOperation](const FString& FromAlias, const FString& ToAlias, const FString& ToInput)
	{
		TSharedPtr<FJsonObject> Operation = MakeShared<FJsonObject>();
		Operation->SetStringField(TEXT("action"), TEXT("connect_nodes"));
		Operation->SetStringField(TEXT("from_node"), FromAlias);
		Operation->SetStringField(TEXT("to_node"), ToAlias);
		if (!ToInput.IsEmpty())
		{
			Operation->SetStringField(TEXT("to_input"), ToInput);
		}
		AddOperation(Operation);
	};

	if (NormalizedScaffoldType == TEXT("feather_wpo"))
	{
		{
			TSharedPtr<FJsonObject> Comment = AddCreateNodeOp(TEXT("wpo_comment"), TEXT("Comment"), -1480, -280);
			Comment->SetStringField(TEXT("text"), TEXT("Reusable feather WPO sway"));
			Comment->SetNumberField(TEXT("size_x"), 1960);
			Comment->SetNumberField(TEXT("size_y"), 880);
		}

		{
			TSharedPtr<FJsonObject> Input = AddCreateNodeOp(TEXT("mask_input"), TEXT("FunctionInput"), -1260, -60);
			Input->SetStringField(TEXT("input_name"), TEXT("Mask"));
			Input->SetStringField(TEXT("description"), TEXT("Per-feather coverage mask"));
			Input->SetStringField(TEXT("input_type"), TEXT("Scalar"));
			Input->SetNumberField(TEXT("sort_priority"), 0);
		}

		{
			TSharedPtr<FJsonObject> Input = AddCreateNodeOp(TEXT("time_input"), TEXT("FunctionInput"), -1260, 140);
			Input->SetStringField(TEXT("input_name"), TEXT("Time"));
			Input->SetStringField(TEXT("description"), TEXT("External time input for deterministic sway"));
			Input->SetStringField(TEXT("input_type"), TEXT("Scalar"));
			Input->SetNumberField(TEXT("sort_priority"), 1);
		}

		{
			TSharedPtr<FJsonObject> Input = AddCreateNodeOp(TEXT("frequency_input"), TEXT("FunctionInput"), -1260, 340);
			Input->SetStringField(TEXT("input_name"), TEXT("Frequency"));
			Input->SetStringField(TEXT("description"), TEXT("Oscillation frequency"));
			Input->SetStringField(TEXT("input_type"), TEXT("Scalar"));
			Input->SetNumberField(TEXT("sort_priority"), 2);
			Input->SetBoolField(TEXT("use_preview_value_as_default"), true);
			Input->SetArrayField(TEXT("preview_value"), MakeNumberArray({SwayFrequency, 0.0, 0.0, 0.0}));
		}

		{
			TSharedPtr<FJsonObject> Input = AddCreateNodeOp(TEXT("intensity_input"), TEXT("FunctionInput"), -1260, 540);
			Input->SetStringField(TEXT("input_name"), TEXT("Intensity"));
			Input->SetStringField(TEXT("description"), TEXT("Final displacement strength"));
			Input->SetStringField(TEXT("input_type"), TEXT("Scalar"));
			Input->SetNumberField(TEXT("sort_priority"), 3);
			Input->SetBoolField(TEXT("use_preview_value_as_default"), true);
			Input->SetArrayField(TEXT("preview_value"), MakeNumberArray({SwayIntensity, 0.0, 0.0, 0.0}));
		}

		{
			TSharedPtr<FJsonObject> Output = AddCreateNodeOp(TEXT("offset_output"), TEXT("FunctionOutput"), 420, 220);
			Output->SetStringField(TEXT("output_name"), TEXT("Offset"));
			Output->SetStringField(TEXT("description"), TEXT("World position offset vector"));
			Output->SetNumberField(TEXT("sort_priority"), 0);
		}

		TSharedPtr<FJsonObject> ZeroScalar = AddCreateNodeOp(TEXT("zero_scalar"), TEXT("Constant"), -780, 700);
		ZeroScalar->SetNumberField(TEXT("value"), 0.0);

		AddCreateNodeOp(TEXT("time_freq_mul"), TEXT("Multiply"), -900, 220);
		AddCreateNodeOp(TEXT("sway_sine"), TEXT("Sine"), -620, 220);
		AddCreateNodeOp(TEXT("sway_mask_mul"), TEXT("Multiply"), -360, 140);
		AddCreateNodeOp(TEXT("sway_intensity_mul"), TEXT("Multiply"), -100, 140);
		AddCreateNodeOp(TEXT("wpo_xy"), TEXT("AppendVector"), 120, 340);
		AddCreateNodeOp(TEXT("wpo_xyz"), TEXT("AppendVector"), 340, 220);

		AddConnectOp(TEXT("time_input"), TEXT("time_freq_mul"), TEXT("A"));
		AddConnectOp(TEXT("frequency_input"), TEXT("time_freq_mul"), TEXT("B"));
		AddConnectOp(TEXT("time_freq_mul"), TEXT("sway_sine"), FString());
		AddConnectOp(TEXT("sway_sine"), TEXT("sway_mask_mul"), TEXT("A"));
		AddConnectOp(TEXT("mask_input"), TEXT("sway_mask_mul"), TEXT("B"));
		AddConnectOp(TEXT("sway_mask_mul"), TEXT("sway_intensity_mul"), TEXT("A"));
		AddConnectOp(TEXT("intensity_input"), TEXT("sway_intensity_mul"), TEXT("B"));
		AddConnectOp(TEXT("zero_scalar"), TEXT("wpo_xy"), TEXT("A"));
		AddConnectOp(TEXT("zero_scalar"), TEXT("wpo_xy"), TEXT("B"));
		AddConnectOp(TEXT("wpo_xy"), TEXT("wpo_xyz"), TEXT("A"));
		AddConnectOp(TEXT("sway_intensity_mul"), TEXT("wpo_xyz"), TEXT("B"));
		AddConnectOp(TEXT("wpo_xyz"), TEXT("offset_output"), FString());
	}
	else
	{
		{
			TSharedPtr<FJsonObject> Comment = AddCreateNodeOp(TEXT("coverage_comment"), TEXT("Comment"), -1240, -220);
			Comment->SetStringField(TEXT("text"), TEXT("Reusable feather coverage curve"));
			Comment->SetNumberField(TEXT("size_x"), 1640);
			Comment->SetNumberField(TEXT("size_y"), 620);
		}

		{
			TSharedPtr<FJsonObject> Input = AddCreateNodeOp(TEXT("mask_input"), TEXT("FunctionInput"), -1020, -20);
			Input->SetStringField(TEXT("input_name"), TEXT("Mask"));
			Input->SetStringField(TEXT("description"), TEXT("Input feather alpha or coverage mask"));
			Input->SetStringField(TEXT("input_type"), TEXT("Scalar"));
			Input->SetNumberField(TEXT("sort_priority"), 0);
		}

		{
			TSharedPtr<FJsonObject> Input = AddCreateNodeOp(TEXT("power_input"), TEXT("FunctionInput"), -1020, 220);
			Input->SetStringField(TEXT("input_name"), TEXT("Power"));
			Input->SetStringField(TEXT("description"), TEXT("Coverage response exponent"));
			Input->SetStringField(TEXT("input_type"), TEXT("Scalar"));
			Input->SetNumberField(TEXT("sort_priority"), 1);
			Input->SetBoolField(TEXT("use_preview_value_as_default"), true);
			Input->SetArrayField(TEXT("preview_value"), MakeNumberArray({CoveragePower, 0.0, 0.0, 0.0}));
		}

		{
			TSharedPtr<FJsonObject> Output = AddCreateNodeOp(TEXT("coverage_output"), TEXT("FunctionOutput"), 320, 80);
			Output->SetStringField(TEXT("output_name"), TEXT("Coverage"));
			Output->SetStringField(TEXT("description"), TEXT("Shaped feather opacity mask"));
			Output->SetNumberField(TEXT("sort_priority"), 0);
		}

		TSharedPtr<FJsonObject> CoverageCurve = AddCreateNodeOp(TEXT("coverage_curve"), TEXT("Power"), -340, 80);
		CoverageCurve->SetNumberField(TEXT("const_exponent"), CoveragePower);

		AddConnectOp(TEXT("mask_input"), TEXT("coverage_curve"), TEXT("Base"));
		AddConnectOp(TEXT("power_input"), TEXT("coverage_curve"), TEXT("Exp"));
		AddConnectOp(TEXT("coverage_curve"), TEXT("coverage_output"), FString());
	}

	if (bLayout)
	{
		TSharedPtr<FJsonObject> Operation = MakeShared<FJsonObject>();
		Operation->SetStringField(TEXT("action"), TEXT("layout_nodes"));
		AddOperation(Operation);
	}

	if (bCompile)
	{
		TSharedPtr<FJsonObject> Operation = MakeShared<FJsonObject>();
		Operation->SetStringField(TEXT("action"), TEXT("compile_function"));
		AddOperation(Operation);
	}

	if (bSave)
	{
		TSharedPtr<FJsonObject> Operation = MakeShared<FJsonObject>();
		Operation->SetStringField(TEXT("action"), TEXT("save_function"));
		AddOperation(Operation);
	}

	TSharedPtr<FJsonObject> PatchParams = MakeShared<FJsonObject>();
	PatchParams->SetStringField(TEXT("function_path"), FunctionPath);
	PatchParams->SetArrayField(TEXT("operations"), Operations);
	const TSharedPtr<FJsonObject> PatchResult = HandleApplyMaterialFunctionPatch(PatchParams);
	if (PatchResult.IsValid())
	{
		PatchResult->SetStringField(TEXT("scaffold_type"), NormalizedScaffoldType);
	}
	return PatchResult;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleConnectMaterialFunctionNodes(const TSharedPtr<FJsonObject>& Params)
{
	FString FunctionPath;
	if (!TryGetFunctionPath(Params, FunctionPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'function_path' parameter"));
	}

	FString FromNodeName;
	if (!Params->TryGetStringField(TEXT("from_node"), FromNodeName))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'from_node' parameter"));
	}

	FString ToNodeName;
	if (!Params->TryGetStringField(TEXT("to_node"), ToNodeName))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'to_node' parameter"));
	}

	UMaterialFunction* MaterialFunction = LoadMaterialFunctionForEditing(FunctionPath);
	if (!MaterialFunction)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material function not found: %s"), *FunctionPath));
	}

	UMaterialExpression* FromNode = FindMaterialFunctionExpressionByReference(MaterialFunction, FromNodeName);
	if (!FromNode)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Source node not found: %s"), *FromNodeName));
	}

	UMaterialExpression* ToNode = FindMaterialFunctionExpressionByReference(MaterialFunction, ToNodeName);
	if (!ToNode)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Target node not found: %s"), *ToNodeName));
	}

	FString FromOutputName;
	Params->TryGetStringField(TEXT("from_output"), FromOutputName);

	FString ToInputName;
	Params->TryGetStringField(TEXT("to_input"), ToInputName);

	if (!UMaterialEditingLibrary::ConnectMaterialExpressions(FromNode, FromOutputName, ToNode, ToInputName))
	{
		return FUECLICommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to connect '%s' to '%s' in material function"), *FromNodeName, *ToNodeName));
	}

	MaterialFunction->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("function_path"), FunctionPath);
	ResultObj->SetStringField(TEXT("from_node"), FromNode->GetName());
	ResultObj->SetStringField(TEXT("to_node"), ToNode->GetName());
	ResultObj->SetStringField(TEXT("from_output"), FromOutputName);
	ResultObj->SetStringField(TEXT("to_input"), ToInputName);
	ResultObj->SetBoolField(TEXT("success"), true);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleDisconnectMaterialFunctionNodes(const TSharedPtr<FJsonObject>& Params)
{
	FString FunctionPath;
	if (!TryGetFunctionPath(Params, FunctionPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'function_path' parameter"));
	}

	FString ToNodeName;
	if (!Params->TryGetStringField(TEXT("to_node"), ToNodeName))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'to_node' parameter"));
	}

	UMaterialFunction* MaterialFunction = LoadMaterialFunctionForEditing(FunctionPath);
	if (!MaterialFunction)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material function not found: %s"), *FunctionPath));
	}

	UMaterialExpression* ToNode = FindMaterialFunctionExpressionByReference(MaterialFunction, ToNodeName);
	if (!ToNode)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Target node not found: %s"), *ToNodeName));
	}

	FString FromNodeName;
	Params->TryGetStringField(TEXT("from_node"), FromNodeName);
	UMaterialExpression* ExpectedFromNode = FromNodeName.IsEmpty() ? nullptr : FindMaterialFunctionExpressionByReference(MaterialFunction, FromNodeName);
	if (!FromNodeName.IsEmpty() && ExpectedFromNode == nullptr)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Source node not found: %s"), *FromNodeName));
	}

	FString ToInputName;
	Params->TryGetStringField(TEXT("to_input"), ToInputName);

	int32 InputIndex = INDEX_NONE;
	FExpressionInput* Input = nullptr;
	if (!TryResolveExpressionInput(ToNode, ToInputName, InputIndex, Input))
	{
		return FUECLICommonUtils::CreateErrorResponse(
			ToInputName.IsEmpty()
				? FString::Printf(TEXT("Node '%s' has no disconnectable inputs"), *ToNodeName)
				: FString::Printf(TEXT("Input '%s' not found on node '%s'"), *ToInputName, *ToNodeName));
	}

	if (Input->Expression == nullptr)
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Target input is already disconnected"));
	}

	if (ExpectedFromNode != nullptr && Input->Expression != ExpectedFromNode)
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Target input is connected to a different source node"));
	}

	const FString ResolvedInputName = ToNode->GetInputName(InputIndex).ToString();
	const FString ResolvedFromNodeName = Input->Expression->GetName();
	ResetExpressionInput(Input);
	MaterialFunction->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("function_path"), FunctionPath);
	ResultObj->SetStringField(TEXT("from_node"), ResolvedFromNodeName);
	ResultObj->SetStringField(TEXT("to_node"), ToNode->GetName());
	ResultObj->SetStringField(TEXT("to_input"), ResolvedInputName);
	ResultObj->SetBoolField(TEXT("success"), true);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleDeleteMaterialFunctionNode(const TSharedPtr<FJsonObject>& Params)
{
	FString FunctionPath;
	if (!TryGetFunctionPath(Params, FunctionPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'function_path' parameter"));
	}

	FString NodeName;
	if (!Params->TryGetStringField(TEXT("node_name"), NodeName))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'node_name' parameter"));
	}

	UMaterialFunction* MaterialFunction = LoadMaterialFunctionForEditing(FunctionPath);
	if (!MaterialFunction)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material function not found: %s"), *FunctionPath));
	}

	UMaterialExpression* Node = FindMaterialFunctionExpressionByReference(MaterialFunction, NodeName);
	if (!Node)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Node not found: %s"), *NodeName));
	}

	const FString ResolvedName = Node->GetName();
	UMaterialEditingLibrary::DeleteMaterialExpressionInFunction(MaterialFunction, Node);
	MaterialFunction->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("function_path"), FunctionPath);
	ResultObj->SetStringField(TEXT("node_name"), ResolvedName);
	ResultObj->SetBoolField(TEXT("success"), true);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleMoveMaterialFunctionNode(const TSharedPtr<FJsonObject>& Params)
{
	FString FunctionPath;
	if (!TryGetFunctionPath(Params, FunctionPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'function_path' parameter"));
	}

	FString NodeName;
	if (!Params->TryGetStringField(TEXT("node_name"), NodeName))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'node_name' parameter"));
	}

	int32 NodePosX = 0;
	int32 NodePosY = 0;
	if (!Params->TryGetNumberField(TEXT("node_pos_x"), NodePosX) || !Params->TryGetNumberField(TEXT("node_pos_y"), NodePosY))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'node_pos_x' or 'node_pos_y' parameter"));
	}

	UMaterialFunction* MaterialFunction = LoadMaterialFunctionForEditing(FunctionPath);
	if (!MaterialFunction)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material function not found: %s"), *FunctionPath));
	}

	UMaterialExpression* Node = FindMaterialFunctionExpressionByReference(MaterialFunction, NodeName);
	if (!Node)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Node not found: %s"), *NodeName));
	}

	Node->MaterialExpressionEditorX = NodePosX;
	Node->MaterialExpressionEditorY = NodePosY;
	MaterialFunction->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("function_path"), FunctionPath);
	ResultObj->SetStringField(TEXT("node_name"), Node->GetName());
	ResultObj->SetNumberField(TEXT("node_pos_x"), NodePosX);
	ResultObj->SetNumberField(TEXT("node_pos_y"), NodePosY);
	ResultObj->SetBoolField(TEXT("success"), true);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleLayoutMaterialFunctionNodes(const TSharedPtr<FJsonObject>& Params)
{
	FString FunctionPath;
	if (!TryGetFunctionPath(Params, FunctionPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'function_path' parameter"));
	}

	UMaterialFunction* MaterialFunction = LoadMaterialFunctionForEditing(FunctionPath);
	if (!MaterialFunction)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material function not found: %s"), *FunctionPath));
	}

	UMaterialEditingLibrary::LayoutMaterialFunctionExpressions(MaterialFunction);
	MaterialFunction->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("function_path"), FunctionPath);
	ResultObj->SetBoolField(TEXT("success"), true);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleSetMaterialFunctionNodeProperty(const TSharedPtr<FJsonObject>& Params)
{
	FString FunctionPath;
	if (!TryGetFunctionPath(Params, FunctionPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'function_path' parameter"));
	}

	FString NodeName;
	if (!Params->TryGetStringField(TEXT("node_name"), NodeName))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'node_name' parameter"));
	}

	FString PropertyName;
	if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
	}

	const TSharedPtr<FJsonValue>* ValuePtr = Params->Values.Find(TEXT("value"));
	if (ValuePtr == nullptr || !ValuePtr->IsValid())
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'value' parameter"));
	}

	UMaterialFunction* MaterialFunction = LoadMaterialFunctionForEditing(FunctionPath);
	if (!MaterialFunction)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material function not found: %s"), *FunctionPath));
	}

	UMaterialExpression* Node = FindMaterialFunctionExpressionByReference(MaterialFunction, NodeName);
	if (!Node)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Node not found: %s"), *NodeName));
	}

	FString ErrorMessage;
	if (!FUECLICommonUtils::SetObjectProperty(Node, PropertyName, *ValuePtr, ErrorMessage))
	{
		return FUECLICommonUtils::CreateErrorResponse(ErrorMessage);
	}

	Node->PostEditChange();
	MaterialFunction->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("function_path"), FunctionPath);
	ResultObj->SetStringField(TEXT("node_name"), Node->GetName());
	ResultObj->SetStringField(TEXT("property_name"), PropertyName);
	ResultObj->SetBoolField(TEXT("success"), true);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleCompileMaterialFunction(const TSharedPtr<FJsonObject>& Params)
{
	FString FunctionPath;
	if (!TryGetFunctionPath(Params, FunctionPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'function_path' parameter"));
	}

	UMaterialFunction* MaterialFunction = LoadMaterialFunctionForEditing(FunctionPath);
	if (!MaterialFunction)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material function not found: %s"), *FunctionPath));
	}

	MaterialFunction->UpdateFromFunctionResource();
	MaterialFunction->PreEditChange(nullptr);
	MaterialFunction->PostEditChange();
	MaterialFunction->MarkPackageDirty();
	UMaterialEditingLibrary::RebuildMaterialInstanceEditors(MaterialFunction);

	UObject* FunctionObject = MaterialFunction;
	FAssetCompilingManager::Get().FinishCompilationForObjects(MakeArrayView(&FunctionObject, 1));
	if (GShaderCompilingManager != nullptr)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}
	FAssetCompilingManager::Get().ProcessAsyncTasks(true);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("function_path"), FunctionPath);
	ResultObj->SetBoolField(TEXT("success"), true);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleSaveMaterialFunction(const TSharedPtr<FJsonObject>& Params)
{
	FString FunctionPath;
	if (!TryGetFunctionPath(Params, FunctionPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'function_path' parameter"));
	}

	if (!UEditorAssetLibrary::DoesAssetExist(FunctionPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material function not found: %s"), *FunctionPath));
	}

	if (!UEditorAssetLibrary::SaveAsset(FunctionPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to save material function: %s"), *FunctionPath));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("function_path"), FunctionPath);
	ResultObj->SetBoolField(TEXT("success"), true);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleApplyMaterialFunctionPatch(const TSharedPtr<FJsonObject>& Params)
{
	FString FunctionPath;
	if (!TryGetFunctionPath(Params, FunctionPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'function_path' parameter"));
	}

	const TArray<TSharedPtr<FJsonValue>>* Operations = nullptr;
	if (!Params->TryGetArrayField(TEXT("operations"), Operations) || Operations == nullptr)
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'operations' parameter"));
	}

	bool bContinueOnError = false;
	Params->TryGetBoolField(TEXT("continue_on_error"), bContinueOnError);

	TMap<FString, FString> Aliases;
	TArray<TSharedPtr<FJsonValue>> OperationResults;
	int32 AppliedCount = 0;

	auto ResolveAliasInObject = [&Aliases](const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
	{
		FString Value;
		if (Object->TryGetStringField(FieldName, Value))
		{
			if (const FString* AliasValue = Aliases.Find(Value))
			{
				Object->SetStringField(FieldName, *AliasValue);
			}
		}
	};

	for (int32 OperationIndex = 0; OperationIndex < Operations->Num(); ++OperationIndex)
	{
		const TSharedPtr<FJsonObject> Operation = (*Operations)[OperationIndex]->AsObject();
		if (!Operation.IsValid())
		{
			return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Operation %d is not an object"), OperationIndex));
		}

		FString Action;
		if (!Operation->TryGetStringField(TEXT("action"), Action))
		{
			return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Operation %d is missing 'action'"), OperationIndex));
		}

		TSharedPtr<FJsonObject> OpParams = MakeShared<FJsonObject>();
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Operation->Values)
		{
			if (Pair.Key != TEXT("action"))
			{
				OpParams->SetField(Pair.Key, Pair.Value);
			}
		}
		OpParams->SetStringField(TEXT("function_path"), FunctionPath);

		ResolveAliasInObject(OpParams, TEXT("node_name"));
		ResolveAliasInObject(OpParams, TEXT("from_node"));
		ResolveAliasInObject(OpParams, TEXT("to_node"));

		TSharedPtr<FJsonObject> OpResult;
		if (Action == TEXT("create_node"))
		{
			OpResult = HandleCreateMaterialFunctionNode(OpParams);
			if (OpResult.IsValid() && (!OpResult->HasField(TEXT("success")) || OpResult->GetBoolField(TEXT("success"))))
			{
				FString Alias;
				if (OpParams->TryGetStringField(TEXT("alias"), Alias) && !Alias.IsEmpty())
				{
					Aliases.Add(Alias, OpResult->GetStringField(TEXT("node_name")));
				}
			}
		}
		else if (Action == TEXT("connect_nodes"))
		{
			OpResult = HandleConnectMaterialFunctionNodes(OpParams);
		}
		else if (Action == TEXT("create_scaffold"))
		{
			OpResult = HandleCreateMaterialFunctionScaffold(OpParams);
		}
		else if (Action == TEXT("disconnect_nodes"))
		{
			OpResult = HandleDisconnectMaterialFunctionNodes(OpParams);
		}
		else if (Action == TEXT("delete_node"))
		{
			OpResult = HandleDeleteMaterialFunctionNode(OpParams);
		}
		else if (Action == TEXT("move_node"))
		{
			OpResult = HandleMoveMaterialFunctionNode(OpParams);
		}
		else if (Action == TEXT("layout_nodes"))
		{
			OpResult = HandleLayoutMaterialFunctionNodes(OpParams);
		}
		else if (Action == TEXT("set_node_property"))
		{
			OpResult = HandleSetMaterialFunctionNodeProperty(OpParams);
		}
		else if (Action == TEXT("compile_function"))
		{
			OpResult = HandleCompileMaterialFunction(OpParams);
		}
		else if (Action == TEXT("save_function"))
		{
			OpResult = HandleSaveMaterialFunction(OpParams);
		}
		else
		{
			OpResult = FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unsupported function patch action: %s"), *Action));
		}

		const bool bSuccess = OpResult.IsValid() && (!OpResult->HasField(TEXT("success")) || OpResult->GetBoolField(TEXT("success")));
		TSharedPtr<FJsonObject> StepResult = MakeShared<FJsonObject>();
		StepResult->SetNumberField(TEXT("operation_index"), OperationIndex);
		StepResult->SetStringField(TEXT("action"), Action);
		StepResult->SetBoolField(TEXT("success"), bSuccess);
		if (OpResult.IsValid())
		{
			StepResult->SetObjectField(TEXT("result"), OpResult);
			if (!bSuccess)
			{
				FString ErrorMessage;
				if (OpResult->TryGetStringField(TEXT("error"), ErrorMessage))
				{
					StepResult->SetStringField(TEXT("error"), ErrorMessage);
				}
			}
		}
		OperationResults.Add(MakeShared<FJsonValueObject>(StepResult));

		if (bSuccess)
		{
			++AppliedCount;
		}
		else if (!bContinueOnError)
		{
			TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
			ErrorObj->SetBoolField(TEXT("success"), false);
			ErrorObj->SetStringField(TEXT("error"), FString::Printf(TEXT("Function patch failed at operation %d"), OperationIndex));
			ErrorObj->SetStringField(TEXT("function_path"), FunctionPath);
			ErrorObj->SetArrayField(TEXT("results"), OperationResults);
			ErrorObj->SetNumberField(TEXT("applied_count"), AppliedCount);
			ErrorObj->SetNumberField(TEXT("operation_count"), Operations->Num());

			TSharedPtr<FJsonObject> AliasObject = MakeShared<FJsonObject>();
			for (const TPair<FString, FString>& Pair : Aliases)
			{
				AliasObject->SetStringField(Pair.Key, Pair.Value);
			}
			ErrorObj->SetObjectField(TEXT("aliases"), AliasObject);
			return ErrorObj;
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("function_path"), FunctionPath);
	ResultObj->SetArrayField(TEXT("results"), OperationResults);
	ResultObj->SetNumberField(TEXT("applied_count"), AppliedCount);
	ResultObj->SetNumberField(TEXT("operation_count"), Operations->Num());

	TSharedPtr<FJsonObject> AliasObject = MakeShared<FJsonObject>();
	for (const TPair<FString, FString>& Pair : Aliases)
	{
		AliasObject->SetStringField(Pair.Key, Pair.Value);
	}
	ResultObj->SetObjectField(TEXT("aliases"), AliasObject);
	ResultObj->SetBoolField(TEXT("success"), true);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleApplyMaterialGraphPatch(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!TryGetMaterialPath(Params, MaterialPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
	}

	const TArray<TSharedPtr<FJsonValue>>* Operations = nullptr;
	if (!Params->TryGetArrayField(TEXT("operations"), Operations) || Operations == nullptr)
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'operations' parameter"));
	}

	bool bContinueOnError = false;
	Params->TryGetBoolField(TEXT("continue_on_error"), bContinueOnError);

	TMap<FString, FString> Aliases;
	TArray<TSharedPtr<FJsonValue>> OperationResults;
	int32 AppliedCount = 0;

	auto ResolveAliasInObject = [&Aliases](const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
	{
		FString Value;
		if (Object->TryGetStringField(FieldName, Value))
		{
			if (const FString* AliasValue = Aliases.Find(Value))
			{
				Object->SetStringField(FieldName, *AliasValue);
			}
		}
	};

	for (int32 OperationIndex = 0; OperationIndex < Operations->Num(); ++OperationIndex)
	{
		const TSharedPtr<FJsonObject> Operation = (*Operations)[OperationIndex]->AsObject();
		if (!Operation.IsValid())
		{
			return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Operation %d is not an object"), OperationIndex));
		}

		FString Action;
		if (!Operation->TryGetStringField(TEXT("action"), Action))
		{
			return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Operation %d is missing 'action'"), OperationIndex));
		}

		TSharedPtr<FJsonObject> OpParams = MakeShared<FJsonObject>();
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Operation->Values)
		{
			if (Pair.Key != TEXT("action"))
			{
				OpParams->SetField(Pair.Key, Pair.Value);
			}
		}
		OpParams->SetStringField(TEXT("material_path"), MaterialPath);

		ResolveAliasInObject(OpParams, TEXT("node_name"));
		ResolveAliasInObject(OpParams, TEXT("from_node"));
		ResolveAliasInObject(OpParams, TEXT("to_node"));

		TSharedPtr<FJsonObject> OpResult;
		if (Action == TEXT("create_node"))
		{
			OpResult = HandleCreateMaterialNode(OpParams);
			if (OpResult.IsValid() && (!OpResult->HasField(TEXT("success")) || OpResult->GetBoolField(TEXT("success"))))
			{
				FString Alias;
				if (OpParams->TryGetStringField(TEXT("alias"), Alias) && !Alias.IsEmpty())
				{
					Aliases.Add(Alias, OpResult->GetStringField(TEXT("node_name")));
				}
			}
		}
		else if (Action == TEXT("connect_nodes"))
		{
			OpResult = HandleConnectMaterialNodes(OpParams);
		}
		else if (Action == TEXT("disconnect_nodes"))
		{
			OpResult = HandleDisconnectMaterialNodes(OpParams);
		}
		else if (Action == TEXT("set_output"))
		{
			OpResult = HandleSetMaterialOutput(OpParams);
		}
		else if (Action == TEXT("clear_output"))
		{
			OpResult = HandleClearMaterialOutput(OpParams);
		}
		else if (Action == TEXT("delete_node"))
		{
			OpResult = HandleDeleteMaterialNode(OpParams);
		}
		else if (Action == TEXT("move_node"))
		{
			OpResult = HandleMoveMaterialNode(OpParams);
		}
		else if (Action == TEXT("set_node_property"))
		{
			OpResult = HandleSetMaterialNodeProperty(OpParams);
		}
		else if (Action == TEXT("set_material_asset_properties"))
		{
			OpResult = HandleSetMaterialAssetProperties(OpParams);
		}
		else if (Action == TEXT("create_scaffold"))
		{
			OpResult = HandleCreateMaterialScaffold(OpParams);
		}
		else if (Action == TEXT("layout_nodes"))
		{
			OpResult = HandleLayoutMaterialNodes(OpParams);
		}
		else if (Action == TEXT("compile_material"))
		{
			OpResult = HandleCompileMaterial(OpParams);
		}
		else if (Action == TEXT("save_material"))
		{
			OpResult = HandleSaveMaterial(OpParams);
		}
		else
		{
			OpResult = FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unsupported patch action: %s"), *Action));
		}

		const bool bSuccess = OpResult.IsValid() && (!OpResult->HasField(TEXT("success")) || OpResult->GetBoolField(TEXT("success")));
		TSharedPtr<FJsonObject> StepResult = MakeShared<FJsonObject>();
		StepResult->SetNumberField(TEXT("operation_index"), OperationIndex);
		StepResult->SetStringField(TEXT("action"), Action);
		StepResult->SetBoolField(TEXT("success"), bSuccess);
		if (OpResult.IsValid())
		{
			StepResult->SetObjectField(TEXT("result"), OpResult);
			if (!bSuccess)
			{
				FString ErrorMessage;
				if (OpResult->TryGetStringField(TEXT("error"), ErrorMessage))
				{
					StepResult->SetStringField(TEXT("error"), ErrorMessage);
				}
			}
		}
		OperationResults.Add(MakeShared<FJsonValueObject>(StepResult));

		if (bSuccess)
		{
			++AppliedCount;
		}
		else if (!bContinueOnError)
		{
			TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
			ErrorObj->SetBoolField(TEXT("success"), false);
			ErrorObj->SetStringField(TEXT("error"), FString::Printf(TEXT("Patch failed at operation %d"), OperationIndex));
			ErrorObj->SetStringField(TEXT("material_path"), MaterialPath);
			ErrorObj->SetArrayField(TEXT("results"), OperationResults);
			ErrorObj->SetNumberField(TEXT("applied_count"), AppliedCount);
			ErrorObj->SetNumberField(TEXT("operation_count"), Operations->Num());

			TSharedPtr<FJsonObject> AliasObject = MakeShared<FJsonObject>();
			for (const TPair<FString, FString>& Pair : Aliases)
			{
				AliasObject->SetStringField(Pair.Key, Pair.Value);
			}
			ErrorObj->SetObjectField(TEXT("aliases"), AliasObject);
			return ErrorObj;
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
	ResultObj->SetArrayField(TEXT("results"), OperationResults);
	ResultObj->SetNumberField(TEXT("applied_count"), AppliedCount);
	ResultObj->SetNumberField(TEXT("operation_count"), Operations->Num());

	TSharedPtr<FJsonObject> AliasObject = MakeShared<FJsonObject>();
	for (const TPair<FString, FString>& Pair : Aliases)
	{
		AliasObject->SetStringField(Pair.Key, Pair.Value);
	}
	ResultObj->SetObjectField(TEXT("aliases"), AliasObject);
	ResultObj->SetBoolField(TEXT("success"), true);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleAnalyzeMaterial(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath;
    
    // Try to get path from params, or use currently opened material
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
        if (AssetEditorSubsystem)
        {
            TArray<UObject*> EditedAssets = AssetEditorSubsystem->GetAllEditedAssets();
            for (UObject* Asset : EditedAssets)
            {
                if (Cast<UMaterial>(Asset))
                {
                    MaterialPath = Asset->GetPathName();
                    break;
                }
            }
        }
    }

    if (MaterialPath.IsEmpty())
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("No material path provided and no material is currently opened"));
    }

    UMaterial* Material = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
    if (!Material)
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
    ResultObj->SetStringField(TEXT("material_name"), Material->GetName());

    // Basic material properties
    FString DomainStr;
    switch (Material->MaterialDomain)
    {
        case MD_Surface: DomainStr = TEXT("Surface"); break;
        case MD_DeferredDecal: DomainStr = TEXT("DeferredDecal"); break;
        case MD_LightFunction: DomainStr = TEXT("LightFunction"); break;
        case MD_Volume: DomainStr = TEXT("Volume"); break;
        case MD_PostProcess: DomainStr = TEXT("PostProcess"); break;
        case MD_UI: DomainStr = TEXT("UI"); break;
        default: DomainStr = TEXT("Unknown"); break;
    }
    ResultObj->SetStringField(TEXT("domain"), DomainStr);

    FString BlendModeStr;
    switch (Material->BlendMode)
    {
        case BLEND_Opaque: BlendModeStr = TEXT("Opaque"); break;
        case BLEND_Masked: BlendModeStr = TEXT("Masked"); break;
        case BLEND_Translucent: BlendModeStr = TEXT("Translucent"); break;
        case BLEND_Additive: BlendModeStr = TEXT("Additive"); break;
        case BLEND_Modulate: BlendModeStr = TEXT("Modulate"); break;
        default: BlendModeStr = TEXT("Unknown"); break;
    }
    ResultObj->SetStringField(TEXT("blend_mode"), BlendModeStr);

    FString ShadingModelStr;
    EMaterialShadingModel ShadingModel = Material->GetShadingModels().GetFirstShadingModel();
    switch (ShadingModel)
    {
        case MSM_Unlit: ShadingModelStr = TEXT("Unlit"); break;
        case MSM_DefaultLit: ShadingModelStr = TEXT("DefaultLit"); break;
        case MSM_Subsurface: ShadingModelStr = TEXT("Subsurface"); break;
        case MSM_PreintegratedSkin: ShadingModelStr = TEXT("PreintegratedSkin"); break;
        case MSM_ClearCoat: ShadingModelStr = TEXT("ClearCoat"); break;
        case MSM_SubsurfaceProfile: ShadingModelStr = TEXT("SubsurfaceProfile"); break;
        case MSM_TwoSidedFoliage: ShadingModelStr = TEXT("TwoSidedFoliage"); break;
        case MSM_Hair: ShadingModelStr = TEXT("Hair"); break;
        case MSM_Cloth: ShadingModelStr = TEXT("Cloth"); break;
        case MSM_Eye: ShadingModelStr = TEXT("Eye"); break;
        default: ShadingModelStr = TEXT("DefaultLit"); break;
    }
    ResultObj->SetStringField(TEXT("shading_model"), ShadingModelStr);
    ResultObj->SetBoolField(TEXT("two_sided"), Material->IsTwoSided());

    // Node statistics
    auto Expressions = Material->GetExpressions();
    ResultObj->SetNumberField(TEXT("total_nodes"), Expressions.Num());

    // Count node types
    TMap<FString, int32> NodeTypeCount;
    int32 TextureSampleCount = 0;
    int32 ParameterCount = 0;
    int32 TextureParameterCount = 0;
    TArray<TSharedPtr<FJsonValue>> ScalarParameterNames;
    TArray<TSharedPtr<FJsonValue>> VectorParameterNames;
    TArray<TSharedPtr<FJsonValue>> TextureParameterNames;
    TArray<TSharedPtr<FJsonValue>> FunctionPaths;
    TSet<FString> UniqueFunctionPaths;

    for (int32 i = 0; i < Expressions.Num(); i++)
    {
        UMaterialExpression* Expr = Expressions[i].Get();
        if (!Expr) continue;
        
        FString TypeName = Expr->GetClass()->GetName();
        TypeName.RemoveFromStart(TEXT("MaterialExpression"));
        NodeTypeCount.FindOrAdd(TypeName)++;

        if (Cast<UMaterialExpressionTextureSample>(Expr))
        {
            TextureSampleCount++;
        }
        if (UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expr))
        {
            ParameterCount++;
            ScalarParameterNames.Add(MakeShared<FJsonValueString>(ScalarParam->ParameterName.ToString()));
        }
        else if (UMaterialExpressionVectorParameter* VectorParam = Cast<UMaterialExpressionVectorParameter>(Expr))
        {
            ParameterCount++;
            VectorParameterNames.Add(MakeShared<FJsonValueString>(VectorParam->ParameterName.ToString()));
        }
        else if (UMaterialExpressionTextureSampleParameter2D* TextureParam = Cast<UMaterialExpressionTextureSampleParameter2D>(Expr))
        {
            TextureParameterCount++;
            TextureParameterNames.Add(MakeShared<FJsonValueString>(TextureParam->ParameterName.ToString()));
        }

        if (UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expr))
        {
            const FString FunctionPath = GetAssetPackagePath(FunctionCall->MaterialFunction.Get());
            if (!FunctionPath.IsEmpty() && !UniqueFunctionPaths.Contains(FunctionPath))
            {
                UniqueFunctionPaths.Add(FunctionPath);
                FunctionPaths.Add(MakeShared<FJsonValueString>(FunctionPath));
            }
        }
    }

    ResultObj->SetNumberField(TEXT("texture_sample_count"), TextureSampleCount);
    ResultObj->SetNumberField(TEXT("parameter_count"), ParameterCount);
    ResultObj->SetNumberField(TEXT("texture_parameter_count"), TextureParameterCount);
    ResultObj->SetNumberField(TEXT("function_call_count"), FunctionPaths.Num());
    ResultObj->SetArrayField(TEXT("scalar_parameter_names"), ScalarParameterNames);
    ResultObj->SetArrayField(TEXT("vector_parameter_names"), VectorParameterNames);
    ResultObj->SetArrayField(TEXT("texture_parameter_names"), TextureParameterNames);
    ResultObj->SetArrayField(TEXT("function_paths"), FunctionPaths);

    // Node type breakdown
    TSharedPtr<FJsonObject> TypeBreakdown = MakeShared<FJsonObject>();
    for (const auto& Pair : NodeTypeCount)
    {
        TypeBreakdown->SetNumberField(Pair.Key, Pair.Value);
    }
    ResultObj->SetObjectField(TEXT("node_type_breakdown"), TypeBreakdown);

    // Active material outputs and warnings
    TArray<TSharedPtr<FJsonValue>> ActiveOutputs;
    TArray<TSharedPtr<FJsonValue>> Warnings;
    
    if (TextureSampleCount > 16)
    {
        Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("High texture sample count (%d) may impact performance"), TextureSampleCount)));
    }
    
    // Helper to check if a property is connected
    auto IsPropertyConnected = [Material](EMaterialProperty Property) -> bool
    {
        const FExpressionInput* Input = Material->GetExpressionInputForProperty(Property);
        return Input && Input->Expression != nullptr;
    };
    
    if (IsPropertyConnected(MP_BaseColor)) ActiveOutputs.Add(MakeShared<FJsonValueString>(TEXT("BaseColor")));
    if (IsPropertyConnected(MP_Metallic)) ActiveOutputs.Add(MakeShared<FJsonValueString>(TEXT("Metallic")));
    if (IsPropertyConnected(MP_Specular)) ActiveOutputs.Add(MakeShared<FJsonValueString>(TEXT("Specular")));
    if (IsPropertyConnected(MP_Roughness)) ActiveOutputs.Add(MakeShared<FJsonValueString>(TEXT("Roughness")));
    if (IsPropertyConnected(MP_EmissiveColor)) ActiveOutputs.Add(MakeShared<FJsonValueString>(TEXT("EmissiveColor")));
    if (IsPropertyConnected(MP_Opacity)) ActiveOutputs.Add(MakeShared<FJsonValueString>(TEXT("Opacity")));
    if (IsPropertyConnected(MP_OpacityMask)) ActiveOutputs.Add(MakeShared<FJsonValueString>(TEXT("OpacityMask")));
    if (IsPropertyConnected(MP_Normal)) ActiveOutputs.Add(MakeShared<FJsonValueString>(TEXT("Normal")));
    if (IsPropertyConnected(MP_WorldPositionOffset)) ActiveOutputs.Add(MakeShared<FJsonValueString>(TEXT("WorldPositionOffset")));
    if (IsPropertyConnected(MP_AmbientOcclusion)) ActiveOutputs.Add(MakeShared<FJsonValueString>(TEXT("AmbientOcclusion")));
    if (IsPropertyConnected(MP_Refraction)) ActiveOutputs.Add(MakeShared<FJsonValueString>(TEXT("Refraction")));
    
    // Check for common issues
    if (Material->BlendMode == BLEND_Masked && !IsPropertyConnected(MP_OpacityMask))
    {
        Warnings.Add(MakeShared<FJsonValueString>(TEXT("Masked blend mode but OpacityMask is not connected")));
    }
    if (Material->BlendMode == BLEND_Translucent && !IsPropertyConnected(MP_Opacity))
    {
        Warnings.Add(MakeShared<FJsonValueString>(TEXT("Translucent blend mode but Opacity is not connected")));
    }
    if (Material->GetShadingModels().HasShadingModel(MSM_TwoSidedFoliage) && !IsPropertyConnected(MP_SubsurfaceColor))
    {
        Warnings.Add(MakeShared<FJsonValueString>(TEXT("TwoSidedFoliage shading model but SubsurfaceColor is not connected")));
    }
    
    ResultObj->SetArrayField(TEXT("active_outputs"), ActiveOutputs);
    ResultObj->SetArrayField(TEXT("warnings"), Warnings);

    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleGetMaterialFunctionInfo(const TSharedPtr<FJsonObject>& Params)
{
    FString FunctionPath;
    if (!Params->TryGetStringField(TEXT("function_path"), FunctionPath))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'function_path' parameter"));
    }

    UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(UEditorAssetLibrary::LoadAsset(FunctionPath));
    if (!MaterialFunction)
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("MaterialFunction not found: %s"), *FunctionPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("function_path"), FunctionPath);
    ResultObj->SetStringField(TEXT("function_name"), MaterialFunction->GetName());
    ResultObj->SetStringField(TEXT("description"), MaterialFunction->GetDescription());

    // Get function expressions
    auto FunctionExpressions = MaterialFunction->GetExpressions();
    ResultObj->SetNumberField(TEXT("expression_count"), FunctionExpressions.Num());

    // Collect inputs and outputs
    TArray<TSharedPtr<FJsonValue>> InputsArray;
    TArray<TSharedPtr<FJsonValue>> OutputsArray;
    TArray<TSharedPtr<FJsonValue>> NodesArray;

    for (int32 i = 0; i < FunctionExpressions.Num(); i++)
    {
        UMaterialExpression* Expr = FunctionExpressions[i].Get();
        if (!Expr) continue;

        // Check for function inputs
        if (UMaterialExpressionFunctionInput* FuncInput = Cast<UMaterialExpressionFunctionInput>(Expr))
        {
            TSharedPtr<FJsonObject> InputObj = MakeShared<FJsonObject>();
            InputObj->SetStringField(TEXT("name"), FuncInput->InputName.ToString());
            InputObj->SetStringField(TEXT("description"), FuncInput->Description);
            InputObj->SetNumberField(TEXT("sort_priority"), FuncInput->SortPriority);
            InputObj->SetBoolField(TEXT("use_preview_value"), FuncInput->bUsePreviewValueAsDefault);
            
            // Get input type
            FString InputTypeStr;
            switch (FuncInput->InputType)
            {
                case FunctionInput_Scalar: InputTypeStr = TEXT("Scalar"); break;
                case FunctionInput_Vector2: InputTypeStr = TEXT("Vector2"); break;
                case FunctionInput_Vector3: InputTypeStr = TEXT("Vector3"); break;
                case FunctionInput_Vector4: InputTypeStr = TEXT("Vector4"); break;
                case FunctionInput_Texture2D: InputTypeStr = TEXT("Texture2D"); break;
                case FunctionInput_TextureCube: InputTypeStr = TEXT("TextureCube"); break;
                case FunctionInput_MaterialAttributes: InputTypeStr = TEXT("MaterialAttributes"); break;
                default: InputTypeStr = TEXT("Unknown"); break;
            }
            InputObj->SetStringField(TEXT("type"), InputTypeStr);
            
            InputsArray.Add(MakeShared<FJsonValueObject>(InputObj));
        }
        // Check for function outputs
        else if (UMaterialExpressionFunctionOutput* FuncOutput = Cast<UMaterialExpressionFunctionOutput>(Expr))
        {
            TSharedPtr<FJsonObject> OutputObj = MakeShared<FJsonObject>();
            OutputObj->SetStringField(TEXT("name"), FuncOutput->OutputName.ToString());
            OutputObj->SetStringField(TEXT("description"), FuncOutput->Description);
            OutputObj->SetNumberField(TEXT("sort_priority"), FuncOutput->SortPriority);
            
            OutputsArray.Add(MakeShared<FJsonValueObject>(OutputObj));
        }

        // Add all nodes
        TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
        NodeObj->SetNumberField(TEXT("index"), i);
        NodeObj->SetStringField(TEXT("name"), Expr->GetName());
        
        FString NodeType = Expr->GetClass()->GetName();
        NodeType.RemoveFromStart(TEXT("MaterialExpression"));
        NodeObj->SetStringField(TEXT("type"), NodeType);
        NodeObj->SetStringField(TEXT("description"), Expr->Desc);
        NodeObj->SetNumberField(TEXT("pos_x"), Expr->MaterialExpressionEditorX);
        NodeObj->SetNumberField(TEXT("pos_y"), Expr->MaterialExpressionEditorY);

        // Handle nested MaterialFunctionCalls
        if (UMaterialExpressionMaterialFunctionCall* NestedFunc = Cast<UMaterialExpressionMaterialFunctionCall>(Expr))
        {
            if (NestedFunc->MaterialFunction)
            {
                NodeObj->SetStringField(TEXT("nested_function_path"), GetAssetPackagePath(NestedFunc->MaterialFunction.Get()));
                NodeObj->SetStringField(TEXT("nested_function_name"), NestedFunc->MaterialFunction->GetName());
            }
        }

        NodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
    }

    // Build expression to index map for connections
    TMap<UMaterialExpression*, int32> ExprToIndex;
    for (int32 i = 0; i < FunctionExpressions.Num(); i++)
    {
        if (FunctionExpressions[i].Get())
        {
            ExprToIndex.Add(FunctionExpressions[i].Get(), i);
        }
    }

    // Collect connections between nodes
    TArray<TSharedPtr<FJsonValue>> ConnectionsArray;
    for (int32 TargetIndex = 0; TargetIndex < FunctionExpressions.Num(); TargetIndex++)
    {
        UMaterialExpression* TargetExpr = FunctionExpressions[TargetIndex].Get();
        if (!TargetExpr) continue;

        // Iterate through all inputs of this node
        for (int32 InputIndex = 0; ; InputIndex++)
        {
            FExpressionInput* Input = TargetExpr->GetInput(InputIndex);
            if (!Input) break;

            if (Input->Expression)
            {
                int32* SourceIndexPtr = ExprToIndex.Find(Input->Expression);
                if (SourceIndexPtr)
                {
                    TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
                    ConnObj->SetNumberField(TEXT("source_node"), *SourceIndexPtr);
                    ConnObj->SetStringField(TEXT("source_node_name"), Input->Expression->GetName());
                    ConnObj->SetNumberField(TEXT("source_output_index"), Input->OutputIndex);
                    ConnObj->SetNumberField(TEXT("target_node"), TargetIndex);
                    ConnObj->SetStringField(TEXT("target_node_name"), TargetExpr->GetName());
                    ConnObj->SetNumberField(TEXT("target_input_index"), InputIndex);
                    
                    FName InputName = TargetExpr->GetInputName(InputIndex);
                    if (!InputName.IsNone())
                    {
                        ConnObj->SetStringField(TEXT("target_input_name"), InputName.ToString());
                    }
                    
                    ConnectionsArray.Add(MakeShared<FJsonValueObject>(ConnObj));
                }
            }
        }
    }

    // Sort inputs and outputs by priority
    ResultObj->SetArrayField(TEXT("inputs"), InputsArray);
    ResultObj->SetArrayField(TEXT("outputs"), OutputsArray);
    ResultObj->SetArrayField(TEXT("internal_nodes"), NodesArray);
    ResultObj->SetArrayField(TEXT("connections"), ConnectionsArray);
    ResultObj->SetNumberField(TEXT("connection_count"), ConnectionsArray.Num());

    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleGetMaterialFlowGraph(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath;
    
    // Try to get path from params, or use currently opened material
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
        if (AssetEditorSubsystem)
        {
            TArray<UObject*> EditedAssets = AssetEditorSubsystem->GetAllEditedAssets();
            for (UObject* Asset : EditedAssets)
            {
                if (Cast<UMaterial>(Asset))
                {
                    MaterialPath = Asset->GetPathName();
                    break;
                }
            }
        }
    }

    if (MaterialPath.IsEmpty())
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("No material path provided and no material is currently opened"));
    }

    UMaterial* Material = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
    if (!Material)
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
    ResultObj->SetStringField(TEXT("material_name"), Material->GetName());

    auto Expressions = Material->GetExpressions();

    // Categorize nodes
    TArray<TSharedPtr<FJsonValue>> InputNodes;      // Parameters, textures, constants
    TArray<TSharedPtr<FJsonValue>> ProcessingNodes; // Operations, functions
    TArray<TSharedPtr<FJsonValue>> OutputConnections;

    // Build node map for quick lookup
    TMap<UMaterialExpression*, int32> ExpressionToIndex;
    for (int32 i = 0; i < Expressions.Num(); i++)
    {
        if (Expressions[i].Get())
        {
            ExpressionToIndex.Add(Expressions[i].Get(), i);
        }
    }

    // Helper to create node info
    auto CreateNodeInfo = [&ExpressionToIndex](UMaterialExpression* Expr, int32 Index) -> TSharedPtr<FJsonObject>
    {
        TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
        NodeObj->SetNumberField(TEXT("index"), Index);
        NodeObj->SetStringField(TEXT("name"), Expr->GetName());
        
        FString NodeType = Expr->GetClass()->GetName();
        NodeType.RemoveFromStart(TEXT("MaterialExpression"));
        NodeObj->SetStringField(TEXT("type"), NodeType);
        
        // Get input connections for this node
        TArray<TSharedPtr<FJsonValue>> InputConnections;
        for (int32 InputIdx = 0; ; InputIdx++)
        {
            FExpressionInput* Input = Expr->GetInput(InputIdx);
            if (!Input) break;
            
            if (Input->Expression)
            {
                TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
                int32* SourceIdx = ExpressionToIndex.Find(Input->Expression);
                ConnObj->SetNumberField(TEXT("from_node"), SourceIdx ? *SourceIdx : -1);
                ConnObj->SetNumberField(TEXT("from_output"), Input->OutputIndex);
                ConnObj->SetNumberField(TEXT("to_input"), InputIdx);
                
                FName InputName = Expr->GetInputName(InputIdx);
                if (!InputName.IsNone())
                {
                    ConnObj->SetStringField(TEXT("input_name"), InputName.ToString());
                }
                
                InputConnections.Add(MakeShared<FJsonValueObject>(ConnObj));
            }
        }
        NodeObj->SetArrayField(TEXT("input_connections"), InputConnections);
        
        return NodeObj;
    };

    // Categorize each expression
    for (int32 i = 0; i < Expressions.Num(); i++)
    {
        UMaterialExpression* Expr = Expressions[i].Get();
        if (!Expr) continue;

        TSharedPtr<FJsonObject> NodeObj = CreateNodeInfo(Expr, i);

        // Determine category
        bool bIsInput = false;
        
        // Check if it's an input node (no incoming connections, provides data)
        if (Cast<UMaterialExpressionScalarParameter>(Expr) ||
            Cast<UMaterialExpressionVectorParameter>(Expr) ||
            Cast<UMaterialExpressionTextureSampleParameter2D>(Expr) ||
            Cast<UMaterialExpressionTextureObjectParameter>(Expr) ||
            Cast<UMaterialExpressionConstant>(Expr) ||
            Cast<UMaterialExpressionConstant2Vector>(Expr) ||
            Cast<UMaterialExpressionConstant3Vector>(Expr) ||
            Cast<UMaterialExpressionConstant4Vector>(Expr) ||
            Cast<UMaterialExpressionTextureSample>(Expr) ||
            Cast<UMaterialExpressionTextureCoordinate>(Expr) ||
            Cast<UMaterialExpressionTime>(Expr) ||
            Cast<UMaterialExpressionWorldPosition>(Expr) ||
            Cast<UMaterialExpressionCameraPositionWS>(Expr) ||
            Cast<UMaterialExpressionVertexNormalWS>(Expr) ||
            Cast<UMaterialExpressionPixelNormalWS>(Expr))
        {
            bIsInput = true;
            
            // Add specific info for input types
            if (UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expr))
            {
                NodeObj->SetStringField(TEXT("parameter_name"), ScalarParam->ParameterName.ToString());
                NodeObj->SetNumberField(TEXT("value"), ScalarParam->DefaultValue);
                NodeObj->SetStringField(TEXT("category"), TEXT("ScalarParameter"));
            }
            else if (UMaterialExpressionVectorParameter* VectorParam = Cast<UMaterialExpressionVectorParameter>(Expr))
            {
                NodeObj->SetStringField(TEXT("parameter_name"), VectorParam->ParameterName.ToString());
                NodeObj->SetStringField(TEXT("category"), TEXT("VectorParameter"));
            }
            else if (UMaterialExpressionTextureSampleParameter2D* TexParam = Cast<UMaterialExpressionTextureSampleParameter2D>(Expr))
            {
                NodeObj->SetStringField(TEXT("parameter_name"), TexParam->ParameterName.ToString());
                if (TexParam->Texture)
                {
                    NodeObj->SetStringField(TEXT("texture_name"), TexParam->Texture->GetName());
                }
                NodeObj->SetStringField(TEXT("category"), TEXT("TextureParameter"));
            }
            else if (UMaterialExpressionTextureSample* TexSample = Cast<UMaterialExpressionTextureSample>(Expr))
            {
                if (TexSample->Texture)
                {
                    NodeObj->SetStringField(TEXT("texture_name"), TexSample->Texture->GetName());
                }
                NodeObj->SetStringField(TEXT("category"), TEXT("TextureSample"));
            }
            else if (UMaterialExpressionConstant* Const = Cast<UMaterialExpressionConstant>(Expr))
            {
                NodeObj->SetNumberField(TEXT("value"), Const->R);
                NodeObj->SetStringField(TEXT("category"), TEXT("Constant"));
            }
            else if (UMaterialExpressionConstant3Vector* Const3 = Cast<UMaterialExpressionConstant3Vector>(Expr))
            {
                NodeObj->SetStringField(TEXT("category"), TEXT("ColorConstant"));
            }
            else
            {
                NodeObj->SetStringField(TEXT("category"), TEXT("Input"));
            }
            
            InputNodes.Add(MakeShared<FJsonValueObject>(NodeObj));
        }
        else
        {
            // It's a processing node
            if (UMaterialExpressionMaterialFunctionCall* FuncCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expr))
            {
                NodeObj->SetStringField(TEXT("category"), TEXT("MaterialFunction"));
                if (FuncCall->MaterialFunction)
                {
                    NodeObj->SetStringField(TEXT("function_name"), FuncCall->MaterialFunction->GetName());
                    NodeObj->SetStringField(TEXT("function_path"), GetAssetPackagePath(FuncCall->MaterialFunction.Get()));
                    NodeObj->SetStringField(TEXT("function_description"), FuncCall->MaterialFunction->GetDescription());
                }
            }
            else if (Cast<UMaterialExpressionAdd>(Expr))
            {
                NodeObj->SetStringField(TEXT("category"), TEXT("Math"));
                NodeObj->SetStringField(TEXT("operation"), TEXT("Add"));
            }
            else if (Cast<UMaterialExpressionMultiply>(Expr))
            {
                NodeObj->SetStringField(TEXT("category"), TEXT("Math"));
                NodeObj->SetStringField(TEXT("operation"), TEXT("Multiply"));
            }
            else if (Cast<UMaterialExpressionLinearInterpolate>(Expr))
            {
                NodeObj->SetStringField(TEXT("category"), TEXT("Math"));
                NodeObj->SetStringField(TEXT("operation"), TEXT("Lerp"));
            }
            else if (Cast<UMaterialExpressionBreakMaterialAttributes>(Expr))
            {
                NodeObj->SetStringField(TEXT("category"), TEXT("MaterialAttributes"));
                NodeObj->SetStringField(TEXT("operation"), TEXT("Break"));
            }
            else if (Cast<UMaterialExpressionMakeMaterialAttributes>(Expr))
            {
                NodeObj->SetStringField(TEXT("category"), TEXT("MaterialAttributes"));
                NodeObj->SetStringField(TEXT("operation"), TEXT("Make"));
            }
            else
            {
                NodeObj->SetStringField(TEXT("category"), TEXT("Processing"));
            }
            
            ProcessingNodes.Add(MakeShared<FJsonValueObject>(NodeObj));
        }
    }

    // Get material output connections
    auto AddOutputConnection = [&](EMaterialProperty Property, const FString& OutputName)
    {
        const FExpressionInput* Input = Material->GetExpressionInputForProperty(Property);
        if (Input && Input->Expression)
        {
            TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
            int32* SourceIdx = ExpressionToIndex.Find(Input->Expression);
            ConnObj->SetNumberField(TEXT("from_node"), SourceIdx ? *SourceIdx : -1);
            ConnObj->SetStringField(TEXT("from_node_name"), Input->Expression->GetName());
            ConnObj->SetNumberField(TEXT("from_output"), Input->OutputIndex);
            ConnObj->SetStringField(TEXT("to_output"), OutputName);
            OutputConnections.Add(MakeShared<FJsonValueObject>(ConnObj));
        }
    };

    AddOutputConnection(MP_BaseColor, TEXT("BaseColor"));
    AddOutputConnection(MP_Metallic, TEXT("Metallic"));
    AddOutputConnection(MP_Specular, TEXT("Specular"));
    AddOutputConnection(MP_Roughness, TEXT("Roughness"));
    AddOutputConnection(MP_EmissiveColor, TEXT("EmissiveColor"));
    AddOutputConnection(MP_Opacity, TEXT("Opacity"));
    AddOutputConnection(MP_OpacityMask, TEXT("OpacityMask"));
    AddOutputConnection(MP_Normal, TEXT("Normal"));
    AddOutputConnection(MP_WorldPositionOffset, TEXT("WorldPositionOffset"));
    AddOutputConnection(MP_AmbientOcclusion, TEXT("AmbientOcclusion"));
    AddOutputConnection(MP_Refraction, TEXT("Refraction"));
    AddOutputConnection(MP_SubsurfaceColor, TEXT("SubsurfaceColor"));

    ResultObj->SetArrayField(TEXT("input_nodes"), InputNodes);
    ResultObj->SetArrayField(TEXT("processing_nodes"), ProcessingNodes);
    ResultObj->SetArrayField(TEXT("output_connections"), OutputConnections);

    // Generate Mermaid diagram
    FString MermaidDiagram = TEXT("flowchart LR\n");
    MermaidDiagram += TEXT("    subgraph Inputs\n");
    for (const auto& Node : InputNodes)
    {
        auto NodeObj = Node->AsObject();
        int32 Index = (int32)NodeObj->GetNumberField(TEXT("index"));
        FString Type = NodeObj->GetStringField(TEXT("type"));
        FString Category = NodeObj->GetStringField(TEXT("category"));
        MermaidDiagram += FString::Printf(TEXT("        N%d[\"%s\"]\n"), Index, *Type);
    }
    MermaidDiagram += TEXT("    end\n");
    
    MermaidDiagram += TEXT("    subgraph Processing\n");
    for (const auto& Node : ProcessingNodes)
    {
        auto NodeObj = Node->AsObject();
        int32 Index = (int32)NodeObj->GetNumberField(TEXT("index"));
        FString Type = NodeObj->GetStringField(TEXT("type"));
        FString Category = NodeObj->GetStringField(TEXT("category"));
        if (Category == TEXT("MaterialFunction"))
        {
            FString FuncName = NodeObj->GetStringField(TEXT("function_name"));
            MermaidDiagram += FString::Printf(TEXT("        N%d[\"%s\"]\n"), Index, *FuncName);
        }
        else
        {
            MermaidDiagram += FString::Printf(TEXT("        N%d[\"%s\"]\n"), Index, *Type);
        }
    }
    MermaidDiagram += TEXT("    end\n");
    
    MermaidDiagram += TEXT("    subgraph MaterialOutputs\n");
    for (const auto& Conn : OutputConnections)
    {
        auto ConnObj = Conn->AsObject();
        FString OutputName = ConnObj->GetStringField(TEXT("to_output"));
        MermaidDiagram += FString::Printf(TEXT("        Out_%s[%s]\n"), *OutputName, *OutputName);
    }
    MermaidDiagram += TEXT("    end\n");
    
    // Add connections
    for (const auto& Node : ProcessingNodes)
    {
        auto NodeObj = Node->AsObject();
        int32 TargetIndex = (int32)NodeObj->GetNumberField(TEXT("index"));
        const TArray<TSharedPtr<FJsonValue>>* Connections;
        if (NodeObj->TryGetArrayField(TEXT("input_connections"), Connections))
        {
            for (const auto& Conn : *Connections)
            {
                auto ConnObj = Conn->AsObject();
                int32 SourceIndex = (int32)ConnObj->GetNumberField(TEXT("from_node"));
                if (SourceIndex >= 0)
                {
                    MermaidDiagram += FString::Printf(TEXT("    N%d --> N%d\n"), SourceIndex, TargetIndex);
                }
            }
        }
    }
    
    // Add output connections
    for (const auto& Conn : OutputConnections)
    {
        auto ConnObj = Conn->AsObject();
        int32 SourceIndex = (int32)ConnObj->GetNumberField(TEXT("from_node"));
        FString OutputName = ConnObj->GetStringField(TEXT("to_output"));
        if (SourceIndex >= 0)
        {
            MermaidDiagram += FString::Printf(TEXT("    N%d --> Out_%s\n"), SourceIndex, *OutputName);
        }
    }

    ResultObj->SetStringField(TEXT("mermaid_diagram"), MermaidDiagram);

    return ResultObj;
}

void FUECLIMaterialCommands::CollectMaterialFunctionGraph(
    UMaterialFunction* MaterialFunction,
    const FString& ParentPath,
    int32 Depth,
    int32 MaxDepth,
    TArray<TSharedPtr<FJsonValue>>& OutNodes,
    TArray<TSharedPtr<FJsonValue>>& OutConnections,
    TSet<FString>& ProcessedFunctions)
{
    if (!MaterialFunction || Depth > MaxDepth) return;
    
    FString FunctionPath = MaterialFunction->GetPathName();
    if (ProcessedFunctions.Contains(FunctionPath))
    {
        return; // Already processed, avoid cycles
    }
    ProcessedFunctions.Add(FunctionPath);
    
    auto FunctionExpressions = MaterialFunction->GetExpressions();
    
    // Build expression to index map
    TMap<UMaterialExpression*, int32> ExprToLocalIndex;
    for (int32 i = 0; i < FunctionExpressions.Num(); i++)
    {
        if (FunctionExpressions[i].Get())
        {
            ExprToLocalIndex.Add(FunctionExpressions[i].Get(), i);
        }
    }
    
    // Collect nodes with full path
    for (int32 i = 0; i < FunctionExpressions.Num(); i++)
    {
        UMaterialExpression* Expr = FunctionExpressions[i].Get();
        if (!Expr) continue;
        
        TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
        
        FString NodePath = ParentPath.IsEmpty() 
            ? FString::Printf(TEXT("%s/Node_%d"), *MaterialFunction->GetName(), i)
            : FString::Printf(TEXT("%s/%s/Node_%d"), *ParentPath, *MaterialFunction->GetName(), i);
        
        NodeObj->SetStringField(TEXT("node_path"), NodePath);
        NodeObj->SetStringField(TEXT("function_context"), ParentPath.IsEmpty() ? MaterialFunction->GetName() : ParentPath + TEXT("/") + MaterialFunction->GetName());
        NodeObj->SetNumberField(TEXT("depth"), Depth);
        NodeObj->SetNumberField(TEXT("local_index"), i);
        NodeObj->SetStringField(TEXT("name"), Expr->GetName());
        
        FString NodeType = Expr->GetClass()->GetName();
        NodeType.RemoveFromStart(TEXT("MaterialExpression"));
        NodeObj->SetStringField(TEXT("type"), NodeType);
        NodeObj->SetStringField(TEXT("description"), Expr->Desc);
        NodeObj->SetNumberField(TEXT("pos_x"), Expr->MaterialExpressionEditorX);
        NodeObj->SetNumberField(TEXT("pos_y"), Expr->MaterialExpressionEditorY);
        
        // Check for FunctionInput/Output
        if (UMaterialExpressionFunctionInput* FuncInput = Cast<UMaterialExpressionFunctionInput>(Expr))
        {
            NodeObj->SetStringField(TEXT("input_name"), FuncInput->InputName.ToString());
            NodeObj->SetStringField(TEXT("node_role"), TEXT("FunctionInput"));
        }
        else if (UMaterialExpressionFunctionOutput* FuncOutput = Cast<UMaterialExpressionFunctionOutput>(Expr))
        {
            NodeObj->SetStringField(TEXT("output_name"), FuncOutput->OutputName.ToString());
            NodeObj->SetStringField(TEXT("node_role"), TEXT("FunctionOutput"));
        }
        
        // Check for nested function call
        if (UMaterialExpressionMaterialFunctionCall* NestedFunc = Cast<UMaterialExpressionMaterialFunctionCall>(Expr))
        {
            if (NestedFunc->MaterialFunction)
            {
                NodeObj->SetStringField(TEXT("nested_function_path"), GetAssetPackagePath(NestedFunc->MaterialFunction.Get()));
                NodeObj->SetStringField(TEXT("nested_function_name"), NestedFunc->MaterialFunction->GetName());
                NodeObj->SetStringField(TEXT("node_role"), TEXT("FunctionCall"));
                
                // Recursively collect nested function
                UMaterialFunction* NestedMF = Cast<UMaterialFunction>(NestedFunc->MaterialFunction.Get());
                if (NestedMF)
                {
                    CollectMaterialFunctionGraph(NestedMF, NodePath, Depth + 1, MaxDepth, OutNodes, OutConnections, ProcessedFunctions);
                }
            }
        }
        
        OutNodes.Add(MakeShared<FJsonValueObject>(NodeObj));
    }
    
    // Collect connections within this function
    for (int32 TargetIndex = 0; TargetIndex < FunctionExpressions.Num(); TargetIndex++)
    {
        UMaterialExpression* TargetExpr = FunctionExpressions[TargetIndex].Get();
        if (!TargetExpr) continue;
        
        FString TargetPath = ParentPath.IsEmpty()
            ? FString::Printf(TEXT("%s/Node_%d"), *MaterialFunction->GetName(), TargetIndex)
            : FString::Printf(TEXT("%s/%s/Node_%d"), *ParentPath, *MaterialFunction->GetName(), TargetIndex);
        
        for (int32 InputIndex = 0; ; InputIndex++)
        {
            FExpressionInput* Input = TargetExpr->GetInput(InputIndex);
            if (!Input) break;
            
            if (Input->Expression)
            {
                int32* SourceIndexPtr = ExprToLocalIndex.Find(Input->Expression);
                if (SourceIndexPtr)
                {
                    FString SourcePath = ParentPath.IsEmpty()
                        ? FString::Printf(TEXT("%s/Node_%d"), *MaterialFunction->GetName(), *SourceIndexPtr)
                        : FString::Printf(TEXT("%s/%s/Node_%d"), *ParentPath, *MaterialFunction->GetName(), *SourceIndexPtr);
                    
                    TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
                    ConnObj->SetStringField(TEXT("source_path"), SourcePath);
                    ConnObj->SetStringField(TEXT("target_path"), TargetPath);
                    ConnObj->SetNumberField(TEXT("source_output_index"), Input->OutputIndex);
                    ConnObj->SetNumberField(TEXT("target_input_index"), InputIndex);
                    ConnObj->SetStringField(TEXT("function_context"), ParentPath.IsEmpty() ? MaterialFunction->GetName() : ParentPath + TEXT("/") + MaterialFunction->GetName());
                    
                    FName InputName = TargetExpr->GetInputName(InputIndex);
                    if (!InputName.IsNone())
                    {
                        ConnObj->SetStringField(TEXT("target_input_name"), InputName.ToString());
                    }
                    
                    OutConnections.Add(MakeShared<FJsonValueObject>(ConnObj));
                }
            }
        }
    }
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleGetMaterialFullGraph(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath;
    
    // Try to get path from params, or use currently opened material
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
        if (AssetEditorSubsystem)
        {
            TArray<UObject*> EditedAssets = AssetEditorSubsystem->GetAllEditedAssets();
            for (UObject* Asset : EditedAssets)
            {
                if (Cast<UMaterial>(Asset))
                {
                    MaterialPath = Asset->GetPathName();
                    break;
                }
            }
        }
    }

    if (MaterialPath.IsEmpty())
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("No material path provided and no material is currently opened"));
    }

    UMaterial* Material = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
    if (!Material)
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
    }

    int32 MaxDepth = 10;
    Params->TryGetNumberField(TEXT("max_depth"), MaxDepth);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
    ResultObj->SetStringField(TEXT("material_name"), Material->GetName());

    auto Expressions = Material->GetExpressions();

    // Build expression to index map
    TMap<UMaterialExpression*, int32> ExprToIndex;
    for (int32 i = 0; i < Expressions.Num(); i++)
    {
        if (Expressions[i].Get())
        {
            ExprToIndex.Add(Expressions[i].Get(), i);
        }
    }

    TArray<TSharedPtr<FJsonValue>> AllNodes;
    TArray<TSharedPtr<FJsonValue>> AllConnections;
    TSet<FString> ProcessedFunctions;
    FString MaterialName = Material->GetName();

    // First, collect all main material nodes
    for (int32 i = 0; i < Expressions.Num(); i++)
    {
        UMaterialExpression* Expr = Expressions[i].Get();
        if (!Expr) continue;

        TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
        
        FString NodePath = FString::Printf(TEXT("%s/Node_%d"), *MaterialName, i);
        NodeObj->SetStringField(TEXT("node_path"), NodePath);
        NodeObj->SetStringField(TEXT("function_context"), MaterialName);
        NodeObj->SetNumberField(TEXT("depth"), 0);
        NodeObj->SetNumberField(TEXT("local_index"), i);
        NodeObj->SetStringField(TEXT("name"), Expr->GetName());
        
        FString NodeType = Expr->GetClass()->GetName();
        NodeType.RemoveFromStart(TEXT("MaterialExpression"));
        NodeObj->SetStringField(TEXT("type"), NodeType);
        NodeObj->SetStringField(TEXT("description"), Expr->Desc);
        NodeObj->SetNumberField(TEXT("pos_x"), Expr->MaterialExpressionEditorX);
        NodeObj->SetNumberField(TEXT("pos_y"), Expr->MaterialExpressionEditorY);

        // Add parameter info if applicable
        if (UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expr))
        {
            NodeObj->SetStringField(TEXT("parameter_name"), ScalarParam->ParameterName.ToString());
            NodeObj->SetNumberField(TEXT("default_value"), ScalarParam->DefaultValue);
            NodeObj->SetStringField(TEXT("node_role"), TEXT("ScalarParameter"));
        }
        else if (UMaterialExpressionVectorParameter* VectorParam = Cast<UMaterialExpressionVectorParameter>(Expr))
        {
            NodeObj->SetStringField(TEXT("parameter_name"), VectorParam->ParameterName.ToString());
            NodeObj->SetStringField(TEXT("node_role"), TEXT("VectorParameter"));
        }
        else if (UMaterialExpressionTextureSampleParameter2D* TexParam = Cast<UMaterialExpressionTextureSampleParameter2D>(Expr))
        {
            NodeObj->SetStringField(TEXT("parameter_name"), TexParam->ParameterName.ToString());
            NodeObj->SetStringField(TEXT("node_role"), TEXT("TextureParameter"));
        }
        
        // Check for material function call - recursively expand
        if (UMaterialExpressionMaterialFunctionCall* FuncCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expr))
        {
            if (FuncCall->MaterialFunction)
            {
                NodeObj->SetStringField(TEXT("nested_function_path"), GetAssetPackagePath(FuncCall->MaterialFunction.Get()));
                NodeObj->SetStringField(TEXT("nested_function_name"), FuncCall->MaterialFunction->GetName());
                NodeObj->SetStringField(TEXT("node_role"), TEXT("FunctionCall"));
                
                // Add function input/output info
                TArray<TSharedPtr<FJsonValue>> FuncInputsJson;
                for (const FFunctionExpressionInput& FuncInput : FuncCall->FunctionInputs)
                {
                    TSharedPtr<FJsonObject> InputObj = MakeShared<FJsonObject>();
                    InputObj->SetStringField(TEXT("name"), FuncInput.ExpressionInput ? FuncInput.ExpressionInput->InputName.ToString() : TEXT(""));
                    InputObj->SetBoolField(TEXT("is_connected"), FuncInput.Input.Expression != nullptr);
                    FuncInputsJson.Add(MakeShared<FJsonValueObject>(InputObj));
                }
                NodeObj->SetArrayField(TEXT("function_inputs"), FuncInputsJson);
                
                TArray<TSharedPtr<FJsonValue>> FuncOutputsJson;
                for (const FFunctionExpressionOutput& FuncOutput : FuncCall->FunctionOutputs)
                {
                    TSharedPtr<FJsonObject> OutputObj = MakeShared<FJsonObject>();
                    OutputObj->SetStringField(TEXT("name"), FuncOutput.ExpressionOutput ? FuncOutput.ExpressionOutput->OutputName.ToString() : TEXT(""));
                    FuncOutputsJson.Add(MakeShared<FJsonValueObject>(OutputObj));
                }
                NodeObj->SetArrayField(TEXT("function_outputs"), FuncOutputsJson);
                
                // Recursively collect function internals
                UMaterialFunction* MatFunc = Cast<UMaterialFunction>(FuncCall->MaterialFunction.Get());
                if (MatFunc)
                {
                    CollectMaterialFunctionGraph(MatFunc, NodePath, 1, MaxDepth, AllNodes, AllConnections, ProcessedFunctions);
                }
            }
        }

        AllNodes.Add(MakeShared<FJsonValueObject>(NodeObj));
    }

    // Collect main material connections
    for (int32 TargetIndex = 0; TargetIndex < Expressions.Num(); TargetIndex++)
    {
        UMaterialExpression* TargetExpr = Expressions[TargetIndex].Get();
        if (!TargetExpr) continue;

        FString TargetPath = FString::Printf(TEXT("%s/Node_%d"), *MaterialName, TargetIndex);

        for (int32 InputIndex = 0; ; InputIndex++)
        {
            FExpressionInput* Input = TargetExpr->GetInput(InputIndex);
            if (!Input) break;

            if (Input->Expression)
            {
                int32* SourceIndexPtr = ExprToIndex.Find(Input->Expression);
                if (SourceIndexPtr)
                {
                    FString SourcePath = FString::Printf(TEXT("%s/Node_%d"), *MaterialName, *SourceIndexPtr);

                    TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
                    ConnObj->SetStringField(TEXT("source_path"), SourcePath);
                    ConnObj->SetStringField(TEXT("target_path"), TargetPath);
                    ConnObj->SetNumberField(TEXT("source_output_index"), Input->OutputIndex);
                    ConnObj->SetNumberField(TEXT("target_input_index"), InputIndex);
                    ConnObj->SetStringField(TEXT("function_context"), MaterialName);

                    FName InputName = TargetExpr->GetInputName(InputIndex);
                    if (!InputName.IsNone())
                    {
                        ConnObj->SetStringField(TEXT("target_input_name"), InputName.ToString());
                    }

                    AllConnections.Add(MakeShared<FJsonValueObject>(ConnObj));
                }
            }
        }
    }

    // Add material output connections
    auto AddOutputConnection = [&](EMaterialProperty Property, const FString& OutputName)
    {
        const FExpressionInput* Input = Material->GetExpressionInputForProperty(Property);
        if (Input && Input->Expression)
        {
            int32* SourceIndexPtr = ExprToIndex.Find(Input->Expression);
            if (SourceIndexPtr)
            {
                FString SourcePath = FString::Printf(TEXT("%s/Node_%d"), *MaterialName, *SourceIndexPtr);

                TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
                ConnObj->SetStringField(TEXT("source_path"), SourcePath);
                ConnObj->SetStringField(TEXT("target_path"), FString::Printf(TEXT("%s/MaterialOutput/%s"), *MaterialName, *OutputName));
                ConnObj->SetNumberField(TEXT("source_output_index"), Input->OutputIndex);
                ConnObj->SetStringField(TEXT("target_output_name"), OutputName);
                ConnObj->SetStringField(TEXT("function_context"), MaterialName);
                ConnObj->SetBoolField(TEXT("is_material_output"), true);

                AllConnections.Add(MakeShared<FJsonValueObject>(ConnObj));
            }
        }
    };

    AddOutputConnection(MP_BaseColor, TEXT("BaseColor"));
    AddOutputConnection(MP_Metallic, TEXT("Metallic"));
    AddOutputConnection(MP_Specular, TEXT("Specular"));
    AddOutputConnection(MP_Roughness, TEXT("Roughness"));
    AddOutputConnection(MP_EmissiveColor, TEXT("EmissiveColor"));
    AddOutputConnection(MP_Opacity, TEXT("Opacity"));
    AddOutputConnection(MP_OpacityMask, TEXT("OpacityMask"));
    AddOutputConnection(MP_Normal, TEXT("Normal"));
    AddOutputConnection(MP_WorldPositionOffset, TEXT("WorldPositionOffset"));
    AddOutputConnection(MP_AmbientOcclusion, TEXT("AmbientOcclusion"));
    AddOutputConnection(MP_Refraction, TEXT("Refraction"));
    AddOutputConnection(MP_SubsurfaceColor, TEXT("SubsurfaceColor"));

    ResultObj->SetArrayField(TEXT("all_nodes"), AllNodes);
    ResultObj->SetArrayField(TEXT("all_connections"), AllConnections);
    ResultObj->SetNumberField(TEXT("total_node_count"), AllNodes.Num());
    ResultObj->SetNumberField(TEXT("total_connection_count"), AllConnections.Num());
    ResultObj->SetNumberField(TEXT("max_depth_reached"), MaxDepth);

    // Generate hierarchical summary
    TMap<FString, int32> ContextNodeCounts;
    for (const auto& Node : AllNodes)
    {
        FString Context = Node->AsObject()->GetStringField(TEXT("function_context"));
        ContextNodeCounts.FindOrAdd(Context)++;
    }

    TArray<TSharedPtr<FJsonValue>> HierarchySummary;
    for (const auto& Pair : ContextNodeCounts)
    {
        TSharedPtr<FJsonObject> SummaryObj = MakeShared<FJsonObject>();
        SummaryObj->SetStringField(TEXT("context"), Pair.Key);
        SummaryObj->SetNumberField(TEXT("node_count"), Pair.Value);
        HierarchySummary.Add(MakeShared<FJsonValueObject>(SummaryObj));
    }
    ResultObj->SetArrayField(TEXT("hierarchy_summary"), HierarchySummary);

    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleGetMaterialShaderInfo(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath;
    
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
        if (AssetEditorSubsystem)
        {
            TArray<UObject*> EditedAssets = AssetEditorSubsystem->GetAllEditedAssets();
            for (UObject* Asset : EditedAssets)
            {
                if (Cast<UMaterial>(Asset))
                {
                    MaterialPath = Asset->GetPathName();
                    break;
                }
            }
        }
    }

    if (MaterialPath.IsEmpty())
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("No material path provided and no material is currently opened"));
    }

    UMaterial* Material = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
    if (!Material)
    {
        return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
    ResultObj->SetStringField(TEXT("material_name"), Material->GetName());

    // Material domain
    FString DomainStr;
    switch (Material->MaterialDomain)
    {
        case MD_Surface: DomainStr = TEXT("Surface"); break;
        case MD_DeferredDecal: DomainStr = TEXT("DeferredDecal"); break;
        case MD_LightFunction: DomainStr = TEXT("LightFunction"); break;
        case MD_Volume: DomainStr = TEXT("Volume"); break;
        case MD_PostProcess: DomainStr = TEXT("PostProcess"); break;
        case MD_UI: DomainStr = TEXT("UI"); break;
        default: DomainStr = TEXT("Unknown"); break;
    }
    ResultObj->SetStringField(TEXT("domain"), DomainStr);

    // Blend mode
    FString BlendModeStr;
    switch (Material->BlendMode)
    {
        case BLEND_Opaque: BlendModeStr = TEXT("Opaque"); break;
        case BLEND_Masked: BlendModeStr = TEXT("Masked"); break;
        case BLEND_Translucent: BlendModeStr = TEXT("Translucent"); break;
        case BLEND_Additive: BlendModeStr = TEXT("Additive"); break;
        case BLEND_Modulate: BlendModeStr = TEXT("Modulate"); break;
        case BLEND_AlphaComposite: BlendModeStr = TEXT("AlphaComposite"); break;
        case BLEND_AlphaHoldout: BlendModeStr = TEXT("AlphaHoldout"); break;
        default: BlendModeStr = TEXT("Unknown"); break;
    }
    ResultObj->SetStringField(TEXT("blend_mode"), BlendModeStr);

    // Shading model
    FString ShadingModelStr;
    switch (Material->GetShadingModels().GetFirstShadingModel())
    {
        case MSM_DefaultLit: ShadingModelStr = TEXT("DefaultLit"); break;
        case MSM_Unlit: ShadingModelStr = TEXT("Unlit"); break;
        case MSM_Subsurface: ShadingModelStr = TEXT("Subsurface"); break;
        case MSM_PreintegratedSkin: ShadingModelStr = TEXT("PreintegratedSkin"); break;
        case MSM_ClearCoat: ShadingModelStr = TEXT("ClearCoat"); break;
        case MSM_SubsurfaceProfile: ShadingModelStr = TEXT("SubsurfaceProfile"); break;
        case MSM_TwoSidedFoliage: ShadingModelStr = TEXT("TwoSidedFoliage"); break;
        case MSM_Hair: ShadingModelStr = TEXT("Hair"); break;
        case MSM_Cloth: ShadingModelStr = TEXT("Cloth"); break;
        case MSM_Eye: ShadingModelStr = TEXT("Eye"); break;
        case MSM_SingleLayerWater: ShadingModelStr = TEXT("SingleLayerWater"); break;
        case MSM_ThinTranslucent: ShadingModelStr = TEXT("ThinTranslucent"); break;
        default: ShadingModelStr = TEXT("Custom"); break;
    }
    ResultObj->SetStringField(TEXT("shading_model"), ShadingModelStr);

    // Usage flags
    TArray<TSharedPtr<FJsonValue>> UsageFlags;
    if (Material->bUsedWithSkeletalMesh) UsageFlags.Add(MakeShared<FJsonValueString>(TEXT("SkeletalMesh")));
    if (Material->bUsedWithStaticLighting) UsageFlags.Add(MakeShared<FJsonValueString>(TEXT("StaticLighting")));
    if (Material->bUsedWithParticleSprites) UsageFlags.Add(MakeShared<FJsonValueString>(TEXT("ParticleSprites")));
    if (Material->bUsedWithMeshParticles) UsageFlags.Add(MakeShared<FJsonValueString>(TEXT("MeshParticles")));
    if (Material->bUsedWithNiagaraSprites) UsageFlags.Add(MakeShared<FJsonValueString>(TEXT("NiagaraSprites")));
    if (Material->bUsedWithNiagaraMeshParticles) UsageFlags.Add(MakeShared<FJsonValueString>(TEXT("NiagaraMeshParticles")));
    if (Material->bUsedWithMorphTargets) UsageFlags.Add(MakeShared<FJsonValueString>(TEXT("MorphTargets")));
    if (Material->bUsedWithSplineMeshes) UsageFlags.Add(MakeShared<FJsonValueString>(TEXT("SplineMeshes")));
    if (Material->bUsedWithInstancedStaticMeshes) UsageFlags.Add(MakeShared<FJsonValueString>(TEXT("InstancedStaticMeshes")));
    if (Material->bUsedWithClothing) UsageFlags.Add(MakeShared<FJsonValueString>(TEXT("Clothing")));
    ResultObj->SetArrayField(TEXT("usage_flags"), UsageFlags);

    // Material properties
    ResultObj->SetBoolField(TEXT("two_sided"), Material->IsTwoSided());
    ResultObj->SetBoolField(TEXT("is_masked"), Material->IsMasked());
    ResultObj->SetBoolField(TEXT("is_translucent"), IsTranslucentBlendMode(Material->BlendMode));
    ResultObj->SetNumberField(TEXT("opacity_mask_clip_value"), Material->GetOpacityMaskClipValue());

    // Expression counts
    auto Expressions = Material->GetExpressions();
    ResultObj->SetNumberField(TEXT("expression_count"), Expressions.Num());

    // Count by type
    TMap<FString, int32> TypeCounts;
    int32 FunctionCallCount = 0;
    int32 ParameterCount = 0;
    int32 TextureSampleCount = 0;

    for (const auto& ExprPtr : Expressions)
    {
        if (UMaterialExpression* Expr = ExprPtr.Get())
        {
            FString TypeName = Expr->GetClass()->GetName();
            TypeName.RemoveFromStart(TEXT("MaterialExpression"));
            TypeCounts.FindOrAdd(TypeName)++;

            if (Cast<UMaterialExpressionMaterialFunctionCall>(Expr)) FunctionCallCount++;
            if (Cast<UMaterialExpressionScalarParameter>(Expr) || 
                Cast<UMaterialExpressionVectorParameter>(Expr) ||
                Cast<UMaterialExpressionTextureSampleParameter2D>(Expr)) ParameterCount++;
            if (Cast<UMaterialExpressionTextureSample>(Expr)) TextureSampleCount++;
        }
    }

    ResultObj->SetNumberField(TEXT("function_call_count"), FunctionCallCount);
    ResultObj->SetNumberField(TEXT("parameter_count"), ParameterCount);
    ResultObj->SetNumberField(TEXT("texture_sample_count"), TextureSampleCount);

    return ResultObj;
}

TSharedPtr<FJsonObject> FUECLIMaterialCommands::HandleExplainMaterialNode(const TSharedPtr<FJsonObject>& Params)
{
    FString NodeType;
    if (!Params->TryGetStringField(TEXT("node_type"), NodeType))
    {
        return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'node_type' parameter"));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_type"), NodeType);

    // Node explanations database
    struct FNodeExplanation
    {
        FString Description;
        FString Category;
        FString UseCases;
        TArray<FString> Inputs;
        TArray<FString> Outputs;
    };

    TMap<FString, FNodeExplanation> NodeDatabase;

    // Math nodes
    NodeDatabase.Add(TEXT("Add"), {
        TEXT("Adds two input values together (A + B). Works with scalars and vectors."),
        TEXT("Math"),
        TEXT("Combining colors, adjusting brightness, blending values"),
        {TEXT("A"), TEXT("B")},
        {TEXT("Result")}
    });
    
    NodeDatabase.Add(TEXT("Multiply"), {
        TEXT("Multiplies two values (A * B). Essential for masking and scaling."),
        TEXT("Math"),
        TEXT("Applying masks, scaling textures, tinting colors, adjusting intensity"),
        {TEXT("A"), TEXT("B")},
        {TEXT("Result")}
    });
    
    NodeDatabase.Add(TEXT("Subtract"), {
        TEXT("Subtracts B from A (A - B)."),
        TEXT("Math"),
        TEXT("Inverting values, creating contrast, offset calculations"),
        {TEXT("A"), TEXT("B")},
        {TEXT("Result")}
    });
    
    NodeDatabase.Add(TEXT("Divide"), {
        TEXT("Divides A by B (A / B). Be careful of division by zero."),
        TEXT("Math"),
        TEXT("Normalizing values, ratio calculations"),
        {TEXT("A"), TEXT("B")},
        {TEXT("Result")}
    });
    
    NodeDatabase.Add(TEXT("LinearInterpolate"), {
        TEXT("Linearly interpolates between A and B based on Alpha (0=A, 1=B). Also known as Lerp."),
        TEXT("Math"),
        TEXT("Blending textures, smooth transitions, mask-based mixing"),
        {TEXT("A"), TEXT("B"), TEXT("Alpha")},
        {TEXT("Result")}
    });
    
    NodeDatabase.Add(TEXT("Power"), {
        TEXT("Raises Base to the power of Exp (Base^Exp)."),
        TEXT("Math"),
        TEXT("Adjusting falloff curves, contrast adjustment, fresnel effects"),
        {TEXT("Base"), TEXT("Exp")},
        {TEXT("Result")}
    });

    NodeDatabase.Add(TEXT("Sine"), {
        TEXT("Returns a sine wave from the input value."),
        TEXT("Math"),
        TEXT("Oscillation, flutter motion, periodic mask animation"),
        {TEXT("Input")},
        {TEXT("Result")}
    });

    NodeDatabase.Add(TEXT("Cosine"), {
        TEXT("Returns a cosine wave from the input value."),
        TEXT("Math"),
        TEXT("Phase-shifted oscillation, circular motion helpers, periodic shading"),
        {TEXT("Input")},
        {TEXT("Result")}
    });
    
    NodeDatabase.Add(TEXT("Clamp"), {
        TEXT("Clamps the input value between Min and Max."),
        TEXT("Math"),
        TEXT("Limiting value ranges, preventing artifacts, clamping colors"),
        {TEXT("Input"), TEXT("Min"), TEXT("Max")},
        {TEXT("Result")}
    });
    
    NodeDatabase.Add(TEXT("OneMinus"), {
        TEXT("Returns 1 - Input. Inverts the value."),
        TEXT("Math"),
        TEXT("Inverting masks, creating negative space, flipping gradients"),
        {TEXT("Input")},
        {TEXT("Result")}
    });
    
    NodeDatabase.Add(TEXT("Abs"), {
        TEXT("Returns the absolute value of the input."),
        TEXT("Math"),
        TEXT("Ensuring positive values, symmetric effects"),
        {TEXT("Input")},
        {TEXT("Result")}
    });

    // Texture nodes
    NodeDatabase.Add(TEXT("TextureSample"), {
        TEXT("Samples a texture at the given UV coordinates."),
        TEXT("Texture"),
        TEXT("Reading texture data, applying textures to materials"),
        {TEXT("UVs"), TEXT("Texture")},
        {TEXT("RGB"), TEXT("R"), TEXT("G"), TEXT("B"), TEXT("A")}
    });
    
    NodeDatabase.Add(TEXT("TextureSampleParameter2D"), {
        TEXT("Texture sample with an exposed parameter for material instances."),
        TEXT("Texture/Parameter"),
        TEXT("Creating customizable materials, swappable textures"),
        {TEXT("UVs")},
        {TEXT("RGB"), TEXT("R"), TEXT("G"), TEXT("B"), TEXT("A")}
    });
    
    NodeDatabase.Add(TEXT("TextureCoordinate"), {
        TEXT("Provides UV coordinates for texture sampling. Can tile and offset."),
        TEXT("Texture"),
        TEXT("UV manipulation, tiling textures, creating patterns"),
        {},
        {TEXT("UV")}
    });

    NodeDatabase.Add(TEXT("Panner"), {
        TEXT("Offsets UV coordinates over time for scrolling and wind-driven motion."),
        TEXT("Texture"),
        TEXT("Wind animation, drifting masks, scrolling noise, feather sway"),
        {TEXT("Coordinate"), TEXT("Time"), TEXT("Speed")},
        {TEXT("UV")}
    });

    NodeDatabase.Add(TEXT("Rotator"), {
        TEXT("Rotates UV coordinates around a pivot over time."),
        TEXT("Texture"),
        TEXT("Twisting anisotropy masks, rotating breakup textures, directional variation"),
        {TEXT("Coordinate"), TEXT("Time")},
        {TEXT("UV")}
    });

    // Parameter nodes
    NodeDatabase.Add(TEXT("ScalarParameter"), {
        TEXT("A single float value that can be adjusted in material instances."),
        TEXT("Parameter"),
        TEXT("Exposing adjustable values like intensity, scale, threshold"),
        {},
        {TEXT("Value")}
    });
    
    NodeDatabase.Add(TEXT("VectorParameter"), {
        TEXT("A 4-component vector (RGBA/XYZW) adjustable in material instances."),
        TEXT("Parameter"),
        TEXT("Exposing colors, directions, multiple related values"),
        {},
        {TEXT("RGBA"), TEXT("R"), TEXT("G"), TEXT("B"), TEXT("A")}
    });

    // Utility nodes
    NodeDatabase.Add(TEXT("Fresnel"), {
        TEXT("Creates a rim lighting effect based on view angle."),
        TEXT("Utility"),
        TEXT("Rim lighting, edge detection, view-dependent effects"),
        {TEXT("ExponentIn"), TEXT("BaseReflectFractionIn"), TEXT("Normal")},
        {TEXT("Result")}
    });
    
    NodeDatabase.Add(TEXT("ComponentMask"), {
        TEXT("Extracts specific channels from a vector (R, G, B, A)."),
        TEXT("Utility"),
        TEXT("Isolating color channels, extracting packed data"),
        {TEXT("Input")},
        {TEXT("Masked channels")}
    });
    
    NodeDatabase.Add(TEXT("AppendVector"), {
        TEXT("Combines two values into a larger vector."),
        TEXT("Utility"),
        TEXT("Building vectors from scalars, combining channels"),
        {TEXT("A"), TEXT("B")},
        {TEXT("Result")}
    });

    NodeDatabase.Add(TEXT("DotProduct"), {
        TEXT("Returns the dot product between two vectors."),
        TEXT("Utility"),
        TEXT("Directional masks, view alignment, feather strand highlight shaping"),
        {TEXT("A"), TEXT("B")},
        {TEXT("Result")}
    });

    NodeDatabase.Add(TEXT("Normalize"), {
        TEXT("Normalizes a vector to unit length."),
        TEXT("Utility"),
        TEXT("Preparing direction vectors, stable dot products, lighting math"),
        {TEXT("Vector")},
        {TEXT("Result")}
    });
    
    NodeDatabase.Add(TEXT("If"), {
        TEXT("Conditional branching based on comparing A to B."),
        TEXT("Utility"),
        TEXT("Conditional logic, threshold-based switching"),
        {TEXT("A"), TEXT("B"), TEXT("A>B"), TEXT("A=B"), TEXT("A<B")},
        {TEXT("Result")}
    });
    
    NodeDatabase.Add(TEXT("StaticSwitch"), {
        TEXT("Compile-time switch that includes only one branch in the final shader."),
        TEXT("Utility"),
        TEXT("Feature toggles, shader variants without runtime cost"),
        {TEXT("True"), TEXT("False"), TEXT("Value")},
        {TEXT("Result")}
    });

    // Material Function nodes
    NodeDatabase.Add(TEXT("MaterialFunctionCall"), {
        TEXT("Calls a reusable material function. Functions encapsulate complex logic."),
        TEXT("Function"),
        TEXT("Code reuse, modular material design, complex effects"),
        {TEXT("Function-specific inputs")},
        {TEXT("Function-specific outputs")}
    });
    
    NodeDatabase.Add(TEXT("FunctionInput"), {
        TEXT("Defines an input for a material function."),
        TEXT("Function"),
        TEXT("Creating function interfaces, exposing function parameters"),
        {},
        {TEXT("Value")}
    });
    
    NodeDatabase.Add(TEXT("FunctionOutput"), {
        TEXT("Defines an output for a material function."),
        TEXT("Function"),
        TEXT("Returning values from functions"),
        {TEXT("Input")},
        {}
    });

    // Material Attributes nodes
    NodeDatabase.Add(TEXT("MakeMaterialAttributes"), {
        TEXT("Packs individual material properties into a single MaterialAttributes struct."),
        TEXT("MaterialAttributes"),
        TEXT("Creating complete material outputs, layered materials"),
        {TEXT("BaseColor"), TEXT("Metallic"), TEXT("Specular"), TEXT("Roughness"), TEXT("Normal"), TEXT("etc.")},
        {TEXT("MaterialAttributes")}
    });
    
    NodeDatabase.Add(TEXT("BreakMaterialAttributes"), {
        TEXT("Unpacks MaterialAttributes into individual properties."),
        TEXT("MaterialAttributes"),
        TEXT("Accessing individual properties from function outputs, material layering"),
        {TEXT("MaterialAttributes")},
        {TEXT("BaseColor"), TEXT("Metallic"), TEXT("Specular"), TEXT("Roughness"), TEXT("Normal"), TEXT("etc.")}
    });

    // World position nodes
    NodeDatabase.Add(TEXT("WorldPosition"), {
        TEXT("Returns the world-space position of the current pixel."),
        TEXT("Coordinates"),
        TEXT("World-space effects, distance-based blending, procedural patterns"),
        {},
        {TEXT("Absolute World Position")}
    });

    NodeDatabase.Add(TEXT("CameraVectorWS"), {
        TEXT("Returns the direction from the pixel toward the camera in world space."),
        TEXT("Coordinates"),
        TEXT("View-dependent highlights, fake anisotropy, feather edge response"),
        {},
        {TEXT("Vector")}
    });

    NodeDatabase.Add(TEXT("ObjectPositionWS"), {
        TEXT("Returns the object position in world space."),
        TEXT("Coordinates"),
        TEXT("Per-object variation, local gradients, flock and feather clustering offsets"),
        {},
        {TEXT("Position")}
    });
    
    NodeDatabase.Add(TEXT("VertexNormalWS"), {
        TEXT("Returns the vertex normal in world space."),
        TEXT("Coordinates"),
        TEXT("Lighting calculations, directional effects"),
        {},
        {TEXT("Normal")}
    });

    NodeDatabase.Add(TEXT("VertexColor"), {
        TEXT("Reads per-vertex RGBA color data from the mesh."),
        TEXT("Coordinates"),
        TEXT("Per-card variation, painted control, masking wind and tint across feathers"),
        {},
        {TEXT("RGBA"), TEXT("R"), TEXT("G"), TEXT("B"), TEXT("A")}
    });
    
    NodeDatabase.Add(TEXT("PixelNormalWS"), {
        TEXT("Returns the pixel normal in world space (after normal mapping)."),
        TEXT("Coordinates"),
        TEXT("Post-normal-map lighting effects"),
        {},
        {TEXT("Normal")}
    });

    // Reroute and organization
    NodeDatabase.Add(TEXT("Reroute"), {
        TEXT("A pass-through node for organizing material graphs. No computation cost."),
        TEXT("Organization"),
        TEXT("Cleaning up wire spaghetti, improving graph readability"),
        {TEXT("Input")},
        {TEXT("Output")}
    });
    
    NodeDatabase.Add(TEXT("Comment"), {
        TEXT("A comment box for documenting and organizing the material graph."),
        TEXT("Organization"),
        TEXT("Documentation, grouping related nodes"),
        {},
        {}
    });

    // Look up the explanation
    if (FNodeExplanation* Found = NodeDatabase.Find(NodeType))
    {
        ResultObj->SetStringField(TEXT("description"), Found->Description);
        ResultObj->SetStringField(TEXT("category"), Found->Category);
        ResultObj->SetStringField(TEXT("use_cases"), Found->UseCases);
        
        TArray<TSharedPtr<FJsonValue>> InputsArray;
        for (const FString& Input : Found->Inputs)
        {
            InputsArray.Add(MakeShared<FJsonValueString>(Input));
        }
        ResultObj->SetArrayField(TEXT("inputs"), InputsArray);
        
        TArray<TSharedPtr<FJsonValue>> OutputsArray;
        for (const FString& Output : Found->Outputs)
        {
            OutputsArray.Add(MakeShared<FJsonValueString>(Output));
        }
        ResultObj->SetArrayField(TEXT("outputs"), OutputsArray);
    }
    else
    {
        ResultObj->SetStringField(TEXT("description"), FString::Printf(TEXT("Material expression node: %s. See Unreal documentation for details."), *NodeType));
        ResultObj->SetStringField(TEXT("category"), TEXT("Unknown"));
        ResultObj->SetStringField(TEXT("use_cases"), TEXT("Check Unreal Engine documentation"));
    }

    return ResultObj;
}
