// Sys6AvatarIntegration.h
// Integration of sys6 operad architecture with Unreal Engine avatar system
// Implements 4E embodied cognition for Deep Tree Echo avatar

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../Core/Sys6OperadEngine.h"
#include "../Core/Sys6CognitiveBridge.h"
#include "Sys6AvatarIntegration.generated.h"

// Forward declarations
class USkeletalMeshComponent;
class UAnimInstance;

/**
 * Avatar expression channel type
 */
UENUM(BlueprintType)
enum class EAvatarExpressionChannel : uint8
{
    Facial          UMETA(DisplayName = "Facial Expression"),
    Body            UMETA(DisplayName = "Body Language"),
    Gesture         UMETA(DisplayName = "Gesture"),
    Gaze            UMETA(DisplayName = "Gaze Direction"),
    Posture         UMETA(DisplayName = "Posture"),
    Breathing       UMETA(DisplayName = "Breathing Pattern"),
    Voice           UMETA(DisplayName = "Voice Modulation"),
    Aura            UMETA(DisplayName = "Emotional Aura")
};

/**
 * 4E Cognition dimension for avatar
 */
UENUM(BlueprintType)
enum class E4EAvatarDimension : uint8
{
    Embodied        UMETA(DisplayName = "Embodied - Body Schema"),
    Embedded        UMETA(DisplayName = "Embedded - Environmental"),
    Enacted         UMETA(DisplayName = "Enacted - Sensorimotor"),
    Extended        UMETA(DisplayName = "Extended - Tool Use")
};

/**
 * Avatar expression state driven by sys6
 */
USTRUCT(BlueprintType)
struct FSys6ExpressionState
{
    GENERATED_BODY()

    /** Expression channel */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EAvatarExpressionChannel Channel = EAvatarExpressionChannel::Facial;

    /** Intensity (0-1); neutral (0) until the model has chosen this channel's expression */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Intensity = 0.0f;

    /** Valence (-1 to 1, negative to positive) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Valence = 0.0f;

    /** Arousal (0-1, calm to excited); neutral (0) until the model has chosen */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Arousal = 0.0f;

    /** True once the model has chosen this channel's expression */
    UPROPERTY(BlueprintReadOnly)
    bool bModelChosen = false;

    /** The model's chosen intensity / valence / arousal (the channel eases toward these) */
    UPROPERTY(BlueprintReadOnly)
    float TargetIntensity = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float TargetValence = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float TargetArousal = 0.0f;

    /** Dyadic phase influence (clock state; model input only) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DyadicInfluence = 0.0f;

    /** Triadic phase influence */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TriadicInfluence = 0.0f;

    /** Stage influence */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float StageInfluence = 0.0f;
};

/**
 * Body schema element for avatar
 */
USTRUCT(BlueprintType)
struct FAvatarBodySchemaElement
{
    GENERATED_BODY()

    /** Bone name in skeleton */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName BoneName;

    /** Current transform */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FTransform CurrentTransform;

    /** Target transform (driven by sys6) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FTransform TargetTransform;

    /** Proprioceptive feedback */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector ProprioceptiveFeedback = FVector::ZeroVector;

    /** Associated 4E dimension */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    E4EAvatarDimension Dimension = E4EAvatarDimension::Embodied;

    /** Sys6 thread assignment (0-7 for C8) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Sys6ThreadID = 0;
};

/**
 * Sensorimotor coupling for avatar
 */
USTRUCT(BlueprintType)
struct FAvatarSensorimotorCoupling
{
    GENERATED_BODY()

    /** Sensory input channel */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SensoryChannel;

    /** Motor output channel */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString MotorChannel;

    /** Coupling strength */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CouplingStrength = 0.5f;

    /** Latency (in sys6 steps) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 LatencySteps = 1;

    /** Associated convolution kernel (0-8 for K9) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ConvolutionKernel = 0;
};

/**
 * Avatar cognitive state
 */
USTRUCT(BlueprintType)
struct FSys6AvatarCognitiveState
{
    GENERATED_BODY()

    /** Current sys6 step */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Sys6Step = 1;

    /** Current cognitive step */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CognitiveStep = 1;

    /** Active 4E dimension */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    E4EAvatarDimension ActiveDimension = E4EAvatarDimension::Embodied;

    /** Expression states per channel */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FSys6ExpressionState> ExpressionStates;

    /** Overall coherence */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Coherence = 1.0f;

    /** Entelechy level (purposeful direction) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EntelechyLevel = 0.5f;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAvatarExpressionChanged, EAvatarExpressionChannel, Channel, float, Intensity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAvatar4EDimensionChanged, E4EAvatarDimension, OldDimension, E4EAvatarDimension, NewDimension);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAvatarSys6Sync, int32, Sys6Step);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAvatarExpressionRequestFailed, const FString&, Error);

/**
 * Sys6 Avatar Integration Component
 * 
 * Integrates the sys6 operad architecture with Unreal Engine's avatar system
 * to create 4E embodied cognition for the Deep Tree Echo avatar.
 * 
 * Key features:
 * - Maps sys6 30-step cycle to avatar expression channels
 * - Implements 4E cognition (Embodied, Embedded, Enacted, Extended)
 * - Drives facial animation, body language, and emotional aura
 * - Synchronizes with cognitive cycle for coherent behavior
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API USys6AvatarIntegration : public UActorComponent
{
    GENERATED_BODY()

public:
    USys6AvatarIntegration();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable sys6 avatar integration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sys6 Avatar|Config")
    bool bEnableIntegration = true;

    /** Expression update rate (Hz) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sys6 Avatar|Config", meta = (ClampMin = "1.0", ClampMax = "120.0"))
    float ExpressionUpdateRate = 60.0f;

    /** Expression smoothing factor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sys6 Avatar|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ExpressionSmoothing = 0.3f;

    /** Enable 4E dimension cycling */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sys6 Avatar|Config")
    bool bEnable4ECycling = true;

    /** 4E dimension cycle duration (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sys6 Avatar|Config", meta = (ClampMin = "1.0", ClampMax = "60.0"))
    float DimensionCycleDuration = 12.0f;

    // ========================================
    // EVENTS
    // ========================================

    /** Minimum seconds between model expression requests (the sys6 clock schedules when to ask) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sys6 Avatar|Model", meta = (ClampMin = "0.1"))
    float ExpressionRequestInterval = 2.0f;

    /**
     * Instructions to the model for choosing the avatar's expression on each channel. Slots:
     * {context}, {sys6}, {dimension}, {coherence}, {entelechy}, {couplings}, {current}, {channels}.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sys6 Avatar|Model", meta = (MultiLine = true))
    FString ExpressionPromptTemplate;

    /** What is happening to the character (set by the game; model input) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sys6 Avatar|Model", meta = (MultiLine = true))
    FString ExpressionContext;

    /** A channel's expression changed toward the model's choice (never fires before the model answers) */
    UPROPERTY(BlueprintAssignable, Category = "Sys6 Avatar|Events")
    FOnAvatarExpressionChanged OnExpressionChanged;

    /** A model expression request failed; no expression changed */
    UPROPERTY(BlueprintAssignable, Category = "Sys6 Avatar|Events")
    FOnAvatarExpressionRequestFailed OnExpressionRequestFailed;

    UPROPERTY(BlueprintAssignable, Category = "Sys6 Avatar|Events")
    FOnAvatar4EDimensionChanged On4EDimensionChanged;

    UPROPERTY(BlueprintAssignable, Category = "Sys6 Avatar|Events")
    FOnAvatarSys6Sync OnSys6Sync;

    // ========================================
    // COMPONENT REFERENCES
    // ========================================

    /** Reference to sys6 operad engine */
    UPROPERTY(BlueprintReadOnly, Category = "Sys6 Avatar|Components")
    USys6OperadEngine* Sys6Engine;

    /** Reference to cognitive bridge */
    UPROPERTY(BlueprintReadOnly, Category = "Sys6 Avatar|Components")
    USys6CognitiveBridge* CognitiveBridge;

    /** Reference to skeletal mesh */
    UPROPERTY(BlueprintReadOnly, Category = "Sys6 Avatar|Components")
    USkeletalMeshComponent* SkeletalMesh;

    // ========================================
    // STATE
    // ========================================

    /** Current avatar cognitive state */
    UPROPERTY(BlueprintReadOnly, Category = "Sys6 Avatar|State")
    FSys6AvatarCognitiveState CognitiveState;

    /** Body schema elements */
    UPROPERTY(BlueprintReadOnly, Category = "Sys6 Avatar|State")
    TArray<FAvatarBodySchemaElement> BodySchema;

    /** Sensorimotor couplings */
    UPROPERTY(BlueprintReadOnly, Category = "Sys6 Avatar|State")
    TArray<FAvatarSensorimotorCoupling> SensorimotorCouplings;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /** Initialize avatar integration */
    UFUNCTION(BlueprintCallable, Category = "Sys6 Avatar")
    void InitializeIntegration();

    /** Initialize body schema from skeleton */
    UFUNCTION(BlueprintCallable, Category = "Sys6 Avatar|Embodied")
    void InitializeBodySchema();

    /** Initialize sensorimotor couplings */
    UFUNCTION(BlueprintCallable, Category = "Sys6 Avatar|Enacted")
    void InitializeSensorimotorCouplings();

    // ========================================
    // PUBLIC API - EXPRESSION
    // ========================================

    /** Get expression state for channel */
    UFUNCTION(BlueprintPure, Category = "Sys6 Avatar|Expression")
    FSys6ExpressionState GetExpressionState(EAvatarExpressionChannel Channel) const;

    /** Set expression intensity */
    UFUNCTION(BlueprintCallable, Category = "Sys6 Avatar|Expression")
    void SetExpressionIntensity(EAvatarExpressionChannel Channel, float Intensity);

    /** Set expression valence */
    UFUNCTION(BlueprintCallable, Category = "Sys6 Avatar|Expression")
    void SetExpressionValence(EAvatarExpressionChannel Channel, float Valence);

    /**
     * Refreshes the sys6 clock influences and eases each channel toward the model's chosen
     * expression. Channels the model has not chosen stay neutral and do not broadcast.
     */
    UFUNCTION(BlueprintCallable, Category = "Sys6 Avatar|Expression")
    void ApplySys6ToExpression();

    /**
     * Asks the model to choose intensity [0,1], valence [-1,1] and arousal [0,1] for each
     * expression channel (ExpressionPromptTemplate). Returns false when nothing was requested.
     * Failures: Error log, GetLastExpressionError, OnExpressionRequestFailed, no change.
     */
    UFUNCTION(BlueprintCallable, Category = "Sys6 Avatar|Expression")
    bool RequestModelExpression();

    /** Why the last expression request failed, or empty */
    UFUNCTION(BlueprintPure, Category = "Sys6 Avatar|Expression")
    FString GetLastExpressionError() const { return LastExpressionError; }

    /** True once the model has chosen at least one channel's expression */
    UFUNCTION(BlueprintPure, Category = "Sys6 Avatar|Expression")
    bool HasModelExpression() const;

    // ========================================
    // PUBLIC API - 4E COGNITION
    // ========================================

    /** Get active 4E dimension */
    UFUNCTION(BlueprintPure, Category = "Sys6 Avatar|4E")
    E4EAvatarDimension GetActive4EDimension() const;

    /** Set active 4E dimension */
    UFUNCTION(BlueprintCallable, Category = "Sys6 Avatar|4E")
    void SetActive4EDimension(E4EAvatarDimension Dimension);

    /** Process embodied cognition (body schema) */
    UFUNCTION(BlueprintCallable, Category = "Sys6 Avatar|4E")
    void ProcessEmbodiedCognition(float DeltaTime);

    /** Process embedded cognition (environmental) */
    UFUNCTION(BlueprintCallable, Category = "Sys6 Avatar|4E")
    void ProcessEmbeddedCognition(float DeltaTime);

    /** Process enacted cognition (sensorimotor) */
    UFUNCTION(BlueprintCallable, Category = "Sys6 Avatar|4E")
    void ProcessEnactedCognition(float DeltaTime);

    /** Process extended cognition (tool use) */
    UFUNCTION(BlueprintCallable, Category = "Sys6 Avatar|4E")
    void ProcessExtendedCognition(float DeltaTime);

    // ========================================
    // PUBLIC API - SYS6 MAPPING
    // ========================================

    /** Clock influence of the sys6 dyadic phase (model input; not an expression) */
    UFUNCTION(BlueprintPure, Category = "Sys6 Avatar|Mapping")
    float MapDyadicToExpression(EDyadicPhase Phase) const;

    /** Clock influence of the sys6 triadic phase (model input; not an expression) */
    UFUNCTION(BlueprintPure, Category = "Sys6 Avatar|Mapping")
    float MapTriadicToExpression(ETriadicPhase Phase) const;

    /** Map sys6 stage to 4E dimension */
    UFUNCTION(BlueprintPure, Category = "Sys6 Avatar|Mapping")
    E4EAvatarDimension MapStageToE4Dimension(EPentadicStage Stage) const;

    /** Map cubic concurrency to body schema */
    UFUNCTION(BlueprintCallable, Category = "Sys6 Avatar|Mapping")
    void MapCubicToBodySchema(const FCubicConcurrencyState& CubicState);

    /** Map triadic convolution to sensorimotor */
    UFUNCTION(BlueprintCallable, Category = "Sys6 Avatar|Mapping")
    void MapConvolutionToSensorimotor(const FTriadicConvolutionState& ConvolutionState);

    // ========================================
    // PUBLIC API - STATE QUERIES
    // ========================================

    /** Get avatar cognitive state */
    UFUNCTION(BlueprintPure, Category = "Sys6 Avatar|State")
    FSys6AvatarCognitiveState GetCognitiveState() const;

    /** Get body schema element by bone name */
    UFUNCTION(BlueprintPure, Category = "Sys6 Avatar|State")
    FAvatarBodySchemaElement GetBodySchemaElement(FName BoneName) const;

    /** Get overall coherence */
    UFUNCTION(BlueprintPure, Category = "Sys6 Avatar|State")
    float GetCoherence() const;

    /** Get entelechy level */
    UFUNCTION(BlueprintPure, Category = "Sys6 Avatar|State")
    float GetEntelechyLevel() const;

protected:
    /** Expression update timer */
    float ExpressionTimer = 0.0f;

    /** 4E dimension timer */
    float DimensionTimer = 0.0f;

    /** Seconds since the last model expression request */
    float SecondsSinceExpressionRequest = 0.0f;
    bool bExpressionRequestInFlight = false;
    FString LastExpressionError;

    void HandleExpressionReply(bool bSuccess, const FString& TextOrError);
    void FailExpressionRequest(const FString& Error);

    // Internal methods
    void FindComponentReferences();
    void InitializeExpressionStates();
    void UpdateExpressionStates(float DeltaTime);
    void Update4EDimension(float DeltaTime);
    void SyncWithSys6();
    void ComputeCoherence();
    void ComputeEntelechy();

    /** Handle sys6 step advanced */
    UFUNCTION()
    void HandleSys6StepAdvanced(int32 OldStep, int32 NewStep);

    /** Handle sys6 stage changed */
    UFUNCTION()
    void HandleSys6StageChanged(EPentadicStage OldStage, EPentadicStage NewStage);

    /** Handle sys6 dyad changed */
    UFUNCTION()
    void HandleSys6DyadChanged(EDyadicPhase OldPhase, EDyadicPhase NewPhase);

    /** Handle sys6 triad changed */
    UFUNCTION()
    void HandleSys6TriadChanged(ETriadicPhase OldPhase, ETriadicPhase NewPhase);
};
