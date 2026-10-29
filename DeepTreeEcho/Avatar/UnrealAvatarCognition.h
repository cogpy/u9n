// UnrealAvatarCognition.h
// Integration of Unreal Engine avatar functions with Deep Tree Echo cognitive framework
// Implements 4E embodied cognition for avatar expression and behavior

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Sound/SoundBase.h"
#include "../Echobeats/EchobeatsStreamEngine.h"
#include "Sys6AvatarIntegration.h"
#include "UnrealAvatarCognition.generated.h"

struct FEchoCharacterReaction;

/**
 * Emotion state for avatar
 */
UENUM(BlueprintType)
enum class EAvatarEmotionState : uint8
{
    Neutral     UMETA(DisplayName = "Neutral"),
    Joy         UMETA(DisplayName = "Joy"),
    Wonder      UMETA(DisplayName = "Wonder"),
    Curiosity   UMETA(DisplayName = "Curiosity"),
    Focus       UMETA(DisplayName = "Focus"),
    Concern     UMETA(DisplayName = "Concern"),
    Contemplation UMETA(DisplayName = "Contemplation"),
    Excitement  UMETA(DisplayName = "Excitement"),
    Serenity    UMETA(DisplayName = "Serenity"),
    Determination UMETA(DisplayName = "Determination")
};

/**
 * Cognitive-to-body-tone mapping (postural/physiological tone only; facial expression and
 * emotion are chosen by the character model)
 */
USTRUCT(BlueprintType)
struct FCognitiveExpressionMapping
{
    GENERATED_BODY()

    /** Source cognitive state parameter */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString CognitiveParameter;

    /** Target expression channel */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EAvatarExpressionChannel ExpressionChannel = EAvatarExpressionChannel::Facial;

    /** Mapping weight (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MappingWeight = 1.0f;

    /** Mapping curve (linear, ease-in, ease-out, etc.) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName MappingCurve = TEXT("Linear");

    /** Minimum threshold for activation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ActivationThreshold = 0.1f;
};

/**
 * Avatar body schema state
 */
USTRUCT(BlueprintType)
struct FAvatarBodySchema
{
    GENERATED_BODY()

    /** Head orientation (pitch, yaw, roll) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRotator HeadOrientation = FRotator::ZeroRotator;

    /** Gaze target world position */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector GazeTarget = FVector::ZeroVector;

    /** Spine curvature (0 = straight, 1 = fully curved) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SpineCurvature = 0.0f;

    /** Shoulder tension (0 = relaxed, 1 = tense) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ShoulderTension = 0.0f;

    /** Arm openness (0 = closed, 1 = open) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ArmOpenness = 0.5f;

    /** Hand expressiveness (0 = still, 1 = very expressive) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HandExpressiveness = 0.3f;

    /** Stance width (0 = narrow, 1 = wide) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float StanceWidth = 0.5f;

    /** Weight distribution (-1 = left, 0 = center, 1 = right) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WeightDistribution = 0.0f;

    /** Breathing rate (breaths per minute) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BreathingRate = 12.0f;

    /** Breathing depth (0 = shallow, 1 = deep) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BreathingDepth = 0.5f;
};

/**
 * Facial expression blend shape state
 */
USTRUCT(BlueprintType)
struct FFacialExpressionState
{
    GENERATED_BODY()

    /** Brow raise (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BrowRaise = 0.0f;

    /** Brow furrow (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BrowFurrow = 0.0f;

    /** Eye openness (0 = closed, 1 = wide open) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float EyeOpenness = 0.7f;

    /** Smile (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Smile = 0.0f;

    /** Mouth openness (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MouthOpenness = 0.0f;

    /** Lip tension (0 = relaxed, 1 = tense) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LipTension = 0.0f;

    /** Cheek raise (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CheekRaise = 0.0f;

    /** Nose wrinkle (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float NoseWrinkle = 0.0f;

    /** Jaw clench (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float JawClench = 0.0f;
};

/**
 * Voice modulation parameters
 */
USTRUCT(BlueprintType)
struct FVoiceModulationState
{
    GENERATED_BODY()

    /** Pitch modifier (0.5 = half, 1.0 = normal, 2.0 = double) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.5", ClampMax = "2.0"))
    float PitchModifier = 1.0f;

    /** Volume modifier (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float VolumeModifier = 0.7f;

    /** Speaking rate modifier (0.5 = slow, 1.0 = normal, 2.0 = fast) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.5", ClampMax = "2.0"))
    float SpeakingRateModifier = 1.0f;

    /** Emotional intensity in voice (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float EmotionalIntensity = 0.5f;

    /** Breathiness (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Breathiness = 0.0f;
};

/**
 * Full avatar cognitive state
 */
USTRUCT(BlueprintType)
struct FAvatarCognitiveState
{
    GENERATED_BODY()

    /** Current emotion state */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EAvatarEmotionState EmotionState = EAvatarEmotionState::Neutral;

    /** Emotion intensity (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmotionIntensity = 0.5f;

    /** Body schema state */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FAvatarBodySchema BodySchema;

    /** Facial expression state */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FFacialExpressionState FacialExpression;

    /** Voice modulation state */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVoiceModulationState VoiceModulation;

    /** Cognitive coherence (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CognitiveCoherence = 1.0f;

    /** Entelechy level (purposeful direction) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EntelechyLevel = 0.5f;

    /** 4E cognition state */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, float> E4State;

    /** Current echobeat step */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CurrentEchobeatStep = 1;

    /** Current sys6 step */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CurrentSys6Step = 1;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEmotionStateChanged, EAvatarEmotionState, OldState, EAvatarEmotionState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnExpressionUpdated, EAvatarExpressionChannel, Channel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAvatarCognitiveStateChanged, FString, Parameter, float, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRelevanceRealizationTriggered, float, RelevanceLevel);

/**
 * Unreal Avatar Cognition Component
 * 
 * Integrates Unreal Engine avatar functions with the Deep Tree Echo cognitive framework.
 * Implements 4E embodied cognition (Embodied, Embedded, Enacted, Extended) for the avatar.
 *
 * The emotion/facial expression shown is chosen by the character model only: cognition events
 * (relevance realization, large coherence shifts, explicit ComputeExpressionFromCognition) send
 * the real cognitive data (coherence, entelechy, 4E, relevance) through ReactionPromptTemplate to
 * the model; its JSON reply is validated against EAvatarEmotionState and broadcast on
 * FEchoReactionBus; this component renders reactions from the bus (emotion -> morph/body maps).
 * 4E values modulate postural/physiological tone (breathing depth, spine, hand activity) only.
 *
 * Key features:
 * - Model-chosen emotion rendering (morph targets, body schema)
 * - Body schema management and 4E postural tone
 * - Voice modulation
 * - Integration with echobeats and sys6 systems
 * - Relevance realization events
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UUnrealAvatarCognition : public UActorComponent
{
    GENERATED_BODY()

public:
    UUnrealAvatarCognition();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Body-tone update rate (Hz) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Config")
    float ExpressionUpdateRate = 30.0f;

    /** Cognition events ask the model for the character's reaction (the face shows only model reactions). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Reaction")
    bool bRequestReactionOnCognitiveEvents = true;

    /** Relevance level at or above which a relevance-realization event asks the model for a reaction. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Reaction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float RelevanceReactionThreshold = 0.6f;

    /** Coherence change (absolute) since the last request that asks the model for a reaction. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Reaction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CoherenceShiftReactionThreshold = 0.25f;

    /** Minimum seconds between reaction requests. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Reaction", meta = (ClampMin = "0.0"))
    float MinSecondsBetweenReactionRequests = 4.0f;

    /** Only reactions for this character are rendered; empty = owner name, all reactions accepted. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Reaction")
    FString ReactionCharacterName;

    /**
     * Prompt template (model input). Slots: {character}, {trigger}, {coherence}, {entelechy}, {embodied},
     * {embedded}, {enacted}, {extended}, {relevance}, {echobeat_step}, {sys6_step}, {current_emotion},
     * {emotions}. The reply must be JSON {"emotion": <one of {emotions}>, "intensity": 0..1}.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Reaction", meta = (MultiLine = "true"))
    FString ReactionPromptTemplate;

    /** Expression smoothing factor (0 = instant, 1 = very smooth) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ExpressionSmoothingFactor = 0.3f;

    /** Cognitive-to-body-tone mappings (Body/Posture/Gesture channels; Facial is not driven by cognition) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Config")
    TArray<FCognitiveExpressionMapping> ExpressionMappings;

    /** Emotion transition duration (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Config")
    float EmotionTransitionDuration = 0.5f;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Avatar|Events")
    FOnEmotionStateChanged OnEmotionStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Avatar|Events")
    FOnExpressionUpdated OnExpressionUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Avatar|Events")
    FOnAvatarCognitiveStateChanged OnCognitiveStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Avatar|Events")
    FOnRelevanceRealizationTriggered OnRelevanceRealizationTriggered;

    // ========================================
    // COMPONENT REFERENCES
    // ========================================

    /** Reference to skeletal mesh component */
    UPROPERTY(BlueprintReadOnly, Category = "Avatar|Components")
    USkeletalMeshComponent* SkeletalMesh;

    /** Reference to echobeats stream engine */
    UPROPERTY(BlueprintReadOnly, Category = "Avatar|Components")
    UEchobeatsStreamEngine* EchobeatsEngine;

    /** Reference to sys6 avatar integration */
    UPROPERTY(BlueprintReadOnly, Category = "Avatar|Components")
    USys6AvatarIntegration* Sys6Avatar;

    // ========================================
    // PUBLIC API - EMOTION CONTROL
    // ========================================

    /** Set emotion state */
    UFUNCTION(BlueprintCallable, Category = "Avatar|Emotion")
    void SetEmotionState(EAvatarEmotionState NewState, float Intensity = 0.5f);

    /** Blend to emotion state over time */
    UFUNCTION(BlueprintCallable, Category = "Avatar|Emotion")
    void BlendToEmotionState(EAvatarEmotionState TargetState, float TargetIntensity, float BlendTime);

    /** Get current emotion state */
    UFUNCTION(BlueprintPure, Category = "Avatar|Emotion")
    EAvatarEmotionState GetEmotionState() const;

    /** Get emotion intensity */
    UFUNCTION(BlueprintPure, Category = "Avatar|Emotion")
    float GetEmotionIntensity() const;

    // ========================================
    // PUBLIC API - EXPRESSION CONTROL
    // ========================================

    /** Update facial expression */
    UFUNCTION(BlueprintCallable, Category = "Avatar|Expression")
    void UpdateFacialExpression(const FFacialExpressionState& NewExpression);

    /** Update body schema */
    UFUNCTION(BlueprintCallable, Category = "Avatar|Expression")
    void UpdateBodySchema(const FAvatarBodySchema& NewSchema);

    /** Update voice modulation */
    UFUNCTION(BlueprintCallable, Category = "Avatar|Expression")
    void UpdateVoiceModulation(const FVoiceModulationState& NewModulation);

    /** Set gaze target */
    UFUNCTION(BlueprintCallable, Category = "Avatar|Expression")
    void SetGazeTarget(const FVector& WorldTarget);

    /** Set head orientation */
    UFUNCTION(BlueprintCallable, Category = "Avatar|Expression")
    void SetHeadOrientation(const FRotator& Orientation);

    // ========================================
    // PUBLIC API - COGNITIVE INTEGRATION
    // ========================================

    /** Process cognitive state from echobeats */
    UFUNCTION(BlueprintCallable, Category = "Avatar|Cognitive")
    void ProcessEchobeatsState(const FEchobeatsFullState& EchobeatsState);

    /** Process sys6 state */
    UFUNCTION(BlueprintCallable, Category = "Avatar|Cognitive")
    void ProcessSys6State(const FSys6FullState& Sys6State);

    /** Update 4E cognition state */
    UFUNCTION(BlueprintCallable, Category = "Avatar|Cognitive")
    void Update4EState(const TMap<FString, float>& E4Values);

    /**
     * Asks the character model how it reacts to the current cognitive state (coherence, entelechy,
     * 4E, relevance). The expression changes only when the model's validated reply arrives on the
     * reaction bus. Returns false if a request is already pending.
     */
    UFUNCTION(BlueprintCallable, Category = "Avatar|Cognitive")
    bool ComputeExpressionFromCognition();

    /** Last model/validation error for reaction requests (system status only). */
    UFUNCTION(BlueprintPure, Category = "Avatar|Reaction")
    FString GetLastReactionError() const { return LastReactionError; }

    // ========================================
    // PUBLIC API - STATE QUERIES
    // ========================================

    /** Get full avatar cognitive state */
    UFUNCTION(BlueprintPure, Category = "Avatar|State")
    FAvatarCognitiveState GetCognitiveState() const;

    /** Get body schema */
    UFUNCTION(BlueprintPure, Category = "Avatar|State")
    FAvatarBodySchema GetBodySchema() const;

    /** Get facial expression */
    UFUNCTION(BlueprintPure, Category = "Avatar|State")
    FFacialExpressionState GetFacialExpression() const;

    /** Get voice modulation */
    UFUNCTION(BlueprintPure, Category = "Avatar|State")
    FVoiceModulationState GetVoiceModulation() const;

    /** Get cognitive coherence */
    UFUNCTION(BlueprintPure, Category = "Avatar|State")
    float GetCognitiveCoherence() const;

    /** Get entelechy level */
    UFUNCTION(BlueprintPure, Category = "Avatar|State")
    float GetEntelechyLevel() const;

protected:
    /** Current avatar cognitive state */
    FAvatarCognitiveState State;

    /** Target facial expression (for blending) */
    FFacialExpressionState TargetFacialExpression;

    /** Target body schema (for blending) */
    FAvatarBodySchema TargetBodySchema;

    /** Expression update timer */
    float ExpressionTimer = 0.0f;

    /** Emotion blend timer */
    float EmotionBlendTimer = 0.0f;

    /** Target emotion state for blending */
    EAvatarEmotionState TargetEmotionState = EAvatarEmotionState::Neutral;

    /** Target emotion intensity for blending */
    float TargetEmotionIntensity = 0.5f;

    /** Body schema of the model-chosen emotion, before 4E postural tone is applied */
    FAvatarBodySchema ReactionBodySchema;

    /** Latest relevance level reported by echobeats */
    float LastRelevanceLevel = 0.0f;

    /** Coherence at the time of the last reaction request */
    float CoherenceAtLastRequest = 1.0f;

    bool bReactionRequestPending = false;
    double LastReactionRequestTime = -1.0e9;
    FString LastReactionError;
    FDelegateHandle ReactionHandle;

    // Internal methods
    void FindComponentReferences();
    void InitializeDefaultMappings();
    void UpdateExpressionFromMappings();
    void BlendExpressionStates(float DeltaTime);
    void ApplyExpressionToAvatar();
    FFacialExpressionState GetEmotionFacialExpression(EAvatarEmotionState Emotion, float Intensity);
    FAvatarBodySchema GetEmotionBodySchema(EAvatarEmotionState Emotion, float Intensity);

    // Model reactions
    bool RequestModelReaction(const FString& Trigger);
    bool CanRequestReaction() const;
    FString GetReactionCharacterNameResolved() const;
    FString BuildReactionPrompt(const FString& Trigger) const;
    void HandleModelReactionReply(bool bSuccess, const FString& TextOrError);
    void HandleCharacterReaction(const FEchoCharacterReaction& Reaction);

    /** Handle echobeats step changed */
    UFUNCTION()
    void HandleEchobeatStepChanged(int32 OldStep, int32 NewStep);

    /** Handle relevance realized */
    UFUNCTION()
    void HandleRelevanceRealized(int32 Step, float RelevanceLevel);

    /** Handle stream activated */
    UFUNCTION()
    void HandleStreamActivated(ECognitiveStreamType StreamType, float ActivationLevel);
};
