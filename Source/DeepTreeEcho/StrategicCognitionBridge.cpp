// StrategicCognitionBridge.cpp
// Deep Tree Echo - Strategic Cognition Bridge for Gaming Mastery
// Copyright (c) 2025 Deep Tree Echo Project

#include "StrategicCognitionBridge.h"

UStrategicCognitionBridge::UStrategicCognitionBridge()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.05f; // 20Hz for responsive cognitive tracking
}

void UStrategicCognitionBridge::BeginPlay()
{
    Super::BeginPlay();

    // Initialize cognitive load levels
    CognitiveLoad.LoadLevels.Add(ECognitiveLoadType::Perceptual, 0.0f);
    CognitiveLoad.LoadLevels.Add(ECognitiveLoadType::WorkingMemory, 0.0f);
    CognitiveLoad.LoadLevels.Add(ECognitiveLoadType::DecisionMaking, 0.0f);
    CognitiveLoad.LoadLevels.Add(ECognitiveLoadType::MotorExecution, 0.0f);
    CognitiveLoad.LoadLevels.Add(ECognitiveLoadType::TimePressure, 0.0f);
    CognitiveLoad.LoadLevels.Add(ECognitiveLoadType::SocialCoordination, 0.0f);

    // Find Gaming Mastery System if not set
    if (!GamingMasterySystem)
    {
        GamingMasterySystem = GetOwner()->FindComponentByClass<UGamingMasterySystem>();
    }

    UE_LOG(LogTemp, Log, TEXT("StrategicCognitionBridge initialized - 4E Gaming Integration Active"));
}

void UStrategicCognitionBridge::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Update cognitive reserve
    UpdateCognitiveReserve(DeltaTime);

    // Track time at current load
    CognitiveLoad.TimeAtCurrentLoad += DeltaTime;

    // Store load history for trend analysis
    CognitiveLoadHistory.Add(CognitiveLoad.TotalLoad);
    if (CognitiveLoadHistory.Num() > 600) // Keep 30 seconds at 20Hz
    {
        CognitiveLoadHistory.RemoveAt(0);
    }

    // Check for cognitive overload
    bool bCurrentlyOverloaded = IsCognitiveOverloadImminent();
    if (bCurrentlyOverloaded && !bWasOverloaded)
    {
        OnCognitiveOverload.Broadcast(CognitiveLoad.TotalLoad);
    }
    bWasOverloaded = bCurrentlyOverloaded;

    // Update attention sustained duration
    AttentionState.SustainedAttentionDuration += DeltaTime;

    // Decay physical fatigue slowly during rest
    if (EmbodiedState.MotorReadiness < 0.3f)
    {
        EmbodiedState.PhysicalFatigue = FMath::Max(0.0f, EmbodiedState.PhysicalFatigue - DeltaTime * 0.01f);
    }
}

// ========================================
// EMBODIED COGNITION API
// ========================================

void UStrategicCognitionBridge::UpdateEmbodiedState(float ReactionTime, float InputAccuracy, float MovementSmoothing)
{
    // Convert reaction time to readiness (faster = higher readiness)
    // Assuming 150ms is excellent, 300ms is average
    EmbodiedState.MotorReadiness = FMath::Clamp(1.0f - (ReactionTime - 0.15f) / 0.15f, 0.0f, 1.0f);

    // Update hand-eye coordination from input accuracy
    EmbodiedState.HandEyeCoordination = EmbodiedState.HandEyeCoordination * 0.9f + InputAccuracy * 0.1f;

    // Update proprioceptive accuracy from movement smoothing
    EmbodiedState.ProprioceptiveAccuracy = EmbodiedState.ProprioceptiveAccuracy * 0.9f + MovementSmoothing * 0.1f;

    // Accumulate fatigue based on activity
    if (EmbodiedState.MotorReadiness > 0.7f)
    {
        EmbodiedState.PhysicalFatigue = FMath::Min(1.0f, EmbodiedState.PhysicalFatigue + 0.001f);
    }
}

void UStrategicCognitionBridge::RecordMotorExecution(const FString& ActionType, float ExecutionQuality, float TimingAccuracy)
{
    // Store execution quality for muscle memory calculation
    if (!MotorExecutionHistory.Contains(ActionType))
    {
        MotorExecutionHistory.Add(ActionType, TArray<float>());
    }

    TArray<float>& History = MotorExecutionHistory[ActionType];
    History.Add(ExecutionQuality * TimingAccuracy);

    // Keep last 100 executions
    if (History.Num() > 100)
    {
        History.RemoveAt(0);
    }

    // Check for muscle memory formation
    float MemoryStrength = CalculateMuscleMemoryStrength(ActionType);
    if (MemoryStrength > 0.8f && History.Num() >= 50)
    {
        OnMuscleMemoryFormed.Broadcast(ActionType, MemoryStrength);
    }

    // Update overall muscle memory activation
    float TotalMemory = 0.0f;
    int32 ActionCount = 0;
    for (const auto& Pair : MotorExecutionHistory)
    {
        TotalMemory += CalculateMuscleMemoryStrength(Pair.Key);
        ActionCount++;
    }
    EmbodiedState.MuscleMemoryActivation = ActionCount > 0 ? TotalMemory / ActionCount : 0.0f;
}

float UStrategicCognitionBridge::GetMotorReadiness(const FString& ActionType) const
{
    float BaseReadiness = EmbodiedState.MotorReadiness;
    float MuscleMemoryBonus = CalculateMuscleMemoryStrength(ActionType) * 0.2f;
    float FatiguePenalty = EmbodiedState.PhysicalFatigue * 0.3f;

    return FMath::Clamp(BaseReadiness + MuscleMemoryBonus - FatiguePenalty, 0.0f, 1.0f);
}

bool UStrategicCognitionBridge::IsMuscleMemoryActive(const FString& ActionType) const
{
    return CalculateMuscleMemoryStrength(ActionType) > 0.7f;
}

float UStrategicCognitionBridge::GetEmbodiedPerformanceMultiplier() const
{
    float Multiplier = 1.0f;

    // Positive factors
    Multiplier += EmbodiedState.MotorReadiness * 0.2f;
    Multiplier += EmbodiedState.HandEyeCoordination * 0.15f;
    Multiplier += EmbodiedState.ProprioceptiveAccuracy * 0.1f;
    Multiplier += EmbodiedState.MuscleMemoryActivation * 0.15f;
    Multiplier += EmbodiedState.PostureQuality * 0.05f;
    Multiplier += EmbodiedState.BreathingRegularity * 0.05f;

    // Negative factors
    Multiplier -= EmbodiedState.PhysicalFatigue * 0.3f;

    return FMath::Clamp(Multiplier, 0.5f, 1.5f);
}

// ========================================
// EMBEDDED COGNITION API
// ========================================

void UStrategicCognitionBridge::UpdateEmbeddedContext(const FString& GamePhase, const TArray<float>& EnvironmentFeatures)
{
    EmbeddedContext.GamePhase = GamePhase;

    // Update awareness metrics based on environment features
    if (EnvironmentFeatures.Num() >= 4)
    {
        EmbeddedContext.ResourceAwareness = EmbeddedContext.ResourceAwareness * 0.8f + EnvironmentFeatures[0] * 0.2f;
        EmbeddedContext.ThreatAssessmentAccuracy = EmbeddedContext.ThreatAssessmentAccuracy * 0.8f + EnvironmentFeatures[1] * 0.2f;
        EmbeddedContext.OpportunityRecognitionSpeed = EmbeddedContext.OpportunityRecognitionSpeed * 0.8f + EnvironmentFeatures[2] * 0.2f;
        EmbeddedContext.TeamCompositionUnderstanding = EmbeddedContext.TeamCompositionUnderstanding * 0.8f + EnvironmentFeatures[3] * 0.2f;
    }
}

void UStrategicCognitionBridge::RecordMapExperience(const FString& MapID, float PerformanceScore)
{
    if (MapExperienceDB.Contains(MapID))
    {
        // Exponential moving average
        MapExperienceDB[MapID] = MapExperienceDB[MapID] * 0.9f + PerformanceScore * 0.1f;
    }
    else
    {
        MapExperienceDB.Add(MapID, PerformanceScore);
    }

    // Update map familiarity
    EmbeddedContext.MapFamiliarity = MapExperienceDB[MapID];
}

void UStrategicCognitionBridge::RequestContextualStrategyRecommendation()
{
    // Phase/context router: chooses the focus of the advice; the model writes it.
    FString Focus;
    if (EmbeddedContext.GamePhase == TEXT("early"))
    {
        Focus = EmbeddedContext.MapFamiliarity < 0.5f
            ? TEXT("map exploration and information gathering (map familiarity is low)")
            : TEXT("establishing early resource control and map presence");
    }
    else if (EmbeddedContext.GamePhase == TEXT("mid"))
    {
        Focus = EmbeddedContext.ThreatAssessmentAccuracy > 0.7f
            ? TEXT("capitalizing on the opponent weaknesses already identified")
            : TEXT("maintaining pressure while building a strategic advantage");
    }
    else if (EmbeddedContext.GamePhase == TEXT("late"))
    {
        Focus = EmbeddedContext.ResourceAwareness > 0.8f
            ? TEXT("decisive plays that use the resource advantage")
            : TEXT("playing conservatively and waiting for opponent mistakes");
    }
    else
    {
        Focus = TEXT("adapting dynamically to the current situation");
    }
    TMap<FString, FString> Slots;
    Slots.Add(TEXT("game_phase"), EmbeddedContext.GamePhase.IsEmpty() ? FString(TEXT("unknown")) : EmbeddedContext.GamePhase);
    Slots.Add(TEXT("focus"), Focus);
    EchoCoaching::RequestFromTemplate(this, TEXT("contextual_strategy"), Slots,
        DescribeCognitiveStateForModel(), &OnCoachingReady, &OnCoachingFailed);
}

float UStrategicCognitionBridge::AssessThreatLevel(const TArray<float>& ThreatIndicators)
{
    if (ThreatIndicators.Num() == 0)
    {
        return 0.0f;
    }

    float TotalThreat = 0.0f;
    for (float Indicator : ThreatIndicators)
    {
        TotalThreat += Indicator;
    }

    float RawThreat = TotalThreat / ThreatIndicators.Num();

    // Adjust based on assessment accuracy
    float AdjustedThreat = RawThreat * EmbeddedContext.ThreatAssessmentAccuracy;

    return FMath::Clamp(AdjustedThreat, 0.0f, 1.0f);
}

void UStrategicCognitionBridge::RequestOpportunityAnalysis(const TArray<float>& GameState)
{
    if (GameState.Num() == 0)
    {
        OnCoachingFailed.Broadcast(TEXT("opportunities"), TEXT("no game state provided"));
        return;
    }

    FString Metrics = DescribeCognitiveStateForModel();
    Metrics += TEXT("game state features (0-1):");
    static const TCHAR* FeatureNames[] = { TEXT("resource advantage"), TEXT("opponent strength"), TEXT("map control") };
    constexpr int32 NumNamedFeatures = UE_ARRAY_COUNT(FeatureNames);
    for (int32 i = 0; i < GameState.Num(); ++i)
    {
        const FString FeatureName = i < NumNamedFeatures ? FString(FeatureNames[i]) : FString::Printf(TEXT("feature_%d"), i);
        Metrics += FString::Printf(TEXT(" %s=%.2f;"), *FeatureName, GameState[i]);
    }
    Metrics += TEXT("\n");

    // Signal router; the model writes the analysis.
    TArray<FString> Signals;
    if (GameState.Num() >= 3)
    {
        if (GameState[0] > 0.7f) Signals.Add(TEXT("resource advantage (aggressive expansion may pay)"));
        if (GameState[1] < 0.3f) Signals.Add(TEXT("opponent vulnerability (timing-attack window)"));
        if (GameState[2] > 0.6f) Signals.Add(TEXT("map control advantage (deny opponent resources)"));
    }
    TMap<FString, FString> Slots;
    Slots.Add(TEXT("signals"), Signals.Num() ? FString::Join(Signals, TEXT("; ")) : FString(TEXT("no strong signal")));
    Slots.Add(TEXT("recognition_note"), EmbeddedContext.OpportunityRecognitionSpeed < 0.5f
        ? FString(TEXT(" The player's opportunity recognition is slow, so keep it to the clearest opportunity.")) : FString());
    EchoCoaching::RequestFromTemplate(this, TEXT("opportunities"), Slots, Metrics, &OnCoachingReady, &OnCoachingFailed);
}

// ========================================
// ENACTED COGNITION API
// ========================================

void UStrategicCognitionBridge::UpdateEnactedStrategy(const FString& ActionTaken, bool bSuccess, float Impact)
{
    // Track strategy effectiveness
    if (!StrategyEffectivenessHistory.Contains(ActionTaken))
    {
        StrategyEffectivenessHistory.Add(ActionTaken, TArray<bool>());
    }

    StrategyEffectivenessHistory[ActionTaken].Add(bSuccess);

    // Keep last 50 outcomes
    if (StrategyEffectivenessHistory[ActionTaken].Num() > 50)
    {
        StrategyEffectivenessHistory[ActionTaken].RemoveAt(0);
    }

    // Update adaptation rate based on outcome variance
    float Effectiveness = CalculateStrategyEffectiveness(ActionTaken);
    if (Effectiveness < 0.4f)
    {
        // Strategy not working - increase adaptation rate
        EnactedStrategy.AdaptationRate = FMath::Min(1.0f, EnactedStrategy.AdaptationRate + 0.1f);
    }
    else if (Effectiveness > 0.7f)
    {
        // Strategy working well - decrease adaptation rate
        EnactedStrategy.AdaptationRate = FMath::Max(0.1f, EnactedStrategy.AdaptationRate - 0.05f);
    }

    // Update counter-play effectiveness
    if (ActionTaken.Contains(TEXT("counter")) || ActionTaken.Contains(TEXT("Counter")))
    {
        EnactedStrategy.CounterPlayEffectiveness = EnactedStrategy.CounterPlayEffectiveness * 0.9f + (bSuccess ? 1.0f : 0.0f) * 0.1f;
    }
}

void UStrategicCognitionBridge::RequestExplorationRecommendation()
{
    FString Metrics = DescribeCognitiveStateForModel();
    for (const auto& Pair : StrategyEffectivenessHistory)
    {
        Metrics += FString::Printf(TEXT("strategy %s: effectiveness %.2f over %d outcomes\n"),
                                   *Pair.Key, CalculateStrategyEffectiveness(Pair.Key), Pair.Value.Num());
    }

    TMap<FString, FString> Slots;
    Slots.Add(TEXT("focus"), EnactedStrategy.ExplorationTendency < 0.3f
        ? FString(TEXT("the current approach may be too predictable - try new strategies"))
        : EnactedStrategy.ExplorationTendency > 0.7f
            ? FString(TEXT("refine the strategies that work rather than exploring further"))
            : FString(TEXT("balance exploring with exploiting known effective strategies")));
    EchoCoaching::RequestFromTemplate(this, TEXT("exploration"), Slots, Metrics, &OnCoachingReady, &OnCoachingFailed);
}

void UStrategicCognitionBridge::AdjustRiskTolerance(float GameProgress, float CurrentAdvantage)
{
    FString OldStance = EnactedStrategy.StrategicStance;

    // Adjust risk tolerance based on game state
    if (CurrentAdvantage > 0.3f)
    {
        // Winning - can afford more risk or play safe
        if (GameProgress > 0.7f)
        {
            // Late game with advantage - play safe
            EnactedStrategy.RiskTolerance = FMath::Max(0.2f, EnactedStrategy.RiskTolerance - 0.1f);
            EnactedStrategy.StrategicStance = TEXT("Conservative");
        }
        else
        {
            // Early/mid with advantage - press it
            EnactedStrategy.RiskTolerance = FMath::Min(0.8f, EnactedStrategy.RiskTolerance + 0.05f);
            EnactedStrategy.StrategicStance = TEXT("Aggressive");
        }
    }
    else if (CurrentAdvantage < -0.3f)
    {
        // Losing - may need to take risks
        EnactedStrategy.RiskTolerance = FMath::Min(0.9f, EnactedStrategy.RiskTolerance + 0.15f);
        EnactedStrategy.StrategicStance = TEXT("Desperate");
    }
    else
    {
        // Even game - balanced approach
        EnactedStrategy.RiskTolerance = 0.5f;
        EnactedStrategy.StrategicStance = TEXT("Balanced");
    }

    // Broadcast if stance changed
    if (OldStance != EnactedStrategy.StrategicStance)
    {
        OnStrategicAdaptation.Broadcast(OldStance, EnactedStrategy.StrategicStance);
    }
}

void UStrategicCognitionBridge::RequestAdaptiveResponse(const FString& OpponentAction, const TArray<float>& GameState)
{
    FString Metrics = DescribeCognitiveStateForModel();
    Metrics += FString::Printf(TEXT("opponent action: %s\ngame state features:"), *OpponentAction);
    for (int32 i = 0; i < GameState.Num(); ++i)
    {
        Metrics += FString::Printf(TEXT(" %.2f"), GameState[i]);
    }
    Metrics += TEXT("\n");

    // Opponent-action router (keywords) picks the kind of response; the model writes it.
    FString Kind = TEXT("unclassified action");
    FString Focus = TEXT("keeping the current strategy and observing");
    if (OpponentAction.Contains(TEXT("aggressive")) || OpponentAction.Contains(TEXT("attack")))
    {
        Kind = TEXT("aggression");
        Focus = EnactedStrategy.CounterPlayEffectiveness > 0.6f
            ? TEXT("a prepared counter-attack (counter-play has been effective)")
            : TEXT("a defensive posture while gathering information");
    }
    else if (OpponentAction.Contains(TEXT("defensive")) || OpponentAction.Contains(TEXT("turtle")))
    {
        Kind = TEXT("defensive play");
        Focus = TEXT("applying pressure and forcing reactions");
    }
    else if (OpponentAction.Contains(TEXT("expand")) || OpponentAction.Contains(TEXT("economic")))
    {
        Kind = TEXT("economic expansion");
        Focus = EnactedStrategy.RiskTolerance > 0.6f
            ? TEXT("punishing greedy play with a timing attack")
            : TEXT("matching the expansion while staying safe");
    }
    TMap<FString, FString> Slots;
    Slots.Add(TEXT("opponent_action"), OpponentAction);
    Slots.Add(TEXT("action_kind"), Kind);
    Slots.Add(TEXT("focus"), Focus);
    EchoCoaching::RequestFromTemplate(this, TEXT("adaptive_response"), Slots, Metrics, &OnCoachingReady, &OnCoachingFailed);
}

float UStrategicCognitionBridge::EvaluateInitiativeOpportunity(const TArray<float>& GameState)
{
    float OpportunityScore = 0.0f;

    if (GameState.Num() >= 4)
    {
        // Factors favoring initiative
        OpportunityScore += GameState[0] * 0.3f; // Resource advantage
        OpportunityScore += (1.0f - GameState[1]) * 0.2f; // Opponent vulnerability
        OpportunityScore += GameState[2] * 0.2f; // Position advantage
        OpportunityScore += GameState[3] * 0.3f; // Timing window

        // Adjust based on initiative-taking tendency
        OpportunityScore *= (0.5f + EnactedStrategy.InitiativeTaking * 0.5f);
    }

    return FMath::Clamp(OpportunityScore, 0.0f, 1.0f);
}

// ========================================
// EXTENDED COGNITION API
// ========================================

void UStrategicCognitionBridge::UpdateExtendedCognition(float InputDevicePerformance, float UIUsage, float TeamCommQuality)
{
    ExtendedCognition.InputDeviceProficiency = ExtendedCognition.InputDeviceProficiency * 0.95f + InputDevicePerformance * 0.05f;
    ExtendedCognition.UIUtilizationEfficiency = ExtendedCognition.UIUtilizationEfficiency * 0.95f + UIUsage * 0.05f;
    ExtendedCognition.TeamCommunicationEffectiveness = ExtendedCognition.TeamCommunicationEffectiveness * 0.9f + TeamCommQuality * 0.1f;
}

void UStrategicCognitionBridge::RecordToolUsage(const FString& ToolType, float EffectivenessScore)
{
    if (!ToolUsageHistory.Contains(ToolType))
    {
        ToolUsageHistory.Add(ToolType, TArray<float>());
    }

    ToolUsageHistory[ToolType].Add(EffectivenessScore);

    // Keep last 50 uses
    if (ToolUsageHistory[ToolType].Num() > 50)
    {
        ToolUsageHistory[ToolType].RemoveAt(0);
    }

    // Update external tool usage metric
    float TotalEffectiveness = 0.0f;
    int32 ToolCount = 0;
    for (const auto& Pair : ToolUsageHistory)
    {
        float ToolAvg = 0.0f;
        for (float Score : Pair.Value)
        {
            ToolAvg += Score;
        }
        if (Pair.Value.Num() > 0)
        {
            TotalEffectiveness += ToolAvg / Pair.Value.Num();
            ToolCount++;
        }
    }

    ExtendedCognition.ExternalToolUsage = ToolCount > 0 ? TotalEffectiveness / ToolCount : 0.0f;
}

void UStrategicCognitionBridge::RequestTeamCoordinationRecommendation(const TArray<FString>& TeamRoles, const TArray<float>& TeamState)
{
    if (TeamRoles.Num() == 0)
    {
        OnCoachingFailed.Broadcast(TEXT("team_coordination"), TEXT("no team roles provided (solo play)"));
        return;
    }

    FString Metrics = DescribeCognitiveStateForModel();
    for (int32 i = 0; i < TeamRoles.Num(); ++i)
    {
        Metrics += TeamState.IsValidIndex(i)
            ? FString::Printf(TEXT("team member %s: state %.2f\n"), *TeamRoles[i], TeamState[i])
            : FString::Printf(TEXT("team member %s: state unknown\n"), *TeamRoles[i]);
    }

    float AvgTeamState = 0.0f;
    for (float State : TeamState)
    {
        AvgTeamState += State;
    }
    AvgTeamState /= FMath::Max(1, TeamState.Num());
    FString Focus;
    if (ExtendedCognition.TeamCommunicationEffectiveness < 0.4f)
    {
        Focus = TEXT("improving communication clarity and frequency (team communication is weak)");
    }
    else if (AvgTeamState > 0.7f)
    {
        Focus = TEXT("coordinating for decisive team play (the team is strong)");
    }
    else if (AvgTeamState < 0.3f)
    {
        Focus = TEXT("supporting the weakest members (the team is struggling)");
    }
    else
    {
        Focus = TEXT("maintaining coordination and adapting to opportunities");
    }
    TMap<FString, FString> Slots;
    Slots.Add(TEXT("team_average"), TeamState.Num() ? FString::Printf(TEXT("%.2f"), AvgTeamState) : FString(TEXT("unknown")));
    Slots.Add(TEXT("focus"), Focus);
    EchoCoaching::RequestFromTemplate(this, TEXT("team_coordination"), Slots, Metrics, &OnCoachingReady, &OnCoachingFailed);
}

void UStrategicCognitionBridge::IntegrateCommunityKnowledge(const FString& KnowledgeType, const FString& Content)
{
    // Track community knowledge integration
    ExtendedCognition.CommunityKnowledgeIntegration = FMath::Min(1.0f, ExtendedCognition.CommunityKnowledgeIntegration + 0.05f);

    // In full implementation, this would update strategy databases
    UE_LOG(LogTemp, Log, TEXT("Integrated community knowledge: %s - %s"), *KnowledgeType, *Content);
}

void UStrategicCognitionBridge::RequestExternalResourceRecommendations()
{
    FString Metrics = DescribeCognitiveStateForModel();
    for (const auto& Pair : ToolUsageHistory)
    {
        float Sum = 0.0f;
        for (float Score : Pair.Value)
        {
            Sum += Score;
        }
        Metrics += FString::Printf(TEXT("tool %s: mean effectiveness %.2f over %d uses\n"),
                                   *Pair.Key, Pair.Value.Num() > 0 ? Sum / Pair.Value.Num() : 0.0f, Pair.Value.Num());
    }

    TMap<FString, FString> Slots;
    const FString Weakest = GetWeakestAssessmentArea();
    Slots.Add(TEXT("weakest_area"), Weakest.IsEmpty() ? FString(TEXT("none measured")) : Weakest);
    EchoCoaching::RequestFromTemplate(this, TEXT("external_resources"), Slots, Metrics, &OnCoachingReady, &OnCoachingFailed);
}

// ========================================
// COGNITIVE LOAD MANAGEMENT
// ========================================

void UStrategicCognitionBridge::UpdateCognitiveLoad(ECognitiveLoadType LoadType, float LoadLevel)
{
    CognitiveLoad.LoadLevels[LoadType] = FMath::Clamp(LoadLevel, 0.0f, 1.0f);

    // Recalculate total load (weighted sum)
    float TotalLoad = 0.0f;
    TotalLoad += CognitiveLoad.LoadLevels[ECognitiveLoadType::Perceptual] * 0.2f;
    TotalLoad += CognitiveLoad.LoadLevels[ECognitiveLoadType::WorkingMemory] * 0.25f;
    TotalLoad += CognitiveLoad.LoadLevels[ECognitiveLoadType::DecisionMaking] * 0.25f;
    TotalLoad += CognitiveLoad.LoadLevels[ECognitiveLoadType::MotorExecution] * 0.1f;
    TotalLoad += CognitiveLoad.LoadLevels[ECognitiveLoadType::TimePressure] * 0.1f;
    TotalLoad += CognitiveLoad.LoadLevels[ECognitiveLoadType::SocialCoordination] * 0.1f;

    CognitiveLoad.TotalLoad = TotalLoad;

    // Reset time at current load if significant change
    if (FMath::Abs(TotalLoad - CognitiveLoad.TotalLoad) > 0.1f)
    {
        CognitiveLoad.TimeAtCurrentLoad = 0.0f;
    }
}

float UStrategicCognitionBridge::GetTotalCognitiveLoad() const
{
    return CognitiveLoad.TotalLoad;
}

bool UStrategicCognitionBridge::IsCognitiveOverloadImminent() const
{
    // Check if approaching overload threshold
    if (CognitiveLoad.TotalLoad > CognitiveLoad.OverloadThreshold)
    {
        return true;
    }

    // Check if sustained high load
    if (CognitiveLoad.TotalLoad > 0.6f && CognitiveLoad.TimeAtCurrentLoad > 300.0f) // 5 minutes
    {
        return true;
    }

    // Check if cognitive reserve is depleted
    if (CognitiveLoad.CognitiveReserve < 0.2f)
    {
        return true;
    }

    return false;
}

void UStrategicCognitionBridge::RequestLoadReductionRecommendations()
{
    FString Metrics = DescribeCognitiveStateForModel();
    Metrics += FString::Printf(TEXT("overload imminent: %s\n"), IsCognitiveOverloadImminent() ? TEXT("yes") : TEXT("no"));

    // Heaviest-load router; the model writes the advice.
    ECognitiveLoadType HighestType = ECognitiveLoadType::Perceptual;
    float HighestLoad = -1.0f;
    for (const auto& Pair : CognitiveLoad.LoadLevels)
    {
        if (Pair.Value > HighestLoad)
        {
            HighestLoad = Pair.Value;
            HighestType = Pair.Key;
        }
    }
    FString Focus;
    switch (HighestType)
    {
        case ECognitiveLoadType::Perceptual: Focus = TEXT("reducing visual clutter and using audio cues to support visual processing"); break;
        case ECognitiveLoadType::WorkingMemory: Focus = TEXT("simplifying decisions with heuristics and externalizing information to the UI"); break;
        case ECognitiveLoadType::DecisionMaking: Focus = TEXT("pre-planning responses to common situations and automating routine choices"); break;
        case ECognitiveLoadType::MotorExecution: Focus = TEXT("simplifying input sequences and taking micro-breaks against motor fatigue"); break;
        case ECognitiveLoadType::TimePressure: Focus = TEXT("practising time management in low-stakes play and faster recognition of common patterns"); break;
        case ECognitiveLoadType::SocialCoordination: Focus = TEXT("clear communication protocols and assigned coordination responsibilities"); break;
        default: Focus = TEXT("lightening whatever is heaviest"); break;
    }
    TMap<FString, FString> Slots;
    Slots.Add(TEXT("load_type"), UEnum::GetDisplayValueAsText(HighestType).ToString());
    Slots.Add(TEXT("load_level"), FString::Printf(TEXT("%.2f"), FMath::Max(0.0f, HighestLoad)));
    Slots.Add(TEXT("focus"), Focus);
    Slots.Add(TEXT("break_note"), CognitiveLoad.TimeAtCurrentLoad > 600.0f
        ? FString(TEXT(" This load has lasted over ten minutes, so a short break to restore cognitive resources is worth suggesting.")) : FString());
    EchoCoaching::RequestFromTemplate(this, TEXT("load_reduction"), Slots, Metrics, &OnCoachingReady, &OnCoachingFailed);
}

void UStrategicCognitionBridge::AllocateCognitiveResources(const TMap<ECognitiveLoadType, float>& Allocation)
{
    // Validate allocation sums to 1.0
    float TotalAllocation = 0.0f;
    for (const auto& Pair : Allocation)
    {
        TotalAllocation += Pair.Value;
    }

    if (FMath::Abs(TotalAllocation - 1.0f) > 0.01f)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cognitive resource allocation does not sum to 1.0"));
        return;
    }

    // Apply allocation (affects load thresholds)
    for (const auto& Pair : Allocation)
    {
        // Higher allocation = higher tolerance for that load type
        // This is a simplified model
        float CurrentLoad = CognitiveLoad.LoadLevels[Pair.Key];
        float EffectiveLoad = CurrentLoad / (0.5f + Pair.Value);
        CognitiveLoad.LoadLevels[Pair.Key] = FMath::Clamp(EffectiveLoad, 0.0f, 1.0f);
    }
}

// ========================================
// ATTENTION MANAGEMENT
// ========================================

void UStrategicCognitionBridge::SetPrimaryFocus(const FString& FocusTarget)
{
    if (AttentionState.PrimaryFocus != FocusTarget)
    {
        // Calculate switching cost
        float Cost = CalculateAttentionSwitchingCost(AttentionState.PrimaryFocus, FocusTarget);
        AttentionState.SwitchingCost = Cost;

        // Reset sustained attention
        AttentionState.SustainedAttentionDuration = 0.0f;

        AttentionState.PrimaryFocus = FocusTarget;
    }
}

void UStrategicCognitionBridge::AddSecondaryTarget(const FString& Target)
{
    if (!AttentionState.SecondaryTargets.Contains(Target))
    {
        AttentionState.SecondaryTargets.Add(Target);

        // Adjust attention split
        AttentionState.AttentionSplitRatio = DetermineOptimalAttentionSplit(AttentionState.SecondaryTargets.Num() + 1);
    }
}

TMap<FString, float> UStrategicCognitionBridge::GetAttentionAllocationRecommendation(const TArray<FString>& PotentialTargets)
{
    TMap<FString, float> Allocation;

    if (PotentialTargets.Num() == 0)
    {
        return Allocation;
    }

    // Simple priority-based allocation
    // In full implementation, this would use ML to determine optimal allocation

    float PrimaryShare = 0.6f;
    float SecondaryShare = 0.4f / FMath::Max(1, PotentialTargets.Num() - 1);

    for (int32 i = 0; i < PotentialTargets.Num(); i++)
    {
        if (i == 0)
        {
            Allocation.Add(PotentialTargets[i], PrimaryShare);
        }
        else
        {
            Allocation.Add(PotentialTargets[i], SecondaryShare);
        }
    }

    return Allocation;
}

float UStrategicCognitionBridge::CalculateAttentionSwitchingCost(const FString& CurrentFocus, const FString& NewFocus) const
{
    // Base switching cost
    float Cost = 0.2f;

    // Higher cost for dissimilar focuses
    if (CurrentFocus.Contains(TEXT("enemy")) && NewFocus.Contains(TEXT("resource")))
    {
        Cost += 0.2f;
    }
    else if (CurrentFocus.Contains(TEXT("micro")) && NewFocus.Contains(TEXT("macro")))
    {
        Cost += 0.3f;
    }

    // Lower cost with practice
    Cost *= (1.0f - EmbodiedState.MuscleMemoryActivation * 0.3f);

    return FMath::Clamp(Cost, 0.1f, 0.5f);
}

// ========================================
// HOLISTIC INTEGRATION
// ========================================

TMap<FString, float> UStrategicCognitionBridge::GetIntegratedPerformanceAssessment() const
{
    TMap<FString, float> Assessment;

    // Embodied factors
    Assessment.Add(TEXT("Embodied_MotorReadiness"), EmbodiedState.MotorReadiness);
    Assessment.Add(TEXT("Embodied_Coordination"), EmbodiedState.HandEyeCoordination);
    Assessment.Add(TEXT("Embodied_MuscleMemory"), EmbodiedState.MuscleMemoryActivation);
    Assessment.Add(TEXT("Embodied_Fatigue"), 1.0f - EmbodiedState.PhysicalFatigue);

    // Embedded factors
    Assessment.Add(TEXT("Embedded_MapFamiliarity"), EmbeddedContext.MapFamiliarity);
    Assessment.Add(TEXT("Embedded_ThreatAwareness"), EmbeddedContext.ThreatAssessmentAccuracy);
    Assessment.Add(TEXT("Embedded_OpportunityRecognition"), EmbeddedContext.OpportunityRecognitionSpeed);

    // Enacted factors
    Assessment.Add(TEXT("Enacted_Adaptation"), EnactedStrategy.AdaptationRate);
    Assessment.Add(TEXT("Enacted_CounterPlay"), EnactedStrategy.CounterPlayEffectiveness);
    Assessment.Add(TEXT("Enacted_Initiative"), EnactedStrategy.InitiativeTaking);

    // Extended factors
    Assessment.Add(TEXT("Extended_ToolProficiency"), ExtendedCognition.InputDeviceProficiency);
    Assessment.Add(TEXT("Extended_TeamComm"), ExtendedCognition.TeamCommunicationEffectiveness);
    Assessment.Add(TEXT("Extended_CommunityKnowledge"), ExtendedCognition.CommunityKnowledgeIntegration);

    // Cognitive factors
    Assessment.Add(TEXT("Cognitive_Reserve"), CognitiveLoad.CognitiveReserve);
    Assessment.Add(TEXT("Cognitive_Load"), 1.0f - CognitiveLoad.TotalLoad);

    // Overall 4E integration
    Assessment.Add(TEXT("Overall_4E_Integration"), Calculate4EIntegrationScore());

    return Assessment;
}

void UStrategicCognitionBridge::RequestHolisticImprovementRecommendations()
{
    FString Metrics = DescribeCognitiveStateForModel();
    for (const auto& Pair : GetIntegratedPerformanceAssessment())
    {
        Metrics += FString::Printf(TEXT("assessment %s: %.2f\n"), *Pair.Key, Pair.Value);
    }
    Metrics += FString::Printf(TEXT("weakest area: %s\noverload imminent: %s\n"),
                               *GetWeakestAssessmentArea(), IsCognitiveOverloadImminent() ? TEXT("yes") : TEXT("no"));

    // Weakest-4E router; the model writes the advice.
    const float EmbodiedScore = GetEmbodiedPerformanceMultiplier() / 1.5f;
    const float EmbeddedScore = (EmbeddedContext.MapFamiliarity + EmbeddedContext.ThreatAssessmentAccuracy + EmbeddedContext.OpportunityRecognitionSpeed) / 3.0f;
    const float EnactedScore = (EnactedStrategy.AdaptationRate + EnactedStrategy.CounterPlayEffectiveness + EnactedStrategy.InitiativeTaking) / 3.0f;
    const float ExtendedScore = (ExtendedCognition.InputDeviceProficiency + ExtendedCognition.TeamCommunicationEffectiveness + ExtendedCognition.CommunityKnowledgeIntegration) / 3.0f;
    const float MinScore = FMath::Min(FMath::Min(EmbodiedScore, EmbeddedScore), FMath::Min(EnactedScore, ExtendedScore));
    FString Area, Focus;
    if (MinScore == EmbodiedScore) { Area = TEXT("embodied"); Focus = TEXT("mechanical skill drills and reaction-time training"); }
    else if (MinScore == EmbeddedScore) { Area = TEXT("embedded"); Focus = TEXT("map study, common strategies and game sense"); }
    else if (MinScore == EnactedScore) { Area = TEXT("enacted"); Focus = TEXT("adaptive play and reading opponents"); }
    else { Area = TEXT("extended"); Focus = TEXT("tool usage, communication and community engagement"); }
    TMap<FString, FString> Slots;
    Slots.Add(TEXT("priority_area"), Area);
    Slots.Add(TEXT("priority_score"), FString::Printf(TEXT("%.2f"), MinScore));
    Slots.Add(TEXT("focus"), Focus);
    Slots.Add(TEXT("urgent_note"), IsCognitiveOverloadImminent()
        ? FString(TEXT(" Cognitive overload is imminent: make reducing load the urgent first point.")) : FString());
    EchoCoaching::RequestFromTemplate(this, TEXT("holistic_improvement"), Slots, Metrics, &OnCoachingReady, &OnCoachingFailed);
}

float UStrategicCognitionBridge::Calculate4EIntegrationScore() const
{
    return Integrate4EFactors();
}

FString UStrategicCognitionBridge::GetWeakestAssessmentArea() const
{
    FString LowestArea;
    float LowestScore = TNumericLimits<float>::Max();

    for (const auto& Pair : GetIntegratedPerformanceAssessment())
    {
        if (Pair.Value < LowestScore && !Pair.Key.Contains(TEXT("Overall")))
        {
            LowestScore = Pair.Value;
            LowestArea = Pair.Key;
        }
    }
    return LowestArea;
}

void UStrategicCognitionBridge::RequestPersonalizedTrainingFocus()
{
    FString Metrics = DescribeCognitiveStateForModel();
    for (const auto& Pair : GetIntegratedPerformanceAssessment())
    {
        Metrics += FString::Printf(TEXT("assessment %s: %.2f\n"), *Pair.Key, Pair.Value);
    }
    Metrics += FString::Printf(TEXT("weakest area: %s\n"), *GetWeakestAssessmentArea());

    const FString Weakest = GetWeakestAssessmentArea();
    const TMap<FString, float> Assessment = GetIntegratedPerformanceAssessment();
    const float* WeakestScore = Assessment.Find(Weakest);
    FString Focus = TEXT("with balanced practice across all areas");
    if (Weakest.Contains(TEXT("Embodied"))) Focus = TEXT("through targeted mechanical practice");
    else if (Weakest.Contains(TEXT("Embedded"))) Focus = TEXT("through map study and game analysis");
    else if (Weakest.Contains(TEXT("Enacted"))) Focus = TEXT("through varied practice scenarios");
    else if (Weakest.Contains(TEXT("Extended"))) Focus = TEXT("through tool training and community engagement");
    TMap<FString, FString> Slots;
    Slots.Add(TEXT("weakest_area"), Weakest.IsEmpty() ? FString(TEXT("none measured")) : Weakest);
    Slots.Add(TEXT("weakest_score"), WeakestScore ? FString::Printf(TEXT("%.2f"), *WeakestScore) : FString(TEXT("n/a")));
    Slots.Add(TEXT("focus"), Focus);
    EchoCoaching::RequestFromTemplate(this, TEXT("training_focus"), Slots, Metrics, &OnCoachingReady, &OnCoachingFailed);
}

FString UStrategicCognitionBridge::DescribeCognitiveStateForModel() const
{
    FString Text;
    Text += FString::Printf(TEXT("embodied: motor readiness %.2f, hand-eye %.2f, proprioception %.2f, muscle memory %.2f, fatigue %.2f, posture %.2f, breathing %.2f\n"),
        EmbodiedState.MotorReadiness, EmbodiedState.HandEyeCoordination, EmbodiedState.ProprioceptiveAccuracy,
        EmbodiedState.MuscleMemoryActivation, EmbodiedState.PhysicalFatigue, EmbodiedState.PostureQuality, EmbodiedState.BreathingRegularity);
    Text += FString::Printf(TEXT("embedded: game phase '%s', map familiarity %.2f, meta awareness %.2f, team composition %.2f, resource awareness %.2f, threat assessment %.2f, opportunity recognition %.2f\n"),
        *EmbeddedContext.GamePhase, EmbeddedContext.MapFamiliarity, EmbeddedContext.MetaAwareness, EmbeddedContext.TeamCompositionUnderstanding,
        EmbeddedContext.ResourceAwareness, EmbeddedContext.ThreatAssessmentAccuracy, EmbeddedContext.OpportunityRecognitionSpeed);
    Text += FString::Printf(TEXT("enacted: stance '%s', exploration %.2f, risk tolerance %.2f, adaptation rate %.2f, initiative %.2f, counter-play %.2f, map control priority %.2f\n"),
        *EnactedStrategy.StrategicStance, EnactedStrategy.ExplorationTendency, EnactedStrategy.RiskTolerance, EnactedStrategy.AdaptationRate,
        EnactedStrategy.InitiativeTaking, EnactedStrategy.CounterPlayEffectiveness, EnactedStrategy.MapControlPriority);
    Text += FString::Printf(TEXT("extended: input device %.2f, UI usage %.2f, tool usage %.2f, team communication %.2f, community knowledge %.2f, replay analysis %.2f, coaching %.2f\n"),
        ExtendedCognition.InputDeviceProficiency, ExtendedCognition.UIUtilizationEfficiency, ExtendedCognition.ExternalToolUsage,
        ExtendedCognition.TeamCommunicationEffectiveness, ExtendedCognition.CommunityKnowledgeIntegration,
        ExtendedCognition.ReplayAnalysisUtilization, ExtendedCognition.CoachingIntegration);
    Text += FString::Printf(TEXT("cognitive load: total %.2f, reserve %.2f, overload threshold %.2f, time at current load %.0fs\n"),
        CognitiveLoad.TotalLoad, CognitiveLoad.CognitiveReserve, CognitiveLoad.OverloadThreshold, CognitiveLoad.TimeAtCurrentLoad);
    for (const auto& Pair : CognitiveLoad.LoadLevels)
    {
        Text += FString::Printf(TEXT("  load %s: %.2f\n"), *UEnum::GetValueAsString(Pair.Key), Pair.Value);
    }
    Text += FString::Printf(TEXT("attention: primary '%s', secondary [%s], split %.2f, peripheral %.2f\n"),
        *AttentionState.PrimaryFocus, *FString::Join(AttentionState.SecondaryTargets, TEXT(", ")),
        AttentionState.AttentionSplitRatio, AttentionState.PeripheralAwareness);
    return Text;
}

// ========================================
// INTERNAL METHODS
// ========================================

float UStrategicCognitionBridge::CalculateMuscleMemoryStrength(const FString& ActionType) const
{
    if (!MotorExecutionHistory.Contains(ActionType))
    {
        return 0.0f;
    }

    const TArray<float>& History = MotorExecutionHistory[ActionType];

    if (History.Num() < 10)
    {
        return 0.0f; // Not enough data
    }

    // Calculate consistency (low variance = strong muscle memory)
    float Mean = 0.0f;
    for (float Value : History)
    {
        Mean += Value;
    }
    Mean /= History.Num();

    float Variance = 0.0f;
    for (float Value : History)
    {
        Variance += FMath::Square(Value - Mean);
    }
    Variance /= History.Num();

    // High mean + low variance = strong muscle memory
    float Consistency = 1.0f - FMath::Sqrt(Variance);
    float Strength = Mean * Consistency;

    // Bonus for volume
    float VolumeBonus = FMath::Min(0.2f, History.Num() / 500.0f);

    return FMath::Clamp(Strength + VolumeBonus, 0.0f, 1.0f);
}

void UStrategicCognitionBridge::UpdateCognitiveReserve(float DeltaTime)
{
    // Deplete reserve under load
    float DepletionRate = CognitiveLoad.TotalLoad * 0.01f;
    CognitiveLoad.CognitiveReserve -= DepletionRate * DeltaTime;

    // Recover reserve when load is low
    if (CognitiveLoad.TotalLoad < 0.3f)
    {
        float RecoveryRate = 0.02f * (1.0f - CognitiveLoad.TotalLoad);
        CognitiveLoad.CognitiveReserve += RecoveryRate * DeltaTime;
    }

    CognitiveLoad.CognitiveReserve = FMath::Clamp(CognitiveLoad.CognitiveReserve, 0.0f, 1.0f);
}

float UStrategicCognitionBridge::CalculateStrategyEffectiveness(const FString& Strategy) const
{
    if (!StrategyEffectivenessHistory.Contains(Strategy))
    {
        return 0.5f; // Unknown effectiveness
    }

    const TArray<bool>& History = StrategyEffectivenessHistory[Strategy];

    if (History.Num() == 0)
    {
        return 0.5f;
    }

    int32 Successes = 0;
    for (bool Success : History)
    {
        if (Success) Successes++;
    }

    return (float)Successes / History.Num();
}

float UStrategicCognitionBridge::DetermineOptimalAttentionSplit(int32 TargetCount) const
{
    // Optimal split based on number of targets
    // More targets = less attention per target
    if (TargetCount <= 1)
    {
        return 1.0f;
    }
    else if (TargetCount == 2)
    {
        return 0.7f; // 70/30 split
    }
    else if (TargetCount == 3)
    {
        return 0.5f; // 50/25/25 split
    }
    else
    {
        return 0.4f; // Primary gets 40%, rest split evenly
    }
}

float UStrategicCognitionBridge::Integrate4EFactors() const
{
    // Calculate integrated 4E score
    float EmbodiedScore = GetEmbodiedPerformanceMultiplier() / 1.5f; // Normalize from 0.5-1.5 to 0-1
    float EmbeddedScore = (EmbeddedContext.MapFamiliarity + EmbeddedContext.ThreatAssessmentAccuracy + 
                          EmbeddedContext.OpportunityRecognitionSpeed + EmbeddedContext.MetaAwareness) / 4.0f;
    float EnactedScore = (EnactedStrategy.AdaptationRate + EnactedStrategy.CounterPlayEffectiveness + 
                         EnactedStrategy.InitiativeTaking + EnactedStrategy.ExplorationTendency) / 4.0f;
    float ExtendedScore = (ExtendedCognition.InputDeviceProficiency + ExtendedCognition.UIUtilizationEfficiency +
                          ExtendedCognition.TeamCommunicationEffectiveness + ExtendedCognition.CommunityKnowledgeIntegration) / 4.0f;

    // Weighted integration (all 4Es contribute equally)
    float IntegratedScore = (EmbodiedScore + EmbeddedScore + EnactedScore + ExtendedScore) / 4.0f;

    // Bonus for balance (all 4Es at similar levels)
    float Variance = 0.0f;
    float Mean = IntegratedScore;
    Variance += FMath::Square(EmbodiedScore - Mean);
    Variance += FMath::Square(EmbeddedScore - Mean);
    Variance += FMath::Square(EnactedScore - Mean);
    Variance += FMath::Square(ExtendedScore - Mean);
    Variance /= 4.0f;

    float BalanceBonus = (1.0f - FMath::Sqrt(Variance)) * 0.1f;

    return FMath::Clamp(IntegratedScore + BalanceBonus, 0.0f, 1.0f);
}
