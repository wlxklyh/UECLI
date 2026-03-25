#include "Server/UECLIServerRunnable.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Server/UECLIServer.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

namespace
{
	constexpr int32 ReceiveBufferSize = 64 * 1024;

	FString SerializeJson(const TSharedPtr<FJsonObject>& JsonObject)
	{
		FString Output;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
		return Output;
	}
}

FUECLIServerRunnable::FUECLIServerRunnable(UUECLIServer* InServer, TSharedPtr<FSocket> InListenerSocket)
	: Server(InServer)
	, ListenerSocket(InListenerSocket)
	, bRunning(true)
{
}

FUECLIServerRunnable::~FUECLIServerRunnable()
{
}

bool FUECLIServerRunnable::Init()
{
	return ListenerSocket.IsValid() && Server != nullptr;
}

uint32 FUECLIServerRunnable::Run()
{
	while (bRunning)
	{
		bool bPendingConnection = false;
		if (!ListenerSocket.IsValid() || !ListenerSocket->HasPendingConnection(bPendingConnection) || !bPendingConnection)
		{
			FPlatformProcess::Sleep(0.05f);
			continue;
		}

		FSocket* RawClientSocket = ListenerSocket->Accept(TEXT("UECLIClient"));
		ClientSocket.Reset();
		if (RawClientSocket != nullptr)
		{
			ClientSocket = MakeShareable(RawClientSocket, ::FSocketDeleter(ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)));
		}
		if (!ClientSocket.IsValid())
		{
			FPlatformProcess::Sleep(0.05f);
			continue;
		}

		ClientSocket->SetNoDelay(true);

		TArray<uint8> Buffer;
		Buffer.SetNumUninitialized(ReceiveBufferSize);
		FString MessageBuffer;

		while (bRunning && ClientSocket.IsValid())
		{
			int32 BytesRead = 0;
			if (!ClientSocket->Recv(Buffer.GetData(), Buffer.Num() - 1, BytesRead))
			{
				const ESocketErrors LastError = ISocketSubsystem::Get()->GetLastErrorCode();
				if (LastError == SE_EWOULDBLOCK || LastError == SE_EINTR)
				{
					FPlatformProcess::Sleep(0.01f);
					continue;
				}
				break;
			}

			if (BytesRead <= 0)
			{
				break;
			}

			Buffer[BytesRead] = '\0';
			MessageBuffer.Append(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Buffer.GetData())));

			TSharedPtr<FJsonObject> RequestObject = MakeShared<FJsonObject>();
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(MessageBuffer);
			if (!FJsonSerializer::Deserialize(Reader, RequestObject) || !RequestObject.IsValid())
			{
				continue;
			}

			FString CommandType;
			if (!RequestObject->TryGetStringField(TEXT("command"), CommandType))
			{
				RequestObject->TryGetStringField(TEXT("type"), CommandType);
			}

			TSharedPtr<FJsonObject> ParamsObject = MakeShared<FJsonObject>();
			const TSharedPtr<FJsonObject>* ParamsField = nullptr;
			if (RequestObject->TryGetObjectField(TEXT("params"), ParamsField) && ParamsField != nullptr && ParamsField->IsValid())
			{
				ParamsObject = *ParamsField;
			}

			TSharedPtr<FJsonObject> ErrorResponse = nullptr;
			FString Response;
			if (CommandType.IsEmpty())
			{
				ErrorResponse = MakeShared<FJsonObject>();
				ErrorResponse->SetStringField(TEXT("status"), TEXT("error"));
				ErrorResponse->SetStringField(TEXT("error"), TEXT("Missing 'command' field"));
				Response = SerializeJson(ErrorResponse);
			}
			else
			{
				Response = Server->ExecuteCommand(CommandType, ParamsObject);
			}

			FTCHARToUTF8 Utf8Response(*Response);
			int32 TotalSent = 0;
			while (TotalSent < Utf8Response.Length())
			{
				int32 BytesSent = 0;
				if (!ClientSocket->Send(reinterpret_cast<const uint8*>(Utf8Response.Get()) + TotalSent, Utf8Response.Length() - TotalSent, BytesSent))
				{
					break;
				}
				TotalSent += BytesSent;
			}

			MessageBuffer.Reset();
		}

		ClientSocket.Reset();
	}

	return 0;
}

void FUECLIServerRunnable::Stop()
{
	bRunning = false;
}

void FUECLIServerRunnable::Exit()
{
}
