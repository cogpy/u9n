/**
 * @file UnrealAvatarEmbodiment.h
 * @brief Header for 4E Embodied Cognition Avatar System in Unreal Engine
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UnrealAvatarEmbodiment.generated.h"

// Forward declarations
class USkeletalMeshComponent;
class UCognitiveCycleManager;
class UEmbodied4ECognition;
class UEchobeatsStreamEngine;
struct FEchoCharacterReaction;

/**
 * Body Part State - Physical state of a body part in the body schema
 */
USTRUCT(BlueprintType)
struct FBodyPartState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FString PartName;

    UPROPERTY(BlueprintReadWrite)
    FVector WorldPosition = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite)
    float Awareness = 1.0f;

    UPROPERTY(BlueprintReadWrite)
    float Activation = 0.5f;
};

/**
 * Embodied State - Physical body state for 4E cognition
 */
USTRUCT(BlueprintType)
struct FAvatarEmbodiedState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    TMap<FString, FBodyPartState> BodySchema;

    UPROPERTY(BlueprintReadWrite)
    float ArousalLevel = 0.5f;

    UPROPERTY(BlueprintReadWrite)
    float Valence = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float EnergyLevel = 0.7f;

    UPROPERTY(BlueprintReadWrite)
    float ProprioceptiveAwareness = 0.5f;
};

/**
 * Embedded State - Environmental coupling state
 */
USTRUCT(BlueprintType)
struct FAvatarEmbeddedState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    float CouplingStrength = 0.5f;

    UPROPERTY(BlueprintReadWrite)
    float EnvironmentAwareness = 0.5f;

    UPROPERTY(BlueprintReadWrite)
    float SocialPresence = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    FVector EnvironmentCenter = FVector::ZeroVector;
};

/**
 * Enacted State - Sensorimotor contingency state
 */
USTRUCT(BlueprintType)
struct FAvatarEnactedState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    float CouplingStrength = 0.5f;

    UPROPERTY(BlueprintReadWrite)
    FString CurrentAction = TEXT("idle");

    UPROPERTY(BlueprintReadWrite)
    float ActionProgress = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    FVector ActionTarget = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite)
    TArray<FString> LearnedContingencies;
};

/**
 * Tool State - State of an integrated tool for extended cognition
 */
USTRUCT(BlueprintType)
struct FToolState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FString ToolName;

    UPROPERTY(BlueprintReadWrite)
    float EngagementLevel = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    bool bIsIntegrated = false;
};

/**
 * Extended State - Tool integration state
 */
USTRUCT(BlueprintType)
struct FAvatarExtendedState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    float OffloadingRatio = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    TArray<FToolState> ActiveTools;
};

/**
 * Visual Coupling State - Visual feedback state tied to cognition
 */
USTRUCT(BlueprintType)
struct FVisualCouplingState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    float HairLuminance = 0.5f;

    UPROPERTY(BlueprintReadWrite)
    float EyeGlow = 0.5f;

    UPROPERTY(BlueprintReadWrite)
    float TechPatternIntensity = 0.3f;

    UPROPERTY(BlueprintReadWrite)
    float Bioluminescence = 0.3f;

    UPROPERTY(BlueprintReadWrite)
    FVector ColorShift = FVector::ZeroVector;
};

/**
 * Avatar Expression State - Current expression state
 */
USTRUCT(BlueprintType)
struct FAvatarExpressionState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    TMap<FString, float> EmotionIntensities;

    UPROPERTY(BlueprintReadWrite)
    FString CognitiveMode;

    UPROPERTY(BlueprintReadWrite)
    FVector AttentionFocus = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite)
    float EyeGlowIntensity = 0.5f;

    UPROPERTY(BlueprintReadWrite)
    FLinearColor AuraColor;

    UPROPERTY(BlueprintReadWrite)
    float AuraPulseRate = 1.0f;

    UPROPERTY(BlueprintReadWrite)
    float HairDynamicsMultiplier = 1.0f;

    UPROPERTY(BlueprintReadWrite)
    float BreathingRate = 12.0f;

    UPROPERTY(BlueprintReadWrite)
    TMap<FString, float> MicroExpressions;
};

/**
 * 4E Coherence State - Overall coherence across 4E dimensions
 */
USTRUCT(BlueprintType)
struct F4ECoherenceState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    float EmbodiedCoherence = 0.5f;

    UPROPERTY(BlueprintReadWrite)
    float EmbeddedCoherence = 0.5f;

    UPROPERTY(BlueprintReadWrite)
    float EnactedCoherence = 0.5f;

    UPROPERTY(BlueprintReadWrite)
    float ExtendedCoherence = 0.5f;

    UPROPERTY(BlueprintReadWrite)
    float OverallCoherence = 0.5f;
};

/**
 * Sensory Input - Gathered sensory information
 */
USTRUCT(BlueprintType)
struct FAvatarSensoryInput
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FVector Position = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite)
    FRotator Rotation = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadWrite)
    FVector Velocity = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite)
    float EnvironmentAwareness = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float SocialPresence = 0.0f;
};

/**
 * Sensorimotor Contingency - Learned action-outcome mapping
 */
USTRUCT(BlueprintType)
struct FAvatarSensorimotorContingency
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FString Action;

    UPROPERTY(BlueprintReadWrite)
    FAvatarSensoryInput InitialState;

    UPROPERTY(BlueprintReadWrite)
    FAvatarSensoryInput ResultState;

    UPROPERTY(BlueprintReadWrite)
    float PredictionError = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float Timestamp = 0.0f;
};

// Delegate declarations
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAvatarActionCompleted, const FString&, ActionName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInsightRealized, float, Relevance);

/**
 * Unreal Avatar Embodiment Component
 * 
 * Implements full 4E embodied cognition for Unreal Engine avatars,
 * integrating with the Deep Tree Echo cognitive framework.
 *
 * The emotions shown on the face are chosen by the character model only. Cognition events
 * (relevance realization, pivotal echobeat steps) send the real 4E/relevance data through
 * ReactionPromptTemplate to the model; the reply is validated against GetReactionVocabulary()
 * and broadcast on FEchoReactionBus, and this component renders bus reactions as blend shapes.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class UUnrealAvatarEmbodiment : public UActorComponent
{
    GENERATED_BODY()

public:
    UUnrealAvatarEmbodiment();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, 
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable embodiment processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embodiment|Config")
    bool bEnableEmbodiment = true;

    /** Enable sensorimotor learning */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embodiment|Config")
    bool bEnableSensorimotorLearning = true;

    /** Environment scan radius for embedded cognition */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embodiment|Config")
    float EnvironmentScanRadius = 1000.0f;

    /** Action progress rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embodiment|Config")
    float ActionProgressRate = 1.0f;

    /** Visual smoothing rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embodiment|Config")
    float VisualSmoothingRate = 5.0f;

    /** Expression blend rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embodiment|Config")
    float ExpressionBlendRate = 3.0f;

    /** Prediction error threshold for learning */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embodiment|Config")
    float PredictionErrorThreshold = 0.1f;

    /** Maximum stored contingencies */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embodiment|Config")
    int32 MaxStoredContingencies = 100;

    /** Cognition events ask the model how the character reacts (the face shows only model reactions). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embodiment|Reaction")
    bool bRequestReactionOnCognitiveEvents = true;

    /** Also ask on pivotal echobeat steps (throttled). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embodiment|Reaction")
    bool bRequestReactionOnPivotalSteps = false;

    /** Relevance at or above which a relevance-realization event asks the model for a reaction. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embodiment|Reaction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float RelevanceReactionThreshold = 0.7f;

    /** Minimum seconds between reaction requests. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embodiment|Reaction", meta = (ClampMin = "0.0"))
    float MinSecondsBetweenReactionRequests = 4.0f;

    /** Only reactions for this character are rendered; empty = owner name, all reactions accepted. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embodiment|Reaction")
    FString ReactionCharacterName;

    /**
     * Prompt template (model input). Slots: {character}, {trigger}, {step}, {step_type}, {mode}, {relevance},
     * {arousal}, {valence}, {energy}, {proprioception}, {embedded_coupling}, {environment_awareness},
     * {social_presence}, {current_action}, {enacted_coupling}, {offloading}, {tool_count}, {coherence},
     * {current_emotions}, {emotions}. Reply: JSON {"emotion": <one of {emotions}>, "intensity": 0..1}.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embodiment|Reaction", meta = (MultiLine = "true"))
    FString ReactionPromptTemplate;

    // ========================================
    // EVENTS
    // ========================================

    /** Called when an action is completed */
    UPROPERTY(BlueprintAssignable, Category = "Embodiment|Events")
    FOnAvatarActionCompleted OnActionCompleted;

    /** Called when an insight is realized */
    UPROPERTY(BlueprintAssignable, Category = "Embodiment|Events")
    FOnInsightRealized OnInsightRealized;

    // ========================================
    // BLUEPRINT CALLABLE FUNCTIONS
    // ========================================

    /** Renderer: set an emotion intensity (model reactions arrive via the reaction bus) */
    UFUNCTION(BlueprintCallable, Category = "Embodiment|Expression")
    void SetEmotion(const FString& Emotion, float Intensity);

    /** Asks the model how the character reacts to the current 4E state. False if a request is pending. */
    UFUNCTION(BlueprintCallable, Category = "Embodiment|Reaction")
    bool RequestModelReaction(const FString& Trigger);

    /** Emotion names the model may choose from (the renderable emotions). */
    UFUNCTION(BlueprintPure, Category = "Embodiment|Reaction")
    TArray<FString> GetReactionVocabulary() const;

    /** Last model/validation error for reaction requests (system status only). */
    UFUNCTION(BlueprintPure, Category = "Embodiment|Reaction")
    FString GetLastReactionError() const { return LastReactionError; }

    /** Initiate an action */
    UFUNCTION(BlueprintCallable, Category = "Embodiment|Action")
    void InitiateAction(const FString& Action, const FVector& Target);

    /** Register a tool for extended cognition */
    UFUNCTION(BlueprintCallable, Category = "Embodiment|Extended")
    void RegisterTool(const FString& ToolName, float InitialEngagement);

    /** Unregister a tool */
    UFUNCTION(BlueprintCallable, Category = "Embodiment|Extended")
    void UnregisterTool(const FString& ToolName);

    /** Get 4E coherence state */
    UFUNCTION(BlueprintCallable, Category = "Embodiment|State")
    F4ECoherenceState Get4ECoherence() const;

    /** Get expression state */
    UFUNCTION(BlueprintCallable, Category = "Embodiment|State")
    FAvatarExpressionState GetExpressionState() const;

    /** Get visual coupling state */
    UFUNCTION(BlueprintCallable, Category = "Embodiment|State")
    FVisualCouplingState GetVisualCouplingState() const;

protected:
    // Initialization
    void FindComponentReferences();
    void InitializeBodySchema();
    void Initialize4EStates();
    void InitializeVisualCoupling();

    // 4E State Updates
    void UpdateEmbodiedState(float DeltaTime);
    void UpdateEmbeddedState(float DeltaTime);
    void UpdateEnactedState(float DeltaTime);
    void UpdateExtendedState(float DeltaTime);

    // Sensorimotor Processing
    void ProcessSensorimotorLoop(float DeltaTime);
    FAvatarSensoryInput GatherSensoryInput();
    FAvatarSensoryInput PredictSensoryOutcome(const FString& Action);
    float ComputePredictionError(const FAvatarSensoryInput& Actual, const FAvatarSensoryInput& Predicted);
    void UpdateInternalModels(const FAvatarSensoryInput& Actual, const FAvatarSensoryInput& Predicted, float Error);
    void UpdateSensorimotorContingencies(float DeltaTime);

    // Visual Updates
    void UpdateVisualCoupling(float DeltaTime);
    void ApplyExpressionToAvatar(float DeltaTime);
    void UpdateTechElements(float DeltaTime);
    void UpdateBodySchemaFromMesh();

    // Action Management
    void CompleteCurrentAction();

    // Event Handlers
    UFUNCTION()
    void HandleCognitiveStepChanged(int32 OldStep, int32 NewStep);
    
    /** Bound to UCognitiveCycleManager::OnPivotalStep (the relevance-realization steps) */
    UFUNCTION()
    void HandlePivotalStep(int32 Step);

    void HandleRelevanceRealized(int32 Step, float Relevance);

    // Model reactions
    bool CanRequestReaction() const;
    FString GetReactionCharacterNameResolved() const;
    FString BuildReactionPrompt(const FString& Trigger) const;
    void HandleModelReactionReply(bool bSuccess, const FString& TextOrError);
    void HandleCharacterReaction(const FEchoCharacterReaction& Reaction);

private:
    // Component references
    UPROPERTY()
    USkeletalMeshComponent* SkeletalMesh = nullptr;

    UPROPERTY()
    UCognitiveCycleManager* CognitiveCycleManager = nullptr;

    UPROPERTY()
    UEmbodied4ECognition* Embodied4ECognition = nullptr;

    UPROPERTY()
    UEchobeatsStreamEngine* EchobeatsEngine = nullptr;

    // 4E States
    FAvatarEmbodiedState EmbodiedState;
    FAvatarEmbeddedState EmbeddedState;
    FAvatarEnactedState EnactedState;
    FAvatarExtendedState ExtendedState;

    // Visual States
    FVisualCouplingState VisualState;
    FAvatarExpressionState ExpressionState;
    TMap<FString, float> TechElementStates;

    // Body Schema
    TMap<FString, FBodyPartState> BodyPartStates;

    // Sensorimotor Learning
    TArray<FAvatarSensorimotorContingency> LearnedContingencies;
    FAvatarSensoryInput LastSensoryInput;
    float LastPredictionError = 0.0f;

    // State
    bool bIsInitialized = false;

    // Cognitive record (cadence; never selects an emotion)
    int32 LastCognitiveStep = 0;
    FString LastStepTypeName;
    float LastRelevance = 0.0f;

    // Model reaction state
    bool bReactionRequestPending = false;
    double LastReactionRequestTime = -1.0e9;
    FString LastReactionError;
    FDelegateHandle ReactionHandle;
};
