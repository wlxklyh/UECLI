#include "Server/UECLIServer.h"

#include "Async/Async.h"
#include "Async/Future.h"
#include "Commands/UECLIAssetCommands.h"
#include "Commands/UECLICommonUtils.h"
#include "Commands/UECLIEditorCommands.h"
#include "Commands/UECLIMaterialCommands.h"
#include "Commands/UECLIProjectCommands.h"
#include "Commands/UECLITextureGraphCommands.h"
#include "HAL/RunnableThread.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "Server/UECLIServerRunnable.h"
#include "ToolRegistry/UECLIToolRegistry.h"

namespace UECLI::Private
{
	bool bToolsRegistered = false;

	TSharedPtr<FJsonObject> HandlePing(const TSharedPtr<FJsonObject>&)
	{
		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("message"), TEXT("pong"));
		return FUECLICommonUtils::CreateSuccessResponse(Data);
	}

	TSharedPtr<FJsonObject> HandleListTools(const TSharedPtr<FJsonObject>&)
	{
		TArray<TSharedPtr<FJsonValue>> Tools;
		for (const FUECLIToolSchema& Schema : FUECLIToolRegistry::Get().GetAll())
		{
			Tools.Add(MakeShared<FJsonValueObject>(Schema.ToJsonSchema()));
		}

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetArrayField(TEXT("tools"), Tools);
		return FUECLICommonUtils::CreateSuccessResponse(Data);
	}
}

UUECLIServer::UUECLIServer()
	: bIsRunning(false)
	, ServerThread(nullptr)
	, Port(31111)
{
	FIPv4Address::Parse(TEXT("127.0.0.1"), ServerAddress);
}

UUECLIServer::~UUECLIServer()
{
}

void UUECLIServer::EnsureToolsRegistered()
{
	using namespace UECLI::Private;

	if (bToolsRegistered)
	{
		return;
	}

	FUECLIToolRegistry& Registry = FUECLIToolRegistry::Get();
	Registry.Reset();

	Registry.Register(FUECLIToolSchema(TEXT("ping"), TEXT("Core"), TEXT("Health check")), &HandlePing);
	Registry.Register(FUECLIToolSchema(TEXT("list_tools"), TEXT("Core"), TEXT("List registered tool schemas")), &HandleListTools);

	FUECLIEditorCommands::RegisterTools(Registry);
	FUECLIAssetCommands::RegisterTools(Registry);
	FUECLIProjectCommands::RegisterTools(Registry);
	FUECLIMaterialCommands::RegisterTools(Registry);
	FUECLITextureGraphCommands::RegisterTools(Registry);

	bToolsRegistered = true;
}

void UUECLIServer::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	EnsureToolsRegistered();

	int32 OverridePort = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("uecliport="), OverridePort) && OverridePort > 0 && OverridePort <= MAX_uint16)
	{
		Port = static_cast<uint16>(OverridePort);
	}

	StartServer();
}

void UUECLIServer::Deinitialize()
{
	StopServer();
	Super::Deinitialize();
}

void UUECLIServer::StartServer()
{
	if (bIsRunning)
	{
		return;
	}

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (SocketSubsystem == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("UECLI: failed to acquire socket subsystem"));
		return;
	}

	FSocket* RawListenerSocket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("UECLIListener"), false);
	TSharedPtr<FSocket> NewListenerSocket;
	if (RawListenerSocket != nullptr)
	{
		NewListenerSocket = MakeShareable(RawListenerSocket, ::FSocketDeleter(SocketSubsystem));
	}
	if (!NewListenerSocket.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("UECLI: failed to create listener socket"));
		return;
	}

	NewListenerSocket->SetReuseAddr(true);
	NewListenerSocket->SetNonBlocking(true);

	const FIPv4Endpoint Endpoint(ServerAddress, Port);
	if (!NewListenerSocket->Bind(*Endpoint.ToInternetAddr()) || !NewListenerSocket->Listen(5))
	{
		UE_LOG(LogTemp, Error, TEXT("UECLI: failed to bind/listen on %s:%d"), *ServerAddress.ToString(), Port);
		NewListenerSocket.Reset();
		return;
	}

	ListenerSocket = NewListenerSocket;
	bIsRunning = true;
	ServerThread = FRunnableThread::Create(new FUECLIServerRunnable(this, ListenerSocket), TEXT("UECLIServerThread"));
	if (ServerThread == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("UECLI: failed to create server thread"));
		StopServer();
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("UECLI server listening on %s:%d"), *ServerAddress.ToString(), Port);
}

void UUECLIServer::StopServer()
{
	if (!bIsRunning)
	{
		return;
	}

	bIsRunning = false;

	if (ServerThread != nullptr)
	{
		ServerThread->Kill(true);
		delete ServerThread;
		ServerThread = nullptr;
	}

	if (ListenerSocket.IsValid())
	{
		ListenerSocket.Reset();
	}
}

TSharedPtr<FJsonObject> UUECLIServer::DispatchCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	EnsureToolsRegistered();
	return FUECLIToolRegistry::Get().DispatchCommand(CommandType, Params.IsValid() ? Params : MakeShared<FJsonObject>());
}

FString UUECLIServer::SerializeJson(const TSharedPtr<FJsonObject>& JsonObject)
{
	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	return Output;
}

FString UUECLIServer::HandleAsyncExecute(const TSharedPtr<FJsonObject>& Params)
{
	FString InnerCommandType;
	if (!Params.IsValid() || !Params->TryGetStringField(TEXT("command"), InnerCommandType))
	{
		TSharedPtr<FJsonObject> ErrorResponse = MakeShared<FJsonObject>();
		ErrorResponse->SetStringField(TEXT("status"), TEXT("error"));
		ErrorResponse->SetStringField(TEXT("error"), TEXT("Missing 'command' field in async_execute params"));
		return SerializeJson(ErrorResponse);
	}

	TSharedPtr<FJsonObject> InnerParams = MakeShared<FJsonObject>();
	const TSharedPtr<FJsonObject>* InnerParamsPtr = nullptr;
	if (Params->TryGetObjectField(TEXT("params"), InnerParamsPtr) && InnerParamsPtr != nullptr && InnerParamsPtr->IsValid())
	{
		InnerParams = *InnerParamsPtr;
	}

	const FString TaskId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	AsyncTask(ENamedThreads::GameThread, [this, TaskId, InnerCommandType, InnerParams]()
	{
		const FString ResultString = SerializeJson(DispatchCommand(InnerCommandType, InnerParams));
		FScopeLock Lock(&AsyncTaskLock);
		AsyncTaskResults.Add(TaskId, ResultString);
	});

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("status"), TEXT("success"));
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("task_id"), TaskId);
	Result->SetObjectField(TEXT("data"), Data);
	return SerializeJson(Result);
}

FString UUECLIServer::HandleGetTaskResult(const TSharedPtr<FJsonObject>& Params)
{
	FString TaskId;
	if (!Params.IsValid() || !Params->TryGetStringField(TEXT("task_id"), TaskId))
	{
		TSharedPtr<FJsonObject> ErrorResponse = MakeShared<FJsonObject>();
		ErrorResponse->SetStringField(TEXT("status"), TEXT("error"));
		ErrorResponse->SetStringField(TEXT("error"), TEXT("Missing 'task_id' field"));
		return SerializeJson(ErrorResponse);
	}

	{
		FScopeLock Lock(&AsyncTaskLock);
		if (FString* FoundResult = AsyncTaskResults.Find(TaskId))
		{
			const FString CompletedResult = *FoundResult;
			AsyncTaskResults.Remove(TaskId);
			return CompletedResult;
		}
	}

	TSharedPtr<FJsonObject> Pending = MakeShared<FJsonObject>();
	Pending->SetStringField(TEXT("status"), TEXT("pending"));
	Pending->SetStringField(TEXT("task_id"), TaskId);
	return SerializeJson(Pending);
}

FString UUECLIServer::ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("async_execute"))
	{
		return HandleAsyncExecute(Params);
	}
	if (CommandType == TEXT("get_task_result"))
	{
		return HandleGetTaskResult(Params);
	}

	TPromise<FString> Promise;
	TFuture<FString> Future = Promise.GetFuture();
	AsyncTask(ENamedThreads::GameThread, [this, CommandType, Params, Promise = MoveTemp(Promise)]() mutable
	{
		Promise.SetValue(SerializeJson(DispatchCommand(CommandType, Params)));
	});
	return Future.Get();
}
