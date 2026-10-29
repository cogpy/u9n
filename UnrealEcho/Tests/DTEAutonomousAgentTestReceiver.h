// DTEAutonomousAgentTestReceiver.h
// Test-only receiver for the dynamic multicast delegates exercised by DTEAutonomousAgentTests.
// Dynamic delegates bind UFUNCTIONs (AddDynamic), not lambdas, so the tests bind this object
// and assert on what it recorded.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "CNS/VirtualCNS.h"
#include "CNS/AutonomousAgentController.h"
#include "GameTraining/GamingMasterySystem.h"
#include "Avatar/MetaHumanCNSBinding.h"
#include "Avatar/Sys6AvatarIntegration.h"
#include "ActiveInference/AXIOMActiveInference.h"
#include "DTEAutonomousAgentTestReceiver.generated.h"

UCLASS()
class UDTEAutonomousTestEventReceiver : public UObject
{
    GENERATED_BODY()

public:
    bool bReflexFired = false;
    bool bPathwayChanged = false;
    ECNSPathwayTier LastPathwayTier = ECNSPathwayTier::Cortical;
    bool bProprioceptiveUpdated = false;
    bool bThreatDetected = false;
    bool bGoalCompleted = false;
    bool bComboExecuted = false;
    bool bTimingWindowOpened = false;
    bool bExpressionStateUpdated = false;
    int32 AvatarExpressionChanges = 0;
    int32 AXIOMActionsSelected = 0;

    UFUNCTION()
    void HandleReflexFired(const FString& ReflexID, const FControllerOutputCommand& Response) { bReflexFired = true; }

    UFUNCTION()
    void HandlePathwayChanged(ECNSPathwayTier OldTier, ECNSPathwayTier NewTier)
    {
        bPathwayChanged = true;
        LastPathwayTier = NewTier;
    }

    UFUNCTION()
    void HandleProprioceptiveUpdate(const FProprioceptiveSnapshot& Snapshot) { bProprioceptiveUpdated = true; }

    UFUNCTION()
    void HandleThreatDetected(const FThreatSignal& Threat) { bThreatDetected = true; }

    UFUNCTION()
    void HandleGoalCompleted(const FCognitiveGoal& Goal) { bGoalCompleted = true; }

    UFUNCTION()
    void HandleComboExecuted(const FString& ComboID, bool bPerfect) { bComboExecuted = true; }

    UFUNCTION()
    void HandleTimingWindowOpened(const FTimingWindow& Window) { bTimingWindowOpened = true; }

    UFUNCTION()
    void HandleExpressionStateUpdated(const FCognitiveExpressionState& Expression) { bExpressionStateUpdated = true; }

    UFUNCTION()
    void HandleAvatarExpressionChanged(EAvatarExpressionChannel Channel, float Intensity) { ++AvatarExpressionChanges; }

    UFUNCTION()
    void HandleAXIOMActionSelected(int32 Action, float ExpectedFreeEnergy) { ++AXIOMActionsSelected; }
};
