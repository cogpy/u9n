#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AGIComms.h"
#include "AGICoreCommunication.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAGIStateUpdate, const FAGIStateUpdateMessage&, StateUpdateMessage);

class AAGIPCGManager;

/**
 * Receives cognitive state updates from a real AGI core and relays them to the avatar.
 *
 * No AGI core transport is implemented in this actor. State updates are broadcast only when a
 * transport hands one in through ReceiveStateUpdate(); nothing is generated locally (no mock
 * state, no autonomous PCG commands). Without updates the actor reports itself disconnected.
 */
UCLASS()
class DEEPTREEECHOAVATAR_API AAGICoreCommunication : public AActor
{
	GENERATED_BODY()
	
public:	
	AAGICoreCommunication();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(BlueprintAssignable, Category = "AGI")
	FOnAGIStateUpdate OnAGIStateUpdate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGI|PCG")
	AAGIPCGManager* PCGManager;

	/** Seconds without an update after which the core is considered disconnected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGI", meta = (ClampMin = "0.1"))
	float ConnectionTimeoutSeconds;

	/**
	 * Entry point for a real AGI core transport: relays the received update (and any PCG
	 * commands the core issued) and marks the core as connected.
	 */
	UFUNCTION(BlueprintCallable, Category = "AGI")
	void ReceiveStateUpdate(const FAGIStateUpdateMessage& StateUpdateMessage);

	/** True only while state updates from an AGI core are arriving. */
	UFUNCTION(BlueprintPure, Category = "AGI")
	bool IsConnected() const { return bConnected; }

	/** Human-readable connection status (system text). */
	UFUNCTION(BlueprintPure, Category = "AGI")
	FString GetConnectionStatus() const { return ConnectionStatus; }

private:
	void SetConnected(bool bNewConnected, const FString& Status);

	bool bConnected;
	FString ConnectionStatus;
	double LastUpdateTimeSeconds;
};
