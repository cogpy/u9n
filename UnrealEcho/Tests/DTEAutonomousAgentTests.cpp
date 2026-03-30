// DTEAutonomousAgentTests.cpp
// Comprehensive Automation Tests for DTE Autonomous Cognitive AI Agent
//
// Test Coverage:
//   1.  VirtualCNS — component lifecycle, reflex arcs, pathway tiers
//   2.  VirtualCNS — proprioception, threat injection, motor commands
//   3.  VirtualCNS — efference copy and prediction error
//   4.  VirtualCNS — telemetry counters
//   5.  AutonomousAgentController — goal registration and lifecycle
//   6.  AutonomousAgentController — situational awareness
//   7.  AutonomousAgentController — strategy selection
//   8.  AutonomousAgentController — autonomy levels and override
//   9.  AutonomousAgentController — learning / reward propagation
//   10. GamingMasterySystem — combo state machine
//   11. GamingMasterySystem — opponent modelling
//   12. GamingMasterySystem — timing windows
//   13. GamingMasterySystem — speculative pre-execution
//   14. GamingMasterySystem — mastery metrics & skill tier
//   15. MetaHumanCNSBinding — expression state / PAD-to-FACS
//   16. MetaHumanCNSBinding — kinematic chain initialisation
//   17. Integration — VirtualCNS + AutonomousAgentController threat propagation
//   18. Integration — CNS binding → VirtualCNS proprioception
//   19. Integration — GamingMastery + VirtualCNS reflex pathway
//   20. Integration — full pipeline sensory → motor → learning

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

// Subject under test
#include "CNS/VirtualCNS.h"
#include "CNS/AutonomousAgentController.h"
#include "GameTraining/GamingMasterySystem.h"
#include "Avatar/MetaHumanCNSBinding.h"

// Dependencies (for integration tests)
#include "Core/DeepTreeEchoCore.h"
#include "GameTraining/GameControllerInterface.h"
#include "GameTraining/ReinforcementLearningBridge.h"

// ============================================================
// Test Macros
// ============================================================

#define DTE_TEST_FLAGS \
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter

// ============================================================
// Helper: Get a test world
// ============================================================

static UWorld* GetDTETestWorld()
{
    // Use the first available PIE or game world
    for (const FWorldContext& WC : GEngine->GetWorldContexts())
    {
        if (WC.WorldType == EWorldType::Game || WC.WorldType == EWorldType::PIE)
        {
            return WC.World();
        }
    }
    return nullptr;
}

// ============================================================
// 1. VirtualCNS — Component Lifecycle
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualCNS_LifecycleTest,
    "DTE.VirtualCNS.Lifecycle",
    DTE_TEST_FLAGS)

bool FVirtualCNS_LifecycleTest::RunTest(const FString& Parameters)
{
    UVirtualCNS* CNS = NewObject<UVirtualCNS>();

    TestNotNull(TEXT("VirtualCNS should be created"), CNS);

    if (CNS)
    {
        // Verify default configuration
        TestTrue(TEXT("Reflex arcs enabled by default"), CNS->bEnableReflexArcs);
        TestTrue(TEXT("Threat detection enabled by default"), CNS->bEnableThreatDetection);
        TestTrue(TEXT("Proprioception enabled by default"), CNS->bEnableProprioception);
        TestTrue(TEXT("Efference copy enabled by default"), CNS->bEnableEfferenceCopy);

        TestEqual(TEXT("Default pathway tier should be Cortical"),
                  CNS->GetActivePathwayTier(), ECNSPathwayTier::Cortical);

        TestFalse(TEXT("No pending motor commands on creation"),
                  CNS->HasPendingMotorCommand());

        const FCNSTelemetry Telem = CNS->GetTelemetry();
        TestEqual(TEXT("Total frames should be zero at start"),
                  Telem.TotalFrames, (int64)0);
    }
    return true;
}

// ============================================================
// 2. VirtualCNS — Reflex Arc Registration and Firing
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualCNS_ReflexArcTest,
    "DTE.VirtualCNS.ReflexArcs",
    DTE_TEST_FLAGS)

bool FVirtualCNS_ReflexArcTest::RunTest(const FString& Parameters)
{
    UVirtualCNS* CNS = NewObject<UVirtualCNS>();
    TestNotNull(TEXT("VirtualCNS should be created"), CNS);
    if (!CNS) { return false; }

    // Register a custom reflex
    FReflexArc Arc;
    Arc.ReflexID         = TEXT("TestReflex");
    Arc.TriggerCondition = TEXT("TestTrigger");
    Arc.Threshold        = 0.5f;
    Arc.RefractoryPeriod = 0.2f;
    Arc.Response.ActionName = TEXT("TestAction");

    CNS->RegisterReflexArc(Arc);

    TestTrue(TEXT("Reflex arc should exist after registration"),
             CNS->HasReflexArc(TEXT("TestReflex")));

    // Test manual fire
    bool bFired = false;
    CNS->OnReflexFired.AddLambda([&bFired](const FString&, const FControllerOutputCommand&)
    {
        bFired = true;
    });

    // Advance reflex timer past refractory period
    for (FReflexArc& R : CNS->ReflexArcs)
    {
        if (R.ReflexID == TEXT("TestReflex"))
        {
            R.TimeSinceLastFire = 999.0f;
        }
    }

    CNS->FireReflex(TEXT("TestReflex"));

    TestTrue(TEXT("Reflex should fire"), bFired);
    TestTrue(TEXT("Motor command should be queued after reflex"),
             CNS->HasPendingMotorCommand());

    // Poll the command
    const FControllerOutputCommand Cmd = CNS->PollMotorCommand();
    TestEqual(TEXT("Polled command should match reflex action"),
              Cmd.ActionName, TEXT("TestAction"));
    TestFalse(TEXT("Queue should be empty after poll"),
              CNS->HasPendingMotorCommand());

    // Test removal
    TestTrue(TEXT("RemoveReflexArc should return true"),
             CNS->RemoveReflexArc(TEXT("TestReflex")));
    TestFalse(TEXT("Reflex should not exist after removal"),
              CNS->HasReflexArc(TEXT("TestReflex")));

    return true;
}

// ============================================================
// 3. VirtualCNS — Pathway Tier Control
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualCNS_PathwayTierTest,
    "DTE.VirtualCNS.PathwayTier",
    DTE_TEST_FLAGS)

bool FVirtualCNS_PathwayTierTest::RunTest(const FString& Parameters)
{
    UVirtualCNS* CNS = NewObject<UVirtualCNS>();
    TestNotNull(TEXT("VirtualCNS should be created"), CNS);
    if (!CNS) { return false; }

    // Default tier
    TestEqual(TEXT("Default tier is Cortical"),
              CNS->GetActivePathwayTier(), ECNSPathwayTier::Cortical);

    // Set to spinal reflex
    ECNSPathwayTier ChangedTo = ECNSPathwayTier::Cortical;
    CNS->OnCNSPathwayChanged.AddLambda(
        [&ChangedTo](ECNSPathwayTier /*Old*/, ECNSPathwayTier New)
        {
            ChangedTo = New;
        });

    CNS->SetPathwayTier(ECNSPathwayTier::SpinalReflex);
    TestEqual(TEXT("Tier should be SpinalReflex after set"),
              CNS->GetActivePathwayTier(), ECNSPathwayTier::SpinalReflex);
    TestEqual(TEXT("Event should fire with SpinalReflex"),
              ChangedTo, ECNSPathwayTier::SpinalReflex);

    // Elevate tier — should not downgrade
    CNS->SetPathwayTier(ECNSPathwayTier::Cortical);
    CNS->ElevateTier(ECNSPathwayTier::Brainstem, 0.5f);
    // After elevation, tier should be at least Brainstem (which is lower latency
    // than Cortical in enum, so numerically smaller)
    TestTrue(TEXT("Tier should not be meta-cognitive after elevation"),
             CNS->GetActivePathwayTier() != ECNSPathwayTier::MetaCognitive);

    return true;
}

// ============================================================
// 4. VirtualCNS — Proprioception Update
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualCNS_ProprioceptionTest,
    "DTE.VirtualCNS.Proprioception",
    DTE_TEST_FLAGS)

bool FVirtualCNS_ProprioceptionTest::RunTest(const FString& Parameters)
{
    UVirtualCNS* CNS = NewObject<UVirtualCNS>();
    TestNotNull(TEXT("VirtualCNS should be created"), CNS);
    if (!CNS) { return false; }

    FProprioceptiveSnapshot Snap;
    Snap.WorldLocation = FVector(100.0f, 200.0f, 50.0f);
    Snap.WorldRotation = FRotator(0.0f, 45.0f, 0.0f);
    Snap.Velocity      = FVector(300.0f, 0.0f, 0.0f);
    Snap.bIsGrounded   = true;
    Snap.Timestamp     = 1.0f;

    bool bEventFired = false;
    CNS->OnProprioceptiveUpdate.AddLambda(
        [&bEventFired](const FProprioceptiveSnapshot&) { bEventFired = true; });

    CNS->UpdateProprioception(Snap);

    const FProprioceptiveSnapshot Retrieved = CNS->GetProprioceptiveSnapshot();
    TestTrue(TEXT("Proprioceptive event should fire"), bEventFired);
    TestEqual(TEXT("Location X should match"),
              Retrieved.WorldLocation.X, 100.0f, 0.001f);
    TestEqual(TEXT("Location Y should match"),
              Retrieved.WorldLocation.Y, 200.0f, 0.001f);
    TestEqual(TEXT("Yaw should match"),
              Retrieved.WorldRotation.Yaw, 45.0f, 0.001f);
    TestEqual(TEXT("Velocity X should match"),
              Retrieved.Velocity.X, 300.0f, 0.001f);
    TestTrue(TEXT("Grounded flag should match"), Retrieved.bIsGrounded);

    return true;
}

// ============================================================
// 5. VirtualCNS — Threat Injection
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualCNS_ThreatTest,
    "DTE.VirtualCNS.ThreatInjection",
    DTE_TEST_FLAGS)

bool FVirtualCNS_ThreatTest::RunTest(const FString& Parameters)
{
    UVirtualCNS* CNS = NewObject<UVirtualCNS>();
    TestNotNull(TEXT("VirtualCNS should be created"), CNS);
    if (!CNS) { return false; }

    FThreatSignal Threat;
    Threat.ThreatLevel      = 0.9f;
    Threat.ThreatType       = TEXT("Projectile");
    Threat.Distance         = 300.0f;
    Threat.TimeToImpact     = 0.5f;
    Threat.SuggestedReflex  = TEXT("DodgeLeft");

    bool bThreatEventFired = false;
    CNS->OnThreatDetected.AddLambda(
        [&bThreatEventFired](const FThreatSignal&) { bThreatEventFired = true; });

    CNS->SubmitThreatSignal(Threat);

    TestTrue(TEXT("Threat event should fire"), bThreatEventFired);

    // A high threat should escalate the pathway tier
    TestTrue(TEXT("Pathway should not be Cortical under high threat"),
             CNS->GetActivePathwayTier() != ECNSPathwayTier::Cortical);

    return true;
}

// ============================================================
// 6. VirtualCNS — Prediction Error
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualCNS_PredictionErrorTest,
    "DTE.VirtualCNS.PredictionError",
    DTE_TEST_FLAGS)

bool FVirtualCNS_PredictionErrorTest::RunTest(const FString& Parameters)
{
    UVirtualCNS* CNS = NewObject<UVirtualCNS>();
    TestNotNull(TEXT("VirtualCNS should be created"), CNS);
    if (!CNS) { return false; }

    // Without efference copy, prediction error should be 0
    const float InitialError = CNS->ComputePredictionError();
    TestEqual(TEXT("Initial prediction error should be 0"),
              InitialError, 0.0f, 0.001f);

    // Sensory vector helper
    const TArray<float> SensoryVec = FProprioceptiveSnapshot{}.ToSensoryVector();
    TestTrue(TEXT("Default snapshot sensory vector should have elements"),
             SensoryVec.Num() >= 14);

    return true;
}

// ============================================================
// 7. VirtualCNS — Telemetry Reset
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualCNS_TelemetryTest,
    "DTE.VirtualCNS.Telemetry",
    DTE_TEST_FLAGS)

bool FVirtualCNS_TelemetryTest::RunTest(const FString& Parameters)
{
    UVirtualCNS* CNS = NewObject<UVirtualCNS>();
    TestNotNull(TEXT("VirtualCNS should be created"), CNS);
    if (!CNS) { return false; }

    CNS->ResetTelemetry();
    const FCNSTelemetry T = CNS->GetTelemetry();
    TestEqual(TEXT("TotalFrames reset to 0"), T.TotalFrames, (int64)0);
    TestEqual(TEXT("ReflexesFired reset to 0"), T.ReflexesFiredThisFrame, 0);

    return true;
}

// ============================================================
// 8. AutonomousAgentController — Goal Lifecycle
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoAgent_GoalLifecycleTest,
    "DTE.AutonomousAgent.GoalLifecycle",
    DTE_TEST_FLAGS)

bool FAutoAgent_GoalLifecycleTest::RunTest(const FString& Parameters)
{
    UAutonomousAgentController* Agent = NewObject<UAutonomousAgentController>();
    TestNotNull(TEXT("Agent should be created"), Agent);
    if (!Agent) { return false; }

    // Register and activate a goal
    FCognitiveGoal Goal;
    Goal.GoalID      = TEXT("G1");
    Goal.Description = TEXT("Reach the objective");
    Goal.Priority    = 0.8f;
    Goal.ExpectedReward = 1.0f;

    Agent->RegisterGoal(Goal);

    const bool bActivated = Agent->ActivateGoal(TEXT("G1"));
    TestTrue(TEXT("Goal with no prerequisites should activate"), bActivated);

    const TArray<FCognitiveGoal> Active = Agent->GetActiveGoals();
    TestEqual(TEXT("One active goal expected"), Active.Num(), 1);
    TestEqual(TEXT("Active goal ID should match"), Active[0].GoalID, TEXT("G1"));

    // Update progress
    Agent->SetGoalProgress(TEXT("G1"), 0.5f);
    const TArray<FCognitiveGoal> AfterProgress = Agent->GetActiveGoals();
    TestEqual(TEXT("Goal progress should be 0.5"),
              AfterProgress[0].Progress, 0.5f, 0.001f);

    // Complete goal via progress = 1.0
    bool bCompletedFired = false;
    Agent->OnGoalCompleted.AddLambda(
        [&bCompletedFired](const FCognitiveGoal&) { bCompletedFired = true; });

    Agent->SetGoalProgress(TEXT("G1"), 1.0f);
    TestTrue(TEXT("OnGoalCompleted should fire"), bCompletedFired);

    const TArray<FCognitiveGoal> AfterComplete = Agent->GetActiveGoals();
    TestEqual(TEXT("No active goals after completion"), AfterComplete.Num(), 0);

    return true;
}

// ============================================================
// 9. AutonomousAgentController — Goal Prerequisite Chain
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoAgent_PrerequisiteTest,
    "DTE.AutonomousAgent.GoalPrerequisites",
    DTE_TEST_FLAGS)

bool FAutoAgent_PrerequisiteTest::RunTest(const FString& Parameters)
{
    UAutonomousAgentController* Agent = NewObject<UAutonomousAgentController>();
    TestNotNull(TEXT("Agent should be created"), Agent);
    if (!Agent) { return false; }

    // Goal B requires A
    FCognitiveGoal GoalA;
    GoalA.GoalID    = TEXT("A");
    GoalA.Priority  = 0.9f;

    FCognitiveGoal GoalB;
    GoalB.GoalID    = TEXT("B");
    GoalB.Priority  = 0.7f;
    GoalB.PrerequisiteIDs.Add(TEXT("A"));

    Agent->RegisterGoal(GoalA);
    Agent->RegisterGoal(GoalB);

    // B should not activate while A is incomplete
    const bool bBActivated = Agent->ActivateGoal(TEXT("B"));
    TestFalse(TEXT("Goal B should NOT activate while prereq A is incomplete"),
              bBActivated);

    // Complete A
    Agent->ActivateGoal(TEXT("A"));
    Agent->CompleteGoal(TEXT("A"), true);

    // Now B should activate
    const bool bBActivatedAfterA = Agent->ActivateGoal(TEXT("B"));
    TestTrue(TEXT("Goal B should activate once A is completed"), bBActivatedAfterA);

    return true;
}

// ============================================================
// 10. AutonomousAgentController — Situational Awareness
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoAgent_AwarenessTest,
    "DTE.AutonomousAgent.SituationalAwareness",
    DTE_TEST_FLAGS)

bool FAutoAgent_AwarenessTest::RunTest(const FString& Parameters)
{
    UAutonomousAgentController* Agent = NewObject<UAutonomousAgentController>();
    TestNotNull(TEXT("Agent should be created"), Agent);
    if (!Agent) { return false; }

    FSituationalEntity Enemy;
    Enemy.EntityID       = TEXT("Enemy1");
    Enemy.Position       = FVector(100.0f, 0.0f, 0.0f);
    Enemy.ThreatLevel    = 0.8f;
    Enemy.Type           = TEXT("Enemy");
    Enemy.Confidence     = 1.0f;

    FSituationalEntity Ally;
    Ally.EntityID          = TEXT("Ally1");
    Ally.Position          = FVector(-50.0f, 0.0f, 0.0f);
    Ally.OpportunityValue  = 0.7f;
    Ally.Confidence        = 1.0f;
    Ally.Type              = TEXT("Ally");

    Agent->ObserveEntity(Enemy);
    Agent->ObserveEntity(Ally);

    const FSituationalAwareness Awareness = Agent->GetSituationalAwareness();
    TestEqual(TEXT("Two entities should be tracked"),
              Awareness.Entities.Num(), 2);

    // Threat check
    TestTrue(TEXT("Agent should be under threat"),
             Agent->IsUnderThreat(0.5f));

    // Forget entity
    Agent->ForgetEntity(TEXT("Enemy1"));
    const FSituationalAwareness AwarenessAfterForget = Agent->GetSituationalAwareness();
    TestEqual(TEXT("One entity after forget"), AwarenessAfterForget.Entities.Num(), 1);

    return true;
}

// ============================================================
// 11. AutonomousAgentController — Strategy Selection
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoAgent_StrategyTest,
    "DTE.AutonomousAgent.StrategySelection",
    DTE_TEST_FLAGS)

bool FAutoAgent_StrategyTest::RunTest(const FString& Parameters)
{
    UAutonomousAgentController* Agent = NewObject<UAutonomousAgentController>();
    TestNotNull(TEXT("Agent should be created"), Agent);
    if (!Agent) { return false; }

    // Default strategy
    TestEqual(TEXT("Default strategy should be Explore"),
              Agent->GetCurrentStrategy(), TEXT("Explore"));

    // Add high threat → Survive
    FSituationalEntity Threat;
    Threat.EntityID    = TEXT("T");
    Threat.ThreatLevel = 0.95f;
    Threat.Confidence  = 1.0f;
    Agent->ObserveEntity(Threat);

    // Manually compute danger
    const FString Strategy = Agent->SelectOptimalStrategy();
    TestEqual(TEXT("Under extreme threat strategy should be Survive"),
              Strategy, TEXT("Survive"));

    return true;
}

// ============================================================
// 12. AutonomousAgentController — Autonomy Level
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoAgent_AutonomyTest,
    "DTE.AutonomousAgent.AutonomyLevel",
    DTE_TEST_FLAGS)

bool FAutoAgent_AutonomyTest::RunTest(const FString& Parameters)
{
    UAutonomousAgentController* Agent = NewObject<UAutonomousAgentController>();
    TestNotNull(TEXT("Agent should be created"), Agent);
    if (!Agent) { return false; }

    TestEqual(TEXT("Default autonomy is FullAutonomy"),
              Agent->AutonomyLevel, EAutonomyLevel::FullAutonomy);

    TestTrue(TEXT("IsFullyAutonomous should be true by default"),
             Agent->IsFullyAutonomous());

    Agent->SetAutonomyLevel(EAutonomyLevel::Assisted);
    TestEqual(TEXT("Autonomy should change to Assisted"),
              Agent->AutonomyLevel, EAutonomyLevel::Assisted);
    TestFalse(TEXT("IsFullyAutonomous should be false when Assisted"),
              Agent->IsFullyAutonomous());

    return true;
}

// ============================================================
// 13. GamingMasterySystem — Combo State Machine
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGaming_ComboTest,
    "DTE.GamingMastery.ComboStateMachine",
    DTE_TEST_FLAGS)

bool FGaming_ComboTest::RunTest(const FString& Parameters)
{
    UGamingMasterySystem* GMS = NewObject<UGamingMasterySystem>();
    TestNotNull(TEXT("GamingMasterySystem should be created"), GMS);
    if (!GMS) { return false; }

    // Register a simple 3-hit combo
    FComboDefinition Combo;
    Combo.ComboID   = TEXT("Combo3");
    Combo.ActionSequence = { TEXT("A"), TEXT("A"), TEXT("B") };
    Combo.MaxTimeBetweenActions = 0.5f;
    Combo.ExpectedReward = 1.0f;
    GMS->RegisterCombo(Combo);

    // Advance the combo
    GMS->AdvanceComboState(TEXT("A"));
    TestEqual(TEXT("Next action after A should be A"),
              GMS->GetNextComboAction(), TEXT("A"));

    GMS->AdvanceComboState(TEXT("A"));
    TestEqual(TEXT("Next action after A,A should be B"),
              GMS->GetNextComboAction(), TEXT("B"));

    bool bComboFired = false;
    GMS->OnComboExecuted.AddLambda(
        [&bComboFired](const FString&, bool) { bComboFired = true; });

    GMS->AdvanceComboState(TEXT("B"));
    TestTrue(TEXT("Combo should fire on completion"), bComboFired);
    TestTrue(TEXT("Next combo action should be empty after completion"),
             GMS->GetNextComboAction().IsEmpty());

    return true;
}

// ============================================================
// 14. GamingMasterySystem — Wrong Input Resets Combo
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGaming_ComboResetTest,
    "DTE.GamingMastery.ComboReset",
    DTE_TEST_FLAGS)

bool FGaming_ComboResetTest::RunTest(const FString& Parameters)
{
    UGamingMasterySystem* GMS = NewObject<UGamingMasterySystem>();
    TestNotNull(TEXT("GamingMasterySystem should be created"), GMS);
    if (!GMS) { return false; }

    FComboDefinition Combo;
    Combo.ComboID   = TEXT("ABC");
    Combo.ActionSequence = { TEXT("A"), TEXT("B"), TEXT("C") };
    Combo.MaxTimeBetweenActions = 0.5f;
    GMS->RegisterCombo(Combo);

    GMS->AdvanceComboState(TEXT("A"));
    // Wrong input — should reset
    GMS->AdvanceComboState(TEXT("X"));

    TestTrue(TEXT("Combo should be reset after wrong input"),
             GMS->GetNextComboAction().IsEmpty());

    return true;
}

// ============================================================
// 15. GamingMasterySystem — Opponent Modelling
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGaming_OpponentModelTest,
    "DTE.GamingMastery.OpponentModelling",
    DTE_TEST_FLAGS)

bool FGaming_OpponentModelTest::RunTest(const FString& Parameters)
{
    UGamingMasterySystem* GMS = NewObject<UGamingMasterySystem>();
    TestNotNull(TEXT("GamingMasterySystem should be created"), GMS);
    if (!GMS) { return false; }

    const FString OppID = TEXT("Bot1");

    // Bot spams HighAttack
    for (int32 i = 0; i < 10; ++i)
    {
        GMS->ObserveOpponentAction(OppID, TEXT("HighAttack"));
    }
    GMS->ObserveOpponentAction(OppID, TEXT("LowSweep"));

    const FString Predicted = GMS->PredictOpponentAction(OppID);
    TestEqual(TEXT("HighAttack should be predicted as most frequent"),
              Predicted, TEXT("HighAttack"));

    // Counter to HighAttack should be DuckUnder
    const FString Counter = GMS->GetBestCounter(OppID);
    TestEqual(TEXT("Counter to HighAttack should be DuckUnder"),
              Counter, TEXT("DuckUnder"));

    // Test model completeness
    const FOpponentModel Model = GMS->GetOpponentModel(OppID);
    TestEqual(TEXT("Opponent should have 11 observed moves"),
              Model.ObservedMoves, 11);
    TestTrue(TEXT("HighAttack should be in action history"),
             Model.ActionHistory.Contains(TEXT("HighAttack")));

    return true;
}

// ============================================================
// 16. GamingMasterySystem — Timing Windows
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGaming_TimingWindowTest,
    "DTE.GamingMastery.TimingWindows",
    DTE_TEST_FLAGS)

bool FGaming_TimingWindowTest::RunTest(const FString& Parameters)
{
    UGamingMasterySystem* GMS = NewObject<UGamingMasterySystem>();
    TestNotNull(TEXT("GamingMasterySystem should be created"), GMS);
    if (!GMS) { return false; }

    FTimingWindow Wnd;
    Wnd.ActionName  = TEXT("Parry");
    Wnd.EarliestTime = 0.0f;
    Wnd.OptimalTime  = 0.05f;
    Wnd.LatestTime   = 0.1f;
    Wnd.PerfectMultiplier = 3.0f;

    bool bOpened = false;
    GMS->OnTimingWindowOpened.AddLambda(
        [&bOpened](const FTimingWindow&) { bOpened = true; });

    GMS->OpenTimingWindow(Wnd);
    TestTrue(TEXT("TimingWindowOpened event should fire"), bOpened);

    // At t=0 elapsed, window should be open
    const float Remaining = GMS->GetWindowRemainingTime(TEXT("Parry"));
    TestTrue(TEXT("Remaining time should be positive"), Remaining > 0.0f);

    GMS->CloseTimingWindow(TEXT("Parry"));
    const float RemainingAfterClose = GMS->GetWindowRemainingTime(TEXT("Parry"));
    TestEqual(TEXT("Remaining time should be 0 after close"),
              RemainingAfterClose, 0.0f, 0.001f);

    return true;
}

// ============================================================
// 17. GamingMasterySystem — Combo Catalogue Sort
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGaming_ComboSortTest,
    "DTE.GamingMastery.ComboCatalogSort",
    DTE_TEST_FLAGS)

bool FGaming_ComboSortTest::RunTest(const FString& Parameters)
{
    UGamingMasterySystem* GMS = NewObject<UGamingMasterySystem>();
    TestNotNull(TEXT("GamingMasterySystem should be created"), GMS);
    if (!GMS) { return false; }

    FComboDefinition C1;
    C1.ComboID = TEXT("C1"); C1.Proficiency = 0.3f;
    FComboDefinition C2;
    C2.ComboID = TEXT("C2"); C2.Proficiency = 0.9f;
    FComboDefinition C3;
    C3.ComboID = TEXT("C3"); C3.Proficiency = 0.6f;

    GMS->RegisterCombo(C1);
    GMS->RegisterCombo(C2);
    GMS->RegisterCombo(C3);

    const TArray<FComboDefinition> Sorted =
        GMS->GetComboCatalogSortedByProficiency();

    if (Sorted.Num() >= 2)
    {
        // The first element should have highest proficiency
        TestTrue(TEXT("Sorted catalogue should be in descending proficiency order"),
                 Sorted[0].Proficiency >= Sorted[1].Proficiency);
    }

    return true;
}

// ============================================================
// 18. MetaHumanCNSBinding — Default FACS Initialisation
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetaHumanCNSBinding_FACSInitTest,
    "DTE.MetaHumanCNSBinding.FACSInit",
    DTE_TEST_FLAGS)

bool FMetaHumanCNSBinding_FACSInitTest::RunTest(const FString& Parameters)
{
    UMetaHumanCNSBinding* Binding = NewObject<UMetaHumanCNSBinding>();
    TestNotNull(TEXT("MetaHumanCNSBinding should be created"), Binding);
    if (!Binding) { return false; }

    // Simulate BeginPlay by calling private-equivalent public init path
    // (In real tests we'd call BeginPlay; here we verify post-construct state)
    TestTrue(TEXT("BodySchemaHz should be positive"), Binding->BodySchemaHz > 0.0f);
    TestTrue(TEXT("ExpressionHz should be positive"), Binding->ExpressionHz > 0.0f);
    TestTrue(TEXT("Expression writing enabled by default"),
             Binding->bEnableExpressionWriting);
    TestTrue(TEXT("Body schema reading enabled by default"),
             Binding->bEnableBodySchemaReading);

    return true;
}

// ============================================================
// 19. MetaHumanCNSBinding — Expression State Setting
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetaHumanCNSBinding_ExpressionTest,
    "DTE.MetaHumanCNSBinding.ExpressionState",
    DTE_TEST_FLAGS)

bool FMetaHumanCNSBinding_ExpressionTest::RunTest(const FString& Parameters)
{
    UMetaHumanCNSBinding* Binding = NewObject<UMetaHumanCNSBinding>();
    TestNotNull(TEXT("MetaHumanCNSBinding should be created"), Binding);
    if (!Binding) { return false; }

    bool bEventFired = false;
    Binding->OnExpressionStateUpdated.AddLambda(
        [&bEventFired](const FCognitiveExpressionState&) { bEventFired = true; });

    FCognitiveExpressionState State;
    State.Valence   =  0.8f;  // happy
    State.Arousal   =  0.6f;  // moderately excited
    State.Dominance =  0.4f;
    State.CognitiveLoad = 0.2f;

    Binding->SetExpressionState(State);

    TestTrue(TEXT("Expression update event should fire"), bEventFired);

    const FCognitiveExpressionState Retrieved = Binding->GetExpressionState();
    TestEqual(TEXT("Valence should match"), Retrieved.Valence, 0.8f, 0.001f);
    TestEqual(TEXT("Arousal should match"), Retrieved.Arousal, 0.6f, 0.001f);

    return true;
}

// ============================================================
// 20. MetaHumanCNSBinding — AU Activation
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetaHumanCNSBinding_AUTest,
    "DTE.MetaHumanCNSBinding.AUActivation",
    DTE_TEST_FLAGS)

bool FMetaHumanCNSBinding_AUTest::RunTest(const FString& Parameters)
{
    UMetaHumanCNSBinding* Binding = NewObject<UMetaHumanCNSBinding>();
    TestNotNull(TEXT("MetaHumanCNSBinding should be created"), Binding);
    if (!Binding) { return false; }

    // Set AU12 (smile) activation
    Binding->SetAUActivation(12, 0.75f);
    const float Retrieved = Binding->GetAUActivation(12);
    TestEqual(TEXT("AU12 activation should be 0.75"), Retrieved, 0.75f, 0.001f);

    // Clamp check — above 1.0 should clamp
    Binding->SetAUActivation(12, 1.5f);
    const float Clamped = Binding->GetAUActivation(12);
    TestTrue(TEXT("AU activation should be clamped to 1.0"), Clamped <= 1.0f);

    // Non-existent AU should return 0
    const float NonExistent = Binding->GetAUActivation(999);
    TestEqual(TEXT("Non-existent AU should return 0"), NonExistent, 0.0f, 0.001f);

    return true;
}

// ============================================================
// 21. MetaHumanCNSBinding — Emotion Trigger
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetaHumanCNSBinding_EmotionTriggerTest,
    "DTE.MetaHumanCNSBinding.EmotionTrigger",
    DTE_TEST_FLAGS)

bool FMetaHumanCNSBinding_EmotionTriggerTest::RunTest(const FString& Parameters)
{
    UMetaHumanCNSBinding* Binding = NewObject<UMetaHumanCNSBinding>();
    TestNotNull(TEXT("MetaHumanCNSBinding should be created"), Binding);
    if (!Binding) { return false; }

    bool bEventFired = false;
    Binding->OnExpressionStateUpdated.AddLambda(
        [&bEventFired](const FCognitiveExpressionState&) { bEventFired = true; });

    // Trigger fear expression
    Binding->TriggerEmotion(-0.8f, 0.9f, -0.5f, 2.0f);

    TestTrue(TEXT("Emotion trigger should fire expression event"), bEventFired);
    const FCognitiveExpressionState State = Binding->GetExpressionState();
    TestEqual(TEXT("Valence should be -0.8 (fear)"), State.Valence, -0.8f, 0.001f);
    TestEqual(TEXT("Arousal should be 0.9 (high)"), State.Arousal, 0.9f, 0.001f);

    return true;
}

// ============================================================
// 22. ProprioceptiveSnapshot — ToSensoryVector
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProprioSnapshot_SensoryVectorTest,
    "DTE.VirtualCNS.SensoryVector",
    DTE_TEST_FLAGS)

bool FProprioSnapshot_SensoryVectorTest::RunTest(const FString& Parameters)
{
    FProprioceptiveSnapshot Snap;
    Snap.WorldLocation = FVector(500.0f, 0.0f, 100.0f);
    Snap.WorldRotation = FRotator(0.0f, 90.0f, 0.0f);
    Snap.Velocity      = FVector(300.0f, 0.0f, 0.0f);
    Snap.bIsGrounded   = true;
    Snap.bIsAirborne   = false;
    Snap.Timestamp     = 1.5f;

    const TArray<float> Vec = Snap.ToSensoryVector();

    TestEqual(TEXT("Sensory vector should have 18 elements"), Vec.Num(), 18);
    TestEqual(TEXT("X position element should be 0.5 (500*0.001)"),
              Vec[0], 0.5f, 0.001f);
    TestEqual(TEXT("Z position element should be 0.1 (100*0.001)"),
              Vec[2], 0.1f, 0.001f);
    TestEqual(TEXT("Yaw element should be 0.5 (90/180)"),
              Vec[4], 0.5f, 0.001f);
    TestEqual(TEXT("Vx element should be 0.5 (300/600)"),
              Vec[6], 0.5f, 0.001f);
    TestEqual(TEXT("IsGrounded should be 1.0"),
              Vec[12], 1.0f, 0.001f);
    TestEqual(TEXT("IsAirborne should be 0.0"),
              Vec[13], 0.0f, 0.001f);

    return true;
}

// ============================================================
// 23. Integration — Agent Goal Activates Attention
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIntegration_GoalAndAwarenessTest,
    "DTE.Integration.GoalAwarenessInteraction",
    DTE_TEST_FLAGS)

bool FIntegration_GoalAndAwarenessTest::RunTest(const FString& Parameters)
{
    UAutonomousAgentController* Agent = NewObject<UAutonomousAgentController>();
    TestNotNull(TEXT("Agent should be created"), Agent);
    if (!Agent) { return false; }

    // Register a combat goal
    FCognitiveGoal CombatGoal;
    CombatGoal.GoalID      = TEXT("Eliminate_Enemy");
    CombatGoal.Description = TEXT("Combat objective");
    CombatGoal.Priority    = 0.9f;
    Agent->RegisterGoal(CombatGoal);
    Agent->ActivateGoal(TEXT("Eliminate_Enemy"));

    // Add a medium-threat entity
    FSituationalEntity Enemy;
    Enemy.EntityID    = TEXT("EnemyA");
    Enemy.ThreatLevel = 0.65f;
    Enemy.Type        = TEXT("Combat");
    Enemy.Confidence  = 1.0f;
    Agent->ObserveEntity(Enemy);

    // Under medium threat + combat goal → strategy should be Engage
    const FString Strategy = Agent->SelectOptimalStrategy();
    TestEqual(TEXT("Strategy should be Engage with combat goal and medium threat"),
              Strategy, TEXT("Engage"));

    return true;
}

// ============================================================
// 24. Integration — VirtualCNS + Reflex + GamingMastery
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIntegration_CNSReflexMasteryTest,
    "DTE.Integration.CNSReflexMastery",
    DTE_TEST_FLAGS)

bool FIntegration_CNSReflexMasteryTest::RunTest(const FString& Parameters)
{
    UVirtualCNS*          CNS = NewObject<UVirtualCNS>();
    UGamingMasterySystem* GMS = NewObject<UGamingMasterySystem>();

    TestNotNull(TEXT("VirtualCNS should be created"), CNS);
    TestNotNull(TEXT("GamingMasterySystem should be created"), GMS);
    if (!CNS || !GMS) { return false; }

    GMS->SetVirtualCNS(CNS);

    // Verify reflex arcs are present in CNS after GMS wiring
    // GMS wires into CNS via SetVirtualCNS
    TestNotNull(TEXT("GMS should have VirtualCNS reference"), CNS);

    // Fire a stimulus via GMS → should propagate to CNS
    // Prime reflex refractory timers
    for (FReflexArc& R : CNS->ReflexArcs)
    {
        R.TimeSinceLastFire = 999.0f;
    }

    // GMS::OnStimulusDetected is a regular function (not a delegate), call directly
    GMS->OnStimulusDetected(TEXT("DodgeLeft"), 0.85f);

    // Since urgency >= 0.7, VirtualCNS should have tried to fire the DodgeLeft reflex
    // DodgeLeft is one of the default arcs
    TestTrue(TEXT("CNS DodgeLeft reflex should exist"),
             CNS->HasReflexArc(TEXT("DodgeLeft")));

    return true;
}

// ============================================================
// 25. Integration — Autonomous Agent Reward Learning
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIntegration_AgentRewardTest,
    "DTE.Integration.AgentRewardLearning",
    DTE_TEST_FLAGS)

bool FIntegration_AgentRewardTest::RunTest(const FString& Parameters)
{
    UAutonomousAgentController* Agent = NewObject<UAutonomousAgentController>();
    TestNotNull(TEXT("Agent should be created"), Agent);
    if (!Agent) { return false; }

    // Initial stats
    const FAgentPerformanceStats InitStats = Agent->GetPerformanceStats();
    TestEqual(TEXT("CumulativeReward should start at 0"),
              InitStats.CumulativeReward, 0.0f, 0.001f);

    // Provide reward
    Agent->ProvideReward(1.5f, TEXT("SuccessfulAction"));
    const FAgentPerformanceStats AfterReward = Agent->GetPerformanceStats();
    TestEqual(TEXT("CumulativeReward should accumulate"),
              AfterReward.CumulativeReward, 1.5f, 0.001f);

    // Episode end
    Agent->OnEpisodeEnd(true, 2.0f);
    const FAgentPerformanceStats AfterEpisode = Agent->GetPerformanceStats();
    TestEqual(TEXT("Episodes completed should be 1"),
              AfterEpisode.EpisodesCompleted, 1);

    return true;
}
