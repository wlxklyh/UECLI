#include "Commands/UECLITextureGraphCommands.h"
#include "Commands/UECLICommonUtils.h"
#include "ToolRegistry/UECLIToolRegistry.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

// TextureGraph core
#include "TextureGraph.h"
#include "TG_Graph.h"
#include "TG_Node.h"
#include "TG_Pin.h"
#include "Expressions/TG_Expression.h"
#include "TG_OutputSettings.h"
#include "Blueprint/TG_BlueprintFunctionLibrary.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"

// Expression types - Procedural
#include "Expressions/Procedural/TG_Expression_Noise.h"
#include "Expressions/Procedural/TG_Expression_Gradient.h"
#include "Expressions/Procedural/TG_Expression_Pattern.h"
#include "Expressions/Procedural/TG_Expression_Shape.h"
#include "Expressions/Procedural/TG_Expression_Transform.h"
// Expression types - Maths
#include "Expressions/Maths/TG_Expression_Maths_TwoInputs.h"
#include "Expressions/Maths/TG_Expression_Maths_OneInput.h"
#include "Expressions/Maths/TG_Expression_Blend.h"
#include "Expressions/Maths/TG_Expression_Clamp.h"
#include "Expressions/Maths/TG_Expression_Invert.h"
#include "Expressions/Maths/TG_Expression_IfThenElse.h"
// Expression types - Color
#include "Expressions/Color/TG_Expression_Grayscale.h"
#include "Expressions/Color/TG_Expression_HSV.h"
#include "Expressions/Color/TG_Expression_ColorCorrection.h"
#include "Expressions/Color/TG_Expression_Premult.h"
// Expression types - Channel
#include "Expressions/Channel/TG_Expression_ChannelSplitter.h"
#include "Expressions/Channel/TG_Expression_ChannelCombiner.h"
#include "Expressions/Channel/TG_Expression_ChannelSwizzle.h"
// Expression types - Filter
#include "Expressions/Filter/TG_Expression_Blur.h"
#include "Expressions/Filter/TG_Expression_Brightness.h"
#include "Expressions/Filter/TG_Expression_Levels.h"
#include "Expressions/Filter/TG_Expression_Threshold.h"
#include "Expressions/Filter/TG_Expression_EdgeDetect.h"
#include "Expressions/Filter/TG_Expression_ErodeDilate.h"
#include "Expressions/Filter/TG_Expression_Warp.h"
#include "Expressions/Adjustment/TG_Expression_NormalFromHeightMap.h"
// Expression types - Input
#include "Expressions/Input/TG_Expression_Scalar.h"
#include "Expressions/Input/TG_Expression_Vector.h"
#include "Expressions/Input/TG_Expression_Color.h"
#include "Expressions/Input/TG_Expression_Bool.h"
#include "Expressions/Input/TG_Expression_Texture.h"
// Expression types - Output
#include "Expressions/Output/TG_Expression_Output.h"

namespace
{
	TSharedRef<FUECLITextureGraphCommands> GetTextureGraphCommands()
	{
		static TSharedRef<FUECLITextureGraphCommands> Instance = MakeShared<FUECLITextureGraphCommands>();
		return Instance;
	}

	void RegisterTGCommand(FUECLIToolRegistry& Registry, const TCHAR* PublicName, const TCHAR* InternalName = nullptr)
	{
		const TSharedRef<FUECLITextureGraphCommands> Handler = GetTextureGraphCommands();
		const FString RoutedName = InternalName != nullptr ? InternalName : PublicName;
		Registry.Register(
			FUECLIToolSchema(PublicName, TEXT("TextureGraph"), FString()),
			[Handler, RoutedName](const TSharedPtr<FJsonObject>& Params)
			{
				return Handler->HandleCommand(RoutedName, Params);
			});
	}

	// Expression type name -> UClass* map
	const TMap<FString, UClass*>& GetExpressionClassMap()
	{
		static TMap<FString, UClass*> Map;
		if (Map.Num() == 0)
		{
			// Procedural
			Map.Add(TEXT("Noise"), UTG_Expression_Noise::StaticClass());
			Map.Add(TEXT("Gradient"), UTG_Expression_Gradient::StaticClass());
			Map.Add(TEXT("Pattern"), UTG_Expression_Pattern::StaticClass());
			Map.Add(TEXT("Shape"), UTG_Expression_Shape::StaticClass());
			Map.Add(TEXT("Transform"), UTG_Expression_Transform::StaticClass());
			// Maths
			Map.Add(TEXT("Add"), UTG_Expression_Add::StaticClass());
			Map.Add(TEXT("Subtract"), UTG_Expression_Subtract::StaticClass());
			Map.Add(TEXT("Multiply"), UTG_Expression_Multiply::StaticClass());
			Map.Add(TEXT("Divide"), UTG_Expression_Divide::StaticClass());
			Map.Add(TEXT("Pow"), UTG_Expression_Pow::StaticClass());
			Map.Add(TEXT("Step"), UTG_Expression_Step::StaticClass());
			Map.Add(TEXT("Dot"), UTG_Expression_Dot::StaticClass());
			Map.Add(TEXT("Cross"), UTG_Expression_Cross::StaticClass());
			Map.Add(TEXT("Blend"), UTG_Expression_Blend::StaticClass());
			Map.Add(TEXT("Clamp"), UTG_Expression_Clamp::StaticClass());
			Map.Add(TEXT("Invert"), UTG_Expression_Invert::StaticClass());
			Map.Add(TEXT("IfThenElse"), UTG_Expression_IfThenElse::StaticClass());
			// Color
			Map.Add(TEXT("Grayscale"), UTG_Expression_Grayscale::StaticClass());
			Map.Add(TEXT("HSV"), UTG_Expression_HSV::StaticClass());
			Map.Add(TEXT("ColorCorrection"), UTG_Expression_ColorCorrection::StaticClass());
			Map.Add(TEXT("Premult"), UTG_Expression_Premult::StaticClass());
			// Channel
			Map.Add(TEXT("ChannelSplitter"), UTG_Expression_ChannelSplitter::StaticClass());
			Map.Add(TEXT("ChannelCombiner"), UTG_Expression_ChannelCombiner::StaticClass());
			Map.Add(TEXT("ChannelSwizzle"), UTG_Expression_ChannelSwizzle::StaticClass());
			// Filter
			Map.Add(TEXT("Blur"), UTG_Expression_Blur::StaticClass());
			Map.Add(TEXT("Brightness"), UTG_Expression_Brightness::StaticClass());
			Map.Add(TEXT("Levels"), UTG_Expression_Levels::StaticClass());
			Map.Add(TEXT("Threshold"), UTG_Expression_Threshold::StaticClass());
			Map.Add(TEXT("EdgeDetect"), UTG_Expression_EdgeDetect::StaticClass());
			Map.Add(TEXT("ErodeDilate"), UTG_Expression_ErodeDilate::StaticClass());
			Map.Add(TEXT("Warp"), UTG_Expression_Warp::StaticClass());
			Map.Add(TEXT("NormalFromHeightMap"), UTG_Expression_NormalFromHeightMap::StaticClass());
			// Input
			Map.Add(TEXT("Scalar"), UTG_Expression_Scalar::StaticClass());
			Map.Add(TEXT("Vector"), UTG_Expression_Vector::StaticClass());
			Map.Add(TEXT("Color"), UTG_Expression_Color::StaticClass());
			Map.Add(TEXT("Bool"), UTG_Expression_Bool::StaticClass());
			Map.Add(TEXT("Texture"), UTG_Expression_Texture::StaticClass());
			// Output
			Map.Add(TEXT("Output"), UTG_Expression_Output::StaticClass());
		}
		return Map;
	}

	UClass* ResolveExpressionClass(const FString& TypeName)
	{
		if (const UClass* const* Found = GetExpressionClassMap().Find(TypeName))
		{
			return const_cast<UClass*>(*Found);
		}
		// Fallback: try UObject reflection
		const FString FullClassName = FString::Printf(TEXT("UTG_Expression_%s"), *TypeName);
		return FindObject<UClass>(nullptr, *FullClassName);
	}

	bool TryGetTGPath(const TSharedPtr<FJsonObject>& Params, FString& OutPath)
	{
		return Params->TryGetStringField(TEXT("tg_path"), OutPath)
			|| Params->TryGetStringField(TEXT("path"), OutPath);
	}

	UTextureGraph* LoadTextureGraph(const FString& Path)
	{
		UObject* Asset = UEditorAssetLibrary::LoadAsset(Path);
		return Cast<UTextureGraph>(Asset);
	}

	UTG_Node* FindNodeByName(UTG_Graph* Graph, const FString& NodeName)
	{
		UTG_Node* Found = nullptr;
		Graph->ForEachNodes([&](const UTG_Node* Node, uint32 Index)
		{
			if (Node && Node->GetNodeName().ToString() == NodeName)
			{
				Found = const_cast<UTG_Node*>(Node);
			}
		});
		return Found;
	}

	TSharedPtr<FJsonObject> SerializeNode(const UTG_Node* Node)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		if (!Node) return Obj;

		Obj->SetStringField(TEXT("node_name"), Node->GetNodeName().ToString());
		Obj->SetNumberField(TEXT("node_id"), Node->GetId().NodeIdx());

		if (UTG_Expression* Expr = const_cast<UTG_Node*>(Node)->GetExpression())
		{
			Obj->SetStringField(TEXT("expression_class"), Expr->GetClass()->GetName());
			Obj->SetStringField(TEXT("category"), Expr->GetCategory().ToString());
		}

		// Pins
		TArray<TSharedPtr<FJsonValue>> InputPins, OutputPins;
		const_cast<UTG_Node*>(Node)->ForEachInputPins([&](const UTG_Pin* Pin, uint32 Idx)
		{
			TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
			PinObj->SetStringField(TEXT("name"), Pin->GetArgumentName().ToString());
			PinObj->SetNumberField(TEXT("index"), Idx);
			PinObj->SetBoolField(TEXT("connected"), Pin->IsConnected());
			InputPins.Add(MakeShared<FJsonValueObject>(PinObj));
		});
		const_cast<UTG_Node*>(Node)->ForEachOutputPins([&](const UTG_Pin* Pin, uint32 Idx)
		{
			TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
			PinObj->SetStringField(TEXT("name"), Pin->GetArgumentName().ToString());
			PinObj->SetNumberField(TEXT("index"), Idx);
			PinObj->SetBoolField(TEXT("connected"), Pin->IsConnected());
			OutputPins.Add(MakeShared<FJsonValueObject>(PinObj));
		});
		Obj->SetArrayField(TEXT("input_pins"), InputPins);
		Obj->SetArrayField(TEXT("output_pins"), OutputPins);

		return Obj;
	}
}

FUECLITextureGraphCommands::FUECLITextureGraphCommands()
{
}

void FUECLITextureGraphCommands::RegisterTools(FUECLIToolRegistry& Registry)
{
	RegisterTGCommand(Registry, TEXT("create_texture_graph"));
	RegisterTGCommand(Registry, TEXT("add_tg_node"));
	RegisterTGCommand(Registry, TEXT("connect_tg_nodes"));
	RegisterTGCommand(Registry, TEXT("set_tg_node_property"));
	RegisterTGCommand(Registry, TEXT("get_tg_nodes"));
	RegisterTGCommand(Registry, TEXT("get_tg_node_info"));
	RegisterTGCommand(Registry, TEXT("list_tg_node_types"));
	RegisterTGCommand(Registry, TEXT("export_texture_graph"));
	RegisterTGCommand(Registry, TEXT("save_texture_graph"));
	RegisterTGCommand(Registry, TEXT("apply_tg_patch"));
}

TSharedPtr<FJsonObject> FUECLITextureGraphCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("create_texture_graph")) return HandleCreateTextureGraph(Params);
	if (CommandType == TEXT("add_tg_node")) return HandleAddTGNode(Params);
	if (CommandType == TEXT("connect_tg_nodes")) return HandleConnectTGNodes(Params);
	if (CommandType == TEXT("set_tg_node_property")) return HandleSetTGNodeProperty(Params);
	if (CommandType == TEXT("get_tg_nodes")) return HandleGetTGNodes(Params);
	if (CommandType == TEXT("get_tg_node_info")) return HandleGetTGNodeInfo(Params);
	if (CommandType == TEXT("list_tg_node_types")) return HandleListTGNodeTypes(Params);
	if (CommandType == TEXT("export_texture_graph")) return HandleExportTextureGraph(Params);
	if (CommandType == TEXT("save_texture_graph")) return HandleSaveTextureGraph(Params);
	if (CommandType == TEXT("apply_tg_patch")) return HandleApplyTGPatch(Params);

	return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown TextureGraph command: %s"), *CommandType));
}

// ============================================================
// create_texture_graph
// ============================================================
TSharedPtr<FJsonObject> FUECLITextureGraphCommands::HandleCreateTextureGraph(const TSharedPtr<FJsonObject>& Params)
{
	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	FString Path = TEXT("/Game/TextureGraphs");
	Params->TryGetStringField(TEXT("path"), Path);

	const FString FullPath = Path / Name;
	const FString PackagePath = FullPath;

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to create package: %s"), *PackagePath));
	}

	UTextureGraph* NewTG = NewObject<UTextureGraph>(Package, *Name, RF_Standalone | RF_Public);
	if (!NewTG)
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to create TextureGraph object"));
	}

	NewTG->Construct(Name);
	FAssetRegistryModule::AssetCreated(NewTG);
	Package->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), Name);
	Data->SetStringField(TEXT("tg_path"), FullPath);
	return FUECLICommonUtils::CreateSuccessResponse(Data);
}

// ============================================================
// add_tg_node
// ============================================================
TSharedPtr<FJsonObject> FUECLITextureGraphCommands::HandleAddTGNode(const TSharedPtr<FJsonObject>& Params)
{
	FString TGPath;
	if (!TryGetTGPath(Params, TGPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'tg_path' parameter"));
	}

	FString NodeType;
	if (!Params->TryGetStringField(TEXT("node_type"), NodeType) || NodeType.IsEmpty())
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'node_type' parameter"));
	}

	UTextureGraph* TG = LoadTextureGraph(TGPath);
	if (!TG)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("TextureGraph not found: %s"), *TGPath));
	}

	UClass* ExprClass = ResolveExpressionClass(NodeType);
	if (!ExprClass)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown node type: %s. Use list_tg_node_types for available types."), *NodeType));
	}

	UTG_Graph* Graph = TG->Graph();
	if (!Graph)
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("TextureGraph has no internal graph"));
	}

	UTG_Node* NewNode = Graph->CreateExpressionNode(ExprClass);
	if (!NewNode)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to create node of type: %s"), *NodeType));
	}

	TG->GetPackage()->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("tg_path"), TGPath);
	Data->SetStringField(TEXT("node_name"), NewNode->GetNodeName().ToString());
	Data->SetNumberField(TEXT("node_id"), NewNode->GetId().NodeIdx());
	Data->SetStringField(TEXT("node_type"), NodeType);
	Data->SetBoolField(TEXT("success"), true);
	return FUECLICommonUtils::CreateSuccessResponse(Data);
}

// ============================================================
// connect_tg_nodes
// ============================================================
TSharedPtr<FJsonObject> FUECLITextureGraphCommands::HandleConnectTGNodes(const TSharedPtr<FJsonObject>& Params)
{
	FString TGPath;
	if (!TryGetTGPath(Params, TGPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'tg_path' parameter"));
	}

	FString FromNodeName, ToNodeName, FromPinStr, ToPinStr;
	if (!Params->TryGetStringField(TEXT("from_node"), FromNodeName))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'from_node' parameter"));
	}
	if (!Params->TryGetStringField(TEXT("to_node"), ToNodeName))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'to_node' parameter"));
	}
	Params->TryGetStringField(TEXT("from_pin"), FromPinStr);
	Params->TryGetStringField(TEXT("to_pin"), ToPinStr);

	if (FromPinStr.IsEmpty()) FromPinStr = TEXT("Output");
	if (ToPinStr.IsEmpty()) ToPinStr = TEXT("Source");

	UTextureGraph* TG = LoadTextureGraph(TGPath);
	if (!TG) return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("TextureGraph not found: %s"), *TGPath));

	UTG_Graph* Graph = TG->Graph();
	if (!Graph) return FUECLICommonUtils::CreateErrorResponse(TEXT("No internal graph"));

	UTG_Node* FromNode = FindNodeByName(Graph, FromNodeName);
	UTG_Node* ToNode = FindNodeByName(Graph, ToNodeName);
	if (!FromNode) return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Source node not found: %s"), *FromNodeName));
	if (!ToNode) return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Target node not found: %s"), *ToNodeName));

	FName PinFrom(*FromPinStr);
	FName PinTo(*ToPinStr);
	bool bConnected = Graph->Connect(*FromNode, PinFrom, *ToNode, PinTo);

	if (!bConnected)
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Failed to connect - pins may be incompatible or would create a cycle"));
	}

	TG->GetPackage()->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("tg_path"), TGPath);
	Data->SetStringField(TEXT("from_node"), FromNodeName);
	Data->SetStringField(TEXT("from_pin"), FromPinStr);
	Data->SetStringField(TEXT("to_node"), ToNodeName);
	Data->SetStringField(TEXT("to_pin"), ToPinStr);
	Data->SetBoolField(TEXT("success"), true);
	return FUECLICommonUtils::CreateSuccessResponse(Data);
}

// ============================================================
// set_tg_node_property
// ============================================================
TSharedPtr<FJsonObject> FUECLITextureGraphCommands::HandleSetTGNodeProperty(const TSharedPtr<FJsonObject>& Params)
{
	FString TGPath;
	if (!TryGetTGPath(Params, TGPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'tg_path' parameter"));
	}

	FString NodeName, PropertyName;
	if (!Params->TryGetStringField(TEXT("node_name"), NodeName))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'node_name' parameter"));
	}
	if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
	}

	UTextureGraph* TG = LoadTextureGraph(TGPath);
	if (!TG) return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("TextureGraph not found: %s"), *TGPath));

	UTG_Graph* Graph = TG->Graph();
	if (!Graph) return FUECLICommonUtils::CreateErrorResponse(TEXT("No internal graph"));

	UTG_Node* Node = FindNodeByName(Graph, NodeName);
	if (!Node) return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Node not found: %s"), *NodeName));

	UTG_Expression* Expression = Node->GetExpression();
	if (!Expression) return FUECLICommonUtils::CreateErrorResponse(TEXT("Node has no expression"));

	const TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));
	if (!Value.IsValid())
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'value' parameter"));
	}

	FString ErrorMessage;
	if (!FUECLICommonUtils::SetObjectProperty(Expression, PropertyName, Value, ErrorMessage))
	{
		return FUECLICommonUtils::CreateErrorResponse(ErrorMessage);
	}

	TG->GetPackage()->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("tg_path"), TGPath);
	Data->SetStringField(TEXT("node_name"), NodeName);
	Data->SetStringField(TEXT("property_name"), PropertyName);
	return FUECLICommonUtils::CreateSuccessResponse(Data);
}

// ============================================================
// get_tg_nodes
// ============================================================
TSharedPtr<FJsonObject> FUECLITextureGraphCommands::HandleGetTGNodes(const TSharedPtr<FJsonObject>& Params)
{
	FString TGPath;
	if (!TryGetTGPath(Params, TGPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'tg_path' parameter"));
	}

	UTextureGraph* TG = LoadTextureGraph(TGPath);
	if (!TG) return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("TextureGraph not found: %s"), *TGPath));

	UTG_Graph* Graph = TG->Graph();
	if (!Graph) return FUECLICommonUtils::CreateErrorResponse(TEXT("No internal graph"));

	TArray<TSharedPtr<FJsonValue>> NodesArray;
	Graph->ForEachNodes([&](const UTG_Node* Node, uint32 Index)
	{
		if (Node)
		{
			NodesArray.Add(MakeShared<FJsonValueObject>(SerializeNode(Node)));
		}
	});

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("tg_path"), TGPath);
	Data->SetArrayField(TEXT("nodes"), NodesArray);
	Data->SetNumberField(TEXT("count"), NodesArray.Num());
	return FUECLICommonUtils::CreateSuccessResponse(Data);
}

// ============================================================
// get_tg_node_info
// ============================================================
TSharedPtr<FJsonObject> FUECLITextureGraphCommands::HandleGetTGNodeInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString TGPath;
	if (!TryGetTGPath(Params, TGPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'tg_path' parameter"));
	}

	FString NodeName;
	if (!Params->TryGetStringField(TEXT("node_name"), NodeName))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'node_name' parameter"));
	}

	UTextureGraph* TG = LoadTextureGraph(TGPath);
	if (!TG) return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("TextureGraph not found: %s"), *TGPath));

	UTG_Graph* Graph = TG->Graph();
	if (!Graph) return FUECLICommonUtils::CreateErrorResponse(TEXT("No internal graph"));

	UTG_Node* Node = FindNodeByName(Graph, NodeName);
	if (!Node) return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Node not found: %s"), *NodeName));

	TSharedPtr<FJsonObject> Data = SerializeNode(Node);

	// Add expression properties via reflection
	if (UTG_Expression* Expr = Node->GetExpression())
	{
		Data->SetObjectField(TEXT("properties"), FUECLICommonUtils::SerializeObjectProperties(Expr, 2, false));
	}

	return FUECLICommonUtils::CreateSuccessResponse(Data);
}

// ============================================================
// list_tg_node_types
// ============================================================
TSharedPtr<FJsonObject> FUECLITextureGraphCommands::HandleListTGNodeTypes(const TSharedPtr<FJsonObject>& Params)
{
	FString CategoryFilter;
	Params->TryGetStringField(TEXT("category"), CategoryFilter);

	TArray<TSharedPtr<FJsonValue>> TypesArray;
	for (const TPair<FString, UClass*>& Pair : GetExpressionClassMap())
	{
		UTG_Expression* CDO = Cast<UTG_Expression>(Pair.Value->GetDefaultObject());
		if (!CDO) continue;

		FString Category = CDO->GetCategory().ToString();
		if (!CategoryFilter.IsEmpty() && !Category.Equals(CategoryFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}

		TSharedPtr<FJsonObject> TypeObj = MakeShared<FJsonObject>();
		TypeObj->SetStringField(TEXT("type_name"), Pair.Key);
		TypeObj->SetStringField(TEXT("category"), Category);
		TypeObj->SetStringField(TEXT("tooltip"), CDO->GetTooltipText().ToString());
		TypesArray.Add(MakeShared<FJsonValueObject>(TypeObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("node_types"), TypesArray);
	Data->SetNumberField(TEXT("count"), TypesArray.Num());
	return FUECLICommonUtils::CreateSuccessResponse(Data);
}

// ============================================================
// export_texture_graph
// ============================================================
TSharedPtr<FJsonObject> FUECLITextureGraphCommands::HandleExportTextureGraph(const TSharedPtr<FJsonObject>& Params)
{
	FString TGPath;
	if (!TryGetTGPath(Params, TGPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'tg_path' parameter"));
	}

	bool bOverwrite = true;
	bool bSave = true;
	bool bExportAll = false;
	Params->TryGetBoolField(TEXT("overwrite"), bOverwrite);
	Params->TryGetBoolField(TEXT("save"), bSave);
	Params->TryGetBoolField(TEXT("export_all"), bExportAll);

	UTextureGraph* TG = LoadTextureGraph(TGPath);
	if (!TG) return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("TextureGraph not found: %s"), *TGPath));

	// Call RenderTextureGraph via reflection (not exported from TextureGraph module)
	UFunction* RenderFunc = UTG_BlueprintFunctionLibrary::StaticClass()->FindFunctionByName(TEXT("RenderTextureGraph"));
	UFunction* ExportFunc = UTG_BlueprintFunctionLibrary::StaticClass()->FindFunctionByName(TEXT("ExportTextureGraph"));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	int32 RenderTargetsCount = 0;

	if (RenderFunc)
	{
		struct { UObject* WorldContext; UTextureGraphBase* InTG; TArray<UTextureRenderTarget2D*> ReturnValue; } RenderParams;
		RenderParams.WorldContext = World;
		RenderParams.InTG = TG;
		UTG_BlueprintFunctionLibrary::StaticClass()->GetDefaultObject()->ProcessEvent(RenderFunc, &RenderParams);
		RenderTargetsCount = RenderParams.ReturnValue.Num();
	}

	if (ExportFunc)
	{
		struct { UObject* WorldContext; UTextureGraphBase* InTG; bool bOverwrite; bool bSave; bool bExportAll; } ExportParams;
		ExportParams.WorldContext = World;
		ExportParams.InTG = TG;
		ExportParams.bOverwrite = bOverwrite;
		ExportParams.bSave = bSave;
		ExportParams.bExportAll = bExportAll;
		UTG_BlueprintFunctionLibrary::StaticClass()->GetDefaultObject()->ProcessEvent(ExportFunc, &ExportParams);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("tg_path"), TGPath);
	Data->SetNumberField(TEXT("render_targets_count"), RenderTargetsCount);
	Data->SetBoolField(TEXT("exported"), ExportFunc != nullptr);
	return FUECLICommonUtils::CreateSuccessResponse(Data);
}

// ============================================================
// save_texture_graph
// ============================================================
TSharedPtr<FJsonObject> FUECLITextureGraphCommands::HandleSaveTextureGraph(const TSharedPtr<FJsonObject>& Params)
{
	FString TGPath;
	if (!TryGetTGPath(Params, TGPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'tg_path' parameter"));
	}

	UTextureGraph* TG = LoadTextureGraph(TGPath);
	if (!TG) return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("TextureGraph not found: %s"), *TGPath));

	UPackage* Package = TG->GetPackage();
	const FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	bool bSaved = UPackage::SavePackage(Package, TG, *PackageFileName, SaveArgs);

	if (!bSaved)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to save: %s"), *TGPath));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("tg_path"), TGPath);
	Data->SetBoolField(TEXT("saved"), true);
	return FUECLICommonUtils::CreateSuccessResponse(Data);
}

// ============================================================
// apply_tg_patch
// ============================================================
TSharedPtr<FJsonObject> FUECLITextureGraphCommands::HandleApplyTGPatch(const TSharedPtr<FJsonObject>& Params)
{
	FString TGPath;
	if (!TryGetTGPath(Params, TGPath))
	{
		return FUECLICommonUtils::CreateErrorResponse(TEXT("Missing 'tg_path' parameter"));
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

	auto ResolveAlias = [&Aliases](const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
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

	for (int32 i = 0; i < Operations->Num(); ++i)
	{
		const TSharedPtr<FJsonObject> Operation = (*Operations)[i]->AsObject();
		if (!Operation.IsValid())
		{
			if (!bContinueOnError) return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Operation %d is not an object"), i));
			continue;
		}

		FString Action;
		if (!Operation->TryGetStringField(TEXT("action"), Action))
		{
			if (!bContinueOnError) return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Operation %d missing 'action'"), i));
			continue;
		}

		// Build OpParams: copy all fields except action, inject tg_path
		TSharedPtr<FJsonObject> OpParams = MakeShared<FJsonObject>();
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Operation->Values)
		{
			if (Pair.Key != TEXT("action"))
			{
				OpParams->SetField(Pair.Key, Pair.Value);
			}
		}
		OpParams->SetStringField(TEXT("tg_path"), TGPath);

		// Resolve aliases
		ResolveAlias(OpParams, TEXT("node_name"));
		ResolveAlias(OpParams, TEXT("from_node"));
		ResolveAlias(OpParams, TEXT("to_node"));

		TSharedPtr<FJsonObject> OpResult;
		if (Action == TEXT("create_node"))
		{
			OpResult = HandleAddTGNode(OpParams);
			// Store alias if provided
			if (OpResult.IsValid() && OpResult->GetBoolField(TEXT("success")))
			{
				const TSharedPtr<FJsonObject>* DataObj = nullptr;
				if (OpResult->TryGetObjectField(TEXT("data"), DataObj) && DataObj && DataObj->IsValid())
				{
					FString Alias;
					if (OpParams->TryGetStringField(TEXT("alias"), Alias) && !Alias.IsEmpty())
					{
						FString NodeName;
						if ((*DataObj)->TryGetStringField(TEXT("node_name"), NodeName))
						{
							Aliases.Add(Alias, NodeName);
						}
					}
				}
			}
		}
		else if (Action == TEXT("connect_nodes"))
		{
			OpResult = HandleConnectTGNodes(OpParams);
		}
		else if (Action == TEXT("set_node_property"))
		{
			OpResult = HandleSetTGNodeProperty(OpParams);
		}
		else if (Action == TEXT("export"))
		{
			OpResult = HandleExportTextureGraph(OpParams);
		}
		else if (Action == TEXT("save"))
		{
			OpResult = HandleSaveTextureGraph(OpParams);
		}
		else if (Action == TEXT("remove_node"))
		{
			FString NodeName;
			if (OpParams->TryGetStringField(TEXT("node_name"), NodeName))
			{
				UTextureGraph* TG = LoadTextureGraph(TGPath);
				if (TG && TG->Graph())
				{
					UTG_Node* Node = FindNodeByName(TG->Graph(), NodeName);
					if (Node)
					{
						TG->Graph()->RemoveNode(Node);
						TSharedPtr<FJsonObject> RemoveData = MakeShared<FJsonObject>();
						RemoveData->SetBoolField(TEXT("success"), true);
						OpResult = FUECLICommonUtils::CreateSuccessResponse(RemoveData);
					}
					else
					{
						OpResult = FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Node not found: %s"), *NodeName));
					}
				}
			}
		}
		else
		{
			OpResult = FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown action: %s"), *Action));
		}

		bool bOpSuccess = OpResult.IsValid() && OpResult->GetBoolField(TEXT("success"));
		TSharedPtr<FJsonObject> ResultEntry = MakeShared<FJsonObject>();
		ResultEntry->SetNumberField(TEXT("operation_index"), i);
		ResultEntry->SetStringField(TEXT("action"), Action);
		ResultEntry->SetBoolField(TEXT("success"), bOpSuccess);
		if (OpResult.IsValid())
		{
			const TSharedPtr<FJsonObject>* DataObj = nullptr;
			if (OpResult->TryGetObjectField(TEXT("data"), DataObj) && DataObj && DataObj->IsValid())
			{
				ResultEntry->SetObjectField(TEXT("result"), *DataObj);
			}
			FString Error;
			if (OpResult->TryGetStringField(TEXT("error"), Error))
			{
				ResultEntry->SetStringField(TEXT("error"), Error);
			}
		}
		OperationResults.Add(MakeShared<FJsonValueObject>(ResultEntry));

		if (bOpSuccess) AppliedCount++;
		else if (!bContinueOnError)
		{
			break;
		}
	}

	// Serialize aliases
	TSharedPtr<FJsonObject> AliasesObj = MakeShared<FJsonObject>();
	for (const TPair<FString, FString>& Pair : Aliases)
	{
		AliasesObj->SetStringField(Pair.Key, Pair.Value);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("tg_path"), TGPath);
	Data->SetArrayField(TEXT("results"), OperationResults);
	Data->SetNumberField(TEXT("applied_count"), AppliedCount);
	Data->SetNumberField(TEXT("operation_count"), Operations->Num());
	Data->SetObjectField(TEXT("aliases"), AliasesObj);
	return FUECLICommonUtils::CreateSuccessResponse(Data);
}
