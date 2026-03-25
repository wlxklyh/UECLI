#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

struct FUECLIToolParam
{
	FString Name;
	FString Type;
	FString Description;
	bool bRequired = false;
	FString DefaultValue;
	FString ItemsType;

	FUECLIToolParam() = default;

	FUECLIToolParam(
		FString InName,
		FString InType,
		FString InDescription,
		bool bInRequired = false,
		FString InDefaultValue = FString(),
		FString InItemsType = FString())
		: Name(MoveTemp(InName))
		, Type(MoveTemp(InType))
		, Description(MoveTemp(InDescription))
		, bRequired(bInRequired)
		, DefaultValue(MoveTemp(InDefaultValue))
		, ItemsType(MoveTemp(InItemsType))
	{
	}

	TSharedPtr<FJsonObject> ToJsonSchema() const
	{
		TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("name"), Name);
		Json->SetStringField(TEXT("type"), Type);
		Json->SetStringField(TEXT("description"), Description);
		Json->SetBoolField(TEXT("required"), bRequired);
		if (!DefaultValue.IsEmpty())
		{
			Json->SetStringField(TEXT("default"), DefaultValue);
		}
		if (!ItemsType.IsEmpty())
		{
			Json->SetStringField(TEXT("itemsType"), ItemsType);
		}
		return Json;
	}

	FString ToCliHelp() const
	{
		const FString RequiredText = bRequired ? TEXT("required") : TEXT("optional");
		const FString DefaultText = DefaultValue.IsEmpty() ? FString() : FString::Printf(TEXT(", default=%s"), *DefaultValue);
		return FString::Printf(TEXT("  -%s <%s> [%s%s] %s"), *Name, *Type, *RequiredText, *DefaultText, *Description);
	}
};

struct FUECLIToolSchema
{
	FString Name;
	FString Category;
	FString Description;
	TArray<FUECLIToolParam> Params;

	FUECLIToolSchema() = default;

	FUECLIToolSchema(FString InName, FString InCategory, FString InDescription, TArray<FUECLIToolParam> InParams = {})
		: Name(MoveTemp(InName))
		, Category(MoveTemp(InCategory))
		, Description(MoveTemp(InDescription))
		, Params(MoveTemp(InParams))
	{
	}

	TSharedPtr<FJsonObject> ToJsonSchema() const
	{
		TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("name"), Name);
		Json->SetStringField(TEXT("category"), Category);
		Json->SetStringField(TEXT("description"), Description);

		TArray<TSharedPtr<FJsonValue>> ParamArray;
		for (const FUECLIToolParam& Param : Params)
		{
			ParamArray.Add(MakeShared<FJsonValueObject>(Param.ToJsonSchema()));
		}
		Json->SetArrayField(TEXT("params"), ParamArray);
		return Json;
	}

	FString ToCliHelp() const
	{
		TArray<FString> Lines;
		Lines.Add(FString::Printf(TEXT("%s [%s]"), *Name, *Category));
		if (!Description.IsEmpty())
		{
			Lines.Add(FString::Printf(TEXT("  %s"), *Description));
		}
		for (const FUECLIToolParam& Param : Params)
		{
			Lines.Add(Param.ToCliHelp());
		}
		return FString::Join(Lines, TEXT("\n"));
	}

	FString ToMarkdownTable() const
	{
		const FString Summary = Description.IsEmpty() ? TEXT("-") : Description;
		return FString::Printf(TEXT("| `%s` | `%s` | %s |"), *Name, *Category, *Summary);
	}
};
