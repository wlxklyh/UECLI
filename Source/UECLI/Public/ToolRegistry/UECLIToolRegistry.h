#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "ToolRegistry/UECLIToolSchema.h"

using FUECLICommandHandler = TFunction<TSharedPtr<FJsonObject>(const TSharedPtr<FJsonObject>&)>;

class UECLI_API FUECLIToolRegistry
{
public:
	static FUECLIToolRegistry& Get();

	void Register(const FUECLIToolSchema& Schema, FUECLICommandHandler Handler);
	const FUECLIToolSchema* Find(const FString& Name) const;
	TSharedPtr<FJsonObject> DispatchCommand(const FString& Name, const TSharedPtr<FJsonObject>& Params) const;

	TArray<FUECLIToolSchema> GetAll() const;
	TArray<FUECLIToolSchema> GetByCategory(const FString& Category) const;
	TArray<FString> GetCategories() const;

	FString ExportAllAsJsonSchema() const;
	FString ExportCliHelp(const FString& Filter = FString()) const;
	FString ExportAllAsSkillMarkdown() const;

	void Reset();

private:
	TMap<FString, FUECLIToolSchema> Tools;
	TMap<FString, FUECLICommandHandler> Handlers;
};
