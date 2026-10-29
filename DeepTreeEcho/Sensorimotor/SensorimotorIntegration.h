#pragma once

/**
 * Sensorimotor Integration for Deep-Tree-Echo Avatar Control
 * 
 * Implements the sensorimotor coupling layer that bridges perception and action
 * for embodied avatar control. Based on 4E cognitive science principles:
 * - Embodied: Cognition shaped by body morphology and capabilities
 * - Embedded: Cognition situated in environmental context
 * - Enacted: Knowledge through sensorimotor interaction
 * - Extended: Cognitive processes distributed across agent-environment
 * 
 * Integrates with:
 * - CognitiveCycleManager: 12-step cognitive loop
 * - AXIOMActiveInference: Active inference planning
 * - EntelechyFramework: Goal-directed actualization
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../Cognitive/CognitiveCycleManager.h"
#include "SensorimotorIntegration.generated.h"

// ========================================
// FORWARD DECLARATIONS
// ========================================

class UAXIOMActiveInference;
class UEntelechyFramework;

// ========================================
// ENUMERATIONS
// ========================================

/**
 * Sensory Modality Types
 */
UENUM(BlueprintType)
enum class ESensorimotorLoopModality : uint8
{
    /** Visual perception */
    Visual UMETA(DisplayName = "Visual"),
    
    /** Auditory perception */
    Auditory UMETA(DisplayName = "Auditory"),
    
    /** Proprioceptive (body position) */
    Proprioceptive UMETA(DisplayName = "Proprioceptive"),
    
    /** Vestibular (balance/orientation) */
    Vestibular UMETA(DisplayName = "Vestibular"),
    
    /** Tactile (touch/contact) */
    Tactile UMETA(DisplayName = "Tactile"),
    
    /** Interoceptive (internal state) */
    Interoceptive UMETA(DisplayName = "Interoceptive")
};

/**
 * Motor Effector Types
 */
UENUM(BlueprintType)
enum class EMotorEffector : uint8
{
    /** Locomotion (movement) */
    Locomotion UMETA(DisplayName = "Locomotion"),
    
    /** Manipulation (hands/arms) */
    Manipulation UMETA(DisplayName = "Manipulation"),
    
    /** Gaze (eye/head direction) */
    Gaze UMETA(DisplayName = "Gaze"),
    
    /** Vocalization (speech/sound) */
    Vocalization UMETA(DisplayName = "Vocalization"),
    
    /** Expression (facial/body) */
    Expression UMETA(DisplayName = "Expression"),
    
    /** Posture (body stance) */
    Posture UMETA(DisplayName = "Posture")
};

/**
 * Sensorimotor Coupling Mode
 */
UENUM(BlueprintType)
enum class ECouplingMode : uint8
{
    /** Reactive: Direct stimulus-response */
    Reactive UMETA(DisplayName = "Reactive"),
    
    /** Predictive: Anticipatory control */
    Predictive UMETA(DisplayName = "Predictive"),
    
    /** Exploratory: Active sensing */
    Exploratory UMETA(DisplayName = "Exploratory"),
    
    /** Habitual: Automated skill execution */
    Habitual UMETA(DisplayName = "Habitual")
};

/**
 * Body Schema State
 */
UENUM(BlueprintType)
enum class EBodySchemaState : uint8
{
    /** Calibrating body model */
    Calibrating UMETA(DisplayName = "Calibrating"),
    
    /** Normal operation */
    Normal UMETA(DisplayName = "Normal"),
    
    /** Adapting to change */
    Adapting UMETA(DisplayName = "Adapting"),
    
    /** Extended (tool use) */
    Extended UMETA(DisplayName = "Extended")
};

// ========================================
// STRUCTURES
// ========================================

/**
 * Sensory Channel - Single modality input stream
 */
USTRUCT(BlueprintType)
struct FSensoryChannel
{
    GENERATED_BODY()

    /** Channel identifier */
    UPROPERTY(BlueprintReadWrite)
    FName ChannelID;

    /** Sensory modality */
    UPROPERTY(BlueprintReadWrite)
    ESensorimotorLoopModality Modality = ESensorimotorLoopModality::Visual;

    /** Raw sensory data (normalized 0-1) */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> RawData;

    /** Processed features */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> Features;

    /** Attention weight (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float AttentionWeight = 1.0f;

    /** Reliability estimate (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Reliability = 1.0f;

    /** Timestamp of last update */
    UPROPERTY(BlueprintReadWrite)
    float LastUpdateTime = 0.0f;

    /** Prediction error from last cycle */
    UPROPERTY(BlueprintReadWrite)
    float PredictionError = 0.0f;
};

/**
 * Motor Channel - Single effector output stream
 */
USTRUCT(BlueprintType)
struct FMotorChannel
{
    GENERATED_BODY()

    /** Channel identifier */
    UPROPERTY(BlueprintReadWrite)
    FName ChannelID;

    /** Motor effector type */
    UPROPERTY(BlueprintReadWrite)
    EMotorEffector Effector = EMotorEffector::Locomotion;

    /** Target command (normalized) */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> TargetCommand;

    /** Current state feedback */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> CurrentState;

    /** Command gain/strength (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float CommandGain = 1.0f;

    /** Execution confidence (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Confidence = 1.0f;

    /** Is currently executing */
    UPROPERTY(BlueprintReadWrite)
    bool bIsExecuting = false;

    /** Execution progress (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Progress = 0.0f;
};

/**
 * Sensorimotor Contingency - Learned action-perception coupling
 */
USTRUCT(BlueprintType)
struct FSensorimotorLoopContingency
{
    GENERATED_BODY()

    /** Contingency identifier */
    UPROPERTY(BlueprintReadWrite)
    int32 ContingencyID = 0;

    /** Associated motor action pattern */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> ActionPattern;

    /** Expected sensory consequence */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> ExpectedSensoryChange;

    /** Sensory modalities involved */
    UPROPERTY(BlueprintReadWrite)
    TArray<ESensorimotorLoopModality> InvolvedModalities;

    /** Motor effectors involved */
    UPROPERTY(BlueprintReadWrite)
    TArray<EMotorEffector> InvolvedEffectors;

    /** Reliability of this contingency */
    UPROPERTY(BlueprintReadWrite)
    float Reliability = 0.5f;

    /** Number of times activated */
    UPROPERTY(BlueprintReadWrite)
    int32 ActivationCount = 0;

    /** Last activation time */
    UPROPERTY(BlueprintReadWrite)
    float LastActivationTime = 0.0f;
};

/**
 * Affordance - Action possibility in current context
 */
USTRUCT(BlueprintType)
struct FAffordance
{
    GENERATED_BODY()

    /** Affordance identifier */
    UPROPERTY(BlueprintReadWrite)
    int32 AffordanceID = 0;

    /** Action type this affords */
    UPROPERTY(BlueprintReadWrite)
    FName ActionType;

    /** Target object/location (if applicable) */
    UPROPERTY(BlueprintReadWrite)
    FVector TargetLocation = FVector::ZeroVector;

    /** Affordance strength (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Strength = 0.0f;

    /** Expected utility if acted upon */
    UPROPERTY(BlueprintReadWrite)
    float ExpectedUtility = 0.0f;

    /** Required motor capabilities */
    UPROPERTY(BlueprintReadWrite)
    TArray<EMotorEffector> RequiredEffectors;

    /** Relevant sensory cues */
    UPROPERTY(BlueprintReadWrite)
    TArray<ESensorimotorLoopModality> RelevantModalities;

    /** Is currently reachable */
    UPROPERTY(BlueprintReadWrite)
    bool bIsReachable = false;
};

/**
 * Body Schema - Internal model of body capabilities
 */
USTRUCT(BlueprintType)
struct FSensorimotorLoopBodySchema
{
    GENERATED_BODY()

    /** Schema state */
    UPROPERTY(BlueprintReadWrite)
    EBodySchemaState State = EBodySchemaState::Normal;

    /** Body part positions (relative to root) */
    UPROPERTY(BlueprintReadWrite)
    TMap<FName, FVector> PartPositions;

    /** Body part orientations */
    UPROPERTY(BlueprintReadWrite)
    TMap<FName, FRotator> PartOrientations;

    /** Reachable workspace bounds */
    UPROPERTY(BlueprintReadWrite)
    FBox ReachableSpace;

    /** Movement capabilities per effector */
    UPROPERTY(BlueprintReadWrite)
    TMap<EMotorEffector, float> EffectorCapabilities;

    /** Current tool extension (if any) */
    UPROPERTY(BlueprintReadWrite)
    FName CurrentTool;

    /** Tool extension offset */
    UPROPERTY(BlueprintReadWrite)
    FVector ToolExtension = FVector::ZeroVector;
};

/**
 * Sensorimotor State - Complete state snapshot
 */
USTRUCT(BlueprintType)
struct FSensorimotorState
{
    GENERATED_BODY()

    /** Timestamp */
    UPROPERTY(BlueprintReadWrite)
    float Timestamp = 0.0f;

    /** Active sensory channels */
    UPROPERTY(BlueprintReadWrite)
    TArray<FSensoryChannel> SensoryChannels;

    /** Active motor channels */
    UPROPERTY(BlueprintReadWrite)
    TArray<FMotorChannel> MotorChannels;

    /** Current body schema */
    UPROPERTY(BlueprintReadWrite)
    FSensorimotorLoopBodySchema BodySchema;

    /** Detected affordances */
    UPROPERTY(BlueprintReadWrite)
    TArray<FAffordance> Affordances;

    /** Current coupling mode */
    UPROPERTY(BlueprintReadWrite)
    ECouplingMode CouplingMode = ECouplingMode::Reactive;

    /** Overall prediction error */
    UPROPERTY(BlueprintReadWrite)
    float TotalPredictionError = 0.0f;

    /** Sensorimotor coherence (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Coherence = 1.0f;
};

/**
 * Sensorimotor Configuration
 */
USTRUCT(BlueprintType)
struct FSensorimotorConfig
{
    GENERATED_BODY()

    /** Prediction horizon (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float PredictionHorizon = 0.1f;

    /** Learning rate for contingencies */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.001", ClampMax = "0.5"))
    float ContingencyLearningRate = 0.01f;

    /** Attention decay rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AttentionDecay = 0.1f;

    /** Prediction error threshold for adaptation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AdaptationThreshold = 0.3f;

    /** Maximum contingencies to store */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "10", ClampMax = "1000"))
    int32 MaxContingencies = 100;

    /** Enable predictive control */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnablePredictiveControl = true;

    /** Enable affordance detection */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableAffordanceDetection = true;

    /** Enable body schema adaptation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableBodySchemaAdaptation = true;
};

/**
 * Sensorimotor Integration Component
 * 
 * Manages the bidirectional coupling between perception and action
 * for embodied avatar control in the Deep-Tree-Echo framework.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API USensorimotorLoopIntegration : public UActorComponent
{
    GENERATED_BODY()

public:
    USensorimotorLoopIntegration();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Sensorimotor configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensorimotor|Config")
    FSensorimotorConfig Config;

    /** Reference to CognitiveCycleManager */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensorimotor|Config")
    UCognitiveStreamCycleManager* CognitiveCycleManager;

    /** Reference to AXIOM Active Inference */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensorimotor|Config")
    UAXIOMActiveInference* AXIOMComponent;

    // ========================================
    // STATE
    // ========================================

    /** Current sensorimotor state */
    UPROPERTY(BlueprintReadOnly, Category = "Sensorimotor|State")
    FSensorimotorState CurrentState;

    /** Previous sensorimotor state */
    UPROPERTY(BlueprintReadOnly, Category = "Sensorimotor|State")
    FSensorimotorState PreviousState;

    /** Learned sensorimotor contingencies */
    UPROPERTY(BlueprintReadOnly, Category = "Sensorimotor|State")
    TArray<FSensorimotorLoopContingency> Contingencies;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /** Initialize sensorimotor system */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor")
    void Initialize();

    /** Reset to initial state */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor")
    void Reset();

    // ========================================
    // PUBLIC API - SENSORY INPUT
    // ========================================

    /** Register a sensory channel */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Sensory")
    void RegisterSensoryChannel(FName ChannelID, ESensorimotorLoopModality Modality, int32 DataDimension);

    /** Update sensory channel data */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Sensory")
    void UpdateSensoryChannel(FName ChannelID, const TArray<float>& RawData);

    /** Set attention weight for channel */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Sensory")
    void SetChannelAttention(FName ChannelID, float AttentionWeight);

    /** Get integrated sensory features */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Sensory")
    TArray<float> GetIntegratedSensoryFeatures() const;

    /** Compute prediction error for sensory channel */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Sensory")
    float ComputeSensoryPredictionError(FName ChannelID) const;

    // ========================================
    // PUBLIC API - MOTOR OUTPUT
    // ========================================

    /** Register a motor channel */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Motor")
    void RegisterMotorChannel(FName ChannelID, EMotorEffector Effector, int32 CommandDimension);

    /** Set motor command for channel */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Motor")
    void SetMotorCommand(FName ChannelID, const TArray<float>& Command, float Gain = 1.0f);

    /** Update motor state feedback */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Motor")
    void UpdateMotorFeedback(FName ChannelID, const TArray<float>& CurrentState);

    /** Get current motor commands for all channels */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Motor")
    TMap<FName, TArray<float>> GetAllMotorCommands() const;

    /** Execute motor command with predictive control */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Motor")
    void ExecuteWithPrediction(FName ChannelID);

    // ========================================
    // PUBLIC API - CONTINGENCY LEARNING
    // ========================================

    /** Learn new sensorimotor contingency */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Learning")
    int32 LearnContingency(const TArray<float>& ActionPattern, const TArray<float>& SensoryChange);

    /** Update contingency reliability */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Learning")
    void UpdateContingencyReliability(int32 ContingencyID, bool bSuccessful);

    /** Predict sensory consequence of action */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Learning")
    TArray<float> PredictSensoryConsequence(const TArray<float>& ActionPattern) const;

    /** Find matching contingency for current context */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Learning")
    int32 FindMatchingContingency(const TArray<float>& ActionPattern) const;

    // ========================================
    // PUBLIC API - AFFORDANCE DETECTION
    // ========================================

    /** Detect affordances in current sensory state */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Affordance")
    void DetectAffordances();

    /** Get affordances for specific action type */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Affordance")
    TArray<FAffordance> GetAffordancesForAction(FName ActionType) const;

    /** Get strongest affordance */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Affordance")
    FAffordance GetStrongestAffordance() const;

    /** Check if action is currently afforded */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Affordance")
    bool IsActionAfforded(FName ActionType) const;

    // ========================================
    // PUBLIC API - BODY SCHEMA
    // ========================================

    /** Update body part position */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|BodySchema")
    void UpdateBodyPartPosition(FName PartName, FVector Position, FRotator Orientation);

    /** Extend body schema with tool */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|BodySchema")
    void ExtendBodySchema(FName ToolName, FVector Extension);

    /** Reset body schema to default */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|BodySchema")
    void ResetBodySchema();

    /** Check if position is reachable */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|BodySchema")
    bool IsPositionReachable(FVector Position) const;

    /** Get effector capability */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|BodySchema")
    float GetEffectorCapability(EMotorEffector Effector) const;

    // ========================================
    // PUBLIC API - COGNITIVE CYCLE INTEGRATION
    // ========================================

    /** Process cognitive cycle step */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|CognitiveCycle")
    void ProcessCognitiveStep(int32 GlobalStep, EConsciousnessStream ActiveStream);

    /** Get salience map for perception stream */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|CognitiveCycle")
    TArray<float> GetSalienceMap() const;

    /** Get action readiness for action stream */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|CognitiveCycle")
    float GetActionReadiness() const;

    // ========================================
    // DELEGATES
    // ========================================

    /** Called when new contingency is learned */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnContingencyLearned, int32, ContingencyID);
    UPROPERTY(BlueprintAssignable, Category = "Sensorimotor|Events")
    FOnContingencyLearned OnContingencyLearned;

    /** Called when affordance is detected */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAffordanceDetected, FName, ActionType, float, Strength);
    UPROPERTY(BlueprintAssignable, Category = "Sensorimotor|Events")
    FOnAffordanceDetected OnAffordanceDetected;

    /** Called when prediction error exceeds threshold */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPredictionError, FName, ChannelID, float, Error);
    UPROPERTY(BlueprintAssignable, Category = "Sensorimotor|Events")
    FOnPredictionError OnPredictionError;

    /** Called when body schema adapts */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBodySchemaAdapted, EBodySchemaState, NewState);
    UPROPERTY(BlueprintAssignable, Category = "Sensorimotor|Events")
    FOnBodySchemaAdapted OnBodySchemaAdapted;

protected:
    virtual void BeginPlay() override;

private:
    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Process sensory prediction */
    void ProcessSensoryPrediction();

    /** Process motor prediction */
    void ProcessMotorPrediction();

    /** Update attention based on prediction error */
    void UpdateAttention();

    /** Consolidate contingencies (merge similar, prune weak) */
    void ConsolidateContingencies();

    /** Adapt body schema based on feedback */
    void AdaptBodySchema();

    /** Compute feature similarity */
    float ComputeFeatureSimilarity(const TArray<float>& A, const TArray<float>& B) const;

    /** Normalize feature vector */
    TArray<float> NormalizeFeatures(const TArray<float>& Features) const;

    // Internal state
    float AccumulatedTime = 0.0f;
    int32 LastProcessedStep = -1;
    TMap<FName, TArray<float>> PredictedSensory;
    TMap<FName, TArray<float>> PredictedMotor;
};
