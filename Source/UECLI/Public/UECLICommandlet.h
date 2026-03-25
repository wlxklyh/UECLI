#pragma once

#include "Commandlets/Commandlet.h"
#include "UECLICommandlet.generated.h"

UCLASS()
class UECLI_API UUECLICommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UUECLICommandlet();

	virtual int32 Main(const FString& Params) override;
};
