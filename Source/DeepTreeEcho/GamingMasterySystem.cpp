// GamingMasterySystem.cpp
// Deep Tree Echo - ML-Assisted Gaming Skills & Strategic Mastery System
// Copyright (c) 2025 Deep Tree Echo Project

#include "GamingMasterySystem.h"
#include "EchoModelClient.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FGamingMasterySystem"

// ========================================
// Companion coaching (model-written)
// ========================================

void EchoCoaching::Request(UObject* Owner, const FString& Topic, const FString& Instruction, const FString& Metrics,
                           FOnCompanionCoaching* Ready, FOnCompanionCoachingFailed* Failed)
{
    TArray<FEchoModelMessage> Messages;
    Messages.Add(FEchoModelMessage(TEXT("system"),
        TEXT("You are Deep Tree Echo, an AI gaming companion coaching the player you play alongside. ")
        TEXT("Speak directly to the player in your own voice, briefly and concretely. Base every statement ")
        TEXT("strictly on the measurements provided; if they are insufficient, say what is missing.")));
    Messages.Add(FEchoModelMessage(TEXT("user"),
        FString::Printf(TEXT("Topic: %s\nMeasurements:\n%s\n\n%s"), *Topic, *Metrics, *Instruction)));

    TWeakObjectPtr<UObject> WeakOwner(Owner);
    UEchoModelClient::Get()->Chat(Messages, FOnEchoModelReply::CreateLambda(
        [WeakOwner, Topic, Ready, Failed](bool bSuccess, const FString& TextOrError)
        {
            if (!WeakOwner.IsValid())
            {
                return; // Ready/Failed belong to the owner
            }

            const FString Text = TextOrError.TrimStartAndEnd();
            if (bSuccess && !Text.IsEmpty())
            {
                if (Ready)
                {
                    Ready->Broadcast(Topic, Text);
                }
                return;
            }

            const FString Error = bSuccess ? FString(TEXT("model reply was empty")) : TextOrError;
            UE_LOG(LogTemp, Warning, TEXT("Deep Tree Echo coaching '%s' not produced: %s"), *Topic, *Error);
            if (Failed)
            {
                Failed->Broadcast(Topic, Error);
            }
        }));
}

TMap<FString, FString>& EchoCoaching::PromptTemplates()
{
    // The structure the companion's coaching lines used to be chosen from (per topic, per
    // measured condition) kept as prompt templates. Call sites route the condition into the
    // {focus}-style slots; the model writes the advice.
    static TMap<FString, FString> Templates = []()
    {
        TMap<FString, FString> T;
        T.Add(TEXT("contextual_strategy"),
            TEXT("The game is in its {game_phase} phase. Recommend a strategy in one or two sentences, centred on {focus}."));
        T.Add(TEXT("opportunities"),
            TEXT("Point out the opportunities this game state offers the player and how to exploit them. ")
            TEXT("Signals detected in the state: {signals}.{recognition_note}"));
        T.Add(TEXT("exploration"),
            TEXT("Advise the player whether to explore new strategies or refine effective ones. The measurements suggest: {focus}."));
        T.Add(TEXT("adaptive_response"),
            TEXT("The opponent just did: \"{opponent_action}\" ({action_kind}). Recommend how the player should respond, ")
            TEXT("leaning toward {focus}."));
        T.Add(TEXT("team_coordination"),
            TEXT("Recommend how the team should coordinate right now (average team state {team_average}), with emphasis on {focus}."));
        T.Add(TEXT("external_resources"),
            TEXT("Suggest which external resources would help this player most, choosing from replay analysis of recent games, ")
            TEXT("community guides on current strategies, structured coaching and performance-tracking tools. Base the choice on ")
            TEXT("the weakest measurements ({weakest_area})."));
        T.Add(TEXT("load_reduction"),
            TEXT("The heaviest cognitive load is {load_type} ({load_level}). Advise the player how to reduce it, for example by {focus}.{break_note}"));
        T.Add(TEXT("holistic_improvement"),
            TEXT("Give the player prioritized improvement advice across embodied, embedded, enacted and extended skills. ")
            TEXT("Top priority: the {priority_area} area ({priority_score}) - {focus}.{urgent_note}"));
        T.Add(TEXT("training_focus"),
            TEXT("Tell the player what to train next: their weakest area is {weakest_area} ({weakest_score}); suggest improving it {focus}."));
        T.Add(TEXT("improvement_recommendations"),
            TEXT("Give the player a short list of concrete improvement recommendations from this replay analysis: for each skill to ")
            TEXT("improve ({skills}) a focused way to practise it, and for each recommended drill ({drills}) why to do it."));
        T.Add(TEXT("flow_optimization"),
            TEXT("Give the player one to three tips to reach or stay in a flow state. Conditions detected: {conditions}."));
        T.Add(TEXT("attention_focus"),
            TEXT("Tell the player where to focus their attention right now. {focus}"));
        T.Add(TEXT("input_improvement"),
            TEXT("Give the player concise suggestions to improve their input execution. Inputs below 60% average timing quality: ")
            TEXT("{weak_inputs}. If there are none, tell them their execution is good and suggest turning to strategic decision-making."));
        T.Add(TEXT("training_guidance"),
            TEXT("Guide the player on what to practise next for the skill {skill}. Their stage: {focus}."));
        return T;
    }();
    return Templates;
}

void EchoCoaching::RequestFromTemplate(UObject* Owner, const FString& Topic, const TMap<FString, FString>& Slots,
                                       const FString& Metrics, FOnCompanionCoaching* Ready, FOnCompanionCoachingFailed* Failed)
{
    const FString* Template = PromptTemplates().Find(Topic);
    if (!Template || Template->IsEmpty())
    {
        const FString Error = FString::Printf(TEXT("no coaching prompt template for topic '%s'"), *Topic);
        UE_LOG(LogTemp, Warning, TEXT("Deep Tree Echo coaching '%s' not produced: %s"), *Topic, *Error);
        if (Failed)
        {
            Failed->Broadcast(Topic, Error);
        }
        return;
    }
    Request(Owner, Topic, UEchoModelClient::FillPrompt(*Template, Slots), Metrics, Ready, Failed);
}

// ========================================
// MCTS Node Structure
// ========================================

struct FMCTSNode
{
    TArray<float> State;
    FString Action;
    FMCTSNode* Parent;
    TArray<FMCTSNode*> Children;
    int32 VisitCount;
    float TotalValue;
    float UCBScore;
    bool bExpanded;

    FMCTSNode()
        : Parent(nullptr)
        , VisitCount(0)
        , TotalValue(0.0f)
        , UCBScore(0.0f)
        , bExpanded(false)
    {}

    ~FMCTSNode()
    {
        for (FMCTSNode* Child : Children)
        {
            delete Child;
        }
    }

    float GetAverageValue() const
    {
        return VisitCount > 0 ? TotalValue / VisitCount : 0.0f;
    }
};

// ========================================
// CONSTRUCTOR & LIFECYCLE
// ========================================

UGamingMasterySystem::UGamingMasterySystem()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f; // 10Hz update rate
}

void UGamingMasterySystem::BeginPlay()
{
    Super::BeginPlay();

    // Initialize Echo State Network with default sizes
    InitializeESN(64, 256, 32);

    // Initialize flow metrics
    FlowMetrics = FFlowStateMetrics();

    UE_LOG(LogTemp, Log, TEXT("GamingMasterySystem initialized - ML-Assisted Gaming Skills Active"));
}

void UGamingMasterySystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Apply skill decay if enabled
    if (bEnableSkillDecay)
    {
        ApplySkillDecay(DeltaTime);
    }

    // Update flow state tracking
    if (bEnableFlowTracking)
    {
        float NewFlowIntensity = CalculateFlowIntensity();
        bool bCurrentlyInFlow = NewFlowIntensity >= FlowEntryThreshold;

        // Check for flow state entry
        if (bCurrentlyInFlow && !bWasInFlowState)
        {
            OnFlowStateEntered.Broadcast(NewFlowIntensity);
        }

        // Update time in flow
        if (bCurrentlyInFlow)
        {
            FlowMetrics.TimeInFlow += DeltaTime;
        }

        FlowMetrics.FlowIntensity = NewFlowIntensity;
        bWasInFlowState = bCurrentlyInFlow;
    }
}

// ========================================
// SKILL MANAGEMENT
// ========================================

void UGamingMasterySystem::RegisterSkill(const FGamingSkillComponent& Skill)
{
    Skills.Add(Skill.SkillID, Skill);
    UE_LOG(LogTemp, Log, TEXT("Registered skill: %s"), *Skill.SkillName);
}

void UGamingMasterySystem::UpdateSkillProficiency(const FString& SkillID, float PerformanceScore, float PracticeMinutes)
{
    if (!Skills.Contains(SkillID))
    {
        UE_LOG(LogTemp, Warning, TEXT("Skill not found: %s"), *SkillID);
        return;
    }

    FGamingSkillComponent& Skill = Skills[SkillID];

    // Update practice time
    Skill.PracticeHours += PracticeMinutes / 60.0f;
    Skill.LastPracticeTime = GetWorld()->GetTimeSeconds();

    // Calculate proficiency change based on deliberate practice principles
    // Learning rate decreases as proficiency increases (diminishing returns)
    float LearningRate = 0.1f * (1.0f - Skill.ProficiencyLevel * 0.5f);
    
    // Performance relative to current skill affects learning
    float PerformanceDelta = PerformanceScore - Skill.ProficiencyLevel;
    
    // Optimal challenge zone (slightly above current level)
    float ChallengeBonus = 1.0f;
    if (PerformanceDelta > 0.0f && PerformanceDelta < 0.3f)
    {
        ChallengeBonus = 1.5f; // Sweet spot for learning
    }

    // Update proficiency
    float ProficiencyGain = LearningRate * PerformanceScore * ChallengeBonus * (PracticeMinutes / 30.0f);
    float OldProficiency = Skill.ProficiencyLevel;
    Skill.ProficiencyLevel = FMath::Clamp(Skill.ProficiencyLevel + ProficiencyGain, 0.0f, 1.0f);

    // Update success rate (exponential moving average)
    Skill.RecentSuccessRate = Skill.RecentSuccessRate * 0.9f + PerformanceScore * 0.1f;

    // Update acquisition phase
    Skill.AcquisitionPhase = CalculateAcquisitionPhase(Skill.ProficiencyLevel);

    // Increment deliberate practice sessions
    if (PracticeMinutes >= 20.0f) // Minimum for deliberate practice
    {
        Skill.DeliberatePracticeSessions++;
    }

    // Broadcast skill change if significant
    if (FMath::Abs(Skill.ProficiencyLevel - OldProficiency) > 0.01f)
    {
        OnSkillLevelChanged.Broadcast(SkillID, Skill.ProficiencyLevel);
    }

    // Update synergistic skills (transfer learning)
    for (const FString& SynergySkillID : Skill.SynergySkills)
    {
        if (Skills.Contains(SynergySkillID))
        {
            FGamingSkillComponent& SynergySkill = Skills[SynergySkillID];
            float TransferBonus = ProficiencyGain * 0.2f; // 20% transfer
            SynergySkill.ProficiencyLevel = FMath::Clamp(SynergySkill.ProficiencyLevel + TransferBonus, 0.0f, 1.0f);
        }
    }
}

FGamingSkillComponent UGamingMasterySystem::GetSkill(const FString& SkillID) const
{
    if (Skills.Contains(SkillID))
    {
        return Skills[SkillID];
    }
    return FGamingSkillComponent();
}

TArray<FGamingSkillComponent> UGamingMasterySystem::GetSkillsNeedingPractice(int32 MaxCount) const
{
    TArray<FGamingSkillComponent> Result;
    
    // Calculate priority score for each skill
    TArray<TPair<float, FString>> PriorityScores;
    
    for (const auto& Pair : Skills)
    {
        const FGamingSkillComponent& Skill = Pair.Value;
        
        // Priority factors:
        // 1. Time since last practice (decay urgency)
        float TimeSincePractice = GetWorld()->GetTimeSeconds() - Skill.LastPracticeTime;
        float DecayUrgency = FMath::Min(TimeSincePractice / 86400.0f, 1.0f); // Normalize to days
        
        // 2. Skill gap (distance from mastery)
        float SkillGap = 1.0f - Skill.ProficiencyLevel;
        
        // 3. Recent success rate (struggling skills need more practice)
        float StruggleFactor = 1.0f - Skill.RecentSuccessRate;
        
        // 4. Prerequisite for other skills (foundational importance)
        float FoundationalScore = 0.0f;
        for (const auto& OtherPair : Skills)
        {
            if (OtherPair.Value.Prerequisites.Contains(Skill.SkillID))
            {
                FoundationalScore += 0.1f;
            }
        }
        
        float PriorityScore = DecayUrgency * 0.3f + SkillGap * 0.3f + StruggleFactor * 0.25f + FoundationalScore * 0.15f;
        PriorityScores.Add(TPair<float, FString>(PriorityScore, Pair.Key));
    }
    
    // Sort by priority (descending)
    PriorityScores.Sort([](const TPair<float, FString>& A, const TPair<float, FString>& B) {
        return A.Key > B.Key;
    });
    
    // Return top skills
    for (int32 i = 0; i < FMath::Min(MaxCount, PriorityScores.Num()); i++)
    {
        Result.Add(Skills[PriorityScores[i].Value]);
    }
    
    return Result;
}

float UGamingMasterySystem::CalculateDomainMastery(EGameDomainType Domain) const
{
    float TotalProficiency = 0.0f;
    int32 SkillCount = 0;
    
    for (const auto& Pair : Skills)
    {
        // In a full implementation, skills would be tagged with their domain
        // For now, we calculate across all skills
        TotalProficiency += Pair.Value.ProficiencyLevel;
        SkillCount++;
    }
    
    return SkillCount > 0 ? TotalProficiency / SkillCount : 0.0f;
}

TArray<FString> UGamingMasterySystem::GetRecommendedLearningPath(const FString& TargetSkillID) const
{
    TArray<FString> Path;
    
    if (!Skills.Contains(TargetSkillID))
    {
        return Path;
    }
    
    // Build prerequisite chain using topological sort
    TSet<FString> Visited;
    TArray<FString> Stack;
    
    // DFS to find all prerequisites
    TFunction<void(const FString&)> DFS = [&](const FString& SkillID)
    {
        if (Visited.Contains(SkillID))
        {
            return;
        }
        Visited.Add(SkillID);
        
        if (Skills.Contains(SkillID))
        {
            const FGamingSkillComponent& Skill = Skills[SkillID];
            for (const FString& PrereqID : Skill.Prerequisites)
            {
                DFS(PrereqID);
            }
        }
        
        Stack.Add(SkillID);
    };
    
    DFS(TargetSkillID);
    
    // Filter to only include skills not yet mastered
    for (const FString& SkillID : Stack)
    {
        if (Skills.Contains(SkillID))
        {
            const FGamingSkillComponent& Skill = Skills[SkillID];
            if (Skill.ProficiencyLevel < 0.8f) // Not yet proficient
            {
                Path.Add(SkillID);
            }
        }
    }
    
    return Path;
}

// ========================================
// STRATEGIC PATTERN RECOGNITION
// ========================================

void UGamingMasterySystem::RegisterPattern(const FStrategicPattern& Pattern)
{
    StrategicPatterns.Add(Pattern);
    UE_LOG(LogTemp, Log, TEXT("Registered strategic pattern: %s"), *Pattern.PatternName);
}

TArray<FStrategicPattern> UGamingMasterySystem::RecognizePatterns(const TArray<float>& CurrentStateFeatures)
{
    TArray<FStrategicPattern> RecognizedPatterns;
    
    for (FStrategicPattern& Pattern : StrategicPatterns)
    {
        if (Pattern.GameDomain != CurrentGameDomain)
        {
            continue;
        }
        
        float Similarity = CalculatePatternSimilarity(CurrentStateFeatures, Pattern.FeatureVector);
        
        if (Similarity >= Pattern.RecognitionThreshold)
        {
            Pattern.EncounterCount++;
            RecognizedPatterns.Add(Pattern);
            OnPatternRecognized.Broadcast(Pattern.PatternID, Similarity);
        }
    }
    
    // Sort by confidence (similarity)
    RecognizedPatterns.Sort([&CurrentStateFeatures, this](const FStrategicPattern& A, const FStrategicPattern& B) {
        float SimA = CalculatePatternSimilarity(CurrentStateFeatures, A.FeatureVector);
        float SimB = CalculatePatternSimilarity(CurrentStateFeatures, B.FeatureVector);
        return SimA > SimB;
    });
    
    return RecognizedPatterns;
}

FString UGamingMasterySystem::GetRecommendedAction(const FString& PatternID) const
{
    for (const FStrategicPattern& Pattern : StrategicPatterns)
    {
        if (Pattern.PatternID == PatternID && Pattern.RecommendedResponses.Num() > 0)
        {
            // Return highest success rate response
            return Pattern.RecommendedResponses[0];
        }
    }
    return TEXT("");
}

void UGamingMasterySystem::UpdatePatternOutcome(const FString& PatternID, bool bSuccess)
{
    for (FStrategicPattern& Pattern : StrategicPatterns)
    {
        if (Pattern.PatternID == PatternID)
        {
            if (bSuccess)
            {
                Pattern.CorrectResponseCount++;
            }
            Pattern.SuccessRate = (float)Pattern.CorrectResponseCount / FMath::Max(1, Pattern.EncounterCount);
            break;
        }
    }
}

void UGamingMasterySystem::LearnPatternFromExperience(const TArray<float>& StateFeatures, const FString& SuccessfulAction)
{
    // Check if similar pattern already exists
    for (FStrategicPattern& Pattern : StrategicPatterns)
    {
        float Similarity = CalculatePatternSimilarity(StateFeatures, Pattern.FeatureVector);
        if (Similarity > 0.9f)
        {
            // Update existing pattern
            if (!Pattern.RecommendedResponses.Contains(SuccessfulAction))
            {
                Pattern.RecommendedResponses.Add(SuccessfulAction);
            }
            return;
        }
    }
    
    // Create new pattern
    FStrategicPattern NewPattern;
    NewPattern.PatternID = FString::Printf(TEXT("learned_%d"), StrategicPatterns.Num());
    NewPattern.PatternName = FString::Printf(TEXT("Learned Pattern %d"), StrategicPatterns.Num());
    NewPattern.GameDomain = CurrentGameDomain;
    NewPattern.FeatureVector = StateFeatures;
    NewPattern.RecommendedResponses.Add(SuccessfulAction);
    NewPattern.EncounterCount = 1;
    NewPattern.CorrectResponseCount = 1;
    NewPattern.SuccessRate = 1.0f;
    
    StrategicPatterns.Add(NewPattern);
    UE_LOG(LogTemp, Log, TEXT("Learned new strategic pattern from experience"));
}

// ========================================
// OPPONENT MODELING
// ========================================

void UGamingMasterySystem::UpdateOpponentModel(const FString& OpponentID, const FString& ObservedAction, const TArray<float>& GameState)
{
    if (!bEnableOpponentModeling)
    {
        return;
    }
    
    if (!OpponentModels.Contains(OpponentID))
    {
        OpponentModels.Add(OpponentID, FOpponentModel());
        OpponentModels[OpponentID].OpponentID = OpponentID;
    }
    
    FOpponentModel& Model = OpponentModels[OpponentID];
    Model.ObservationCount++;
    
    // Update behavioral tendencies
    if (Model.BehavioralTendencies.Contains(ObservedAction))
    {
        Model.BehavioralTendencies[ObservedAction] += 1.0f;
    }
    else
    {
        Model.BehavioralTendencies.Add(ObservedAction, 1.0f);
    }
    
    // Normalize tendencies
    float TotalTendency = 0.0f;
    for (const auto& Pair : Model.BehavioralTendencies)
    {
        TotalTendency += Pair.Value;
    }
    // Normalize using key-based access
    TArray<FString> TendencyKeys;
    for (const auto& Pair : Model.BehavioralTendencies)
    {
        TendencyKeys.Add(Pair.Key);
    }
    for (const FString& Key : TendencyKeys)
    {
        Model.BehavioralTendencies[Key] /= TotalTendency;
    }
    
    // Update Bayesian model
    UpdateBayesianModel(Model, ObservedAction, GameState);
    
    // Update model confidence
    Model.ModelConfidence = FMath::Min(1.0f, Model.ObservationCount / 100.0f);
}

TMap<FString, float> UGamingMasterySystem::PredictOpponentAction(const FString& OpponentID, const TArray<float>& CurrentState)
{
    TMap<FString, float> Predictions;
    
    if (!OpponentModels.Contains(OpponentID))
    {
        return Predictions;
    }
    
    const FOpponentModel& Model = OpponentModels[OpponentID];
    
    // Use behavioral tendencies as base predictions
    for (const auto& Pair : Model.BehavioralTendencies)
    {
        Predictions.Add(Pair.Key, Pair.Value);
    }
    
    // Adjust based on current state using ESN
    TArray<float> StateWithHistory = CurrentState;
    TArray<float> ESNOutput = RunESNInference(StateWithHistory);
    
    // Combine ESN predictions with behavioral model
    // (In full implementation, ESN would be trained to predict opponent actions)
    
    return Predictions;
}

TArray<FString> UGamingMasterySystem::GetOpponentWeaknesses(const FString& OpponentID) const
{
    if (!OpponentModels.Contains(OpponentID))
    {
        return TArray<FString>();
    }
    
    return OpponentModels[OpponentID].IdentifiedWeaknesses;
}

FString UGamingMasterySystem::GetCounterStrategy(const FString& OpponentID) const
{
    if (!OpponentModels.Contains(OpponentID))
    {
        return TEXT("");
    }
    
    const FOpponentModel& Model = OpponentModels[OpponentID];
    
    // Find most common opponent strategy
    FString MostCommonStrategy;
    float MaxTendency = 0.0f;
    for (const auto& Pair : Model.BehavioralTendencies)
    {
        if (Pair.Value > MaxTendency)
        {
            MaxTendency = Pair.Value;
            MostCommonStrategy = Pair.Key;
        }
    }
    
    // Return counter-strategy (in full implementation, this would use a strategy database)
    return FString::Printf(TEXT("Counter_%s"), *MostCommonStrategy);
}

// ========================================
// REPLAY ANALYSIS
// ========================================

FReplayAnalysisResult UGamingMasterySystem::AnalyzeReplay(const FString& ReplayPath)
{
    FReplayAnalysisResult Result;
    Result.ReplayID = ReplayPath;
    Result.GameDomain = CurrentGameDomain;
    
    // In full implementation, this would:
    // 1. Load replay file
    // 2. Extract game states and actions
    // 3. Run ML analysis on decisions
    // 4. Compare to optimal play
    // 5. Identify mistakes and good plays
    
    // No replay loader exists yet, so nothing was analysed: return an empty result rather than
    // fabricated scores, skills or drills.
    UE_LOG(LogTemp, Error, TEXT("AnalyzeReplay(%s): replay analysis is not implemented (no replay loader); no results produced"), *ReplayPath);
    
    return Result;
}

void UGamingMasterySystem::RequestImprovementRecommendations(const FReplayAnalysisResult& Analysis)
{
    const FString Topic = TEXT("improvement_recommendations");

    if (Analysis.SkillsToImprove.Num() == 0 && Analysis.RecommendedDrills.Num() == 0 &&
        Analysis.IdentifiedMistakes.Num() == 0 && Analysis.DecisionMetrics.Num() == 0 && Analysis.MechanicalMetrics.Num() == 0)
    {
        OnCoachingFailed.Broadcast(Topic, TEXT("replay analysis contains no data to base recommendations on"));
        return;
    }

    FString Metrics = FString::Printf(TEXT("replay: %s\noverall score: %.2f\n"), *Analysis.ReplayID, Analysis.OverallScore);
    Metrics += FString::Printf(TEXT("skills to improve: %s\n"), *FString::Join(Analysis.SkillsToImprove, TEXT(", ")));
    Metrics += FString::Printf(TEXT("recommended drills: %s\n"), *FString::Join(Analysis.RecommendedDrills, TEXT(", ")));
    for (const auto& Pair : Analysis.IdentifiedMistakes)
    {
        Metrics += FString::Printf(TEXT("mistake at %.1fs: %s\n"), Pair.Key, *Pair.Value);
    }
    for (const auto& Pair : Analysis.IdentifiedGoodPlays)
    {
        Metrics += FString::Printf(TEXT("good play at %.1fs: %s\n"), Pair.Key, *Pair.Value);
    }
    for (const auto& Pair : Analysis.DecisionMetrics)
    {
        Metrics += FString::Printf(TEXT("decision %s: %.2f\n"), *Pair.Key, Pair.Value);
    }
    for (const auto& Pair : Analysis.MechanicalMetrics)
    {
        Metrics += FString::Printf(TEXT("mechanics %s: %.2f\n"), *Pair.Key, Pair.Value);
    }

    TMap<FString, FString> Slots;
    Slots.Add(TEXT("skills"), Analysis.SkillsToImprove.Num() ? FString::Join(Analysis.SkillsToImprove, TEXT(", ")) : FString(TEXT("none listed")));
    Slots.Add(TEXT("drills"), Analysis.RecommendedDrills.Num() ? FString::Join(Analysis.RecommendedDrills, TEXT(", ")) : FString(TEXT("none listed")));
    EchoCoaching::RequestFromTemplate(this, Topic, Slots, Metrics, &OnCoachingReady, &OnCoachingFailed);
}

TMap<FString, float> UGamingMasterySystem::CompareToExpert(const FReplayAnalysisResult& Analysis) const
{
    TMap<FString, float> Comparison;
    
    // Compare various metrics to expert benchmark
    Comparison.Add(TEXT("overall"), Analysis.OverallScore / 0.95f); // Expert baseline 0.95
    
    for (const auto& Pair : Analysis.DecisionMetrics)
    {
        Comparison.Add(FString::Printf(TEXT("decision_%s"), *Pair.Key), Pair.Value / 0.9f);
    }
    
    for (const auto& Pair : Analysis.MechanicalMetrics)
    {
        Comparison.Add(FString::Printf(TEXT("mechanical_%s"), *Pair.Key), Pair.Value / 0.95f);
    }
    
    return Comparison;
}

// ========================================
// TRAINING MANAGEMENT
// ========================================

void UGamingMasterySystem::StartTrainingSession(const TArray<FString>& TargetSkillIDs, const FString& SessionType, float Duration)
{
    CurrentSession = FTrainingSession();
    CurrentSession.SessionID = FString::Printf(TEXT("session_%d"), TrainingHistory.Num());
    CurrentSession.TargetSkillIDs = TargetSkillIDs;
    CurrentSession.SessionType = SessionType;
    CurrentSession.PlannedDuration = Duration;
    
    // Set difficulty based on current skill levels
    float AvgProficiency = 0.0f;
    for (const FString& SkillID : TargetSkillIDs)
    {
        if (Skills.Contains(SkillID))
        {
            AvgProficiency += Skills[SkillID].ProficiencyLevel;
        }
    }
    AvgProficiency /= FMath::Max(1, TargetSkillIDs.Num());
    
    // Set difficulty slightly above current level for optimal learning
    CurrentSession.DifficultyLevel = FMath::Clamp(AvgProficiency + 0.1f, 0.0f, 1.0f);
    
    UE_LOG(LogTemp, Log, TEXT("Started training session: %s, Type: %s, Duration: %.1f min"), 
        *CurrentSession.SessionID, *SessionType, Duration);
}

FTrainingSession UGamingMasterySystem::EndTrainingSession(const TMap<FString, float>& PerformanceMetrics)
{
    CurrentSession.PerformanceMetrics = PerformanceMetrics;
    CurrentSession.bCompleted = true;
    
    // Calculate actual duration (in full implementation, this would be tracked)
    CurrentSession.ActualDuration = CurrentSession.PlannedDuration;
    
    // Update skills based on session performance
    float AvgPerformance = 0.0f;
    for (const auto& Pair : PerformanceMetrics)
    {
        AvgPerformance += Pair.Value;
    }
    AvgPerformance /= FMath::Max(1, PerformanceMetrics.Num());
    
    for (const FString& SkillID : CurrentSession.TargetSkillIDs)
    {
        UpdateSkillProficiency(SkillID, AvgPerformance, CurrentSession.ActualDuration);
    }
    
    // Add to history
    TrainingHistory.Add(CurrentSession);
    
    // Broadcast completion
    OnTrainingComplete.Broadcast(CurrentSession);
    
    UE_LOG(LogTemp, Log, TEXT("Completed training session: %s, Avg Performance: %.2f"), 
        *CurrentSession.SessionID, AvgPerformance);
    
    return CurrentSession;
}

TArray<FTrainingSession> UGamingMasterySystem::GenerateTrainingPlan(int32 DaysAhead) const
{
    TArray<FTrainingSession> Plan;
    
    // Get skills needing practice
    TArray<FGamingSkillComponent> PrioritySkills = GetSkillsNeedingPractice(10);
    
    for (int32 Day = 0; Day < DaysAhead; Day++)
    {
        FTrainingSession Session;
        Session.SessionID = FString::Printf(TEXT("planned_day%d"), Day);
        Session.PlannedDuration = 60.0f; // 1 hour default
        
        // Rotate through priority skills
        int32 SkillIndex = Day % FMath::Max(1, PrioritySkills.Num());
        if (PrioritySkills.IsValidIndex(SkillIndex))
        {
            Session.TargetSkillIDs.Add(PrioritySkills[SkillIndex].SkillID);
            
            // Add synergy skills
            for (const FString& SynergyID : PrioritySkills[SkillIndex].SynergySkills)
            {
                if (Session.TargetSkillIDs.Num() < 3)
                {
                    Session.TargetSkillIDs.Add(SynergyID);
                }
            }
        }
        
        // Determine session type based on skill phase
        if (PrioritySkills.IsValidIndex(SkillIndex))
        {
            switch (PrioritySkills[SkillIndex].AcquisitionPhase)
            {
                case ESkillAcquisitionPhase::Novice:
                case ESkillAcquisitionPhase::AdvancedBeginner:
                    Session.SessionType = TEXT("Tutorial");
                    break;
                case ESkillAcquisitionPhase::Competent:
                    Session.SessionType = TEXT("Drill");
                    break;
                case ESkillAcquisitionPhase::Proficient:
                    Session.SessionType = TEXT("Scrimmage");
                    break;
                case ESkillAcquisitionPhase::Expert:
                case ESkillAcquisitionPhase::Master:
                    Session.SessionType = TEXT("Analysis");
                    break;
            }
        }
        
        Plan.Add(Session);
    }
    
    return Plan;
}

TMap<FString, float> UGamingMasterySystem::GetOptimalPracticeSchedule() const
{
    TMap<FString, float> Schedule;
    
    // Calculate optimal practice time per skill based on:
    // 1. Current proficiency (lower = more time needed)
    // 2. Decay rate (higher = more frequent practice)
    // 3. Importance (foundational skills get more time)
    
    float TotalWeight = 0.0f;
    TMap<FString, float> Weights;
    
    for (const auto& Pair : Skills)
    {
        const FGamingSkillComponent& Skill = Pair.Value;
        
        float ProficiencyWeight = 1.0f - Skill.ProficiencyLevel;
        float DecayWeight = Skill.DecayRate * 10.0f;
        float FoundationalWeight = 1.0f;
        
        // Check if this is a prerequisite for other skills
        for (const auto& OtherPair : Skills)
        {
            if (OtherPair.Value.Prerequisites.Contains(Skill.SkillID))
            {
                FoundationalWeight += 0.2f;
            }
        }
        
        float Weight = ProficiencyWeight * 0.4f + DecayWeight * 0.3f + FoundationalWeight * 0.3f;
        Weights.Add(Pair.Key, Weight);
        TotalWeight += Weight;
    }
    
    // Normalize to minutes per day (assuming 60 min total practice)
    for (const auto& Pair : Weights)
    {
        Schedule.Add(Pair.Key, (Pair.Value / TotalWeight) * 60.0f);
    }
    
    return Schedule;
}

// ========================================
// FLOW STATE MANAGEMENT
// ========================================

void UGamingMasterySystem::UpdateFlowState(float PerformanceScore, float ChallengeLevel)
{
    // Calculate challenge-skill balance
    float CurrentSkillLevel = CalculateDomainMastery(CurrentGameDomain);
    FlowMetrics.ChallengeSkillBalance = ChallengeLevel - CurrentSkillLevel;
    
    // Update anxiety and boredom based on balance
    if (FlowMetrics.ChallengeSkillBalance > 0.2f)
    {
        // Challenge too high -> anxiety
        FlowMetrics.AnxietyLevel = FMath::Min(1.0f, FlowMetrics.AnxietyLevel + 0.1f);
        FlowMetrics.BoredomLevel = FMath::Max(0.0f, FlowMetrics.BoredomLevel - 0.1f);
    }
    else if (FlowMetrics.ChallengeSkillBalance < -0.2f)
    {
        // Challenge too low -> boredom
        FlowMetrics.BoredomLevel = FMath::Min(1.0f, FlowMetrics.BoredomLevel + 0.1f);
        FlowMetrics.AnxietyLevel = FMath::Max(0.0f, FlowMetrics.AnxietyLevel - 0.1f);
    }
    else
    {
        // Optimal zone
        FlowMetrics.AnxietyLevel = FMath::Max(0.0f, FlowMetrics.AnxietyLevel - 0.15f);
        FlowMetrics.BoredomLevel = FMath::Max(0.0f, FlowMetrics.BoredomLevel - 0.15f);
    }
    
    // Update focus based on performance
    FlowMetrics.FocusLevel = FlowMetrics.FocusLevel * 0.9f + PerformanceScore * 0.1f;
    
    // Update intrinsic motivation
    if (PerformanceScore > 0.7f && FMath::Abs(FlowMetrics.ChallengeSkillBalance) < 0.2f)
    {
        FlowMetrics.IntrinsicMotivation = FMath::Min(1.0f, FlowMetrics.IntrinsicMotivation + 0.05f);
    }
}

float UGamingMasterySystem::GetDifficultyAdjustment() const
{
    // Recommend difficulty adjustment to maintain flow
    if (FlowMetrics.AnxietyLevel > 0.5f)
    {
        return -0.1f; // Decrease difficulty
    }
    else if (FlowMetrics.BoredomLevel > 0.5f)
    {
        return 0.1f; // Increase difficulty
    }
    return 0.0f; // Maintain current difficulty
}

bool UGamingMasterySystem::IsInFlowState() const
{
    return FlowMetrics.FlowIntensity >= FlowEntryThreshold;
}

void UGamingMasterySystem::RequestFlowOptimizationTips()
{
    const FString Metrics = FString::Printf(
        TEXT("flow intensity: %.2f (in flow: %s)\nchallenge-skill balance: %.2f\nfocus: %.2f\nintrinsic motivation: %.2f\n")
        TEXT("anxiety: %.2f\nboredom: %.2f\ntime distortion: %.2f\ntime in flow: %.0fs\nsuggested difficulty adjustment: %+.2f\n"),
        FlowMetrics.FlowIntensity, IsInFlowState() ? TEXT("yes") : TEXT("no"), FlowMetrics.ChallengeSkillBalance,
        FlowMetrics.FocusLevel, FlowMetrics.IntrinsicMotivation, FlowMetrics.AnxietyLevel, FlowMetrics.BoredomLevel,
        FlowMetrics.TimeDistortion, FlowMetrics.TimeInFlow, GetDifficultyAdjustment());

    // Condition router (which kinds of tip are relevant); the model writes the tips.
    TArray<FString> Conditions;
    if (FlowMetrics.AnxietyLevel > 0.5f)
    {
        Conditions.Add(TEXT("anxiety is high - the challenge may be too high (fundamentals practice, short breaks to reduce stress)"));
    }
    if (FlowMetrics.BoredomLevel > 0.5f)
    {
        Conditions.Add(TEXT("boredom is high - the challenge may be too low (raise difficulty, set specific session goals)"));
    }
    if (FlowMetrics.FocusLevel < 0.5f)
    {
        Conditions.Add(TEXT("focus is low (remove distractions, warm up before practice)"));
    }
    if (FlowMetrics.IntrinsicMotivation < 0.5f)
    {
        Conditions.Add(TEXT("intrinsic motivation is low (reconnect with why they enjoy the game, vary the activity)"));
    }
    if (Conditions.Num() == 0)
    {
        Conditions.Add(TEXT("none of concern - the player is in a good state for learning; encourage them to keep it up"));
    }
    TMap<FString, FString> Slots;
    Slots.Add(TEXT("conditions"), FString::Join(Conditions, TEXT("; ")));
    EchoCoaching::RequestFromTemplate(this, TEXT("flow_optimization"), Slots, Metrics, &OnCoachingReady, &OnCoachingFailed);
}

// ========================================
// ML MODEL INTERFACE
// ========================================

TArray<float> UGamingMasterySystem::RunESNInference(const TArray<float>& InputFeatures)
{
    TArray<float> Output;
    
    if (ESNReservoirState.Num() == 0 || ESNInputWeights.Num() == 0)
    {
        return Output;
    }
    
    // ESN update equation: x(t) = (1-a)*x(t-1) + a*tanh(W_in*u(t) + W*x(t-1))
    TArray<float> NewState;
    NewState.SetNum(ESNReservoirState.Num());
    
    for (int32 i = 0; i < ESNReservoirState.Num(); i++)
    {
        float InputSum = 0.0f;
        for (int32 j = 0; j < FMath::Min(InputFeatures.Num(), ESNInputWeights[i].Num()); j++)
        {
            InputSum += ESNInputWeights[i][j] * InputFeatures[j];
        }
        
        float ReservoirSum = 0.0f;
        for (int32 j = 0; j < ESNReservoirWeights[i].Num(); j++)
        {
            ReservoirSum += ESNReservoirWeights[i][j] * ESNReservoirState[j];
        }
        
        float Activation = FMath::Tanh(InputSum + ReservoirSum);
        NewState[i] = (1.0f - ESNLeakingRate) * ESNReservoirState[i] + ESNLeakingRate * Activation;
    }
    
    ESNReservoirState = NewState;
    
    // Compute output: y(t) = W_out * x(t)
    if (ESNOutputWeights.Num() > 0)
    {
        Output.SetNum(ESNOutputWeights.Num());
        for (int32 i = 0; i < ESNOutputWeights.Num(); i++)
        {
            float Sum = 0.0f;
            for (int32 j = 0; j < FMath::Min(ESNReservoirState.Num(), ESNOutputWeights[i].Num()); j++)
            {
                Sum += ESNOutputWeights[i][j] * ESNReservoirState[j];
            }
            Output[i] = Sum;
        }
    }
    
    return Output;
}

void UGamingMasterySystem::UpdateESNTraining(const TArray<float>& InputFeatures, const TArray<float>& TargetOutput)
{
    // Run inference to update reservoir state
    TArray<float> CurrentOutput = RunESNInference(InputFeatures);
    
    // Simple online learning: adjust output weights based on error
    if (CurrentOutput.Num() == TargetOutput.Num() && ESNOutputWeights.Num() == TargetOutput.Num())
    {
        float LearningRate = 0.01f;
        
        for (int32 i = 0; i < TargetOutput.Num(); i++)
        {
            float Error = TargetOutput[i] - CurrentOutput[i];
            
            for (int32 j = 0; j < ESNOutputWeights[i].Num(); j++)
            {
                ESNOutputWeights[i][j] += LearningRate * Error * ESNReservoirState[j];
            }
        }
    }
}

FString UGamingMasterySystem::RunMCTS(const TArray<float>& GameState, int32 SimulationCount)
{
    LastMCTSError.Reset();
    if (MCTSLegalActions.Num() == 0)
    {
        LastMCTSError = TEXT("MCTS unavailable: MCTSLegalActions is empty (the game must supply its legal action set)");
    }
    else if (!MCTSTransitionModel.IsBound())
    {
        LastMCTSError = TEXT("MCTS unavailable: no transition model bound (SetMCTSTransitionModel); without one every action leads to the same state");
    }
    if (!LastMCTSError.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("%s"), *LastMCTSError);
        return FString();
    }

    // Create root node
    FMCTSNode* Root = new FMCTSNode();
    Root->State = GameState;
    
    // Run simulations
    for (int32 i = 0; i < SimulationCount; i++)
    {
        FMCTSNode* Node = Root;
        
        // Selection: traverse tree using UCB
        while (Node->bExpanded && Node->Children.Num() > 0)
        {
            float BestUCB = -FLT_MAX;
            FMCTSNode* BestChild = nullptr;
            
            for (FMCTSNode* Child : Node->Children)
            {
                float UCB = Child->GetAverageValue() + 
                    FMath::Sqrt(2.0f * FMath::Loge(Node->VisitCount + 1) / (Child->VisitCount + 1));
                
                if (UCB > BestUCB)
                {
                    BestUCB = UCB;
                    BestChild = Child;
                }
            }
            
            if (BestChild)
            {
                Node = BestChild;
            }
            else
            {
                break;
            }
        }
        
        // Expansion
        if (!Node->bExpanded)
        {
            Node = ExpandMCTSNode(Node, Node->State);
        }
        
        // Simulation
        float Value = SimulateMCTS(Node->State, 10);
        
        // Backpropagation
        BackpropagateMCTS(Node, Value);
    }
    
    // Select best action from root
    FString BestAction;
    float BestValue = -FLT_MAX;
    
    for (FMCTSNode* Child : Root->Children)
    {
        if (Child->GetAverageValue() > BestValue)
        {
            BestValue = Child->GetAverageValue();
            BestAction = Child->Action;
        }
    }
    
    delete Root;

    if (BestAction.IsEmpty())
    {
        LastMCTSError = TEXT("MCTS produced no action: the transition model applied none of the legal actions to this state");
        UE_LOG(LogTemp, Warning, TEXT("%s"), *LastMCTSError);
    }
    return BestAction;
}

float UGamingMasterySystem::EstimateStateValue(const TArray<float>& GameState)
{
    // Use ESN to estimate state value
    TArray<float> Output = RunESNInference(GameState);
    
    if (Output.Num() > 0)
    {
        return Output[0]; // First output is value estimate
    }
    
    return 0.5f; // Neutral value
}

// ========================================
// INTERNAL METHODS
// ========================================

void UGamingMasterySystem::InitializeESN(int32 InputSize, int32 ReservoirSize, int32 OutputSize)
{
    // Initialize reservoir state
    ESNReservoirState.SetNum(ReservoirSize);
    for (int32 i = 0; i < ReservoirSize; i++)
    {
        ESNReservoirState[i] = FMath::FRandRange(-0.1f, 0.1f);
    }
    
    // Initialize input weights
    ESNInputWeights.SetNum(ReservoirSize);
    for (int32 i = 0; i < ReservoirSize; i++)
    {
        ESNInputWeights[i].SetNum(InputSize);
        for (int32 j = 0; j < InputSize; j++)
        {
            ESNInputWeights[i][j] = FMath::FRandRange(-1.0f, 1.0f);
        }
    }
    
    // Initialize reservoir weights (sparse, scaled by spectral radius)
    ESNReservoirWeights.SetNum(ReservoirSize);
    for (int32 i = 0; i < ReservoirSize; i++)
    {
        ESNReservoirWeights[i].SetNum(ReservoirSize);
        for (int32 j = 0; j < ReservoirSize; j++)
        {
            // Sparse connectivity (~10%)
            if (FMath::FRand() < 0.1f)
            {
                ESNReservoirWeights[i][j] = FMath::FRandRange(-1.0f, 1.0f) * ESNSpectralRadius;
            }
            else
            {
                ESNReservoirWeights[i][j] = 0.0f;
            }
        }
    }
    
    // Initialize output weights
    ESNOutputWeights.SetNum(OutputSize);
    for (int32 i = 0; i < OutputSize; i++)
    {
        ESNOutputWeights[i].SetNum(ReservoirSize);
        for (int32 j = 0; j < ReservoirSize; j++)
        {
            ESNOutputWeights[i][j] = FMath::FRandRange(-0.5f, 0.5f);
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("Initialized ESN: Input=%d, Reservoir=%d, Output=%d"), 
        InputSize, ReservoirSize, OutputSize);
}

void UGamingMasterySystem::ApplySkillDecay(float DeltaTime)
{
    float CurrentTime = GetWorld()->GetTimeSeconds();
    
    TArray<FString> SkillKeys;
    for (const auto& Pair : Skills)
    {
        SkillKeys.Add(Pair.Key);
    }
    
    for (const FString& Key : SkillKeys)
    {
        FGamingSkillComponent& Skill = Skills[Key];
        
        // Calculate time since last practice in days
        float DaysSincePractice = (CurrentTime - Skill.LastPracticeTime) / 86400.0f;
        
        if (DaysSincePractice > 1.0f)
        {
            // Apply decay (exponential)
            float DecayAmount = Skill.DecayRate * DeltaTime * DaysSincePractice;
            Skill.ProficiencyLevel = FMath::Max(0.0f, Skill.ProficiencyLevel - DecayAmount);
            
            // Update acquisition phase if needed
            Skill.AcquisitionPhase = CalculateAcquisitionPhase(Skill.ProficiencyLevel);
        }
    }
}

ESkillAcquisitionPhase UGamingMasterySystem::CalculateAcquisitionPhase(float Proficiency) const
{
    if (Proficiency < 0.15f) return ESkillAcquisitionPhase::Novice;
    if (Proficiency < 0.35f) return ESkillAcquisitionPhase::AdvancedBeginner;
    if (Proficiency < 0.55f) return ESkillAcquisitionPhase::Competent;
    if (Proficiency < 0.75f) return ESkillAcquisitionPhase::Proficient;
    if (Proficiency < 0.95f) return ESkillAcquisitionPhase::Expert;
    return ESkillAcquisitionPhase::Master;
}

float UGamingMasterySystem::CalculatePatternSimilarity(const TArray<float>& Features1, const TArray<float>& Features2) const
{
    if (Features1.Num() != Features2.Num() || Features1.Num() == 0)
    {
        return 0.0f;
    }
    
    // Cosine similarity
    float DotProduct = 0.0f;
    float Norm1 = 0.0f;
    float Norm2 = 0.0f;
    
    for (int32 i = 0; i < Features1.Num(); i++)
    {
        DotProduct += Features1[i] * Features2[i];
        Norm1 += Features1[i] * Features1[i];
        Norm2 += Features2[i] * Features2[i];
    }
    
    if (Norm1 == 0.0f || Norm2 == 0.0f)
    {
        return 0.0f;
    }
    
    return DotProduct / (FMath::Sqrt(Norm1) * FMath::Sqrt(Norm2));
}

void UGamingMasterySystem::UpdateBayesianModel(FOpponentModel& Model, const FString& Action, const TArray<float>& State)
{
    // Simple Bayesian update for action predictions
    // P(action | state) = P(state | action) * P(action) / P(state)
    
    // Update prior probability of action
    float Alpha = 1.0f; // Smoothing parameter
    float TotalActions = Model.ObservationCount + Alpha * Model.BehavioralTendencies.Num();
    
    TArray<FString> ActionKeys;
    for (const auto& Pair : Model.ActionPredictions)
    {
        ActionKeys.Add(Pair.Key);
    }
    for (const FString& Key : ActionKeys)
    {
        float ActionCount = Model.BehavioralTendencies.Contains(Key) ? 
            Model.BehavioralTendencies[Key] * Model.ObservationCount : Alpha;
        Model.ActionPredictions[Key] = ActionCount / TotalActions;
    }
    
    // Update skill level estimate based on action quality
    // (In full implementation, this would use a more sophisticated model)
    Model.EstimatedSkillLevel = Model.EstimatedSkillLevel * 0.99f + 0.01f * 0.5f;
}

float UGamingMasterySystem::CalculateFlowIntensity() const
{
    // Flow intensity based on:
    // 1. Challenge-skill balance (optimal when close to 0)
    // 2. Low anxiety and boredom
    // 3. High focus
    // 4. High intrinsic motivation
    
    float BalanceScore = 1.0f - FMath::Abs(FlowMetrics.ChallengeSkillBalance);
    float EmotionalScore = 1.0f - (FlowMetrics.AnxietyLevel + FlowMetrics.BoredomLevel) / 2.0f;
    float FocusScore = FlowMetrics.FocusLevel;
    float MotivationScore = FlowMetrics.IntrinsicMotivation;
    
    return (BalanceScore * 0.3f + EmotionalScore * 0.3f + FocusScore * 0.2f + MotivationScore * 0.2f);
}

FMCTSNode* UGamingMasterySystem::ExpandMCTSNode(FMCTSNode* Parent, const TArray<float>& State)
{
    Parent->bExpanded = true;

    // One child per legal action the game's forward model can apply to this state
    for (const FString& Action : MCTSLegalActions)
    {
        TArray<float> NextState;
        if (!MCTSTransitionModel.IsBound() || !MCTSTransitionModel.Execute(State, Action, NextState))
        {
            continue;
        }
        FMCTSNode* Child = new FMCTSNode();
        Child->Parent = Parent;
        Child->Action = Action;
        Child->State = MoveTemp(NextState);
        Parent->Children.Add(Child);
    }

    // Evaluate the first new child now; UCB selection visits the remaining unvisited children
    // in later iterations. A node with no applicable action is a leaf.
    return Parent->Children.Num() > 0 ? Parent->Children[0] : Parent;
}

float UGamingMasterySystem::SimulateMCTS(const TArray<float>& State, int32 Depth)
{
    // Use ESN value estimate for simulation
    return EstimateStateValue(State);
}

void UGamingMasterySystem::BackpropagateMCTS(FMCTSNode* Node, float Value)
{
    while (Node != nullptr)
    {
        Node->VisitCount++;
        Node->TotalValue += Value;
        Node = Node->Parent;
    }
}

#undef LOCTEXT_NAMESPACE
