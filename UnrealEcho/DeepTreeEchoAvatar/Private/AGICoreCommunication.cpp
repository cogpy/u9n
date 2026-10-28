#include "AGICoreCommunication.h"
#include "AGIPCGManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogAGICoreComm, Log, All);

AAGICoreCommunication::AAGICoreCommunication()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f; // only watches for a stale connection
	PCGManager = nullptr;
	ConnectionTimeoutSeconds = 5.0f;
	bConnected = false;
	ConnectionStatus = TEXT("Disconnected: no AGI core transport implemented; waiting for ReceiveStateUpdate");
	LastUpdateTimeSeconds = 0.0;
}

void AAGICoreCommunication::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogAGICoreComm, Warning, TEXT("%s"), *ConnectionStatus);
}

void AAGICoreCommunication::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bConnected && FPlatformTime::Seconds() - LastUpdateTimeSeconds > ConnectionTimeoutSeconds)
	{
		SetConnected(false, FString::Printf(TEXT("Disconnected: no AGI core update for %.1f s"), ConnectionTimeoutSeconds));
	}
}

void AAGICoreCommunication::ReceiveStateUpdate(const FAGIStateUpdateMessage& StateUpdateMessage)
{
	LastUpdateTimeSeconds = FPlatformTime::Seconds();
	if (!bConnected)
	{
		SetConnected(true, TEXT("Connected: receiving AGI core state updates"));
	}

	OnAGIStateUpdate.Broadcast(StateUpdateMessage);

	// Only commands issued by the AGI core itself are executed.
	if (PCGManager)
	{
		for (const FPCGCommand& Command : StateUpdateMessage.PCGCommands)
		{
			PCGManager->ExecutePCGCommand(Command);
		}
	}
}

void AAGICoreCommunication::SetConnected(bool bNewConnected, const FString& Status)
{
	bConnected = bNewConnected;
	ConnectionStatus = Status;
	UE_LOG(LogAGICoreComm, Log, TEXT("%s"), *ConnectionStatus);
}
