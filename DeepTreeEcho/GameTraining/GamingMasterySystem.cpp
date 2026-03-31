// GamingMasterySystem.cpp
// Deep Tree Echo — Gaming Mastery System Implementation

#include "GameTraining/GamingMasterySystem.h"
#include "CNS/VirtualCNS.h"
#include "Core/DeepTreeEchoCore.h"
#include "GameTraining/ReinforcementLearningBridge.h"
#include "Attention/AttentionSystem.h"
#include "GameFramework/Actor.h"
#include "Math/UnrealMathUtility.h"
#include "Algo/Sort.h"

// -------------------------------------------------------
// Constructor
// -------------------------------------------------------

UGamingMasterySystem::UGamingMasterySystem()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

// -------------------------------------------------------
// BeginPlay
// -------------------------------------------------------

void UGamingMasterySystem::BeginPlay()
{
    Super::BeginPlay();
    FindAndCacheComponents();
    InitialiseDefaultCombos();
    InitialiseCounterTable();
    bInitialised = true;
}

// -------------------------------------------------------
// TickComponent
// -------------------------------------------------------

void UGamingMasterySystem::TickComponent(float DeltaTime,
                                          ELevelTick TickType,
                                          FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bInitialised) { return; }

    UpdateTimingWindows(DeltaTime);
    UpdateComboStateMachine(DeltaTime);
    UpdateMetrics(DeltaTime);
}

// -------------------------------------------------------
// Lightning Reflex Engine
// -------------------------------------------------------

void UGamingMasterySystem::OnStimulusDetected(const FString& StimulusType,
                                               float Urgency)
{
    Metrics.TotalActions++;

    // Route high-urgency stimuli directly to reflex arc via VirtualCNS
    if (VirtualCNS && Urgency >= 0.7f)
    {
        VirtualCNS->FireReflex(StimulusType);
    }

    // Spotlight attention on the stimulus
    if (AttentionSys)
    {
        // AttentionSystem would receive a salience spike here
    }
}

FControllerOutputCommand UGamingMasterySystem::SpeculativelyPreExecute(
    const TArray<float>& GameState)
{
    if (!bSpeculativeExecution || !RLBridge)
    {
        return FControllerOutputCommand{};
    }

    // Use the RL bridge's greedy policy to predict the best action
    const FRLAction Action = RLBridge->GetGreedyAction(GameState);

    if (Action.Probability < MinPredictionConfidence)
    {
        return FControllerOutputCommand{};
    }

    FControllerOutputCommand Cmd = RLBridge->ActionToControllerOutput(Action);
    Cmd.Confidence = Action.Probability;
    return Cmd;
}

void UGamingMasterySystem::RecordReflexLatency(float LatencyMs)
{
    const float Alpha = 0.05f;
    Metrics.AvgReflexMs =
        Metrics.AvgReflexMs * (1.0f - Alpha) + LatencyMs * Alpha;
}

EReflexSpeedTier UGamingMasterySystem::GetCurrentReflexTier() const
{
    if (Metrics.AvgReflexMs < 4.0f)   { return EReflexSpeedTier::Instantaneous; }
    if (Metrics.AvgReflexMs < 16.0f)  { return EReflexSpeedTier::Fast; }
    if (Metrics.AvgReflexMs < 50.0f)  { return EReflexSpeedTier::Normal; }
    return EReflexSpeedTier::Considered;
}

// -------------------------------------------------------
// Combo Mastery Engine
// -------------------------------------------------------

void UGamingMasterySystem::RegisterCombo(const FComboDefinition& Combo)
{
    for (FComboDefinition& Existing : ComboCatalog)
    {
        if (Existing.ComboID == Combo.ComboID)
        {
            Existing = Combo;
            return;
        }
    }
    ComboCatalog.Add(Combo);
}

void UGamingMasterySystem::AdvanceComboState(const FString& ActionName)
{
    // Check if no combo is in progress — try to start one
    if (ActiveComboID.IsEmpty())
    {
        for (const FComboDefinition& Combo : ComboCatalog)
        {
            if (Combo.ActionSequence.Num() > 0 &&
                Combo.ActionSequence[0] == ActionName)
            {
                ActiveComboID = Combo.ComboID;
                ComboProgress = 1;
                ComboTimer    = 0.0f;
                return;
            }
        }
        return;
    }

    // Advance the active combo
    const FComboDefinition* Combo = FindComboConst(ActiveComboID);
    if (!Combo)
    {
        ActiveComboID.Empty();
        ComboProgress = 0;
        return;
    }

    if (ComboProgress < Combo->ActionSequence.Num() &&
        Combo->ActionSequence[ComboProgress] == ActionName)
    {
        ComboProgress++;
        ComboTimer = 0.0f;

        // Combo complete
        if (ComboProgress >= Combo->ActionSequence.Num())
        {
            FComboDefinition* MutableCombo = FindCombo(ActiveComboID);
            if (MutableCombo)
            {
                MutableCombo->ExecutionCount++;
                MutableCombo->SuccessCount++;
                MutableCombo->Proficiency =
                    FMath::Min(1.0f, static_cast<float>(MutableCombo->SuccessCount) /
                                     FMath::Max(1, MutableCombo->ExecutionCount));
            }
            OnComboExecuted.Broadcast(ActiveComboID, false);
            ActiveComboID.Empty();
            ComboProgress = 0;
        }
    }
    else
    {
        // Wrong input — reset
        FComboDefinition* MutableCombo = FindCombo(ActiveComboID);
        if (MutableCombo) { MutableCombo->ExecutionCount++; }
        ActiveComboID.Empty();
        ComboProgress = 0;
        // Try to start fresh with this new action
        AdvanceComboState(ActionName);
    }
}

FString UGamingMasterySystem::GetNextComboAction() const
{
    if (ActiveComboID.IsEmpty()) { return TEXT(""); }
    const FComboDefinition* Combo = FindComboConst(ActiveComboID);
    if (!Combo || ComboProgress >= Combo->ActionSequence.Num()) { return TEXT(""); }
    return Combo->ActionSequence[ComboProgress];
}

TArray<FControllerOutputCommand> UGamingMasterySystem::GetComboCompletionSequence() const
{
    TArray<FControllerOutputCommand> Commands;
    if (ActiveComboID.IsEmpty()) { return Commands; }

    const FComboDefinition* Combo = FindComboConst(ActiveComboID);
    if (!Combo) { return Commands; }

    for (int32 i = ComboProgress; i < Combo->ActionSequence.Num(); ++i)
    {
        FControllerOutputCommand Cmd;
        Cmd.ActionName = Combo->ActionSequence[i];
        Cmd.Duration   = Combo->MaxTimeBetweenActions * 0.8f;
        Cmd.Priority   = 5.0f;
        Cmd.Confidence = Combo->Proficiency;
        Commands.Add(Cmd);
    }
    return Commands;
}

void UGamingMasterySystem::RecordComboOutcome(const FString& ComboID, bool bSuccess)
{
    FComboDefinition* Combo = FindCombo(ComboID);
    if (!Combo) { return; }

    Combo->ExecutionCount++;
    if (bSuccess) { Combo->SuccessCount++; }
    Combo->Proficiency = (Combo->ExecutionCount > 0)
        ? static_cast<float>(Combo->SuccessCount) / Combo->ExecutionCount
        : 0.0f;

    // Update metrics
    float Total = 0.0f, Count = 0.0f;
    for (const FComboDefinition& C : ComboCatalog)
    {
        if (C.ExecutionCount > 0)
        {
            Total += static_cast<float>(C.SuccessCount) / C.ExecutionCount;
            Count++;
        }
    }
    Metrics.ComboSuccessRate = (Count > 0.0f) ? Total / Count : 0.0f;
}

TArray<FComboDefinition> UGamingMasterySystem::GetComboCatalogSortedByProficiency() const
{
    TArray<FComboDefinition> Sorted = ComboCatalog;
    Sorted.Sort([](const FComboDefinition& A, const FComboDefinition& B)
    {
        return A.Proficiency > B.Proficiency;
    });
    return Sorted;
}

// -------------------------------------------------------
// Opponent Modelling
// -------------------------------------------------------

void UGamingMasterySystem::ObserveOpponentAction(const FString& OpponentID,
                                                   const FString& ActionName)
{
    FOpponentModel& Model = OpponentModels.FindOrAdd(OpponentID);
    Model.OpponentID = OpponentID;
    Model.ObservedMoves++;

    int32& Count = Model.ActionHistory.FindOrAdd(ActionName);
    Count++;

    // Update predicted action
    Model.PredictedAction   = SelectActionByFrequency(Model.ActionHistory);
    const int32 TotalMoves  = Model.ObservedMoves;
    const int32 PredCount   = Model.ActionHistory.FindRef(Model.PredictedAction);
    Model.PredictionConfidence = (TotalMoves > 0)
        ? static_cast<float>(PredCount) / TotalMoves
        : 0.0f;

    // Detect exploitable patterns (action that appears > 40% of the time)
    Model.ExploitablePatterns.Empty();
    for (const auto& KV : Model.ActionHistory)
    {
        const float Freq = static_cast<float>(KV.Value) / TotalMoves;
        if (Freq > 0.4f)
        {
            Model.ExploitablePatterns.Add(KV.Key + TEXT(":") + FString::SanitizeFloat(Freq));
        }
    }

    if (Model.PredictionConfidence >= MinPredictionConfidence)
    {
        OnOpponentActionPredicted.Broadcast(OpponentID, Model.PredictedAction);
    }
}

FString UGamingMasterySystem::PredictOpponentAction(const FString& OpponentID) const
{
    const FOpponentModel* Model = OpponentModels.Find(OpponentID);
    if (!Model || Model->ObservedMoves == 0) { return TEXT("Unknown"); }
    return Model->PredictedAction;
}

FOpponentModel UGamingMasterySystem::GetOpponentModel(const FString& OpponentID) const
{
    const FOpponentModel* Model = OpponentModels.Find(OpponentID);
    return Model ? *Model : FOpponentModel{};
}

FString UGamingMasterySystem::GetBestCounter(const FString& OpponentID) const
{
    const FString Predicted = PredictOpponentAction(OpponentID);
    const FString* Counter  = CounterTable.Find(Predicted);
    return Counter ? *Counter : TEXT("Neutral");
}

void UGamingMasterySystem::RecordCounterOutcome(const FString& OpponentID,
                                                  bool bSuccess)
{
    FOpponentModel* Model = OpponentModels.Find(OpponentID);
    if (!Model) { return; }

    if (bSuccess) { Model->SuccessfulCounters++; }
    Model->CounterSuccessRate = (Model->ObservedMoves > 0)
        ? static_cast<float>(Model->SuccessfulCounters) / Model->ObservedMoves
        : 0.0f;

    // Update prediction accuracy metric
    const float Alpha = 0.05f;
    Metrics.PredictionAccuracy =
        Metrics.PredictionAccuracy * (1.0f - Alpha) + (bSuccess ? 1.0f : 0.0f) * Alpha;
}

// -------------------------------------------------------
// Timing Windows
// -------------------------------------------------------

void UGamingMasterySystem::OpenTimingWindow(const FTimingWindow& Window)
{
    // Replace if already exists
    for (FTimingWindow& W : ActiveTimingWindows)
    {
        if (W.ActionName == Window.ActionName)
        {
            W = Window;
            W.ElapsedTime = 0.0f;
            OnTimingWindowOpened.Broadcast(W);
            return;
        }
    }
    FTimingWindow New = Window;
    New.ElapsedTime = 0.0f;
    ActiveTimingWindows.Add(New);
    OnTimingWindowOpened.Broadcast(New);
}

bool UGamingMasterySystem::IsInPerfectWindow(const FString& ActionName) const
{
    for (const FTimingWindow& W : ActiveTimingWindows)
    {
        if (W.ActionName == ActionName && W.IsOpen())
        {
            return W.IsPerfect();
        }
    }
    return false;
}

float UGamingMasterySystem::GetWindowRemainingTime(const FString& ActionName) const
{
    for (const FTimingWindow& W : ActiveTimingWindows)
    {
        if (W.ActionName == ActionName && W.IsOpen())
        {
            return W.LatestTime - W.ElapsedTime;
        }
    }
    return 0.0f;
}

void UGamingMasterySystem::CloseTimingWindow(const FString& ActionName)
{
    ActiveTimingWindows.RemoveAll([&ActionName](const FTimingWindow& W)
    {
        return W.ActionName == ActionName;
    });
}

// -------------------------------------------------------
// Component Wiring
// -------------------------------------------------------

void UGamingMasterySystem::SetVirtualCNS(UVirtualCNS* CNS)
{
    VirtualCNS = CNS;
}

void UGamingMasterySystem::SetDTECore(UDeepTreeEchoCore* Core)
{
    DTECore = Core;
}

// -------------------------------------------------------
// PRIVATE — FindAndCacheComponents
// -------------------------------------------------------

void UGamingMasterySystem::FindAndCacheComponents()
{
    AActor* Owner = GetOwner();
    if (!Owner) { return; }

    if (!VirtualCNS)   { VirtualCNS   = Owner->FindComponentByClass<UVirtualCNS>(); }
    if (!DTECore)      { DTECore      = Owner->FindComponentByClass<UDeepTreeEchoCore>(); }
    if (!RLBridge)     { RLBridge     = Owner->FindComponentByClass<UReinforcementLearningBridge>(); }
    if (!AttentionSys) { AttentionSys = Owner->FindComponentByClass<UAttentionSystem>(); }
}

// -------------------------------------------------------
// PRIVATE — InitialiseDefaultCombos
// -------------------------------------------------------

void UGamingMasterySystem::InitialiseDefaultCombos()
{
    if (!ComboCatalog.IsEmpty()) { return; }

    // ── Basic Attack Combo ────────────────────────────────
    {
        FComboDefinition Combo;
        Combo.ComboID    = TEXT("BasicAttack3");
        Combo.ComboName  = TEXT("Three-Hit Basic");
        Combo.ActionSequence = { TEXT("LightAttack"), TEXT("LightAttack"), TEXT("HeavyAttack") };
        Combo.MaxTimeBetweenActions = 0.4f;
        Combo.Difficulty   = 0.2f;
        Combo.ExpectedReward = 0.6f;
        ComboCatalog.Add(Combo);
    }

    // ── Rising Launcher ───────────────────────────────────
    {
        FComboDefinition Combo;
        Combo.ComboID    = TEXT("RisingLauncher");
        Combo.ComboName  = TEXT("Rising Launcher");
        Combo.ActionSequence = {
            TEXT("LightAttack"), TEXT("LightAttack"),
            TEXT("Jump"), TEXT("HeavyAttack")
        };
        Combo.MaxTimeBetweenActions = 0.35f;
        Combo.Difficulty   = 0.4f;
        Combo.ExpectedReward = 1.0f;
        ComboCatalog.Add(Combo);
    }

    // ── Dodge Cancel ──────────────────────────────────────
    {
        FComboDefinition Combo;
        Combo.ComboID    = TEXT("DodgeCancel");
        Combo.ComboName  = TEXT("Dodge Cancel Punish");
        Combo.ActionSequence = {
            TEXT("DodgeForward"), TEXT("LightAttack"), TEXT("HeavyFinisher")
        };
        Combo.MaxTimeBetweenActions = 0.25f;
        Combo.Difficulty   = 0.6f;
        Combo.ExpectedReward = 1.2f;
        ComboCatalog.Add(Combo);
    }

    // ── Parry Counter ─────────────────────────────────────
    {
        FComboDefinition Combo;
        Combo.ComboID    = TEXT("ParryCounter");
        Combo.ComboName  = TEXT("Perfect Parry Counter");
        Combo.ActionSequence = {
            TEXT("PerfectParry"), TEXT("HeavyAttack"), TEXT("SpecialFinisher")
        };
        Combo.MaxTimeBetweenActions = 0.2f;
        Combo.Difficulty   = 0.8f;
        Combo.ExpectedReward = 2.0f;
        ComboCatalog.Add(Combo);
    }
}

// -------------------------------------------------------
// PRIVATE — InitialiseCounterTable
// -------------------------------------------------------

void UGamingMasterySystem::InitialiseCounterTable()
{
    CounterTable.Add(TEXT("HighAttack"),    TEXT("DuckUnder"));
    CounterTable.Add(TEXT("LowSweep"),      TEXT("JumpDodge"));
    CounterTable.Add(TEXT("ProjectileHigh"),TEXT("BlockHigh"));
    CounterTable.Add(TEXT("ProjectileLow"), TEXT("BlockLow"));
    CounterTable.Add(TEXT("Grab"),          TEXT("DodgeBack"));
    CounterTable.Add(TEXT("ChargeAttack"),  TEXT("PerfectParry"));
    CounterTable.Add(TEXT("ComboOpener"),   TEXT("InterruptLow"));
    CounterTable.Add(TEXT("Unknown"),       TEXT("Neutral"));
}

// -------------------------------------------------------
// PRIVATE — UpdateTimingWindows
// -------------------------------------------------------

void UGamingMasterySystem::UpdateTimingWindows(float DeltaTime)
{
    // Advance elapsed time and remove expired windows
    TArray<int32> ToRemove;
    for (int32 i = 0; i < ActiveTimingWindows.Num(); ++i)
    {
        FTimingWindow& W = ActiveTimingWindows[i];
        W.ElapsedTime += DeltaTime;

        if (!W.IsOpen())
        {
            ToRemove.Add(i);
        }
    }
    // Remove in reverse order
    for (int32 i = ToRemove.Num() - 1; i >= 0; --i)
    {
        ActiveTimingWindows.RemoveAt(ToRemove[i]);
    }
}

// -------------------------------------------------------
// PRIVATE — UpdateComboStateMachine
// -------------------------------------------------------

void UGamingMasterySystem::UpdateComboStateMachine(float DeltaTime)
{
    if (ActiveComboID.IsEmpty()) { return; }

    const FComboDefinition* Combo = FindComboConst(ActiveComboID);
    if (!Combo) { ActiveComboID.Empty(); ComboProgress = 0; return; }

    ComboTimer += DeltaTime;
    if (ComboTimer > Combo->MaxTimeBetweenActions)
    {
        // Timed out — reset combo
        FComboDefinition* MutableCombo = FindCombo(ActiveComboID);
        if (MutableCombo) { MutableCombo->ExecutionCount++; }
        ActiveComboID.Empty();
        ComboProgress = 0;
        ComboTimer    = 0.0f;
    }
}

// -------------------------------------------------------
// PRIVATE — UpdateMetrics
// -------------------------------------------------------

void UGamingMasterySystem::UpdateMetrics(float DeltaTime)
{
    MetricsTimer += DeltaTime;
    if (MetricsTimer < 1.0f) { return; }
    MetricsTimer = 0.0f;

    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    UpdateAPM(Now);
    AdvanceSkillTier();
    OnMasteryMetricsUpdated.Broadcast(Metrics);
}

// -------------------------------------------------------
// PRIVATE — UpdateAPM
// -------------------------------------------------------

void UGamingMasterySystem::UpdateAPM(float CurrentTime)
{
    // Remove timestamps older than 60 seconds
    Metrics.RecentActionTimestamps.RemoveAll([CurrentTime](float T)
    {
        return CurrentTime - T > 60.0f;
    });

    Metrics.APM = static_cast<float>(Metrics.RecentActionTimestamps.Num());
}

// -------------------------------------------------------
// PRIVATE — AdvanceSkillTier
// -------------------------------------------------------

void UGamingMasterySystem::AdvanceSkillTier()
{
    // Composite score from APM, reflex speed, combo success, prediction
    const float ApmScore     = FMath::Clamp(Metrics.APM / TargetAPM, 0.0f, 1.0f);
    const float ReflexScore  = 1.0f - FMath::Clamp(Metrics.AvgReflexMs / 100.0f, 0.0f, 1.0f);
    const float ComboScore   = Metrics.ComboSuccessRate;
    const float PredScore    = Metrics.PredictionAccuracy;
    const float TimingScore  = Metrics.PerfectTimingRate;

    const float Composite = (ApmScore + ReflexScore + ComboScore + PredScore + TimingScore) / 5.0f;
    const int32 NewTier   = FMath::Clamp(static_cast<int32>(Composite * 10.0f), 0, 10);

    if (NewTier != Metrics.SkillTier)
    {
        const int32 Old = Metrics.SkillTier;
        Metrics.SkillTier = NewTier;
        if (NewTier > Old)
        {
            OnSkillTierAdvanced.Broadcast(Old, NewTier);
        }
    }
}

// -------------------------------------------------------
// PRIVATE — FindCombo helpers
// -------------------------------------------------------

FComboDefinition* UGamingMasterySystem::FindCombo(const FString& ComboID)
{
    for (FComboDefinition& C : ComboCatalog)
    {
        if (C.ComboID == ComboID) { return &C; }
    }
    return nullptr;
}

const FComboDefinition* UGamingMasterySystem::FindComboConst(const FString& ComboID) const
{
    for (const FComboDefinition& C : ComboCatalog)
    {
        if (C.ComboID == ComboID) { return &C; }
    }
    return nullptr;
}

// -------------------------------------------------------
// PRIVATE — SelectActionByFrequency
// -------------------------------------------------------

FString UGamingMasterySystem::SelectActionByFrequency(
    const TMap<FString, int32>& ActionHistory) const
{
    FString Best;
    int32 BestCount = -1;
    for (const auto& KV : ActionHistory)
    {
        if (KV.Value > BestCount)
        {
            BestCount = KV.Value;
            Best = KV.Key;
        }
    }
    return Best;
}
