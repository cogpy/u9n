// VirtualCNS.h
// Virtual Central Nervous System - The Spinal Cord of DTE Autonomous AI
//
// Architecture:
//   ┌─────────────────────────────────────────────────────┐
//   │               Virtual CNS Pipeline                  │
//   │                                                     │
//   │  Sensory Input ──▶ Spinal Cord ──▶ Motor Output    │
//   │       ▲                │               │            │
//   │       │         ┌──────▼──────┐        ▼            │
//   │  Controller     │  DTE Core   │   Controller        │
//   │   Polling       │  (Brain)    │   Commands          │
//   │                 └─────────────┘                     │
//   │                       │                             │
//   │              MetaHuman Body Schema                  │
//   │              Proprioception Binding                 │
//   └─────────────────────────────────────────────────────┘
//
// Latency Budget:
//   - Reflex arc: <4ms  (spinal cord only, no cortex)
//   - Full loop:  <16ms (one frame at 60Hz)
//   - DTE cycle:  <33ms (cognitive update at 30Hz)

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameTraining/GameControllerInterface.h"
#include "VirtualCNS.generated.h"

// Forward declarations
class UDeepTreeEchoCore;
class UEmbodiedCognitionComponent;
class UReinforcementLearningBridge;
class UGameControllerInterface;
class UGameTrainingEnvironment;
class UAttentionSystem;
class USensorimotorIntegration;
class UMetaHumanCNSBinding;

// ============================================================
// CNS Pathway Speed Tiers
// ============================================================

/** Signal propagation speed tier — mirrors biological CNS */
UENUM(BlueprintType)
enum class ECNSPathwayTier : uint8
{
    /** Spinal reflex: < 4 ms, no cortical involvement */
    SpinalReflex    UMETA(DisplayName = "Spinal Reflex"),

    /** Brainstem: < 8 ms, basic threat / balance */
    Brainstem       UMETA(DisplayName = "Brainstem"),

    /** Subcortical: < 12 ms, emotion-gated action */
    Subcortical     UMETA(DisplayName = "Subcortical"),

    /** Cortical: < 16 ms, full DTE cognitive loop */
    Cortical        UMETA(DisplayName = "Cortical"),

    /** Meta-cognitive: < 33 ms, planning / wisdom */
    MetaCognitive   UMETA(DisplayName = "Meta-Cognitive")
};

// ============================================================
// Reflex Arc — ultra-low-latency stimulus-response
// ============================================================

/** Hardwired reflex rule — bypasses deliberative cognition */
USTRUCT(BlueprintType)
struct FReflexArc
{
    GENERATED_BODY()

    /** Human-readable ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ReflexID;

    /** Trigger condition (e.g. "DodgeLeft", "BlockHigh") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TriggerCondition;

    /** Motor response to emit */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FControllerOutputCommand Response;

    /** Activation threshold [0,1] */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin="0.0", ClampMax="1.0"))
    float Threshold = 0.7f;

    /** Refractory period in seconds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin="0.0"))
    float RefractoryPeriod = 0.1f;

    /** Time since last fire */
    UPROPERTY(BlueprintReadOnly)
    float TimeSinceLastFire = 999.0f;

    /** Is currently in refractory state */
    bool IsRefractory() const { return TimeSinceLastFire < RefractoryPeriod; }
};

// ============================================================
// Proprioceptive Snapshot — body state at an instant
// ============================================================

/** Full proprioceptive snapshot of the MetaHuman avatar */
USTRUCT(BlueprintType)
struct FProprioceptiveSnapshot
{
    GENERATED_BODY()

    /** World location */
    UPROPERTY(BlueprintReadWrite)
    FVector WorldLocation = FVector::ZeroVector;

    /** World rotation */
    UPROPERTY(BlueprintReadWrite)
    FRotator WorldRotation = FRotator::ZeroRotator;

    /** Velocity vector */
    UPROPERTY(BlueprintReadWrite)
    FVector Velocity = FVector::ZeroVector;

    /** Acceleration vector */
    UPROPERTY(BlueprintReadWrite)
    FVector Acceleration = FVector::ZeroVector;

    /** Is grounded */
    UPROPERTY(BlueprintReadWrite)
    bool bIsGrounded = true;

    /** Is in air */
    UPROPERTY(BlueprintReadWrite)
    bool bIsAirborne = false;

    /** Centre of mass offset */
    UPROPERTY(BlueprintReadWrite)
    FVector CentreOfMass = FVector::ZeroVector;

    /** Active bone rotations (key = bone name) */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, FRotator> BoneRotations;

    /** Efference copy — what motor commands were sent */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> EfferenceCopy;

    /** Timestamp of this snapshot */
    UPROPERTY(BlueprintReadWrite)
    float Timestamp = 0.0f;

    /** Convert to flat sensory vector for reservoir input */
    TArray<float> ToSensoryVector() const;
};

// ============================================================
// Threat Signal — fast-path danger detection
// ============================================================

/** Rapid threat signal for sub-cortical response */
USTRUCT(BlueprintType)
struct FThreatSignal
{
    GENERATED_BODY()

    /** Threat level [0,1] */
    UPROPERTY(BlueprintReadWrite)
    float ThreatLevel = 0.0f;

    /** Direction to threat (world space) */
    UPROPERTY(BlueprintReadWrite)
    FVector ThreatDirection = FVector::ZeroVector;

    /** Distance to threat */
    UPROPERTY(BlueprintReadWrite)
    float Distance = 9999.0f;

    /** Threat type tag */
    UPROPERTY(BlueprintReadWrite)
    FString ThreatType;

    /** Time-to-impact in seconds */
    UPROPERTY(BlueprintReadWrite)
    float TimeToImpact = 9999.0f;

    /** Suggested reflex response */
    UPROPERTY(BlueprintReadWrite)
    FString SuggestedReflex;
};

// ============================================================
// CNS Telemetry — per-frame diagnostics
// ============================================================

USTRUCT(BlueprintType)
struct FCNSTelemetry
{
    GENERATED_BODY()

    /** Frames processed total */
    UPROPERTY(BlueprintReadOnly)
    int64 TotalFrames = 0;

    /** Average afferent latency (ms) — sensory → CNS */
    UPROPERTY(BlueprintReadOnly)
    float AvgAfferentLatencyMs = 0.0f;

    /** Average efferent latency (ms) — CNS → motor */
    UPROPERTY(BlueprintReadOnly)
    float AvgEfferentLatencyMs = 0.0f;

    /** Reflex arcs fired this frame */
    UPROPERTY(BlueprintReadOnly)
    int32 ReflexesFiredThisFrame = 0;

    /** DTE cognitive cycles completed */
    UPROPERTY(BlueprintReadOnly)
    int32 CognitiveCyclesCompleted = 0;

    /** Current pathway tier active */
    UPROPERTY(BlueprintReadOnly)
    ECNSPathwayTier ActiveTier = ECNSPathwayTier::Cortical;

    /** Body schema coherence [0,1] */
    UPROPERTY(BlueprintReadOnly)
    float BodySchemaCoherence = 1.0f;
};

// ============================================================
// Delegates
// ============================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnReflexFired,
    const FString&, ReflexID, const FControllerOutputCommand&, Response);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnThreatDetected,
    const FThreatSignal&, Threat);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProprioceptiveUpdate,
    const FProprioceptiveSnapshot&, Snapshot);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCNSPathwayChanged,
    ECNSPathwayTier, OldTier, ECNSPathwayTier, NewTier);

// ============================================================
// UVirtualCNS — The Central Nervous System Component
// ============================================================

/**
 * UVirtualCNS
 *
 * The Virtual Central Nervous System is the primary integration hub
 * that wires the console controller directly to the Deep Tree Echo
 * cognitive architecture, creating a closed sensorimotor loop:
 *
 *   Gamepad/Sensor  ──▶  Afferent Pathway  ──▶  DTE Cognitive Core
 *                                                        │
 *   Avatar/Physics  ◀──  Efferent Pathway  ◀────────────┘
 *
 * It implements five layered pathways (reflex → meta-cognitive) to
 * deliver both lightning-fast reflexes and higher-order planning.
 *
 * All subsystems (EmbodiedCognition, ReinforcementLearning,
 * AttentionSystem, MetaHumanDNA) are routed through the CNS so
 * that the DTE core sees a single coherent sensorimotor interface.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent),
       DisplayName="Virtual CNS")
class DEEPTREEECHO_API UVirtualCNS : public UActorComponent
{
    GENERATED_BODY()

public:
    UVirtualCNS();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ====================================================
    // CONFIGURATION
    // ====================================================

    /** Enable spinal reflex arcs (< 4 ms fast path) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CNS|Config")
    bool bEnableReflexArcs = true;

    /** Enable threat detection subsystem */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CNS|Config")
    bool bEnableThreatDetection = true;

    /** Enable proprioceptive update loop */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CNS|Config")
    bool bEnableProprioception = true;

    /** Enable efference copy (predict sensory consequence of motor command) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CNS|Config")
    bool bEnableEfferenceCopy = true;

    /** Threat detection radius (cm) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CNS|Config",
              meta=(ClampMin="50.0", ClampMax="2000.0"))
    float ThreatDetectionRadius = 500.0f;

    /** Minimum threat level to trigger reflex [0,1] */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CNS|Config",
              meta=(ClampMin="0.0", ClampMax="1.0"))
    float ReflexThreatThreshold = 0.6f;

    /** Proprioceptive update rate (Hz) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CNS|Config",
              meta=(ClampMin="10.0", ClampMax="120.0"))
    float ProprioceptiveHz = 60.0f;

    /** Registered reflex arcs */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CNS|Config")
    TArray<FReflexArc> ReflexArcs;

    // ====================================================
    // EVENTS
    // ====================================================

    UPROPERTY(BlueprintAssignable, Category="CNS|Events")
    FOnReflexFired OnReflexFired;

    UPROPERTY(BlueprintAssignable, Category="CNS|Events")
    FOnThreatDetected OnThreatDetected;

    UPROPERTY(BlueprintAssignable, Category="CNS|Events")
    FOnProprioceptiveUpdate OnProprioceptiveUpdate;

    UPROPERTY(BlueprintAssignable, Category="CNS|Events")
    FOnCNSPathwayChanged OnCNSPathwayChanged;

    // ====================================================
    // AFFERENT (Sensory → Brain)
    // ====================================================

    /** Inject raw sensory data into the CNS afferent pathway */
    UFUNCTION(BlueprintCallable, Category="CNS|Afferent")
    void InjectSensoryInput(const TArray<float>& Data, const FString& Modality);

    /** Feed controller input into the sensorimotor pipeline */
    UFUNCTION(BlueprintCallable, Category="CNS|Afferent")
    void ProcessControllerInput(const FControllerInputState& InputState);

    /** Update proprioceptive state from avatar skeleton */
    UFUNCTION(BlueprintCallable, Category="CNS|Afferent")
    void UpdateProprioception(const FProprioceptiveSnapshot& Snapshot);

    /** Submit a threat signal for fast-path processing */
    UFUNCTION(BlueprintCallable, Category="CNS|Afferent")
    void SubmitThreatSignal(const FThreatSignal& Threat);

    // ====================================================
    // EFFERENT (Brain → Motor)
    // ====================================================

    /** Poll the next motor command from the CNS efferent queue */
    UFUNCTION(BlueprintCallable, Category="CNS|Efferent")
    FControllerOutputCommand PollMotorCommand();

    /** Check whether the efferent queue is non-empty */
    UFUNCTION(BlueprintPure, Category="CNS|Efferent")
    bool HasPendingMotorCommand() const;

    /** Override the efferent output with an external command (e.g. human takeover) */
    UFUNCTION(BlueprintCallable, Category="CNS|Efferent")
    void OverrideMotorOutput(const FControllerOutputCommand& Command);

    // ====================================================
    // REFLEX MANAGEMENT
    // ====================================================

    /** Register a new reflex arc */
    UFUNCTION(BlueprintCallable, Category="CNS|Reflex")
    void RegisterReflexArc(const FReflexArc& Arc);

    /** Remove a reflex arc by ID */
    UFUNCTION(BlueprintCallable, Category="CNS|Reflex")
    bool RemoveReflexArc(const FString& ReflexID);

    /** Manually fire a reflex (testing / override) */
    UFUNCTION(BlueprintCallable, Category="CNS|Reflex")
    void FireReflex(const FString& ReflexID);

    /** Check whether a reflex arc exists */
    UFUNCTION(BlueprintPure, Category="CNS|Reflex")
    bool HasReflexArc(const FString& ReflexID) const;

    // ====================================================
    // PATHWAY CONTROL
    // ====================================================

    /** Force a specific pathway tier */
    UFUNCTION(BlueprintCallable, Category="CNS|Pathway")
    void SetPathwayTier(ECNSPathwayTier Tier);

    /** Get currently active tier */
    UFUNCTION(BlueprintPure, Category="CNS|Pathway")
    ECNSPathwayTier GetActivePathwayTier() const { return ActivePathwayTier; }

    /** Elevate tier temporarily (urgent signal) */
    UFUNCTION(BlueprintCallable, Category="CNS|Pathway")
    void ElevateTier(ECNSPathwayTier MinTier, float Duration);

    // ====================================================
    // SENSORIMOTOR COHERENCE
    // ====================================================

    /** Get the current proprioceptive snapshot */
    UFUNCTION(BlueprintPure, Category="CNS|Sensorimotor")
    FProprioceptiveSnapshot GetProprioceptiveSnapshot() const { return CurrentProprioception; }

    /** Get body schema coherence [0,1] */
    UFUNCTION(BlueprintPure, Category="CNS|Sensorimotor")
    float GetBodySchemaCoherence() const;

    /** Compute prediction error between efference copy and actual sensory data */
    UFUNCTION(BlueprintPure, Category="CNS|Sensorimotor")
    float ComputePredictionError() const;

    // ====================================================
    // TELEMETRY
    // ====================================================

    /** Get current CNS telemetry */
    UFUNCTION(BlueprintPure, Category="CNS|Telemetry")
    FCNSTelemetry GetTelemetry() const { return Telemetry; }

    /** Reset telemetry counters */
    UFUNCTION(BlueprintCallable, Category="CNS|Telemetry")
    void ResetTelemetry();

    // ====================================================
    // COMPONENT WIRING
    // ====================================================

    /** Manually set DTE Core reference */
    UFUNCTION(BlueprintCallable, Category="CNS|Wiring")
    void SetDTECore(UDeepTreeEchoCore* Core);

    /** Manually set controller interface reference */
    UFUNCTION(BlueprintCallable, Category="CNS|Wiring")
    void SetControllerInterface(UGameControllerInterface* Controller);

    /** Manually set MetaHuman CNS binding reference */
    UFUNCTION(BlueprintCallable, Category="CNS|Wiring")
    void SetMetaHumanBinding(UMetaHumanCNSBinding* Binding);

protected:
    // ====================================================
    // CACHED COMPONENT REFERENCES
    // ====================================================

    UPROPERTY()
    UDeepTreeEchoCore* DTECore = nullptr;

    UPROPERTY()
    UEmbodiedCognitionComponent* EmbodiedCognition = nullptr;

    UPROPERTY()
    UReinforcementLearningBridge* RLBridge = nullptr;

    UPROPERTY()
    UGameControllerInterface* ControllerInterface = nullptr;

    UPROPERTY()
    UAttentionSystem* AttentionSys = nullptr;

    UPROPERTY()
    USensorimotorIntegration* SensorimotorSys = nullptr;

    UPROPERTY()
    UMetaHumanCNSBinding* MetaHumanBinding = nullptr;

    // ====================================================
    // INTERNAL STATE
    // ====================================================

    /** Active pathway tier */
    ECNSPathwayTier ActivePathwayTier = ECNSPathwayTier::Cortical;

    /** Tier elevation timer */
    float TierElevationTimer = 0.0f;
    ECNSPathwayTier TierElevationMin = ECNSPathwayTier::SpinalReflex;

    /** Current proprioceptive snapshot */
    FProprioceptiveSnapshot CurrentProprioception;

    /** Last efference copy */
    TArray<float> LastEfferenceCopy;

    /** Latest threat signal */
    FThreatSignal CurrentThreat;

    /** Efferent motor command queue */
    TArray<FControllerOutputCommand> MotorCommandQueue;

    /** Telemetry data */
    FCNSTelemetry Telemetry;

    /** Proprioception timer */
    float ProprioceptionTimer = 0.0f;

    /** System initialised flag */
    bool bInitialised = false;

    // ====================================================
    // INTERNAL METHODS
    // ====================================================

    void FindAndCacheComponents();
    void InitialiseDefaultReflexArcs();

    /** Fast path: spinal reflex arc evaluation */
    void ProcessSpinalReflexes(const FControllerInputState& Input, float DeltaTime);

    /** Medium path: brainstem / subcortical processing */
    void ProcessSubcortical(const FControllerInputState& Input, float DeltaTime);

    /** Full path: cortical DTE cognitive loop */
    void ProcessCortical(const FControllerInputState& Input, float DeltaTime);

    /** Evaluate threat and update CurrentThreat */
    void EvaluateThreat(float DeltaTime);

    /** Update proprioception sampling */
    void TickProprioception(float DeltaTime);

    /** Push a motor command onto the efferent queue */
    void EnqueueMotorCommand(const FControllerOutputCommand& Cmd, ECNSPathwayTier Tier);

    /** Propagate efferent copy back to embodied cognition */
    void PropagateEfferenceCopy(const FControllerOutputCommand& Cmd);

    /** Update telemetry rolling averages */
    void UpdateTelemetry(float DeltaTime, int32 ReflexesFired);

    /** Determine the appropriate tier based on threat level and cognitive load */
    ECNSPathwayTier SelectPathwayTier() const;
};
