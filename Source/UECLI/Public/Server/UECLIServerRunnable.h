#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Sockets.h"
#include "Interfaces/IPv4/IPv4Address.h"

class UUECLIServer;

/**
 * Runnable for the UECLI TCP listener.
 */
class FUECLIServerRunnable : public FRunnable
{
public:
	FUECLIServerRunnable(UUECLIServer* InServer, TSharedPtr<FSocket> InListenerSocket);
	virtual ~FUECLIServerRunnable();

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

private:
	UUECLIServer* Server;
	TSharedPtr<FSocket> ListenerSocket;
	TSharedPtr<FSocket> ClientSocket;
	bool bRunning;
}; 
