#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Dom/JsonObject.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "UECLIServer.generated.h"

class FSocket;
class FUECLIServerRunnable;
class FRunnableThread;

/**
 * TCP server subsystem for UECLI.
 */
UCLASS()
class UECLI_API UUECLIServer : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	UUECLIServer();
	virtual ~UUECLIServer();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void StartServer();
	void StopServer();
	bool IsRunning() const { return bIsRunning; }

	static void EnsureToolsRegistered();

	FString ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	TSharedPtr<FJsonObject> DispatchCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);
	static FString SerializeJson(const TSharedPtr<FJsonObject>& JsonObject);
	FString HandleAsyncExecute(const TSharedPtr<FJsonObject>& Params);
	FString HandleGetTaskResult(const TSharedPtr<FJsonObject>& Params);

	FCriticalSection AsyncTaskLock;
	TMap<FString, FString> AsyncTaskResults;

	bool bIsRunning;
	TSharedPtr<FSocket> ListenerSocket;
	FRunnableThread* ServerThread;

	FIPv4Address ServerAddress;
	uint16 Port;
}; 
