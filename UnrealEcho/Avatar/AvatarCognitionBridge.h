#pragma once

/**
 * Avatar Cognition Bridge
 * 
 * Bridges the Deep Tree Echo cognitive framework with the avatar expression system.
 * Implements 4E embodied cognition principles by coupling cognitive states with
 * physical avatar manifestations.
 * 
 * Architecture:
 * - Connects EchobeatsCognitiveInterface with DeepTreeEchoExpressionSystem
 * - Maps reservoir states to embodiment parameters (emissives, breathing, posture, gaze)
 * - Implements relevance realization through emissive salience
 * - Provides embodied grounding for cognitive processes
 *
 * The bridge never chooses the avatar's facial expression: the displayed expression is
 * the one the character model chose (applied by UDeepTreeEchoExpressionSystem from
 * FEchoReactionBus). FEmbodimentState::Expression mirrors it read-only.
 * 
 * 4E Cognition Integration:
 * - Embodied: Avatar body schema influences cognitive processing
 * - Embedded: Environmental context modulates expression
 * - Enacted: Expression-action coupling creates sensorimotor loops
 * - Extended: Avatar serves as cognitive extension for user
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepTreeEchoExpressionSystem.h"
#include "EchobeatsCognitiveInterface.h"
#include "../../DeepTreeEcho/Reservoir/DeepTreeEchoReservoir.h"
#include "../../DeepTreeEcho/Reservoir/ReservoirCognitiveIntegration.h"
#include "../../DeepTreeEcho/4ECognition/EmbodiedCognitionComponent.h"
#include "AvatarCognitionBridge.generated.h"

/**
 * Embodiment State - Physical manifestation of cognitive state
 */
USTRUCT(BlueprintType)
struct FEmbodimentState
{
    GENERATED_BODY()

    /** Displayed expression (mirrors the last model-chosen expression; set by the expression system) */
    UPROPERTY(BlueprintReadWrite)
    EExpressionState Expression = EExpressionState::Neutral;

    /** Displayed expression intensity (0-1, the model's intensity) */
    UPROPERTY(BlueprintReadWrite)
    float ExpressionIntensity = 0.0f;

    /** Emissive glow intensity */
    UPROPERTY(BlueprintReadWrite)
    float EmissiveIntensity = 1.0f;

    /** Breathing rate multiplier */
    UPROPERTY(BlueprintReadWrite)
    float BreathingRate = 1.0f;

    /** Blink rate multiplier */
    UPROPERTY(BlueprintReadWrite)
    float BlinkRate = 1.0f;

    /** Saccade frequency multiplier */
    UPROPERTY(BlueprintReadWrite)
    float SaccadeFrequency = 1.0f;

    /** Head movement amplitude */
    UPROPERTY(BlueprintReadWrite)
    float HeadMovementAmplitude = 1.0f;

    /** Attention focus point (world space) */
    UPROPERTY(BlueprintReadWrite)
    FVector AttentionFocus = FVector::ZeroVector;

    /** Body posture openness (0=closed, 1=open) */
    UPROPERTY(BlueprintReadWrite)
    float PostureOpenness = 0.5f;
};

/**
 * Reservoir-Expression Mapping - Maps reservoir states to expression parameters
 */
USTRUCT(BlueprintType)
struct FReservoirExpressionMapping
{
    GENERATED_BODY()

    /** Reservoir function type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EReservoirFunction ReservoirFunction = EReservoirFunction::Salience;

    /** Expression parameter to modulate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TargetParameter;

    /** Mapping curve (reservoir output -> parameter value) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UCurveFloat* MappingCurve = nullptr;

    /** Sensitivity multiplier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Sensitivity = 1.0f;

    /** Smoothing factor (0=instant, 1=very smooth) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Smoothing = 0.5f;
};

/**
 * Avatar Cognition Bridge Component
 * 
 * Central integration point between cognitive systems and avatar expression.
 * Implements embodied cognition by grounding abstract cognitive states in
 * physical avatar manifestations.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class UNREALECHO_API UAvatarCognitionBridge : public UActorComponent
{
    GENERATED_BODY()

public:
    UAvatarCognitionBridge();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable cognitive-expression coupling */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Config")
    bool bEnableCognitiveCoupling = true;

    /** Enable reservoir-expression mapping */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Config")
    bool bEnableReservoirMapping = true;

    /** Enable echobeats synchronization */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Config")
    bool bEnableEchobeatSync = true;

    /** Reservoir-expression mappings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Config")
    TArray<FReservoirExpressionMapping> ReservoirExpressionMappings;

    /** Global expression smoothing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float GlobalSmoothing = 0.3f;

    // ========================================
    // COMPONENT REFERENCES
    // ========================================

    /** Expression system reference */
    UPROPERTY(BlueprintReadOnly, Category = "Bridge|Components")
    UDeepTreeEchoExpressionSystem* ExpressionSystem;

    /** Echobeats cognitive interface reference */
    UPROPERTY(BlueprintReadOnly, Category = "Bridge|Components")
    UEchobeatsCognitiveInterface* EchobeatsInterface;

    /** Base reservoir reference */
    UPROPERTY(BlueprintReadOnly, Category = "Bridge|Components")
    UDeepTreeEchoReservoir* Reservoir;

    /** Reservoir cognitive integration reference */
    UPROPERTY(BlueprintReadOnly, Category = "Bridge|Components")
    UReservoirCognitiveIntegration* ReservoirCognition;

    /** 4E Embodied cognition component */
    UPROPERTY(BlueprintReadOnly, Category = "Bridge|Components")
    UEmbodiedCognitionComponent* EmbodiedCognition;

    // ========================================
    // STATE
    // ========================================

    /** Current embodiment state */
    UPROPERTY(BlueprintReadOnly, Category = "Bridge|State")
    FEmbodimentState CurrentEmbodiment;

    /** Target embodiment state (for smoothing) */
    UPROPERTY(BlueprintReadOnly, Category = "Bridge|State")
    FEmbodimentState TargetEmbodiment;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /** Initialize the bridge and find component references */
    UFUNCTION(BlueprintCallable, Category = "Bridge")
    void InitializeBridge();

    // ========================================
    // PUBLIC API - COGNITIVE COUPLING
    // ========================================

    /** Record cognitive state (does not change the displayed expression) */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Cognitive")
    void UpdateFromCognitiveState(ECognitiveState NewState);

    /** Process relevance realization event */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Cognitive")
    void ProcessRelevanceRealization(float Salience, bool bIsInsight);

    /** Update attention focus */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Cognitive")
    void UpdateAttentionFocus(const FVector& WorldPosition, float Salience);

    // ========================================
    // PUBLIC API - RESERVOIR COUPLING
    // ========================================

    /** Update embodiment from reservoir state */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Reservoir")
    void UpdateFromReservoirState();

    /** Map salience landscape to emissive intensity (does not change the displayed expression) */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Reservoir")
    void MapSalienceToExpression(const TMap<FString, float>& SalienceMap);

    /** Map affordance detection to posture */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Reservoir")
    void MapAffordanceToPosture(const TMap<FString, float>& AffordanceMap);

    // ========================================
    // PUBLIC API - ECHOBEATS COUPLING
    // ========================================

    /** Handle echobeat step change (cadence only; no expression change) */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Echobeats")
    void OnEchobeatStepChanged(int32 NewStep, const FString& PhaseName);

    /** Handle cognitive cycle complete */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Echobeats")
    void OnCognitiveCycleComplete(int32 CycleCount);

    /** Handle insight event (emissive pulse only; no expression change) */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Echobeats")
    void OnInsightTriggered(float Intensity);

    // ========================================
    // PUBLIC API - 4E COGNITION
    // ========================================

    /** Update embodied grounding from body schema */
    UFUNCTION(BlueprintCallable, Category = "Bridge|4E")
    void UpdateEmbodiedGrounding();

    /** Process environmental embedding */
    UFUNCTION(BlueprintCallable, Category = "Bridge|4E")
    void ProcessEnvironmentalEmbedding(const FString& ContextDescription);

    /** Execute enacted expression-action loop */
    UFUNCTION(BlueprintCallable, Category = "Bridge|4E")
    void ExecuteEnactedLoop(const FString& ActionType);

    /** Extend cognition through avatar */
    UFUNCTION(BlueprintCallable, Category = "Bridge|4E")
    void ExtendCognition(const FString& ExtensionType);

    // ========================================
    // PUBLIC API - STATE ACCESS
    // ========================================

    /** Get current embodiment state */
    UFUNCTION(BlueprintCallable, Category = "Bridge|State")
    FEmbodimentState GetCurrentEmbodiment() const { return CurrentEmbodiment; }

    /** Get embodiment coherence (how well-integrated the state is) */
    UFUNCTION(BlueprintCallable, Category = "Bridge|State")
    float GetEmbodimentCoherence() const;

protected:
    virtual void BeginPlay() override;

private:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Smoothed reservoir values */
    TMap<FString, float> SmoothedReservoirValues;

    /** Last echobeat step */
    int32 LastEchobeatStep = 0;

    /** Embodiment coherence */
    float EmbodimentCoherence = 1.0f;

    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Find and cache component references */
    void FindComponentReferences();

    /** Bind to cognitive events */
    void BindCognitiveEvents();

    /** Apply embodiment state (emissives) to the expression system and mirror its displayed expression */
    void ApplyEmbodimentToExpression();

    /** Smooth transition between embodiment states */
    void SmoothEmbodimentTransition(float DeltaTime);

    /** Compute embodiment coherence */
    void UpdateEmbodimentCoherence();
};
