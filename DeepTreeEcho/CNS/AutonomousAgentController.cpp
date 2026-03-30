// AutonomousAgentController.cpp
// Deep Tree Echo — Autonomous Agent Controller Implementation

#include "CNS/AutonomousAgentController.h"
#include "CNS/VirtualCNS.h"
#include "Core/DeepTreeEchoCore.h"
#include "GameTraining/ReinforcementLearningBridge.h"
#include "GameTraining/GameTrainingEnvironment.h"
#include "GameTraining/GameSkillTrainingSystem.h"
#include "Attention/AttentionSystem.h"
#include "Planning/PlanningSystem.h"
#include "Learning/OnlineLearningSystem.h"
#include "GameFramework/Actor.h"
#include "Math/UnrealMathUtility.h"

// -------------------------------------------------------
// Constructor
// -------------------------------------------------------

UAutonomousAgentController::UAutonomousAgentController()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.TickGroup = TG_DuringPhysics;
}

// -------------------------------------------------------
// BeginPlay
// -------------------------------------------------------

void UAutonomousAgentController::BeginPlay()
{
    Super::BeginPlay();
    FindAndCacheComponents();
    RegisterInitialGoals();
    bInitialised = true;
}

// -------------------------------------------------------
// TickComponent
// -------------------------------------------------------

void UAutonomousAgentController::TickComponent(float DeltaTime,
                                                ELevelTick TickType,
                                                FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bInitialised) { return; }

    // ── Human override countdown ──────────────────────────
    if (HumanOverrideTimer > 0.0f)
    {
        HumanOverrideTimer -= DeltaTime;
        return; // let human drive
    }

    // ── Awareness update ──────────────────────────────────
    AwarenessUpdateTimer += DeltaTime;
    if (AwarenessHz > 0.0f && AwarenessUpdateTimer >= 1.0f / AwarenessHz)
    {
        AwarenessUpdateTimer = 0.0f;
        UpdateAwarenessModel(DeltaTime);
        PropagateThreatsToVirtualCNS();
    }

    // ── Goal evaluation ───────────────────────────────────
    GoalEvalTimer += DeltaTime;
    if (GoalEvalHz > 0.0f && GoalEvalTimer >= 1.0f / GoalEvalHz)
    {
        GoalEvalTimer = 0.0f;
        EvaluateGoals(DeltaTime);
    }

    // ── Strategy update ───────────────────────────────────
    StrategyUpdateTimer += DeltaTime;
    if (StrategyUpdateHz > 0.0f && StrategyUpdateTimer >= 1.0f / StrategyUpdateHz)
    {
        StrategyUpdateTimer = 0.0f;
        UpdateStrategy(DeltaTime);
    }

    // ── Sync context to DTE cognition ─────────────────────
    SyncContextToCognition();

    // Increment elapsed time for active goals
    for (auto& KV : GoalRegistry)
    {
        if (KV.Value.bActive && !KV.Value.bCompleted)
        {
            KV.Value.ElapsedTime += DeltaTime;
        }
    }
}

// -------------------------------------------------------
// Goal Management
// -------------------------------------------------------

void UAutonomousAgentController::RegisterGoal(const FCognitiveGoal& Goal)
{
    GoalRegistry.Add(Goal.GoalID, Goal);
}

bool UAutonomousAgentController::ActivateGoal(const FString& GoalID)
{
    FCognitiveGoal* Goal = GoalRegistry.Find(GoalID);
    if (!Goal || Goal->bCompleted) { return false; }

    // Check prerequisites
    for (const FString& PrereqID : Goal->PrerequisiteIDs)
    {
        const FCognitiveGoal* Prereq = GoalRegistry.Find(PrereqID);
        if (!Prereq || !Prereq->bCompleted) { return false; }
    }

    Goal->bActive = true;
    OnGoalActivated.Broadcast(*Goal);

    // Create desire in DTE memory
    if (DTECore)
    {
        DTECore->CreateDesire(Goal->Description, Goal->Priority);
    }

    return true;
}

void UAutonomousAgentController::CompleteGoal(const FString& GoalID, bool bSuccess)
{
    FCognitiveGoal* Goal = GoalRegistry.Find(GoalID);
    if (!Goal) { return; }

    Goal->bCompleted = true;
    Goal->bActive    = false;
    Goal->Progress   = bSuccess ? 1.0f : Goal->Progress;

    OnGoalCompleted.Broadcast(*Goal);

    if (bSuccess)
    {
        PerfStats.GoalsCompleted++;
        ProvideReward(Goal->ExpectedReward, TEXT("GoalCompleted:") + GoalID);

        // Store in episodic memory
        if (DTECore)
        {
            TArray<float> Embedding = { Goal->Priority, 1.0f, Goal->ElapsedTime * 0.01f };
            DTECore->StoreEpisodicMemory(TEXT("GoalCompleted:") + Goal->GoalID, Embedding);
        }
    }
}

void UAutonomousAgentController::SetGoalProgress(const FString& GoalID,
                                                   float Progress)
{
    FCognitiveGoal* Goal = GoalRegistry.Find(GoalID);
    if (Goal)
    {
        Goal->Progress = FMath::Clamp(Progress, 0.0f, 1.0f);
        if (Goal->Progress >= 1.0f)
        {
            CompleteGoal(GoalID, true);
        }
    }
}

TArray<FCognitiveGoal> UAutonomousAgentController::GetActiveGoals() const
{
    TArray<FCognitiveGoal> Active;
    for (const auto& KV : GoalRegistry)
    {
        if (KV.Value.bActive && !KV.Value.bCompleted)
        {
            Active.Add(KV.Value);
        }
    }
    Active.Sort([](const FCognitiveGoal& A, const FCognitiveGoal& B)
    {
        return A.Priority > B.Priority;
    });
    return Active;
}

FCognitiveGoal UAutonomousAgentController::GetCurrentPrimaryGoal() const
{
    const TArray<FCognitiveGoal> Active = GetActiveGoals();
    return Active.Num() > 0 ? Active[0] : FCognitiveGoal{};
}

void UAutonomousAgentController::ClearGoals()
{
    GoalRegistry.Empty();
}

// -------------------------------------------------------
// Situational Awareness
// -------------------------------------------------------

void UAutonomousAgentController::UpdateSituationalAwareness(
    const TArray<FSituationalEntity>& ObservedEntities)
{
    for (const FSituationalEntity& Observed : ObservedEntities)
    {
        ObserveEntity(Observed);
    }
}

void UAutonomousAgentController::ObserveEntity(const FSituationalEntity& Entity)
{
    // Find or add
    for (FSituationalEntity& Existing : CurrentAwareness.Entities)
    {
        if (Existing.EntityID == Entity.EntityID)
        {
            // Bayesian update: blend old estimate with new observation
            const float Alpha = Entity.Confidence;
            Existing.Position         = FMath::Lerp(Existing.Position, Entity.Position, Alpha);
            Existing.Velocity         = FMath::Lerp(Existing.Velocity, Entity.Velocity, Alpha);
            Existing.ThreatLevel      = FMath::Lerp(Existing.ThreatLevel, Entity.ThreatLevel, Alpha);
            Existing.OpportunityValue = FMath::Lerp(Existing.OpportunityValue, Entity.OpportunityValue, Alpha);
            Existing.Confidence       = FMath::Min(1.0f, Existing.Confidence + Alpha * 0.1f);
            Existing.TimeSinceObserved = 0.0f;
            return;
        }
    }
    FSituationalEntity New = Entity;
    New.TimeSinceObserved = 0.0f;
    CurrentAwareness.Entities.Add(New);
}

void UAutonomousAgentController::ForgetEntity(const FString& EntityID)
{
    CurrentAwareness.Entities.RemoveAll([&EntityID](const FSituationalEntity& E)
    {
        return E.EntityID == EntityID;
    });
}

bool UAutonomousAgentController::IsUnderThreat(float Threshold) const
{
    return CurrentAwareness.DangerIndex >= Threshold;
}

// -------------------------------------------------------
// Strategy
// -------------------------------------------------------

void UAutonomousAgentController::SetStrategy(const FString& Strategy)
{
    if (Strategy == CurrentStrategy) { return; }
    const FString Old = CurrentStrategy;
    CurrentStrategy = Strategy;
    OnStrategyChanged.Broadcast(Old, Strategy);
}

FString UAutonomousAgentController::SelectOptimalStrategy()
{
    // Strategy selection matrix based on danger + goal context
    const float Danger = CurrentAwareness.DangerIndex;
    const FCognitiveGoal PrimaryGoal = GetCurrentPrimaryGoal();

    FString Best;

    if (Danger >= 0.8f)
    {
        Best = TEXT("Survive");
    }
    else if (Danger >= 0.5f)
    {
        if (PrimaryGoal.Description.Contains(TEXT("Combat")))
        {
            Best = TEXT("Engage");
        }
        else
        {
            Best = TEXT("Evade");
        }
    }
    else if (CurrentAwareness.BestOpportunity.OpportunityValue > 0.6f)
    {
        Best = TEXT("Exploit");
    }
    else if (PrimaryGoal.GoalID.Len() > 0)
    {
        Best = TEXT("PursueGoal");
    }
    else
    {
        Best = TEXT("Explore");
    }

    SetStrategy(Best);
    return Best;
}

// -------------------------------------------------------
// Autonomy Control
// -------------------------------------------------------

void UAutonomousAgentController::SetAutonomyLevel(EAutonomyLevel Level)
{
    if (Level == AutonomyLevel) { return; }
    const EAutonomyLevel Old = AutonomyLevel;
    AutonomyLevel = Level;
    OnAutonomyLevelChanged.Broadcast(Old, Level);
}

bool UAutonomousAgentController::IsFullyAutonomous() const
{
    return AutonomyLevel == EAutonomyLevel::FullAutonomy && HumanOverrideTimer <= 0.0f;
}

void UAutonomousAgentController::HumanOverride(float Duration)
{
    HumanOverrideTimer = FMath::Max(Duration, 0.0f);
    if (VirtualCNS)
    {
        FControllerOutputCommand NullCmd;
        VirtualCNS->OverrideMotorOutput(NullCmd);
    }
}

// -------------------------------------------------------
// Learning & Adaptation
// -------------------------------------------------------

void UAutonomousAgentController::ProvideReward(float Reward, const FString& Reason)
{
    PerfStats.CumulativeReward += Reward;

    if (bContinuousLearning)
    {
        ApplyRewardToLearning(Reward);
    }

    // Create belief about the rewarding action in DTE memory
    if (DTECore && Reward > 0.1f)
    {
        DTECore->CreateBelief(TEXT("GoodAction:") + Reason,
                              FMath::Clamp(Reward, 0.0f, 1.0f));
    }
}

void UAutonomousAgentController::OnEpisodeEnd(bool bSuccess, float TotalReward)
{
    PerfStats.EpisodesCompleted++;

    const float Alpha = 0.1f;
    PerfStats.AvgEpisodeReward =
        PerfStats.AvgEpisodeReward * (1.0f - Alpha) + TotalReward * Alpha;

    if (RLBridge)
    {
        // Provide terminal reward and decay exploration at episode boundary
        const float TerminalReward = bSuccess ? 1.0f : -0.5f;
        const TArray<float> EmptyState;
        const FRLAction DummyAction;
        RLBridge->RecordTransition(EmptyState, DummyAction, TerminalReward,
                                   EmptyState, /*bTerminal=*/true);
        RLBridge->PerformBatchUpdate();
        RLBridge->DecayExploration();
    }

    // Consolidate episodic memory
    if (DTECore)
    {
        DTECore->TriggerMemoryConsolidation();
    }
}

// -------------------------------------------------------
// Component Wiring
// -------------------------------------------------------

void UAutonomousAgentController::SetVirtualCNS(UVirtualCNS* CNS)
{
    VirtualCNS = CNS;
}

void UAutonomousAgentController::SetDTECore(UDeepTreeEchoCore* Core)
{
    DTECore = Core;
}

// -------------------------------------------------------
// PRIVATE — FindAndCacheComponents
// -------------------------------------------------------

void UAutonomousAgentController::FindAndCacheComponents()
{
    AActor* Owner = GetOwner();
    if (!Owner) { return; }

    if (!VirtualCNS)       { VirtualCNS    = Owner->FindComponentByClass<UVirtualCNS>(); }
    if (!DTECore)          { DTECore       = Owner->FindComponentByClass<UDeepTreeEchoCore>(); }
    if (!RLBridge)         { RLBridge      = Owner->FindComponentByClass<UReinforcementLearningBridge>(); }
    if (!TrainingEnv)      { TrainingEnv   = Owner->FindComponentByClass<UGameTrainingEnvironment>(); }
    if (!SkillSystem)      { SkillSystem   = Owner->FindComponentByClass<UGameSkillTrainingSystem>(); }
    if (!AttentionSys)     { AttentionSys  = Owner->FindComponentByClass<UAttentionSystem>(); }
    if (!PlanningSys)      { PlanningSys   = Owner->FindComponentByClass<UPlanningSystem>(); }
    if (!LearningSystem)   { LearningSystem= Owner->FindComponentByClass<UOnlineLearningSystem>(); }
}

// -------------------------------------------------------
// PRIVATE — RegisterInitialGoals
// -------------------------------------------------------

void UAutonomousAgentController::RegisterInitialGoals()
{
    for (const FCognitiveGoal& Goal : InitialGoals)
    {
        GoalRegistry.Add(Goal.GoalID, Goal);
    }

    // Auto-activate goals with no prerequisites
    for (auto& KV : GoalRegistry)
    {
        if (KV.Value.PrerequisiteIDs.IsEmpty())
        {
            ActivateGoal(KV.Key);
        }
    }
}

// -------------------------------------------------------
// PRIVATE — EvaluateGoals
// -------------------------------------------------------

void UAutonomousAgentController::EvaluateGoals(float DeltaTime)
{
    (void)DeltaTime;

    for (auto& KV : GoalRegistry)
    {
        FCognitiveGoal& Goal = KV.Value;
        if (Goal.bCompleted) { continue; }

        // Check deadline
        if (Goal.bActive && Goal.DeadlineSeconds > 0.0f &&
            Goal.ElapsedTime >= Goal.DeadlineSeconds)
        {
            CompleteGoal(Goal.GoalID, false); // timed out → failure
            continue;
        }

        // Activate goals whose prerequisites are now met
        if (!Goal.bActive)
        {
            bool AllMet = true;
            for (const FString& PrereqID : Goal.PrerequisiteIDs)
            {
                const FCognitiveGoal* Prereq = GoalRegistry.Find(PrereqID);
                if (!Prereq || !Prereq->bCompleted) { AllMet = false; break; }
            }
            if (AllMet)
            {
                ActivateGoal(Goal.GoalID);
            }
        }
    }
}

// -------------------------------------------------------
// PRIVATE — UpdateStrategy
// -------------------------------------------------------

void UAutonomousAgentController::UpdateStrategy(float /*DeltaTime*/)
{
    SelectOptimalStrategy();
}

// -------------------------------------------------------
// PRIVATE — UpdateAwarenessModel
// -------------------------------------------------------

void UAutonomousAgentController::UpdateAwarenessModel(float DeltaTime)
{
    // Decay stale entity confidence
    TArray<FString> ToRemove;
    for (FSituationalEntity& E : CurrentAwareness.Entities)
    {
        E.TimeSinceObserved += DeltaTime;

        // Degrade confidence over time (forget stale observations)
        E.Confidence = FMath::Max(0.0f, E.Confidence - DeltaTime * 0.1f);

        // Remove entities that have been lost for more than 5 seconds
        if (E.TimeSinceObserved > 5.0f && E.Confidence < 0.05f)
        {
            ToRemove.Add(E.EntityID);
        }
    }
    for (const FString& ID : ToRemove)
    {
        ForgetEntity(ID);
    }

    // Recompute aggregate statistics
    CurrentAwareness.DangerIndex      = ComputeDangerIndex();
    CurrentAwareness.DominantThreat   = FindDominantThreat();
    CurrentAwareness.BestOpportunity  = FindBestOpportunity();

    // Count entities in range
    const AActor* Owner = GetOwner();
    if (Owner)
    {
        const FVector MyPos = Owner->GetActorLocation();
        int32 Count = 0;
        for (const FSituationalEntity& E : CurrentAwareness.Entities)
        {
            if (FVector::Dist(MyPos, E.Position) <= EngagementRange) { Count++; }
        }
        CurrentAwareness.EntitiesInRange = Count;
    }

    CurrentAwareness.LastUpdateTime =
        GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    OnSituationalAwarenessUpdated.Broadcast(CurrentAwareness);
}

// -------------------------------------------------------
// PRIVATE — SyncContextToCognition
// -------------------------------------------------------

void UAutonomousAgentController::SyncContextToCognition()
{
    if (!DTECore) { return; }

    // Build salience map from situational awareness
    TMap<FString, float> Salience;
    for (const FSituationalEntity& E : CurrentAwareness.Entities)
    {
        Salience.Add(E.EntityID, FMath::Max(E.ThreatLevel, E.OpportunityValue));
    }
    Salience.Add(TEXT("Strategy_") + CurrentStrategy, 1.0f);

    DTECore->UpdateSalienceLandscape(Salience);

    // Update relevance frame with current strategy
    TArray<FString> Constraints;
    Constraints.Add(CurrentStrategy);
    const FCognitiveGoal Primary = GetCurrentPrimaryGoal();
    if (Primary.GoalID.Len() > 0)
    {
        Constraints.Add(Primary.Description);
    }
    DTECore->UpdateRelevanceFrame(CurrentStrategy, Constraints);

    // Update embedded state with environmental affordances
    TArray<FString> Affordances;
    for (const FSituationalEntity& E : CurrentAwareness.Entities)
    {
        if (E.OpportunityValue > 0.3f)
        {
            Affordances.Add(TEXT("Opportunity:") + E.EntityID);
        }
    }
    DTECore->UpdateEmbeddedState(Affordances, CurrentStrategy);
}

// -------------------------------------------------------
// PRIVATE — PropagateThreatsToVirtualCNS
// -------------------------------------------------------

void UAutonomousAgentController::PropagateThreatsToVirtualCNS()
{
    if (!VirtualCNS) { return; }

    const FSituationalEntity& Threat = CurrentAwareness.DominantThreat;
    if (Threat.ThreatLevel < 0.1f) { return; }

    FThreatSignal Signal;
    Signal.ThreatLevel   = Threat.ThreatLevel;
    Signal.Distance      = FVector::Dist(
        GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector,
        Threat.Position);
    Signal.ThreatType    = Threat.Type;
    Signal.ThreatDirection = (Threat.Position - (GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector)).GetSafeNormal();

    // Estimate time to impact based on velocity and distance
    const float RelativeSpeed = (Threat.Velocity - (GetOwner() ? FVector::ZeroVector : FVector::ZeroVector)).Size();
    Signal.TimeToImpact = (RelativeSpeed > 1.0f) ? Signal.Distance / RelativeSpeed : 9999.0f;

    // Map threat direction to reflex
    const FVector Right = GetOwner() ? GetOwner()->GetActorRightVector() : FVector(0, 1, 0);
    const float DotRight = FVector::DotProduct(Signal.ThreatDirection, Right);
    if (DotRight > 0.3f)
    {
        Signal.SuggestedReflex = TEXT("ThreatRight");
    }
    else if (DotRight < -0.3f)
    {
        Signal.SuggestedReflex = TEXT("ThreatLeft");
    }
    else
    {
        Signal.SuggestedReflex = TEXT("ThreatFront");
    }

    VirtualCNS->SubmitThreatSignal(Signal);
}

// -------------------------------------------------------
// PRIVATE — ComputeDangerIndex
// -------------------------------------------------------

float UAutonomousAgentController::ComputeDangerIndex() const
{
    float Max = 0.0f;
    float WeightedSum = 0.0f;
    float WeightTotal = 0.0f;

    for (const FSituationalEntity& E : CurrentAwareness.Entities)
    {
        const float W = E.Confidence;
        WeightedSum += E.ThreatLevel * W;
        WeightTotal += W;
        Max = FMath::Max(Max, E.ThreatLevel * E.Confidence);
    }

    const float Avg = (WeightTotal > 0.0f) ? WeightedSum / WeightTotal : 0.0f;
    // Blend max with avg for a responsive-but-stable danger index
    return FMath::Clamp(0.6f * Max + 0.4f * Avg, 0.0f, 1.0f);
}

// -------------------------------------------------------
// PRIVATE — FindDominantThreat
// -------------------------------------------------------

FSituationalEntity UAutonomousAgentController::FindDominantThreat() const
{
    FSituationalEntity Best;
    float BestScore = -1.0f;
    for (const FSituationalEntity& E : CurrentAwareness.Entities)
    {
        const float Score = E.ThreatLevel * E.Confidence;
        if (Score > BestScore)
        {
            BestScore = Score;
            Best = E;
        }
    }
    return Best;
}

// -------------------------------------------------------
// PRIVATE — FindBestOpportunity
// -------------------------------------------------------

FSituationalEntity UAutonomousAgentController::FindBestOpportunity() const
{
    FSituationalEntity Best;
    float BestScore = -1.0f;
    for (const FSituationalEntity& E : CurrentAwareness.Entities)
    {
        const float Score = E.OpportunityValue * E.Confidence;
        if (Score > BestScore)
        {
            BestScore = Score;
            Best = E;
        }
    }
    return Best;
}

// -------------------------------------------------------
// PRIVATE — ApplyRewardToLearning
// -------------------------------------------------------

void UAutonomousAgentController::ApplyRewardToLearning(float Reward)
{
    if (!RLBridge) { return; }

    // Build a minimal state vector from current awareness
    TArray<float> State;
    State.Add(CurrentAwareness.DangerIndex);
    State.Add(CurrentAwareness.PositionScore);
    State.Add(static_cast<float>(CurrentAwareness.EntitiesInRange) * 0.1f);

    const FCognitiveGoal Primary = GetCurrentPrimaryGoal();
    State.Add(Primary.Progress);
    State.Add(Primary.Priority);

    // Record reward for the last selected action
    const FRLAction LastAction = RLBridge->GetGreedyAction(State);
    TArray<float> NextState = State; // simplified: state hasn't changed this tick
    RLBridge->RecordTransition(State, LastAction, Reward, NextState, false);
    RLBridge->PerformLearningUpdate();
}
