#include "AvatarCognitionBridge.h"

UAvatarCognitionBridge::UAvatarCognitionBridge()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;

    ExpressionSystem = nullptr;
    EchobeatsInterface = nullptr;
    Reservoir = nullptr;
    ReservoirCognition = nullptr;
    EmbodiedCognition = nullptr;

    LastEchobeatStep = 0;
    EmbodimentCoherence = 1.0f;
}

void UAvatarCognitionBridge::BeginPlay()
{
    Super::BeginPlay();

    FindComponentReferences();
    BindCognitiveEvents();

    UE_LOG(LogTemp, Log, TEXT("AvatarCognitionBridge initialized"));
}

void UAvatarCognitionBridge::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bEnableCognitiveCoupling)
        return;

    // Update from reservoir state
    if (bEnableReservoirMapping && ReservoirCognition)
    {
        UpdateFromReservoirState();
    }

    // Smooth transition to target embodiment
    SmoothEmbodimentTransition(DeltaTime);

    // Apply embodiment to expression system
    ApplyEmbodimentToExpression();

    // Update coherence metric
    UpdateEmbodimentCoherence();
}

void UAvatarCognitionBridge::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (!Owner)
        return;

    ExpressionSystem = Owner->FindComponentByClass<UDeepTreeEchoExpressionSystem>();
    EchobeatsInterface = Owner->FindComponentByClass<UEchobeatsCognitiveInterface>();
    Reservoir = Owner->FindComponentByClass<UDeepTreeEchoReservoir>();
    ReservoirCognition = Owner->FindComponentByClass<UReservoirCognitiveIntegration>();
    EmbodiedCognition = Owner->FindComponentByClass<UEmbodiedCognitionComponent>();

    UE_LOG(LogTemp, Log, TEXT("AvatarCognitionBridge found components: Expression=%s, Echobeats=%s, Reservoir=%s"),
        ExpressionSystem ? TEXT("Yes") : TEXT("No"),
        EchobeatsInterface ? TEXT("Yes") : TEXT("No"),
        Reservoir ? TEXT("Yes") : TEXT("No"));
}

void UAvatarCognitionBridge::BindCognitiveEvents()
{
    if (EchobeatsInterface)
    {
        EchobeatsInterface->OnCognitiveStepChanged.AddUniqueDynamic(this, &UAvatarCognitionBridge::OnEchobeatStepChanged);
        EchobeatsInterface->OnCognitiveCycleComplete.AddUniqueDynamic(this, &UAvatarCognitionBridge::OnCognitiveCycleComplete);
    }
}

void UAvatarCognitionBridge::InitializeBridge()
{
    FindComponentReferences();
    BindCognitiveEvents();
}

// ========================================
// COGNITIVE COUPLING
// ========================================

void UAvatarCognitionBridge::UpdateFromCognitiveState(ECognitiveState NewState)
{
    // Records the state; the expression system may ask the model (throttled) how the character
    // reacts. The face changes only when the model's validated reaction arrives on the bus.
    if (ExpressionSystem)
    {
        ExpressionSystem->SetCognitiveState(NewState);
    }
}

void UAvatarCognitionBridge::ProcessRelevanceRealization(float Salience, bool bIsInsight)
{
    if (bIsInsight)
    {
        OnInsightTriggered(Salience);
    }
    else if (Salience > 0.7f)
    {
        // High salience brightens the emissive tech elements
        TargetEmbodiment.EmissiveIntensity = 1.0f + Salience * 0.5f;

        if (ExpressionSystem)
        {
            ExpressionSystem->PulseEmissives(0.3f, 1.5f + Salience);
        }
    }
}

void UAvatarCognitionBridge::UpdateAttentionFocus(const FVector& WorldPosition, float Salience)
{
    TargetEmbodiment.AttentionFocus = WorldPosition;

    // Higher salience = more focused gaze
    if (Salience > 0.5f)
    {
        TargetEmbodiment.SaccadeFrequency = 0.5f;  // Reduce saccades when focused
    }
    else
    {
        TargetEmbodiment.SaccadeFrequency = 1.0f + (1.0f - Salience);  // More saccades when scanning
    }
}

// ========================================
// RESERVOIR COUPLING
// ========================================

void UAvatarCognitionBridge::UpdateFromReservoirState()
{
    if (!ReservoirCognition)
        return;

    FReservoirCognitiveState CogState = ReservoirCognition->GetCognitiveState();

    // Map salience to expression
    MapSalienceToExpression(CogState.SalienceMap);

    // Map affordances to posture
    MapAffordanceToPosture(CogState.AffordanceMap);

    // Prediction confidence affects breathing
    TargetEmbodiment.BreathingRate = 0.8f + CogState.PredictionConfidence * 0.4f;
}

void UAvatarCognitionBridge::MapSalienceToExpression(const TMap<FString, float>& SalienceMap)
{
    // Find highest salience
    float MaxSalience = 0.0f;
    FString MostSalient;

    for (const auto& Pair : SalienceMap)
    {
        if (Pair.Value > MaxSalience)
        {
            MaxSalience = Pair.Value;
            MostSalient = Pair.Key;
        }
    }

    // High salience brightens the emissive tech elements (the face is left to the model)
    if (MaxSalience > 0.8f)
    {
        TargetEmbodiment.EmissiveIntensity = 1.2f + MaxSalience * 0.3f;
    }
    else if (MaxSalience > 0.5f)
    {
        TargetEmbodiment.EmissiveIntensity = 1.0f + MaxSalience * 0.2f;
    }
}

void UAvatarCognitionBridge::MapAffordanceToPosture(const TMap<FString, float>& AffordanceMap)
{
    // Calculate average affordance confidence
    float TotalAffordance = 0.0f;
    int32 Count = 0;

    for (const auto& Pair : AffordanceMap)
    {
        TotalAffordance += Pair.Value;
        Count++;
    }

    float AvgAffordance = Count > 0 ? TotalAffordance / Count : 0.5f;

    // More affordances = more open posture
    TargetEmbodiment.PostureOpenness = 0.3f + AvgAffordance * 0.7f;
}

// ========================================
// ECHOBEATS COUPLING
// ========================================

void UAvatarCognitionBridge::OnEchobeatStepChanged(int32 NewStep, const FString& PhaseName)
{
    if (!bEnableEchobeatSync)
        return;

    // The 12-step loop is cognition cadence; it does not set expressions, micro-expressions
    // or emissive pulses on the avatar.
    LastEchobeatStep = NewStep;

    UE_LOG(LogTemp, Verbose, TEXT("Echobeat step %d (%s)"), NewStep, *PhaseName);
}

void UAvatarCognitionBridge::OnCognitiveCycleComplete(int32 CycleCount)
{
    UE_LOG(LogTemp, Verbose, TEXT("Cognitive cycle %d complete"), CycleCount);
}

void UAvatarCognitionBridge::OnInsightTriggered(float Intensity)
{
    // Insight is visualised on the emissive tech elements only; facial expression stays
    // whatever the character model chose.
    if (ExpressionSystem)
    {
        ExpressionSystem->PulseEmissives(0.5f, 2.0f + Intensity);
    }

    TargetEmbodiment.EmissiveIntensity = 1.5f + Intensity * 0.5f;

    UE_LOG(LogTemp, Log, TEXT("Insight signalled with intensity %.2f"), Intensity);
}

// ========================================
// 4E COGNITION
// ========================================

void UAvatarCognitionBridge::UpdateEmbodiedGrounding()
{
    if (!EmbodiedCognition)
        return;

    // Get body schema influence
    // This would integrate with the EmbodiedCognitionComponent
    // to ground cognitive states in physical avatar state
}

void UAvatarCognitionBridge::ProcessEnvironmentalEmbedding(const FString& ContextDescription)
{
    // Environmental context modulates expression
    // E.g., dark environment -> more subdued expressions
    // Social context -> more expressive
}

void UAvatarCognitionBridge::ExecuteEnactedLoop(const FString& ActionType)
{
    // Expression-action coupling
    // Actions trigger corresponding expressions
    // Expressions prepare for actions
}

void UAvatarCognitionBridge::ExtendCognition(const FString& ExtensionType)
{
    // Avatar as cognitive extension
    // User's cognitive processes extend through avatar
}

// ========================================
// INTERNAL METHODS
// ========================================

void UAvatarCognitionBridge::ApplyEmbodimentToExpression()
{
    if (!ExpressionSystem)
        return;

    // Emissives follow cognitive signals
    ExpressionSystem->SetEmissiveIntensity(CurrentEmbodiment.EmissiveIntensity);

    // Expression is owned by the model's reactions; mirror it for Blueprint queries
    CurrentEmbodiment.Expression = ExpressionSystem->GetCurrentExpressionState();
    CurrentEmbodiment.ExpressionIntensity = ExpressionSystem->GetReactionIntensity();
    TargetEmbodiment.Expression = CurrentEmbodiment.Expression;
    TargetEmbodiment.ExpressionIntensity = CurrentEmbodiment.ExpressionIntensity;
}

void UAvatarCognitionBridge::SmoothEmbodimentTransition(float DeltaTime)
{
    float Alpha = 1.0f - FMath::Pow(GlobalSmoothing, DeltaTime * 10.0f);

    // Smooth numeric values
    CurrentEmbodiment.EmissiveIntensity = FMath::Lerp(CurrentEmbodiment.EmissiveIntensity, TargetEmbodiment.EmissiveIntensity, Alpha);
    CurrentEmbodiment.BreathingRate = FMath::Lerp(CurrentEmbodiment.BreathingRate, TargetEmbodiment.BreathingRate, Alpha);
    CurrentEmbodiment.BlinkRate = FMath::Lerp(CurrentEmbodiment.BlinkRate, TargetEmbodiment.BlinkRate, Alpha);
    CurrentEmbodiment.SaccadeFrequency = FMath::Lerp(CurrentEmbodiment.SaccadeFrequency, TargetEmbodiment.SaccadeFrequency, Alpha);
    CurrentEmbodiment.HeadMovementAmplitude = FMath::Lerp(CurrentEmbodiment.HeadMovementAmplitude, TargetEmbodiment.HeadMovementAmplitude, Alpha);
    CurrentEmbodiment.PostureOpenness = FMath::Lerp(CurrentEmbodiment.PostureOpenness, TargetEmbodiment.PostureOpenness, Alpha);
    CurrentEmbodiment.AttentionFocus = FMath::Lerp(CurrentEmbodiment.AttentionFocus, TargetEmbodiment.AttentionFocus, Alpha);
}

void UAvatarCognitionBridge::UpdateEmbodimentCoherence()
{
    // Coherence is high when all systems are aligned
    float ExpressionMatch = (CurrentEmbodiment.Expression == TargetEmbodiment.Expression) ? 1.0f : 0.5f;
    float IntensityMatch = 1.0f - FMath::Abs(CurrentEmbodiment.ExpressionIntensity - TargetEmbodiment.ExpressionIntensity);
    float EmissiveMatch = 1.0f - FMath::Abs(CurrentEmbodiment.EmissiveIntensity - TargetEmbodiment.EmissiveIntensity) / 2.0f;

    EmbodimentCoherence = (ExpressionMatch + IntensityMatch + EmissiveMatch) / 3.0f;
}

float UAvatarCognitionBridge::GetEmbodimentCoherence() const
{
    return EmbodimentCoherence;
}
