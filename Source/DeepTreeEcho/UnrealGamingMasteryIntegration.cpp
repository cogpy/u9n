// UnrealGamingMasteryIntegration.cpp
// Deep Tree Echo - Unreal Engine Gaming Mastery Integration
// Copyright (c) 2025 Deep Tree Echo Project

#include "UnrealGamingMasteryIntegration.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "EchoModelClient.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

UUnrealGamingMasteryIntegration::UUnrealGamingMasteryIntegration()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f; // Every frame

    ComboMoveVocabulary = { TEXT("light_attack"), TEXT("heavy_attack"), TEXT("special_attack"),
                            TEXT("block"), TEXT("dodge"), TEXT("counter"), TEXT("grab") };

    ComboPromptTemplate = TEXT(
        "You are Deep Tree Echo, fighting in a real-time game. You are in a {state} position. "
        "Your measured combat performance:\n{metrics}\nYour most recent inputs: {recent_inputs}.\n"
        "Choose the combo you will use next: a sequence of 1 to {max_length} moves, each exactly one of: {moves}. "
        "Reply with only a JSON object: {\"combo\": [\"<move>\", ...]}");
}

void UUnrealGamingMasteryIntegration::BeginPlay()
{
    Super::BeginPlay();

    InitializeReferences();

    UE_LOG(LogTemp, Log, TEXT("UnrealGamingMasteryIntegration initialized - Full Unreal Engine Integration Active"));
}

void UUnrealGamingMasteryIntegration::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    FrameCounter++;

    // Update perception data every frame
    UpdatePerceptionData();

    // Update movement analysis
    if (bEnableMovementAnalysis)
    {
        UpdateMovementAnalysis(DeltaTime);
    }

    // Update combat analysis
    if (bEnableCombatAnalysis)
    {
        UpdateCombatAnalysis(DeltaTime);
    }

    // Process ML recommendations every 5 frames
    if (FrameCounter % 5 == 0)
    {
        ProcessMLRecommendations();
    }

    // Update game state for ML every 10 frames
    if (FrameCounter % 10 == 0)
    {
        UpdateGameStateForML();
    }

    // Record frame for replay if active
    if (bReplayRecordingActive)
    {
        RecordFrameForReplay();
    }
}

// ========================================
// INITIALIZATION
// ========================================

void UUnrealGamingMasteryIntegration::InitializeReferences()
{
    // Find Gaming Mastery System
    if (!GamingMasterySystem)
    {
        GamingMasterySystem = GetOwner()->FindComponentByClass<UGamingMasterySystem>();
    }

    // Find Strategic Cognition Bridge
    if (!CognitionBridge)
    {
        CognitionBridge = GetOwner()->FindComponentByClass<UStrategicCognitionBridge>();
    }

    // Find Echobeats Integration
    if (!EchobeatsIntegration)
    {
        EchobeatsIntegration = GetOwner()->FindComponentByClass<UEchobeatsGamingIntegration>();
    }

    // Initialize position history
    PositionHistory.Reserve(100);
}

// ========================================
// INPUT SYSTEM INTEGRATION
// ========================================

void UUnrealGamingMasteryIntegration::ProcessInputAction(const FString& ActionName, float InputValue, float Timestamp)
{
    // Store in history
    InputHistory.Add(TPair<FString, float>(ActionName, Timestamp));
    if (InputHistory.Num() > 100)
    {
        InputHistory.RemoveAt(0);
    }

    // Analyze input timing
    float TimingQuality = AnalyzeInputTiming(ActionName, Timestamp);

    // Update cognition bridge with motor execution
    if (CognitionBridge)
    {
        CognitionBridge->RecordMotorExecution(ActionName, FMath::Abs(InputValue), TimingQuality);
    }

    // Update gaming mastery system
    if (GamingMasterySystem)
    {
        GamingMasterySystem->UpdateSkillProficiency(ActionName, TimingQuality, 0.1f);
    }

    // Broadcast the measured execution quality (no canned feedback text)
    OnSkillExecutionAnalyzed.Broadcast(ActionName, TimingQuality);
}

float UUnrealGamingMasteryIntegration::GetOptimizedInputTiming(const FString& ActionName) const
{
    // Calculate optimal timing based on game state
    if (EchobeatsIntegration)
    {
        return EchobeatsIntegration->GetActionTimingRecommendation(ActionName);
    }

    return 0.0f; // Immediate
}

FString UUnrealGamingMasteryIntegration::PredictNextInput(const TArray<FString>& RecentInputs) const
{
    // Use pattern recognition to predict next input
    if (RecentInputs.Num() < 2)
    {
        return TEXT("");
    }

    // Simple pattern matching (in full implementation, use ML)
    TMap<FString, int32> FollowCounts;

    for (int32 i = 0; i < InputHistory.Num() - 1; i++)
    {
        // Check if recent pattern matches
        bool bMatches = true;
        for (int32 j = 0; j < FMath::Min(RecentInputs.Num(), InputHistory.Num() - i - 1); j++)
        {
            if (InputHistory[i + j].Key != RecentInputs[j])
            {
                bMatches = false;
                break;
            }
        }

        if (bMatches && i + RecentInputs.Num() < InputHistory.Num())
        {
            FString NextInput = InputHistory[i + RecentInputs.Num()].Key;
            if (FollowCounts.Contains(NextInput))
            {
                FollowCounts[NextInput]++;
            }
            else
            {
                FollowCounts.Add(NextInput, 1);
            }
        }
    }

    // Return most common follow-up
    FString MostLikely;
    int32 MaxCount = 0;
    for (const auto& Pair : FollowCounts)
    {
        if (Pair.Value > MaxCount)
        {
            MaxCount = Pair.Value;
            MostLikely = Pair.Key;
        }
    }

    return MostLikely;
}

TMap<FString, float> UUnrealGamingMasteryIntegration::AnalyzeInputSequence(const TArray<FString>& InputSequence) const
{
    TMap<FString, float> Analysis;

    if (InputSequence.Num() == 0)
    {
        return Analysis;
    }

    // Calculate sequence metrics
    float TotalTimingQuality = 0.0f;
    int32 ComboCount = 0;
    float SequenceSpeed = 0.0f;

    for (int32 i = 0; i < InputSequence.Num(); i++)
    {
        // Find this input in history
        for (int32 j = 0; j < InputHistory.Num(); j++)
        {
            if (InputHistory[j].Key == InputSequence[i])
            {
                TotalTimingQuality += AnalyzeInputTiming(InputSequence[i], InputHistory[j].Value);
                ComboCount++;

                // Calculate speed between inputs
                if (j > 0)
                {
                    SequenceSpeed += InputHistory[j].Value - InputHistory[j - 1].Value;
                }
                break;
            }
        }
    }

    Analysis.Add(TEXT("timing_quality"), ComboCount > 0 ? TotalTimingQuality / ComboCount : 0.0f);
    Analysis.Add(TEXT("combo_length"), (float)ComboCount);
    Analysis.Add(TEXT("sequence_speed"), ComboCount > 1 ? SequenceSpeed / (ComboCount - 1) : 0.0f);
    Analysis.Add(TEXT("completion_rate"), (float)ComboCount / InputSequence.Num());

    return Analysis;
}

void UUnrealGamingMasteryIntegration::RequestInputImprovementSuggestions()
{
    const FString Topic = TEXT("input_improvement");
    if (InputHistory.Num() == 0)
    {
        OnCoachingFailed.Broadcast(Topic, TEXT("no inputs recorded yet"));
        return;
    }

    // Measure average timing quality per input
    TMap<FString, float> InputQuality;
    TMap<FString, int32> InputCounts;
    for (const auto& Input : InputHistory)
    {
        InputQuality.FindOrAdd(Input.Key) += AnalyzeInputTiming(Input.Key, Input.Value);
        InputCounts.FindOrAdd(Input.Key) += 1;
    }

    FString Metrics = FString::Printf(TEXT("recent inputs analysed: %d\n"), InputHistory.Num());
    TArray<FString> WeakInputs;
    for (const auto& Pair : InputQuality)
    {
        const int32 Count = InputCounts[Pair.Key];
        const float AvgQuality = Pair.Value / Count;
        Metrics += FString::Printf(TEXT("input %s: %d presses, average timing quality %.0f%%\n"),
                                   *Pair.Key, Count, 100.0f * AvgQuality);
        if (AvgQuality < 0.6f)
        {
            WeakInputs.Add(FString::Printf(TEXT("%s (%.0f%%)"), *Pair.Key, 100.0f * AvgQuality));
        }
    }

    TMap<FString, FString> Slots;
    Slots.Add(TEXT("weak_inputs"), WeakInputs.Num() ? FString::Join(WeakInputs, TEXT(", ")) : FString(TEXT("none")));
    EchoCoaching::RequestFromTemplate(this, Topic, Slots, Metrics, &OnCoachingReady, &OnCoachingFailed);
}

// ========================================
// MOVEMENT SYSTEM INTEGRATION
// ========================================

FMovementAnalysis UUnrealGamingMasteryIntegration::AnalyzeMovementFrame(FVector CurrentPosition, FVector CurrentVelocity, FVector TargetPosition)
{
    FMovementAnalysis Analysis;

    // Store position for history
    PositionHistory.Add(CurrentPosition);
    if (PositionHistory.Num() > 100)
    {
        PositionHistory.RemoveAt(0);
    }

    // Calculate efficiency (direct distance vs actual distance traveled)
    if (PositionHistory.Num() >= 2)
    {
        float DirectDistance = FVector::Dist(PositionHistory[0], CurrentPosition);
        float ActualDistance = 0.0f;
        for (int32 i = 1; i < PositionHistory.Num(); i++)
        {
            ActualDistance += FVector::Dist(PositionHistory[i - 1], PositionHistory[i]);
        }

        Analysis.Efficiency = DirectDistance > 0.0f ? FMath::Min(1.0f, DirectDistance / FMath::Max(ActualDistance, 1.0f)) : 1.0f;
    }

    // Calculate path optimality (how well we're moving toward target)
    FVector ToTarget = TargetPosition - CurrentPosition;
    ToTarget.Normalize();
    FVector VelocityDir = CurrentVelocity;
    VelocityDir.Normalize();

    Analysis.PathOptimality = (FVector::DotProduct(ToTarget, VelocityDir) + 1.0f) / 2.0f;

    // Calculate smoothness (low acceleration changes = smooth)
    if (PositionHistory.Num() >= 3)
    {
        FVector Accel1 = PositionHistory[PositionHistory.Num() - 1] - PositionHistory[PositionHistory.Num() - 2];
        FVector Accel2 = PositionHistory[PositionHistory.Num() - 2] - PositionHistory[PositionHistory.Num() - 3];
        float AccelChange = FVector::Dist(Accel1, Accel2);
        Analysis.Smoothness = 1.0f - FMath::Min(1.0f, AccelChange / 100.0f);
    }

    // Classify movement type
    float Speed = CurrentVelocity.Size();
    if (Speed < 10.0f)
    {
        Analysis.MovementType = TEXT("stationary");
    }
    else if (Speed < 300.0f)
    {
        Analysis.MovementType = TEXT("walking");
    }
    else if (Speed < 600.0f)
    {
        Analysis.MovementType = TEXT("running");
    }
    else
    {
        Analysis.MovementType = TEXT("sprinting");
    }

    MovementAnalysis = Analysis;
    return Analysis;
}

TArray<FVector> UUnrealGamingMasteryIntegration::GetOptimalMovementPath(FVector Start, FVector End, const TArray<AActor*>& Obstacles)
{
    TArray<FVector> Path;
    Path.Add(Start);

    // Simple pathfinding (in full implementation, use NavMesh)
    FVector Current = Start;
    FVector Direction = (End - Start).GetSafeNormal();

    int32 MaxIterations = 100;
    float StepSize = 100.0f;

    for (int32 i = 0; i < MaxIterations && FVector::Dist(Current, End) > StepSize; i++)
    {
        // Check for obstacles
        bool bBlocked = false;
        FVector NextPos = Current + Direction * StepSize;

        for (AActor* Obstacle : Obstacles)
        {
            if (Obstacle && FVector::Dist(NextPos, Obstacle->GetActorLocation()) < 100.0f)
            {
                bBlocked = true;
                break;
            }
        }

        if (bBlocked)
        {
            // Try to go around
            FVector Right = FVector::CrossProduct(Direction, FVector::UpVector);
            NextPos = Current + Right * StepSize;
        }

        Current = NextPos;
        Path.Add(Current);
    }

    Path.Add(End);
    return Path;
}

FVector UUnrealGamingMasteryIntegration::PredictMovementOutcome(FVector CurrentPosition, FVector InputDirection, float DeltaTime)
{
    // Get character movement component if available
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (Character)
    {
        UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement();
        if (MovementComp)
        {
            float Speed = MovementComp->MaxWalkSpeed;
            return CurrentPosition + InputDirection * Speed * DeltaTime;
        }
    }

    // Default prediction
    return CurrentPosition + InputDirection * 600.0f * DeltaTime;
}

FVector UUnrealGamingMasteryIntegration::GetDodgeRecommendation(FVector ThreatDirection, float ThreatSpeed)
{
    // Calculate optimal dodge direction (perpendicular to threat)
    FVector DodgeDir = FVector::CrossProduct(ThreatDirection, FVector::UpVector);
    DodgeDir.Normalize();

    // Choose direction based on current position and obstacles
    // (In full implementation, consider environment)

    return DodgeDir;
}

float UUnrealGamingMasteryIntegration::AnalyzePositioning(FVector CurrentPosition, const TArray<FVector>& EnemyPositions, const TArray<FVector>& CoverPositions)
{
    float Score = 0.5f; // Base score

    // Factor 1: Distance from enemies (not too close, not too far)
    float AvgEnemyDist = 0.0f;
    for (const FVector& EnemyPos : EnemyPositions)
    {
        AvgEnemyDist += FVector::Dist(CurrentPosition, EnemyPos);
    }
    if (EnemyPositions.Num() > 0)
    {
        AvgEnemyDist /= EnemyPositions.Num();
        // Optimal distance around 1000 units
        float DistScore = 1.0f - FMath::Abs(AvgEnemyDist - 1000.0f) / 1000.0f;
        Score += DistScore * 0.3f;
    }

    // Factor 2: Proximity to cover
    float MinCoverDist = FLT_MAX;
    for (const FVector& CoverPos : CoverPositions)
    {
        float Dist = FVector::Dist(CurrentPosition, CoverPos);
        MinCoverDist = FMath::Min(MinCoverDist, Dist);
    }
    if (MinCoverDist < FLT_MAX)
    {
        float CoverScore = 1.0f - FMath::Min(1.0f, MinCoverDist / 500.0f);
        Score += CoverScore * 0.2f;
    }

    // Factor 3: Not surrounded
    if (EnemyPositions.Num() >= 2)
    {
        // Check if enemies are on multiple sides
        int32 SidesWithEnemies = 0;
        FVector Forward = GetOwner()->GetActorForwardVector();
        FVector Right = GetOwner()->GetActorRightVector();

        for (const FVector& EnemyPos : EnemyPositions)
        {
            FVector ToEnemy = (EnemyPos - CurrentPosition).GetSafeNormal();
            // Count unique directions
        }

        float SurroundScore = 1.0f - (float)SidesWithEnemies / 4.0f;
        Score += SurroundScore * 0.2f;
    }

    return FMath::Clamp(Score, 0.0f, 1.0f);
}

// ========================================
// COMBAT SYSTEM INTEGRATION
// ========================================

void UUnrealGamingMasteryIntegration::ProcessCombatEvent(const FString& EventType, AActor* Source, AActor* Target, float Damage)
{
    // Store in history
    float CurrentTime = GetWorld()->GetTimeSeconds();
    CombatEventHistory.Add(TPair<FString, float>(EventType, CurrentTime));
    if (CombatEventHistory.Num() > 200)
    {
        CombatEventHistory.RemoveAt(0);
    }

    // Update combat analysis
    if (EventType == TEXT("hit"))
    {
        CombatAnalysis.Accuracy = (CombatAnalysis.Accuracy * 0.9f) + 0.1f;
    }
    else if (EventType == TEXT("miss"))
    {
        CombatAnalysis.Accuracy = CombatAnalysis.Accuracy * 0.9f;
    }

    // Update damage efficiency
    if (Source == GetOwner())
    {
        // We dealt damage
        CombatAnalysis.DamageEfficiency += Damage * 0.01f;
    }
    else if (Target == GetOwner())
    {
        // We took damage
        CombatAnalysis.DamageEfficiency -= Damage * 0.01f;
        
        // Broadcast threat
        if (Source)
        {
            FVector ThreatDir = (Source->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal();
            OnThreatDetected.Broadcast(Source, Damage / 100.0f, ThreatDir);
        }
    }

    // Update gaming mastery system
    if (GamingMasterySystem && Source == GetOwner())
    {
        GamingMasterySystem->UpdateSkillProficiency(TEXT("combat"), CombatAnalysis.Accuracy, 0.1f);
    }
}

AActor* UUnrealGamingMasteryIntegration::GetHighestPriorityTarget(const TArray<AActor*>& PotentialTargets)
{
    AActor* BestTarget = nullptr;
    float BestScore = -FLT_MAX;

    for (AActor* Target : PotentialTargets)
    {
        if (!Target) continue;

        float Score = CalculateTargetPriorityScore(Target);
        if (Score > BestScore)
        {
            BestScore = Score;
            BestTarget = Target;
        }
    }

    if (BestTarget)
    {
        HighlightOptimalTarget(BestTarget);
    }

    return BestTarget;
}

FCombatAnalysis UUnrealGamingMasteryIntegration::AnalyzeCombatPerformance() const
{
    return CombatAnalysis;
}

float UUnrealGamingMasteryIntegration::GetOptimalAttackTiming(AActor* Target) const
{
    if (!Target) return 0.0f;

    // Check target's animation state for vulnerability windows
    // (In full implementation, analyze target's animation)

    // Use echobeats for timing recommendation
    if (EchobeatsIntegration)
    {
        return EchobeatsIntegration->GetActionTimingRecommendation(TEXT("attack"));
    }

    return 0.0f;
}

void UUnrealGamingMasteryIntegration::RequestComboRecommendation(const FString& CurrentState)
{
    if (ComboMoveVocabulary.Num() == 0)
    {
        LastComboRecommendationError = TEXT("ComboMoveVocabulary is empty");
        UE_LOG(LogTemp, Error, TEXT("UnrealGamingMasteryIntegration: %s"), *LastComboRecommendationError);
        OnComboRecommendationFailed.Broadcast(CurrentState, LastComboRecommendationError);
        return;
    }

    const FString Metrics = FString::Printf(
        TEXT("accuracy %.2f\ndamage efficiency %.2f\nreaction time %.2f s\ncombo execution rate %.2f\n")
        TEXT("positioning %.2f\ntarget prioritization %.2f\ncooldown management %.2f"),
        CombatAnalysis.Accuracy, CombatAnalysis.DamageEfficiency, CombatAnalysis.ReactionTime,
        CombatAnalysis.ComboExecutionRate, CombatAnalysis.PositioningScore,
        CombatAnalysis.TargetPrioritization, CombatAnalysis.CooldownManagement);

    TArray<FString> Recent;
    for (int32 Index = FMath::Max(0, InputHistory.Num() - 8); Index < InputHistory.Num(); ++Index)
    {
        Recent.Add(InputHistory[Index].Key);
    }

    TMap<FString, FString> Slots;
    Slots.Add(TEXT("state"), CurrentState);
    Slots.Add(TEXT("moves"), FString::Join(ComboMoveVocabulary, TEXT(", ")));
    Slots.Add(TEXT("max_length"), FString::FromInt(FMath::Max(1, MaxComboLength)));
    Slots.Add(TEXT("metrics"), Metrics);
    Slots.Add(TEXT("recent_inputs"), Recent.Num() ? FString::Join(Recent, TEXT(", ")) : FString(TEXT("none yet")));

    TArray<FEchoModelMessage> Messages;
    Messages.Add(FEchoModelMessage(TEXT("user"), UEchoModelClient::FillPrompt(ComboPromptTemplate, Slots)));

    const int32 RequestSerial = ++ComboRequestSerial;
    TWeakObjectPtr<UUnrealGamingMasteryIntegration> WeakThis(this);
    UEchoModelClient::Get()->Chat(Messages, FOnEchoModelReply::CreateLambda(
        [WeakThis, RequestSerial, CurrentState](bool bSuccess, const FString& TextOrError)
        {
            if (UUnrealGamingMasteryIntegration* Self = WeakThis.Get())
            {
                Self->HandleComboReply(RequestSerial, CurrentState, bSuccess, TextOrError);
            }
        }), 0.7f);
}

void UUnrealGamingMasteryIntegration::HandleComboReply(int32 RequestSerial, const FString& CurrentState,
                                                       bool bSuccess, const FString& TextOrError)
{
    if (RequestSerial != ComboRequestSerial)
    {
        return; // superseded by a newer request
    }

    auto Fail = [this, &CurrentState](const FString& Error)
    {
        LastComboRecommendationError = Error;
        UE_LOG(LogTemp, Error, TEXT("UnrealGamingMasteryIntegration: no model combo for '%s': %s"), *CurrentState, *Error);
        OnComboRecommendationFailed.Broadcast(CurrentState, Error);
    };

    if (!bSuccess)
    {
        Fail(TextOrError);
        return;
    }

    TSharedPtr<FJsonObject> Json;
    const TArray<TSharedPtr<FJsonValue>>* MoveValues = nullptr;
    if (!UEchoModelClient::ParseJsonObject(TextOrError, Json) || !Json->TryGetArrayField(TEXT("combo"), MoveValues) ||
        !MoveValues || MoveValues->Num() == 0)
    {
        Fail(FString::Printf(TEXT("model reply is not the requested JSON: %s"), *TextOrError.Left(200)));
        return;
    }
    if (MoveValues->Num() > FMath::Max(1, MaxComboLength))
    {
        Fail(FString::Printf(TEXT("model combo has %d moves, more than %d"), MoveValues->Num(), MaxComboLength));
        return;
    }

    TArray<FString> Combo;
    for (const TSharedPtr<FJsonValue>& Value : *MoveValues)
    {
        FString Move;
        if (!Value.IsValid() || !Value->TryGetString(Move))
        {
            Fail(TEXT("model combo contains a non-string move"));
            return;
        }
        Move.TrimStartAndEndInline();
        const FString* Known = ComboMoveVocabulary.FindByPredicate(
            [&Move](const FString& Candidate) { return Candidate.Equals(Move, ESearchCase::IgnoreCase); });
        if (!Known)
        {
            Fail(FString::Printf(TEXT("model chose a move outside the vocabulary: %s"), *Move));
            return;
        }
        Combo.Add(*Known);
    }

    LastComboRecommendationError.Reset();
    ComboRecommendations.Add(CurrentState, Combo);
    OnComboRecommended.Broadcast(CurrentState, Combo);
}

TArray<FString> UUnrealGamingMasteryIntegration::GetComboRecommendation(const FString& CurrentState) const
{
    const TArray<FString>* Combo = ComboRecommendations.Find(CurrentState);
    return Combo ? *Combo : TArray<FString>();
}

FString UUnrealGamingMasteryIntegration::PredictEnemyAttack(AActor* Enemy) const
{
    if (!Enemy) return TEXT("");

    // Use gaming mastery system's opponent modeling
    if (GamingMasterySystem)
    {
        FString OpponentID = Enemy->GetName();
        TArray<float> GameState = LastGameStateFeatures;
        TMap<FString, float> Predictions = GamingMasterySystem->PredictOpponentAction(OpponentID, GameState);

        // Return highest probability action
        FString MostLikely;
        float MaxProb = 0.0f;
        for (const auto& Pair : Predictions)
        {
            if (Pair.Value > MaxProb)
            {
                MaxProb = Pair.Value;
                MostLikely = Pair.Key;
            }
        }

        return MostLikely;
    }

    // No opponent model: no prediction (never a fabricated one).
    return TEXT("");
}

// ========================================
// AI SYSTEM INTEGRATION
// ========================================

void UUnrealGamingMasteryIntegration::UpdateAIPerception(UAIPerceptionComponent* PerceptionComponent)
{
    if (!PerceptionComponent) return;

    CurrentPerception.PerceivedActors.Empty();
    CurrentPerception.SightLocations.Empty();
    CurrentPerception.HearingLocations.Empty();
    CurrentPerception.PerceptionConfidence.Empty();

    // Get all perceived actors
    TArray<AActor*> PerceivedActors;
    PerceptionComponent->GetCurrentlyPerceivedActors(nullptr, PerceivedActors);

    for (AActor* Actor : PerceivedActors)
    {
        if (Actor)
        {
            CurrentPerception.PerceivedActors.Add(Actor);
            CurrentPerception.SightLocations.Add(Actor->GetActorLocation());

            // Get perception info
            FActorPerceptionBlueprintInfo Info;
            PerceptionComponent->GetActorsPerception(Actor, Info);

            // Calculate confidence based on last perception time
            float Confidence = 1.0f; // Simplified
            CurrentPerception.PerceptionConfidence.Add(Actor, Confidence);
        }
    }

    CurrentPerception.LastUpdateTime = GetWorld()->GetTimeSeconds();

    // Feed perception data to echobeats
    if (EchobeatsIntegration)
    {
        TArray<float> PerceptionFeatures;
        PerceptionFeatures.Add((float)CurrentPerception.PerceivedActors.Num() / 10.0f);
        PerceptionFeatures.Add(CurrentPerception.SightLocations.Num() > 0 ? 1.0f : 0.0f);
        PerceptionFeatures.Add(CurrentPerception.HearingLocations.Num() > 0 ? 1.0f : 0.0f);

        EchobeatsIntegration->ProcessSensoryInput(PerceptionFeatures);
    }
}

FString UUnrealGamingMasteryIntegration::GetAIDecisionRecommendation(UBlackboardComponent* Blackboard)
{
    if (!Blackboard) return TEXT("");

    // Get integrated decision from echobeats
    if (EchobeatsIntegration)
    {
        return EchobeatsIntegration->GetIntegratedDecision();
    }

    // No decision model: no recommendation (never a hard-coded default action).
    return TEXT("");
}

void UUnrealGamingMasteryIntegration::UpdateBlackboardWithMasteryData(UBlackboardComponent* Blackboard)
{
    if (!Blackboard) return;

    // Update blackboard with mastery system data
    if (GamingMasterySystem)
    {
        float DomainMastery = GamingMasterySystem->CalculateDomainMastery(GamingMasterySystem->CurrentGameDomain);
        Blackboard->SetValueAsFloat(TEXT("MasteryLevel"), DomainMastery);

        bool bInFlow = GamingMasterySystem->IsInFlowState();
        Blackboard->SetValueAsBool(TEXT("InFlowState"), bInFlow);
    }

    // Update with cognition bridge data
    if (CognitionBridge)
    {
        float CognitiveLoad = CognitionBridge->GetTotalCognitiveLoad();
        Blackboard->SetValueAsFloat(TEXT("CognitiveLoad"), CognitiveLoad);

        float Integration = CognitionBridge->Calculate4EIntegrationScore();
        Blackboard->SetValueAsFloat(TEXT("4EIntegration"), Integration);
    }
}

void UUnrealGamingMasteryIntegration::ModelOpponentBehavior(AActor* Opponent, const FString& ObservedAction)
{
    if (!Opponent || !GamingMasterySystem) return;

    FString OpponentID = Opponent->GetName();
    GamingMasterySystem->UpdateOpponentModel(OpponentID, ObservedAction, LastGameStateFeatures);
}

FString UUnrealGamingMasteryIntegration::GetCounterStrategyForOpponent(AActor* Opponent) const
{
    if (!Opponent || !GamingMasterySystem) return TEXT("");

    FString OpponentID = Opponent->GetName();
    return GamingMasterySystem->GetCounterStrategy(OpponentID);
}

// ========================================
// ANIMATION SYSTEM INTEGRATION
// ========================================

void UUnrealGamingMasteryIntegration::AnalyzeAnimationState(UAnimInstance* AnimInstance)
{
    if (!AnimInstance) return;

    // Get current animation state
    // (In full implementation, analyze montages, state machines, etc.)
}

bool UUnrealGamingMasteryIntegration::GetCancelWindow(UAnimInstance* AnimInstance, FString& OutCancelType, float& OutWindowStart, float& OutWindowEnd)
{
    OutCancelType.Reset();
    OutWindowStart = 0.0f;
    OutWindowEnd = 0.0f;
    if (!AnimInstance) return false;

    // Cancel windows are authored as notify states on the active montage whose name contains
    // "Cancel" (e.g. "Cancel_Dodge"). Returns the current or next such window, normalized to the
    // montage's play length. No such notify = no known cancel window.
    UAnimMontage* Montage = AnimInstance->GetCurrentActiveMontage();
    if (!Montage) return false;

    const float Length = Montage->GetPlayLength();
    if (Length <= KINDA_SMALL_NUMBER) return false;
    const float Position = AnimInstance->Montage_GetPosition(Montage);

    const FAnimNotifyEvent* Best = nullptr;
    for (const FAnimNotifyEvent& Notify : Montage->Notifies)
    {
        const FString Name = Notify.NotifyName.ToString();
        if (!Name.Contains(TEXT("Cancel")) || Notify.GetEndTriggerTime() < Position)
        {
            continue;
        }
        if (!Best || Notify.GetTriggerTime() < Best->GetTriggerTime())
        {
            Best = &Notify;
        }
    }
    if (!Best) return false;

    FString Type = Best->NotifyName.ToString();
    Type.ReplaceInline(TEXT("Cancel"), TEXT(""));
    Type.TrimStartAndEndInline();
    while (Type.StartsWith(TEXT("_"))) Type.RightChopInline(1);
    OutCancelType = Type.IsEmpty() ? FString(TEXT("any")) : Type;
    OutWindowStart = FMath::Clamp(Best->GetTriggerTime() / Length, 0.0f, 1.0f);
    OutWindowEnd = FMath::Clamp(Best->GetEndTriggerTime() / Length, 0.0f, 1.0f);
    return true;
}

float UUnrealGamingMasteryIntegration::PredictAnimationCompletion(UAnimInstance* AnimInstance) const
{
    // Progress of the active montage in [0, 1]; -1 = unknown (no anim instance or no montage playing).
    if (!AnimInstance) return -1.0f;

    UAnimMontage* Montage = AnimInstance->GetCurrentActiveMontage();
    if (!Montage) return -1.0f;

    const float Length = Montage->GetPlayLength();
    if (Length <= KINDA_SMALL_NUMBER) return -1.0f;

    return FMath::Clamp(AnimInstance->Montage_GetPosition(Montage) / Length, 0.0f, 1.0f);
}

FString UUnrealGamingMasteryIntegration::GetOptimalAnimationTransition(UAnimInstance* AnimInstance, const FString& TargetAction) const
{
    if (!AnimInstance) return TEXT("");

    // Recommend optimal transition based on current state
    return FString::Printf(TEXT("transition_to_%s"), *TargetAction);
}

// ========================================
// VISUAL FEEDBACK SYSTEM
// ========================================

void UUnrealGamingMasteryIntegration::ShowVisualFeedback(EVisualFeedbackType FeedbackType, FVector Location, float Duration)
{
    if (!VisualFeedbackConfig.bEnabled) return;

    float ActualDuration = Duration > 0.0f ? Duration : VisualFeedbackConfig.FeedbackDuration;

    // Draw debug visualization (in full implementation, use proper UI/particles)
    switch (FeedbackType)
    {
        case EVisualFeedbackType::TargetHighlight:
            DrawDebugSphere(GetWorld(), Location, 50.0f, 16, FColor::Green, false, ActualDuration);
            break;
        case EVisualFeedbackType::MovementPath:
            // Draw path
            break;
        case EVisualFeedbackType::TimingWindow:
            DrawDebugSphere(GetWorld(), Location, 30.0f, 8, FColor::Yellow, false, ActualDuration);
            break;
        case EVisualFeedbackType::ThreatIndicator:
            DrawDebugSphere(GetWorld(), Location, 100.0f, 16, FColor::Red, false, ActualDuration);
            break;
        case EVisualFeedbackType::OpportunityMarker:
            DrawDebugSphere(GetWorld(), Location, 50.0f, 16, FColor::Blue, false, ActualDuration);
            break;
        case EVisualFeedbackType::SkillFeedback:
            // Show skill feedback UI
            break;
    }
}

void UUnrealGamingMasteryIntegration::HideVisualFeedback(EVisualFeedbackType FeedbackType)
{
    // Clear specific feedback type
    // (In full implementation, manage feedback actors/widgets)
}

void UUnrealGamingMasteryIntegration::UpdateHUDWithMasteryData()
{
    // Update HUD elements with mastery data
    // (In full implementation, interface with UMG widgets)
}

void UUnrealGamingMasteryIntegration::ShowSkillExecutionFeedback(const FString& SkillName, float ExecutionQuality)
{
    FVector Location = GetOwner()->GetActorLocation() + FVector(0, 0, 100);

    if (ExecutionQuality > 0.8f)
    {
        ShowVisualFeedback(EVisualFeedbackType::SkillFeedback, Location, 0.5f);
        // Play positive sound
    }
    else if (ExecutionQuality < 0.4f)
    {
        // Show improvement needed feedback
    }
}

void UUnrealGamingMasteryIntegration::HighlightOptimalTarget(AActor* Target)
{
    if (!Target) return;

    ShowVisualFeedback(EVisualFeedbackType::TargetHighlight, Target->GetActorLocation(), 2.0f);
}

void UUnrealGamingMasteryIntegration::ShowTimingWindow(float WindowStart, float WindowEnd, float CurrentTime)
{
    // Show timing window UI
    // (In full implementation, use proper UI widget)
}

// ========================================
// AUDIO SYSTEM INTEGRATION
// ========================================

void UUnrealGamingMasteryIntegration::ProcessAudioCue(USoundBase* Sound, FVector Location, float Volume)
{
    // Process audio cue for perception
    CurrentPerception.HearingLocations.Add(Location);

    // Feed to echobeats
    if (EchobeatsIntegration)
    {
        TArray<float> AudioFeatures;
        AudioFeatures.Add(Volume);
        AudioFeatures.Add(FVector::Dist(GetOwner()->GetActorLocation(), Location) / 1000.0f);

        // Process as part of sensory input
    }
}

float UUnrealGamingMasteryIntegration::GetAudioThreatAssessment() const
{
    // Assess threat level from audio cues
    float ThreatLevel = 0.0f;

    for (const FVector& AudioLoc : CurrentPerception.HearingLocations)
    {
        float Distance = FVector::Dist(GetOwner()->GetActorLocation(), AudioLoc);
        ThreatLevel += 1.0f - FMath::Min(1.0f, Distance / 1000.0f);
    }

    return FMath::Clamp(ThreatLevel / FMath::Max(1, CurrentPerception.HearingLocations.Num()), 0.0f, 1.0f);
}

void UUnrealGamingMasteryIntegration::PlayFeedbackSound(const FString& FeedbackType, float Quality)
{
    // Play appropriate feedback sound
    // (In full implementation, use sound cues)
}

// ========================================
// REPLAY SYSTEM INTEGRATION
// ========================================

void UUnrealGamingMasteryIntegration::StartReplayRecording(const FString& SessionName)
{
    bReplayRecordingActive = true;
    CurrentReplaySession = SessionName;

    UE_LOG(LogTemp, Log, TEXT("Started replay recording: %s"), *SessionName);
}

void UUnrealGamingMasteryIntegration::StopReplayRecording()
{
    bReplayRecordingActive = false;

    UE_LOG(LogTemp, Log, TEXT("Stopped replay recording: %s"), *CurrentReplaySession);
}

FReplayAnalysisResult UUnrealGamingMasteryIntegration::AnalyzeReplaySession(const FString& SessionName)
{
    FReplayAnalysisResult Result;

    if (GamingMasterySystem)
    {
        Result = GamingMasterySystem->AnalyzeReplay(SessionName);
    }

    return Result;
}

TArray<float> UUnrealGamingMasteryIntegration::GetKeyMomentsFromReplay(const FString& SessionName) const
{
    TArray<float> KeyMoments;

    // Identify key moments (kills, deaths, objectives, etc.)
    // (In full implementation, analyze replay data)

    return KeyMoments;
}

TMap<FString, float> UUnrealGamingMasteryIntegration::ComparePerformanceToBaseline(const FString& SessionName, const FString& BaselineName) const
{
    TMap<FString, float> Comparison;

    // Compare metrics between sessions
    // (In full implementation, load and compare replay data)

    return Comparison;
}

// ========================================
// GAME STATE EXTRACTION
// ========================================

TArray<float> UUnrealGamingMasteryIntegration::ExtractGameStateFeatures()
{
    TArray<float> Features;

    // Extract features from current game state
    AActor* Owner = GetOwner();
    if (!Owner) return Features;

    // Position features
    FVector Position = Owner->GetActorLocation();
    Features.Add(Position.X / 10000.0f);
    Features.Add(Position.Y / 10000.0f);
    Features.Add(Position.Z / 1000.0f);

    // Velocity features
    FVector Velocity = FVector::ZeroVector;
    if (ACharacter* Character = Cast<ACharacter>(Owner))
    {
        Velocity = Character->GetVelocity();
    }
    Features.Add(Velocity.Size() / 1000.0f);

    // Perception features
    Features.Add((float)CurrentPerception.PerceivedActors.Num() / 10.0f);

    // Combat features
    Features.Add(CombatAnalysis.Accuracy);
    Features.Add(CombatAnalysis.DamageEfficiency);

    // Movement features
    Features.Add(MovementAnalysis.Efficiency);
    Features.Add(MovementAnalysis.PathOptimality);

    // Cognitive features
    if (CognitionBridge)
    {
        Features.Add(CognitionBridge->GetTotalCognitiveLoad());
        Features.Add(CognitionBridge->Calculate4EIntegrationScore());
    }
    else
    {
        Features.Add(0.5f);
        Features.Add(0.5f);
    }

    LastGameStateFeatures = Features;
    return Features;
}

TArray<float> UUnrealGamingMasteryIntegration::GetNormalizedGameState() const
{
    return LastGameStateFeatures;
}

void UUnrealGamingMasteryIntegration::UpdateGameStateForML()
{
    TArray<float> Features = ExtractGameStateFeatures();

    // Feed to echobeats
    if (EchobeatsIntegration)
    {
        EchobeatsIntegration->ProcessGameFrame(Features, GetWorld()->GetDeltaSeconds());
    }
}

// ========================================
// TRAINING MODE
// ========================================

void UUnrealGamingMasteryIntegration::EnterTrainingMode(const FString& SkillToTrain)
{
    bTrainingModeActive = true;
    CurrentTrainingSkill = SkillToTrain;
    InputMode = EInputProcessingMode::Training;

    // Start training session in gaming mastery system
    if (GamingMasterySystem)
    {
        TArray<FString> Skills;
        Skills.Add(SkillToTrain);
        GamingMasterySystem->StartTrainingSession(Skills, TEXT("Focused"), 30.0f);
    }

    UE_LOG(LogTemp, Log, TEXT("Entered training mode for skill: %s"), *SkillToTrain);
}

void UUnrealGamingMasteryIntegration::ExitTrainingMode()
{
    bTrainingModeActive = false;
    InputMode = EInputProcessingMode::MLAssisted;

    // End training session
    if (GamingMasterySystem)
    {
        TMap<FString, float> Metrics;
        Metrics.Add(TEXT("accuracy"), CombatAnalysis.Accuracy);
        Metrics.Add(TEXT("efficiency"), MovementAnalysis.Efficiency);
        GamingMasterySystem->EndTrainingSession(Metrics);
    }

    UE_LOG(LogTemp, Log, TEXT("Exited training mode"));
}

void UUnrealGamingMasteryIntegration::RequestTrainingGuidance()
{
    const FString Topic = TEXT("training_guidance");
    if (!bTrainingModeActive)
    {
        OnCoachingFailed.Broadcast(Topic, TEXT("training mode is not active"));
        return;
    }
    if (!GamingMasterySystem)
    {
        OnCoachingFailed.Broadcast(Topic, TEXT("GamingMasterySystem not available; no skill data"));
        return;
    }

    const FGamingSkillComponent Skill = GamingMasterySystem->GetSkill(CurrentTrainingSkill);
    const FString Metrics = FString::Printf(
        TEXT("skill in training: %s (%s, category %s)\nproficiency: %.2f\nacquisition phase: %s\npractice hours: %.1f\n")
        TEXT("deliberate practice sessions: %d\nrecent success rate: %.2f\ntraining difficulty: %.2f\n")
        TEXT("combat accuracy: %.2f\nmovement efficiency: %.2f\n"),
        *CurrentTrainingSkill, *Skill.SkillName, *Skill.Category, Skill.ProficiencyLevel,
        *UEnum::GetValueAsString(Skill.AcquisitionPhase), Skill.PracticeHours, Skill.DeliberatePracticeSessions,
        Skill.RecentSuccessRate, TrainingDifficulty, CombatAnalysis.Accuracy, MovementAnalysis.Efficiency);

    // Proficiency router picks the stage; the model writes the guidance.
    TMap<FString, FString> Slots;
    Slots.Add(TEXT("skill"), CurrentTrainingSkill);
    Slots.Add(TEXT("focus"), Skill.ProficiencyLevel < 0.3f
        ? FString(TEXT("beginner - careful, unhurried basic execution"))
        : Skill.ProficiencyLevel < 0.6f
            ? FString(TEXT("progressing - timing and consistency"))
            : FString(TEXT("advanced - applying the skill in varied situations")));
    EchoCoaching::RequestFromTemplate(this, Topic, Slots, Metrics, &OnCoachingReady, &OnCoachingFailed);
}

void UUnrealGamingMasteryIntegration::SetTrainingDifficulty(float Difficulty)
{
    TrainingDifficulty = FMath::Clamp(Difficulty, 0.0f, 1.0f);
}

float UUnrealGamingMasteryIntegration::GetTrainingProgress() const
{
    if (!bTrainingModeActive || !GamingMasterySystem) return 0.0f;

    FGamingSkillComponent Skill = GamingMasterySystem->GetSkill(CurrentTrainingSkill);
    return Skill.ProficiencyLevel;
}

// ========================================
// INTERNAL METHODS
// ========================================

void UUnrealGamingMasteryIntegration::UpdatePerceptionData()
{
    // Update perception from AI perception component if available
    UAIPerceptionComponent* PerceptionComp = GetOwner()->FindComponentByClass<UAIPerceptionComponent>();
    if (PerceptionComp)
    {
        UpdateAIPerception(PerceptionComp);
    }
}

void UUnrealGamingMasteryIntegration::UpdateMovementAnalysis(float DeltaTime)
{
    AActor* Owner = GetOwner();
    if (!Owner) return;

    FVector CurrentPosition = Owner->GetActorLocation();
    FVector CurrentVelocity = FVector::ZeroVector;

    if (ACharacter* Character = Cast<ACharacter>(Owner))
    {
        CurrentVelocity = Character->GetVelocity();
    }

    // Use forward position as target for now
    FVector TargetPosition = CurrentPosition + Owner->GetActorForwardVector() * 1000.0f;

    AnalyzeMovementFrame(CurrentPosition, CurrentVelocity, TargetPosition);
}

void UUnrealGamingMasteryIntegration::UpdateCombatAnalysis(float DeltaTime)
{
    // Decay combat metrics over time
    CombatAnalysis.DamageEfficiency *= 0.999f;

    // Update reaction time tracking
    // (In full implementation, track actual reaction times)
}

void UUnrealGamingMasteryIntegration::ProcessMLRecommendations()
{
    if (InputMode != EInputProcessingMode::MLAssisted) return;

    // Get recommendations from echobeats
    if (EchobeatsIntegration)
    {
        FString RecommendedAction = EchobeatsIntegration->GetRecommendedAction(LastGameStateFeatures);
        if (!RecommendedAction.IsEmpty())
        {
            // Calculate confidence
            float Confidence = EchobeatsIntegration->CalculateCrossStreamCoherence();
            OnOptimalActionIdentified.Broadcast(RecommendedAction, Confidence);
        }

        // Check for opportunities
        for (const FAffordanceDetection& Affordance : EchobeatsIntegration->DetectedAffordances)
        {
            if (Affordance.Confidence > 0.7f)
            {
                OnOpportunityDetected.Broadcast(Affordance.AffordanceType, Affordance.EstimatedValue, Affordance.TimeWindow);
            }
        }
    }
}

void UUnrealGamingMasteryIntegration::RecordFrameForReplay()
{
    // Record current frame data for replay
    // (In full implementation, serialize game state)
}

float UUnrealGamingMasteryIntegration::CalculateTargetPriorityScore(AActor* Target) const
{
    if (!Target) return 0.0f;

    float Score = 0.5f;

    // Distance factor (closer = higher priority)
    float Distance = FVector::Dist(GetOwner()->GetActorLocation(), Target->GetActorLocation());
    Score += (1.0f - FMath::Min(1.0f, Distance / 2000.0f)) * 0.3f;

    // Threat factor (from opponent model)
    if (GamingMasterySystem)
    {
        FString OpponentID = Target->GetName();
        TArray<FString> Weaknesses = GamingMasterySystem->GetOpponentWeaknesses(OpponentID);
        Score += Weaknesses.Num() * 0.1f;
    }

    // Health factor (lower health = higher priority)
    // (In full implementation, get actual health)

    return FMath::Clamp(Score, 0.0f, 1.0f);
}

float UUnrealGamingMasteryIntegration::CalculateMovementEfficiency(const TArray<FVector>& Path) const
{
    if (Path.Num() < 2) return 1.0f;

    float DirectDistance = FVector::Dist(Path[0], Path.Last());
    float ActualDistance = 0.0f;

    for (int32 i = 1; i < Path.Num(); i++)
    {
        ActualDistance += FVector::Dist(Path[i - 1], Path[i]);
    }

    return DirectDistance > 0.0f ? FMath::Min(1.0f, DirectDistance / FMath::Max(ActualDistance, 1.0f)) : 1.0f;
}

float UUnrealGamingMasteryIntegration::AnalyzeInputTiming(const FString& ActionName, float Timestamp) const
{
    // Analyze timing quality based on optimal windows
    // (In full implementation, compare to learned optimal timings)

    // For now, return based on consistency
    float Quality = 0.7f; // Base quality

    // Check for timing patterns in history
    int32 SimilarInputs = 0;
    float TimingVariance = 0.0f;

    for (const auto& Input : InputHistory)
    {
        if (Input.Key == ActionName)
        {
            SimilarInputs++;
            // Calculate variance from expected timing
        }
    }

    if (SimilarInputs > 5)
    {
        Quality += 0.2f; // Bonus for consistent practice
    }

    return FMath::Clamp(Quality, 0.0f, 1.0f);
}
