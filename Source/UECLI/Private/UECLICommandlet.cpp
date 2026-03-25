#include "UECLICommandlet.h"

#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Server/UECLIServer.h"
#include "ToolRegistry/UECLIToolRegistry.h"

namespace UECLICommandletPrivate
{
	FString SerializeJson(const TSharedPtr<FJsonObject>& JsonObject)
	{
		FString Output;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
		return Output;
	}

	TSharedPtr<FJsonObject> ParseParamsJson(const FString& RawParams)
	{
		TSharedPtr<FJsonObject> ParamsObject = MakeShared<FJsonObject>();
		if (RawParams.IsEmpty())
		{
			return ParamsObject;
		}

		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawParams);
		if (FJsonSerializer::Deserialize(Reader, ParamsObject) && ParamsObject.IsValid())
		{
			return ParamsObject;
		}
		return MakeShared<FJsonObject>();
	}

	void GatherKeyValueParams(const FString& RawCommandLine, const FUECLIToolSchema* Schema, TSharedPtr<FJsonObject>& ParamsObject)
	{
		TArray<FString> Tokens;
		TArray<FString> Switches;
		TMap<FString, FString> ParamVals;
		UCommandlet::ParseCommandLine(*RawCommandLine, Tokens, Switches, ParamVals);

		for (const TPair<FString, FString>& Pair : ParamVals)
		{
			if (Pair.Key.Equals(TEXT("command"), ESearchCase::IgnoreCase) ||
				Pair.Key.Equals(TEXT("params"), ESearchCase::IgnoreCase) ||
				Pair.Key.Equals(TEXT("file"), ESearchCase::IgnoreCase) ||
				Pair.Key.Equals(TEXT("format"), ESearchCase::IgnoreCase) ||
				Pair.Key.Equals(TEXT("category"), ESearchCase::IgnoreCase))
			{
				continue;
			}

			const FUECLIToolParam* ToolParam = nullptr;
			if (Schema != nullptr)
			{
				for (const FUECLIToolParam& Candidate : Schema->Params)
				{
					if (Candidate.Name.Equals(Pair.Key, ESearchCase::IgnoreCase))
					{
						ToolParam = &Candidate;
						break;
					}
				}
			}

			if (ToolParam != nullptr && ToolParam->Type.Equals(TEXT("number"), ESearchCase::IgnoreCase))
			{
				ParamsObject->SetNumberField(Pair.Key, FCString::Atod(*Pair.Value));
			}
			else if (ToolParam != nullptr && ToolParam->Type.Equals(TEXT("boolean"), ESearchCase::IgnoreCase))
			{
				ParamsObject->SetBoolField(Pair.Key, Pair.Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Pair.Value == TEXT("1"));
			}
			else
			{
				ParamsObject->SetStringField(Pair.Key, Pair.Value);
			}
		}
	}
}

UUECLICommandlet::UUECLICommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UUECLICommandlet::Main(const FString& Params)
{
	FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("UECLI"));
	UUECLIServer::EnsureToolsRegistered();

	const FUECLIToolRegistry& Registry = FUECLIToolRegistry::Get();
	const bool bHelp = FParse::Param(*Params, TEXT("help"));
	const bool bJsonMode = FParse::Param(*Params, TEXT("json"));
	const bool bBatchMode = FParse::Param(*Params, TEXT("batch"));

	FString Category;
	FParse::Value(*Params, TEXT("category="), Category);

	FString Format;
	FParse::Value(*Params, TEXT("format="), Format);
	if (!Format.IsEmpty())
	{
		const FString Output = Format.Equals(TEXT("markdown"), ESearchCase::IgnoreCase)
			? Registry.ExportAllAsSkillMarkdown()
			: Registry.ExportAllAsJsonSchema();
		UE_LOG(LogTemp, Display, TEXT("%s"), *Output);
		return 0;
	}

	if (bHelp)
	{
		UE_LOG(LogTemp, Display, TEXT("%s"), *Registry.ExportCliHelp(Category));
		return 0;
	}

	if (bBatchMode)
	{
		FString BatchFile;
		if (!FParse::Value(*Params, TEXT("file="), BatchFile) || BatchFile.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("Missing -file for batch mode"));
			return 1;
		}

		FString BatchText;
		if (!FFileHelper::LoadFileToString(BatchText, *BatchFile))
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to load batch file: %s"), *BatchFile);
			return 1;
		}

		TArray<TSharedPtr<FJsonValue>> Commands;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BatchText);
		if (!FJsonSerializer::Deserialize(Reader, Commands))
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to parse batch JSON: %s"), *BatchFile);
			return 1;
		}

		TArray<TSharedPtr<FJsonValue>> Results;
		for (const TSharedPtr<FJsonValue>& CommandValue : Commands)
		{
			const TSharedPtr<FJsonObject> CommandObject = CommandValue.IsValid() ? CommandValue->AsObject() : nullptr;
			if (!CommandObject.IsValid())
			{
				continue;
			}

			FString CommandName;
			if (!CommandObject->TryGetStringField(TEXT("command"), CommandName))
			{
				continue;
			}

			TSharedPtr<FJsonObject> CommandParams = MakeShared<FJsonObject>();
			const TSharedPtr<FJsonObject>* JsonParams = nullptr;
			if (CommandObject->TryGetObjectField(TEXT("params"), JsonParams) && JsonParams != nullptr && JsonParams->IsValid())
			{
				CommandParams = *JsonParams;
			}

			Results.Add(MakeShared<FJsonValueObject>(Registry.DispatchCommand(CommandName, CommandParams)));
		}

		TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetArrayField(TEXT("results"), Results);
		const FString Output = UECLICommandletPrivate::SerializeJson(Root);
		if (bJsonMode)
		{
			UE_LOG(LogTemp, Display, TEXT("JSON_BEGIN\n%s\nJSON_END"), *Output);
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("%s"), *Output);
		}
		return 0;
	}

	FString CommandName;
	if (!FParse::Value(*Params, TEXT("command="), CommandName) || CommandName.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Missing -command. Use -help to inspect available commands."));
		return 1;
	}

	FString RawParams;
	FParse::Value(*Params, TEXT("params="), RawParams);

	TSharedPtr<FJsonObject> CommandParams = UECLICommandletPrivate::ParseParamsJson(RawParams);
	UECLICommandletPrivate::GatherKeyValueParams(Params, Registry.Find(CommandName), CommandParams);

	const TSharedPtr<FJsonObject> Result = Registry.DispatchCommand(CommandName, CommandParams);
	const FString Output = UECLICommandletPrivate::SerializeJson(Result);
	if (bJsonMode)
	{
		UE_LOG(LogTemp, Display, TEXT("JSON_BEGIN\n%s\nJSON_END"), *Output);
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("%s"), *Output);
	}

	return Result->GetStringField(TEXT("status")) == TEXT("success") ? 0 : 1;
}
