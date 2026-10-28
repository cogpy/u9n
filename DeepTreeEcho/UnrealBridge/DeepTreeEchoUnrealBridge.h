#pragma once

/**
 * Deep Tree Echo Unreal Bridge
 * 
 * Comprehensive integration layer between Deep Tree Echo cognitive framework
 * and Unreal Engine systems. Implements all Unreal Engine functions for
 * 4E embodied cognition within the Deep Tree Echo architecture.
 * 
 * Architecture:
 * - 12-step cognitive cycle with 3 concurrent consciousness streams
 * - OEIS A000081 nested shell structure (1→1, 2→2, 3→4, 4→9 terms)
 * - 4E Cognition: Embodied, Embedded, Enacted, Extended
 * - Relevance Realization through salience landscape navigation
 * - Reservoir Computing via ReservoirCpp integration
 * 
 * @see DeepTreeEchoCore for cognitive architecture
 * @see DeepTreeEchoReservoir for ESN implementation
 * @see EmbodiedCognitionComponent for 4E integration
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Character.h"
#include "../Core/DeepTreeEchoCore.h"
#include "../Reservoir/DeepTreeEchoReservoir.h"
#include "../4ECognition/EmbodiedCognitionComponent.h"
#include "DeepTreeEchoUnrealBridge.generated.h"

// Forward declarations
struct FEchoCharacterReaction;
class UAnimInstance;
class USkeletalMeshComponent;
class UAudioComponent;
class UNiagaraComponent;
class UAnimMontage;

/**
 * Unreal Engine Integration Mode
 */
UENUM(BlueprintType)
enum class EUnrealIntegrationMode : uint8
{
    /** Full integration - all systems active */
    Full UMETA(DisplayName = "Full Integration"),
    
    /** Cognitive only - no rendering/animation */
    CognitiveOnly UMETA(DisplayName = "Cognitive Only"),
    
    /** Avatar only - rendering without full cognition */
    AvatarOnly UMETA(DisplayName = "Avatar Only"),
    
    /** Minimal - basic functionality */
    Minimal UMETA(DisplayName = "Minimal")
};

/**
 * Avatar Expression State
 * The emotion/blend-shape fields hold the expression most recently chosen by the character model
 * (received on FEchoReactionBus); gaze and blink rate are procedural.
 */
USTRUCT(BlueprintType)
struct FAvatarExpressionState
{
    GENERATED_BODY()

    /** Primary emotion (0-1 intensity) */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, float> EmotionIntensities;

    /** Facial blend shape targets */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, float> FacialBlendShapes;

    /** Eye gaze direction */
    UPROPERTY(BlueprintReadWrite)
    FVector GazeDirection = FVector::ForwardVector;

    /** Blink rate modifier */
    UPROPERTY(BlueprintReadWrite)
    float BlinkRateModifier = 1.0f;

    /** Breathing rate modifier */
    UPROPERTY(BlueprintReadWrite)
    float BreathingRateModifier = 1.0f;

    /** Overall expression intensity */
    UPROPERTY(BlueprintReadWrite)
    float ExpressionIntensity = 1.0f;
};

/**
 * Cognitive-Motor Mapping
 * Maps cognitive states to motor outputs
 */
USTRUCT(BlueprintType)
struct FCognitiveMotorMapping
{
    GENERATED_BODY()

    /** Cognitive mode to animation state mapping */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, FString> ModeToAnimationMap;

    /** Emotion to gesture mapping */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, FString> EmotionToGestureMap;

    /** Attention to gaze mapping weight */
    UPROPERTY(BlueprintReadWrite)
    float AttentionToGazeWeight = 0.8f;

    /** Cognitive load to posture mapping weight */
    UPROPERTY(BlueprintReadWrite)
    float CognitiveLoadToPostureWeight = 0.6f;
};

/**
 * Sensory Input Configuration
 * Configures how Unreal Engine inputs map to cognitive sensory channels
 */
USTRUCT(BlueprintType)
struct FSensoryInputConfig
{
    GENERATED_BODY()

    /** Visual input enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableVisualInput = true;

    /** Auditory input enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableAuditoryInput = true;

    /** Proprioceptive input enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableProprioceptiveInput = true;

    /** Environmental input enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableEnvironmentalInput = true;

    /** Social input enabled (other characters) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableSocialInput = true;

    /** Visual field of view (degrees) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "30.0", ClampMax = "180.0"))
    float VisualFieldOfView = 120.0f;

    /** Auditory range (units) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "100.0", ClampMax = "10000.0"))
    float AuditoryRange = 2000.0f;
};

/**
 * A diary entry written by the character model from the cognitive state at the time it was
 * requested (see UDeepTreeEchoUnrealBridge::AddDiaryEntryFromCognitiveState).
 */
USTRUCT(BlueprintType)
struct FEchoBridgeDiaryEntry
{
    GENERATED_BODY()

    /** World time (seconds) when the entry was requested */
    UPROPERTY(BlueprintReadOnly)
    double WorldTime = 0.0;

    /** Context supplied by the caller */
    UPROPERTY(BlueprintReadOnly)
    FString Context;

    /** Cognitive mode when the entry was requested */
    UPROPERTY(BlueprintReadOnly)
    FString CognitiveMode;

    /** The model-written entry */
    UPROPERTY(BlueprintReadOnly)
    FString Text;
};

/** A model-written insight arrived for request RequestId */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBridgeInsightReady, int32, RequestId, const FString&, Insight);

/** A model-written diary entry was stored for request RequestId */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBridgeDiaryEntryAdded, int32, RequestId, const FEchoBridgeDiaryEntry&, Entry);

/** A narrative / gesture request (Kind: "insight", "diary", "gesture") failed; Error is system text */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnBridgeNarrativeFailed, int32, RequestId, const FString&, Kind, const FString&, Error);

/**
 * Deep Tree Echo Unreal Bridge Component
 * 
 * Main integration component that bridges the Deep Tree Echo cognitive
 * framework with Unreal Engine's character, animation, and rendering systems.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UDeepTreeEchoUnrealBridge : public UActorComponent
{
    GENERATED_BODY()

public:
    UDeepTreeEchoUnrealBridge();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Integration mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeepTreeEcho|Config")
    EUnrealIntegrationMode IntegrationMode = EUnrealIntegrationMode::Full;

    /** Sensory input configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeepTreeEcho|Config")
    FSensoryInputConfig SensoryConfig;

    /** Cognitive-motor mapping */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeepTreeEcho|Config")
    FCognitiveMotorMapping MotorMapping;

    /** Name used for this character in model requests and to filter bus reactions (empty = owner name, accept any) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeepTreeEcho|Config")
    FString ReactionCharacterName;

    /** Ask the character model for a facial reaction whenever the cognitive mode changes */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeepTreeEcho|Config")
    bool bRequestReactionOnModeChange = true;

    /** Minimum seconds between model reaction requests */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeepTreeEcho|Config", meta = (ClampMin = "0.0"))
    float MinSecondsBetweenReactionRequests = 2.0f;

    /**
     * Prompt template for the model reaction request. Slots: {character}, {mode}, {previous_mode},
     * {coherence}, {frame}, {salience}, {expressions}. The model's JSON answer is the reaction.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeepTreeEcho|Config", meta = (MultiLine = "true"))
    FString ReactionPromptTemplate;

    /** Enable cognitive visualization (debug) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeepTreeEcho|Config")
    bool bEnableCognitiveVisualization = false;

    /**
     * Narrative prompt templates (model input), keyed "insight", "diary" and "gesture".
     * Slots for all: {character}, {mode}, {coherence}, {frame}, {salience}, {integration_4e},
     * {experiences}, {diary}. "diary" adds {context}; "gesture" adds {requested_gesture},
     * {requested_intensity}, {gestures}. The model's reply is the insight / diary entry / gesture JSON.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeepTreeEcho|Narrative", meta = (MultiLine = "true"))
    TMap<FString, FString> NarrativePromptTemplates;

    /** Recent experiences kept for insight / diary prompts */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeepTreeEcho|Narrative", meta = (ClampMin = "1"))
    int32 MaxRecentExperiences = 32;

    /** Gesture name -> montage. The names are the gesture vocabulary offered to the model; only these can be played. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeepTreeEcho|Motor")
    TMap<FString, UAnimMontage*> GestureMontages;

    // ========================================
    // COMPONENT REFERENCES
    // ========================================

    /** Reference to Deep Tree Echo Core */
    UPROPERTY(BlueprintReadOnly, Category = "DeepTreeEcho|Components")
    UDeepTreeEchoCore* CognitiveCore;

    /** Reference to Reservoir system */
    UPROPERTY(BlueprintReadOnly, Category = "DeepTreeEcho|Components")
    UDeepTreeEchoReservoir* ReservoirSystem;

    /** Reference to 4E Cognition component */
    UPROPERTY(BlueprintReadOnly, Category = "DeepTreeEcho|Components")
    UEmbodiedCognitionComponent* EmbodiedCognition;

    /** Reference to skeletal mesh (avatar body) */
    UPROPERTY(BlueprintReadOnly, Category = "DeepTreeEcho|Components")
    USkeletalMeshComponent* AvatarMesh;

    /** Reference to animation instance */
    UPROPERTY(BlueprintReadOnly, Category = "DeepTreeEcho|Components")
    UAnimInstance* AnimInstance;

    // ========================================
    // AVATAR STATE
    // ========================================

    /** Expression last chosen by the character model (via FEchoReactionBus); neutral until one arrives */
    UPROPERTY(BlueprintReadOnly, Category = "DeepTreeEcho|State")
    FAvatarExpressionState CurrentExpression;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /** Initialize the bridge with all required components */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Bridge")
    void InitializeBridge();

    /** Set the avatar mesh component */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Bridge")
    void SetAvatarMesh(USkeletalMeshComponent* InMesh);

    // ========================================
    // PUBLIC API - SENSORY INPUT
    // ========================================

    /** Process visual input from scene */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Sensory")
    void ProcessVisualInput(const TArray<AActor*>& VisibleActors);

    /** Process auditory input */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Sensory")
    void ProcessAuditoryInput(const TArray<FVector>& SoundSources, const TArray<float>& SoundIntensities);

    /** Process proprioceptive input from animation */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Sensory")
    void ProcessProprioceptiveInput();

    /** Process environmental input (affordances) */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Sensory")
    void ProcessEnvironmentalInput(const TArray<FString>& Affordances);

    /** Process social input (other characters) */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Sensory")
    void ProcessSocialInput(const TArray<AActor*>& NearbyCharacters);

    // ========================================
    // PUBLIC API - MOTOR OUTPUT
    // ========================================

    /** Update procedural gaze/blink and, on a cognitive mode change, ask the model for a facial reaction */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Motor")
    void UpdateAvatarExpression();

    /** Ask the character model to choose a facial reaction to the current cognitive state. Returns false if a request is already pending. */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Motor")
    bool RequestModelReaction();

    /** Last model reaction request error (system-level, never shown as the character's reaction), or empty */
    UFUNCTION(BlueprintPure, Category = "DeepTreeEcho|Motor")
    FString GetLastReactionError() const { return LastReactionError; }

    /** Apply facial blend shapes */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Motor")
    void ApplyFacialBlendShapes(const TMap<FString, float>& BlendShapes);

    /** Set gaze target */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Motor")
    void SetGazeTarget(const FVector& TargetLocation);

    /**
     * Request a gesture. The request is routed through the character model, which decides (from the
     * GestureMontages vocabulary) whether and which gesture to make; its choice is broadcast on
     * FEchoReactionBus and played from GestureMontages when it arrives. Returns false (nothing
     * requested) when no gesture montages are configured; failures arrive on OnNarrativeFailed.
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Motor")
    bool TriggerGesture(const FString& GestureName, float Intensity = 1.0f);

    /** Update body posture based on cognitive state */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Motor")
    void UpdateBodyPosture();

    // ========================================
    // PUBLIC API - COGNITIVE INTEGRATION
    // ========================================

    /** Get current cognitive mode as string */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Cognitive")
    FString GetCurrentCognitiveModeString() const;

    /** Get current consciousness stream states */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Cognitive")
    TArray<FString> GetConsciousnessStreamStates() const;

    /** Get current 4E integration score */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Cognitive")
    float Get4EIntegrationScore() const;

    /** Get current relevance frame */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Cognitive")
    FString GetCurrentRelevanceFrame() const;

    /** Get reservoir state summary */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Cognitive")
    TArray<float> GetReservoirStateSummary() const;

    // ========================================
    // PUBLIC API - NARRATIVE INTEGRATION
    // ========================================

    /** Record an experience (system description of an event) for later insight / diary prompts */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Narrative")
    void RecordExperience(const FString& Experience);

    /** Recent recorded experiences, oldest first */
    UFUNCTION(BlueprintPure, Category = "DeepTreeEcho|Narrative")
    TArray<FString> GetRecentExperiences() const { return RecentExperiences; }

    /**
     * Ask the model to write a diary entry from the current cognitive state, Context and recent
     * experience. Asynchronous: the entry is stored in DiaryEntries and broadcast on
     * OnDiaryEntryAdded, or the error on OnNarrativeFailed. Returns the request id (0 = not requested).
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Narrative")
    int32 AddDiaryEntryFromCognitiveState(const FString& Context);

    /**
     * Ask the model for an insight drawn from recent experiences and the cognitive state.
     * Asynchronous: the insight arrives on OnInsightReady, or the error on OnNarrativeFailed.
     * Returns the request id, or 0 when there is no recent experience to draw on (nothing requested).
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Narrative")
    int32 RequestInsight();

    /** Model-written diary entries stored by AddDiaryEntryFromCognitiveState */
    UPROPERTY(BlueprintReadOnly, Category = "DeepTreeEcho|Narrative")
    TArray<FEchoBridgeDiaryEntry> DiaryEntries;

    /** Last model-written insight (empty until one arrives) */
    UPROPERTY(BlueprintReadOnly, Category = "DeepTreeEcho|Narrative")
    FString LastInsight;

    UPROPERTY(BlueprintAssignable, Category = "DeepTreeEcho|Narrative")
    FOnBridgeInsightReady OnInsightReady;

    UPROPERTY(BlueprintAssignable, Category = "DeepTreeEcho|Narrative")
    FOnBridgeDiaryEntryAdded OnDiaryEntryAdded;

    UPROPERTY(BlueprintAssignable, Category = "DeepTreeEcho|Narrative")
    FOnBridgeNarrativeFailed OnNarrativeFailed;

    /** Get current wisdom weight */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Narrative")
    float GetCurrentWisdomWeight() const;

    // ========================================
    // PUBLIC API - EVOLUTION
    // ========================================

    /**
     * Advance the owner's UAvatarEvolutionSystem by StepSeconds of development. Returns false (and
     * does nothing) when the owner has no evolution system.
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Evolution")
    bool TriggerDevelopmentStep(float StepSeconds = 1.0f);

    /**
     * Current developmental stage of the owner's UAvatarEvolutionSystem, 1-based
     * (1 Embryonic .. 4 Transcendent); 0 = unknown (no evolution system).
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Evolution")
    int32 GetDevelopmentalStage() const;

    /** Get entelechy actualization score */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Evolution")
    float GetEntelechyScore() const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Sensory accumulator for visual input */
    TArray<float> VisualSensoryBuffer;

    /** Sensory accumulator for auditory input */
    TArray<float> AuditorySensoryBuffer;

    /** Proprioceptive state buffer */
    TArray<float> ProprioceptiveBuffer;

    /** Environmental affordance buffer */
    TArray<FString> AffordanceBuffer;

    /** Social context buffer */
    TArray<FString> SocialContextBuffer;

    /** Expression update timer */
    float ExpressionUpdateTimer = 0.0f;

    /** Expression update interval */
    float ExpressionUpdateInterval = 0.05f; // 20 Hz

    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Initialize component references */
    void InitializeComponentReferences();

    /** Process all sensory inputs */
    void ProcessAllSensoryInputs();

    /** Update motor outputs */
    void UpdateMotorOutputs(float DeltaTime);

    /** Build the prompt that asks the model for a facial reaction to the current cognitive state */
    FString BuildReactionPrompt(const FString& PreviousMode) const;

    /** Model reply to RequestModelReaction: validate the JSON and broadcast it on FEchoReactionBus */
    void HandleModelReactionReply(bool bSuccess, const FString& TextOrError);

    /** Render a model reaction received on FEchoReactionBus (expression + intensity) to the face */
    void HandleCharacterReaction(const FEchoCharacterReaction& Reaction);

    /** Character name used in requests / reactions */
    FString GetReactionCharacterName() const;

    /** Slots shared by the narrative prompt templates (real cognitive state + recent experience) */
    TMap<FString, FString> BuildNarrativeSlots() const;

    /** Sends a narrative template to the model; OnText receives the trimmed reply. Returns the request id or 0. */
    int32 SendNarrativeRequest(const FString& Kind, const TMap<FString, FString>& Slots, TFunction<void(int32, const FString&)> OnText, float Temperature);

    /** Model reply to a gesture request: validate and broadcast on FEchoReactionBus */
    void HandleGestureReply(int32 RequestId, const FString& Text);

    /** Play GestureName from GestureMontages (model-chosen gestures only) */
    void PlayGestureMontage(const FString& GestureName, float Intensity);

    /** Recent experiences, oldest first */
    TArray<FString> RecentExperiences;

    /** Mode / affordances last recorded as experience */
    FString LastExperienceMode;
    FString LastExperienceAffordances;

    /** Narrative request id counter */
    int32 NextNarrativeRequestId = 1;

    /** Expression names MapEmotionToBlendShapes can render (the vocabulary offered to the model) */
    static const TArray<FString>& GetRenderableExpressions();

    /** Render a model-chosen expression name at the model's intensity as facial blend shapes */
    TMap<FString, float> MapEmotionToBlendShapes(const FString& Emotion, float Intensity) const;

    /** Compute gaze direction from attention */
    FVector ComputeGazeFromAttention() const;

    /** Draw cognitive visualization (debug) */
    void DrawCognitiveVisualization() const;

    /** Subscription to FEchoReactionBus */
    FDelegateHandle ReactionHandle;

    /** Cognitive mode seen at the last expression update (empty = none yet) */
    FString LastObservedMode;

    /** A model reaction request is in flight */
    bool bReactionRequestPending = false;

    /** World time of the last model reaction request */
    double LastReactionRequestTime = -1.0e9;

    /** Last request error */
    FString LastReactionError;
};
