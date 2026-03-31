// MetaHumanCNSBinding.h
// MetaHuman → Virtual CNS Proprioceptive & Expressive Binding
//
// This component bridges the MetaHuman DNA avatar rig with the
// Virtual CNS to create a fully embodied cognitive agent:
//
//   MetaHuman Skeleton  ──▶  Body Schema  ──▶  Virtual CNS
//        (joints)               (map)         (proprioception)
//
//   Virtual CNS  ──▶  Emotional State  ──▶  MetaHuman Blend Shapes
//   (cognitive)                                 (expression)
//
// The binding implements Merleau-Ponty's concept of body schema:
// the pre-reflective sense of one's body that enables skilful
// action without explicit representation.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CNS/VirtualCNS.h"
#include "MetaHumanCNSBinding.generated.h"

// Forward declarations
class UVirtualCNS;
class UDeepTreeEchoCore;
class USkeletalMeshComponent;
class UMetaHumanDNABridge;

// ============================================================
// Expression Blend Shape Target
// ============================================================

/** Single expression target with blend shape name and weight */
USTRUCT(BlueprintType)
struct FExpressionBlendTarget
{
    GENERATED_BODY()

    /** Blend shape name in MetaHuman rig */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString BlendShapeName;

    /** Target weight [0,1] */
    UPROPERTY(BlueprintReadWrite, meta=(ClampMin="0.0", ClampMax="1.0"))
    float TargetWeight = 0.0f;

    /** Current weight (interpolated) */
    UPROPERTY(BlueprintReadOnly)
    float CurrentWeight = 0.0f;

    /** Interpolation speed */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1", ClampMax="20.0"))
    float InterpSpeed = 5.0f;
};

// ============================================================
// Facial Action Unit — FACS mapping
// ============================================================

/** FACS Action Unit mapped to MetaHuman blend shapes */
USTRUCT(BlueprintType)
struct FFACSActionUnit
{
    GENERATED_BODY()

    /** AU number (e.g. AU1 = inner brow raise) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 AUNumber = 0;

    /** Human-readable description */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;

    /** MetaHuman blend shape targets */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FExpressionBlendTarget> BlendTargets;

    /** Current activation [0,1] */
    UPROPERTY(BlueprintReadWrite, meta=(ClampMin="0.0", ClampMax="1.0"))
    float Activation = 0.0f;
};

// ============================================================
// Emotional Expression State
// ============================================================

/** Full facial expression state driven by cognitive emotional state */
USTRUCT(BlueprintType)
struct FCognitiveExpressionState
{
    GENERATED_BODY()

    // PAD (Pleasure-Arousal-Dominance) emotional space
    UPROPERTY(BlueprintReadWrite, meta=(ClampMin="-1.0", ClampMax="1.0"))
    float Valence = 0.0f;  // Pleasure: negative → positive

    UPROPERTY(BlueprintReadWrite, meta=(ClampMin="-1.0", ClampMax="1.0"))
    float Arousal = 0.0f;  // Calm → excited

    UPROPERTY(BlueprintReadWrite, meta=(ClampMin="-1.0", ClampMax="1.0"))
    float Dominance = 0.0f; // Submissive → dominant

    // Cognitive load affects micro-expressions
    UPROPERTY(BlueprintReadWrite, meta=(ClampMin="0.0", ClampMax="1.0"))
    float CognitiveLoad = 0.0f;

    // Flow state index
    UPROPERTY(BlueprintReadWrite, meta=(ClampMin="0.0", ClampMax="1.0"))
    float FlowIndex = 0.0f;

    // Derived FACS Action Unit activations
    UPROPERTY(BlueprintReadOnly)
    TMap<int32, float> AUActivations;
};

// ============================================================
// Kinematic Chain — body part linkage for proprioception
// ============================================================

USTRUCT(BlueprintType)
struct FKinematicJoint
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FString BoneName;

    UPROPERTY(BlueprintReadWrite)
    FVector LocalPosition = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite)
    FRotator LocalRotation = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadWrite)
    FVector WorldPosition = FVector::ZeroVector;

    /** Joint angle (primary axis, degrees) */
    UPROPERTY(BlueprintReadWrite)
    float JointAngle = 0.0f;

    /** Angular velocity (degrees/s) */
    UPROPERTY(BlueprintReadWrite)
    float AngularVelocity = 0.0f;
};

// ============================================================
// CNS Binding Telemetry
// ============================================================

USTRUCT(BlueprintType)
struct FCNSBindingTelemetry
{
    GENERATED_BODY()

    /** Proprioceptive update rate (Hz) */
    UPROPERTY(BlueprintReadOnly)
    float ProprioceptiveHz = 0.0f;

    /** Expression update rate (Hz) */
    UPROPERTY(BlueprintReadOnly)
    float ExpressionHz = 0.0f;

    /** Number of bones tracked */
    UPROPERTY(BlueprintReadOnly)
    int32 TrackedBones = 0;

    /** Body schema coherence [0,1] */
    UPROPERTY(BlueprintReadOnly)
    float BodySchemaCoherence = 1.0f;

    /** Facial FACS AUs active */
    UPROPERTY(BlueprintReadOnly)
    int32 ActiveAUs = 0;
};

// ============================================================
// Delegates
// ============================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBodySchemaUpdated,
    const FProprioceptiveSnapshot&, Snapshot);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnExpressionStateUpdated,
    const FCognitiveExpressionState&, Expression);

// ============================================================
// UMetaHumanCNSBinding
// ============================================================

/**
 * UMetaHumanCNSBinding
 *
 * Creates a living, breathing body schema for the DTE autonomous
 * agent by continuously reading the MetaHuman skeleton and
 * writing the proprioceptive snapshot into the Virtual CNS.
 *
 * The reverse path drives MetaHuman blend shapes from the
 * cognitive emotional state, creating authentic, real-time
 * facial expressions that reflect the agent's inner life.
 *
 * Body Schema reading (60Hz):
 *   MetaHuman bones → FKinematicJoint array
 *                  → FProprioceptiveSnapshot
 *                  → VirtualCNS::UpdateProprioception()
 *
 * Expression writing (30Hz):
 *   DTECore emotional state → FCognitiveExpressionState
 *                           → PAD → FACS AUs
 *                           → MetaHuman blend shapes
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent),
       DisplayName="MetaHuman CNS Binding")
class DEEPTREEECHO_API UMetaHumanCNSBinding : public UActorComponent
{
    GENERATED_BODY()

public:
    UMetaHumanCNSBinding();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ====================================================
    // CONFIGURATION
    // ====================================================

    /** Enable body schema reading (proprioception) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CNSBinding|Config")
    bool bEnableBodySchemaReading = true;

    /** Enable expression writing (blend shapes) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CNSBinding|Config")
    bool bEnableExpressionWriting = true;

    /** Enable micro-expressions driven by cognitive load */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CNSBinding|Config")
    bool bEnableMicroExpressions = true;

    /** Body schema reading rate (Hz) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CNSBinding|Config",
              meta=(ClampMin="10.0", ClampMax="120.0"))
    float BodySchemaHz = 60.0f;

    /** Expression update rate (Hz) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CNSBinding|Config",
              meta=(ClampMin="10.0", ClampMax="60.0"))
    float ExpressionHz = 30.0f;

    /** Skeletal mesh component to read bones from */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CNSBinding|Config")
    TObjectPtr<USkeletalMeshComponent> SkeletalMesh;

    /** Bone names to track for proprioception */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CNSBinding|Config")
    TArray<FString> TrackedBones;

    /** FACS Action Unit definitions */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CNSBinding|Config")
    TArray<FFACSActionUnit> FACSUnits;

    // ====================================================
    // EVENTS
    // ====================================================

    UPROPERTY(BlueprintAssignable, Category="CNSBinding|Events")
    FOnBodySchemaUpdated OnBodySchemaUpdated;

    UPROPERTY(BlueprintAssignable, Category="CNSBinding|Events")
    FOnExpressionStateUpdated OnExpressionStateUpdated;

    // ====================================================
    // BODY SCHEMA
    // ====================================================

    /** Get current kinematic joint state */
    UFUNCTION(BlueprintPure, Category="CNSBinding|Body")
    TArray<FKinematicJoint> GetKinematicChain() const { return KinematicChain; }

    /** Get the latest proprioceptive snapshot */
    UFUNCTION(BlueprintPure, Category="CNSBinding|Body")
    FProprioceptiveSnapshot GetLatestSnapshot() const { return LatestSnapshot; }

    /** Get bone world transform */
    UFUNCTION(BlueprintPure, Category="CNSBinding|Body")
    FTransform GetBoneWorldTransform(const FString& BoneName) const;

    /** Manually set body schema (for testing / non-skeletal use) */
    UFUNCTION(BlueprintCallable, Category="CNSBinding|Body")
    void SetBodySchema(const TArray<FKinematicJoint>& Joints);

    // ====================================================
    // EXPRESSION
    // ====================================================

    /** Get current cognitive expression state */
    UFUNCTION(BlueprintPure, Category="CNSBinding|Expression")
    FCognitiveExpressionState GetExpressionState() const { return ExpressionState; }

    /** Manually set expression state (overrides cognitive driving) */
    UFUNCTION(BlueprintCallable, Category="CNSBinding|Expression")
    void SetExpressionState(const FCognitiveExpressionState& State);

    /** Set a single FACS AU activation */
    UFUNCTION(BlueprintCallable, Category="CNSBinding|Expression")
    void SetAUActivation(int32 AUNumber, float Activation);

    /** Get AU activation */
    UFUNCTION(BlueprintPure, Category="CNSBinding|Expression")
    float GetAUActivation(int32 AUNumber) const;

    /** Trigger a specific emotional expression */
    UFUNCTION(BlueprintCallable, Category="CNSBinding|Expression")
    void TriggerEmotion(float Valence, float Arousal, float Dominance,
                        float Duration = 1.0f);

    // ====================================================
    // TELEMETRY
    // ====================================================

    UFUNCTION(BlueprintPure, Category="CNSBinding|Telemetry")
    FCNSBindingTelemetry GetTelemetry() const { return BindingTelemetry; }

    // ====================================================
    // COMPONENT WIRING
    // ====================================================

    UFUNCTION(BlueprintCallable, Category="CNSBinding|Wiring")
    void SetVirtualCNS(UVirtualCNS* CNS);

    UFUNCTION(BlueprintCallable, Category="CNSBinding|Wiring")
    void SetDTECore(UDeepTreeEchoCore* Core);

protected:
    // ====================================================
    // CACHED REFERENCES
    // ====================================================

    UPROPERTY()
    UVirtualCNS* VirtualCNS = nullptr;

    UPROPERTY()
    UDeepTreeEchoCore* DTECore = nullptr;

    UPROPERTY()
    UMetaHumanDNABridge* DNABridge = nullptr;

    // ====================================================
    // INTERNAL STATE
    // ====================================================

    TArray<FKinematicJoint> KinematicChain;
    FProprioceptiveSnapshot LatestSnapshot;
    FCognitiveExpressionState ExpressionState;
    FCNSBindingTelemetry BindingTelemetry;

    float BodySchemaTimer    = 0.0f;
    float ExpressionTimer    = 0.0f;

    // Emotion override
    float EmotionOverrideTimer    = 0.0f;
    FCognitiveExpressionState EmotionOverride;

    bool bInitialised = false;

    // ====================================================
    // INTERNAL METHODS
    // ====================================================

    void FindAndCacheComponents();
    void InitialiseDefaultBones();
    void InitialiseFACSUnits();

    /** Read MetaHuman skeleton → kinematic chain */
    void ReadBodySchema(float DeltaTime);

    /** Write expression state → MetaHuman blend shapes */
    void WriteExpressions(float DeltaTime);

    /** Convert kinematic chain to proprioceptive snapshot */
    FProprioceptiveSnapshot BuildProprioceptiveSnapshot() const;

    /** Map PAD coordinates to FACS Action Unit activations */
    void MapPADToFACS(const FCognitiveExpressionState& State);

    /** Apply FACS AUs to skeletal mesh blend shapes */
    void ApplyFACSToBlendShapes();

    /** Generate micro-expression based on cognitive load */
    void ApplyMicroExpression(float CognitiveLoad, float DeltaTime);

    /** Sample DTE cognitive state and update expression */
    void SampleCognitiveExpression();
};
