#include "ToolRegistry/UECLIToolRegistry.h"

#include "Commands/UECLICommonUtils.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FUECLIToolRegistry& FUECLIToolRegistry::Get()
{
	static FUECLIToolRegistry Instance;
	return Instance;
}

void FUECLIToolRegistry::Register(const FUECLIToolSchema& Schema, FUECLICommandHandler Handler)
{
	Tools.Add(Schema.Name, Schema);
	Handlers.Add(Schema.Name, MoveTemp(Handler));
}

const FUECLIToolSchema* FUECLIToolRegistry::Find(const FString& Name) const
{
	return Tools.Find(Name);
}

TSharedPtr<FJsonObject> FUECLIToolRegistry::DispatchCommand(const FString& Name, const TSharedPtr<FJsonObject>& Params) const
{
	const FUECLICommandHandler* Handler = Handlers.Find(Name);
	if (Handler == nullptr)
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown command: %s"), *Name));
	}

	TSharedPtr<FJsonObject> RawResult = (*Handler)(Params);
	if (!RawResult.IsValid())
	{
		return FUECLICommonUtils::CreateErrorResponse(FString::Printf(TEXT("Command returned no result: %s"), *Name));
	}

	const bool bSuccess = !RawResult->HasField(TEXT("success")) || RawResult->GetBoolField(TEXT("success"));
	if (!bSuccess)
	{
		TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
		Error->SetStringField(TEXT("status"), TEXT("error"));
		Error->SetStringField(TEXT("error"), RawResult->GetStringField(TEXT("error")));
		return Error;
	}

	TSharedPtr<FJsonObject> Wrapped = MakeShared<FJsonObject>();
	Wrapped->SetStringField(TEXT("status"), TEXT("success"));
	const TSharedPtr<FJsonObject>* Data = nullptr;
	if (RawResult->TryGetObjectField(TEXT("data"), Data) && Data != nullptr && Data->IsValid())
	{
		Wrapped->SetObjectField(TEXT("data"), *Data);
	}
	else
	{
		TSharedPtr<FJsonObject> DataObject = MakeShared<FJsonObject>();
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : RawResult->Values)
		{
			if (Pair.Key != TEXT("success"))
			{
				DataObject->SetField(Pair.Key, Pair.Value);
			}
		}
		Wrapped->SetObjectField(TEXT("data"), DataObject);
	}
	return Wrapped;
}

TArray<FUECLIToolSchema> FUECLIToolRegistry::GetAll() const
{
	TArray<FUECLIToolSchema> Result;
	Tools.GenerateValueArray(Result);
	Result.Sort([](const FUECLIToolSchema& A, const FUECLIToolSchema& B) { return A.Name < B.Name; });
	return Result;
}

TArray<FUECLIToolSchema> FUECLIToolRegistry::GetByCategory(const FString& Category) const
{
	TArray<FUECLIToolSchema> Result;
	for (const TPair<FString, FUECLIToolSchema>& Pair : Tools)
	{
		if (Pair.Value.Category.Equals(Category, ESearchCase::IgnoreCase))
		{
			Result.Add(Pair.Value);
		}
	}
	Result.Sort([](const FUECLIToolSchema& A, const FUECLIToolSchema& B) { return A.Name < B.Name; });
	return Result;
}

TArray<FString> FUECLIToolRegistry::GetCategories() const
{
	TSet<FString> UniqueCategories;
	for (const TPair<FString, FUECLIToolSchema>& Pair : Tools)
	{
		UniqueCategories.Add(Pair.Value.Category);
	}

	TArray<FString> Categories = UniqueCategories.Array();
	Categories.Sort();
	return Categories;
}

FString FUECLIToolRegistry::ExportAllAsJsonSchema() const
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> ToolArray;
	for (const FUECLIToolSchema& Schema : GetAll())
	{
		ToolArray.Add(MakeShared<FJsonValueObject>(Schema.ToJsonSchema()));
	}
	Root->SetArrayField(TEXT("tools"), ToolArray);

	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return Output;
}

FString FUECLIToolRegistry::ExportCliHelp(const FString& Filter) const
{
	TArray<FString> Lines;
	Lines.Add(TEXT("UECLI"));
	Lines.Add(TEXT("  -help"));
	Lines.Add(TEXT("  -command=<name>"));
	Lines.Add(TEXT("  -category=<name>"));
	Lines.Add(TEXT("  -params=\"{...}\""));
	Lines.Add(TEXT("  -batch -file=<path>"));
	Lines.Add(TEXT("  -format=json|markdown"));

	const bool bHasFilter = !Filter.IsEmpty();
	const TArray<FUECLIToolSchema> Schemas = bHasFilter ? GetByCategory(Filter) : GetAll();
	if (Schemas.Num() > 0)
	{
		Lines.Add(TEXT(""));
		Lines.Add(TEXT("Commands:"));
		for (const FUECLIToolSchema& Schema : Schemas)
		{
			Lines.Add(Schema.ToCliHelp());
		}
	}
	return FString::Join(Lines, TEXT("\n"));
}

FString FUECLIToolRegistry::ExportAllAsSkillMarkdown() const
{
	TArray<FString> Lines;
	Lines.Add(TEXT("| Command | Category | Description |"));
	Lines.Add(TEXT("|---|---|---|"));
	for (const FUECLIToolSchema& Schema : GetAll())
	{
		Lines.Add(Schema.ToMarkdownTable());
	}
	return FString::Join(Lines, TEXT("\n"));
}

void FUECLIToolRegistry::Reset()
{
	Tools.Reset();
	Handlers.Reset();
}
