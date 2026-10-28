// MetaHumanCNSBinding.h
// MetaHuman → Virtual CNS Proprioceptive & Expressive Binding
//
// This component bridges the MetaHuman DNA avatar rig with the
// Virtual CNS to create a fully embodied cognitive agent:
//
//   MetaHuman Skeleton  ──▶  Body Schema  ──▶  Virtual CNS
//        (joints)               (map)         (proprioception)
//
//   Model reaction (FEchoReactionBus)  ──▶  PAD  ──▶  FACS AUs  ──▶  MetaHuman Blend Shapes
//   (expression chosen by the character model)                       (expression)
//
// The facial expression is never derived from cognitive metrics: cognition events (large
// 4E / coherence shifts, RequestModelReaction) ask the model how the character reacts via
// ReactionPromptTemplate; the validated reply is broadcast on the reaction bus and the FACS
// writer renders the last bus reaction.
//
// The binding implements Merleau-Ponty's concept of body schema:
// the pre-reflective sense of one's body that enables skilful
// action without explicit representation.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CNS/VirtualCNS.h"
#include "MetaHumanCNSBinding.generated.h"

struct FEchoCharacterReaction;

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

/** Full facial expression state: PAD of the last model-chosen reaction (+ recorded cognitive telemetry) */
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

    // Cognitive load (recorded; does not drive the face)
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
 * The reverse path renders the character model's reactions (from
 * FEchoReactionBus) on the MetaHuman blend shapes. Cognitive data is
 * sent to the model as prompt input; it never selects an expression.
 *
 * Body Schema reading (60Hz):
 *   MetaHuman bones → FKinematicJoint array
 *                  → FProprioceptiveSnapshot
 *                  → VirtualCNS::UpdateProprioception()
 *
 * Expression writing (30Hz):
 *   last model reaction → ExpressionPADMap → FCognitiveExpressionState
 *                       → PAD → FACS AUs
 *                       → MetaHuman blend shapes
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent),
       DisplayName="MetaHuman CNS Binding")
class DEEPTREEECHO_API UMetaHumanCNSBinding : public UActorComponent
{
    GENERATED_BODY()

public:
    UMetaHumanCNSBinding();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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

    /**
     * Renderer map: model-chosen expression name → PAD (X = valence, Y = arousal, Z = dominance).
     * Its keys are the vocabulary offered to the model.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CNSBinding|Reaction")
    TMap<FString, FVector> ExpressionPADMap;

    /** Only reactions for this character are rendered; empty = owner name, all reactions accepted. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CNSBinding|Reaction")
    FString ReactionCharacterName;

    /** A large shift in 4E / coherence state asks the model how the character reacts. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CNSBinding|Reaction")
    bool bRequestReactionOnCognitiveShift = true;

    /** Largest per-dimension change (motor readiness, enactive engagement, extension, coherence) that counts as a shift. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CNSBinding|Reaction", meta=(ClampMin="0.0", ClampMax="1.0"))
    float CognitiveShiftThreshold = 0.3f;

    /** Minimum seconds between reaction requests. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CNSBinding|Reaction", meta=(ClampMin="0.0"))
    float MinSecondsBetweenReactionRequests = 6.0f;

    /**
     * Prompt template (model input). Slots: {character}, {trigger}, {motor_readiness}, {enactive_engagement},
     * {extension_integration}, {environment_coupling}, {niche}, {relevance_coherence}, {gestalt_coherence},
     * {body_schema_coherence}, {current_expression}, {expressions}.
     * Reply: JSON {"expression": <one of {expressions}>, "intensity": 0..1}.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CNSBinding|Reaction", meta=(MultiLine="true"))
    FString ReactionPromptTemplate;

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

    /** Renderer: set the PAD expression state directly (bus reactions use this path) */
    UFUNCTION(BlueprintCallable, Category="CNSBinding|Expression")
    void SetExpressionState(const FCognitiveExpressionState& State);

    /** Set a single FACS AU activation */
    UFUNCTION(BlueprintCallable, Category="CNSBinding|Expression")
    void SetAUActivation(int32 AUNumber, float Activation);

    /** Get AU activation */
    UFUNCTION(BlueprintPure, Category="CNSBinding|Expression")
    float GetAUActivation(int32 AUNumber) const;

    /**
     * Asks the character model how it reacts now (Trigger describes the event; the current 4E and
     * coherence state fill the prompt). The face changes only if the validated reply arrives on the
     * reaction bus. False if a request is already pending.
     */
    UFUNCTION(BlueprintCallable, Category="CNSBinding|Expression")
    bool RequestModelReaction(const FString& Trigger);

    /**
     * Reaction-bus subscriber: renders a model reaction addressed to this character (unmapped
     * expressions leave the face unchanged). Public so a holder of a bus reaction can apply it.
     */
    void HandleCharacterReaction(const FEchoCharacterReaction& Reaction);

    /** Expression currently rendered (last model reaction), empty before any. */
    UFUNCTION(BlueprintPure, Category="CNSBinding|Expression")
    FString GetCurrentReactionExpression() const { return CurrentReactionExpression; }

    /** Last model/validation error for reaction requests (system status only). */
    UFUNCTION(BlueprintPure, Category="CNSBinding|Expression")
    FString GetLastReactionError() const { return LastReactionError; }

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

    // Model reactions
    FString CurrentReactionExpression;
    float CurrentReactionIntensity = 0.0f;
    bool bReactionRequestPending = false;
    double LastReactionRequestTime = -1.0e9;
    FString LastReactionError;
    FDelegateHandle ReactionHandle;

    // 4E / coherence values at the last reaction request (for shift detection)
    bool bHasCognitiveBaseline = false;
    FVector4 CognitiveBaseline = FVector4(0.5f, 0.5f, 0.5f, 1.0f);

    bool bInitialised = false;

    // ====================================================
    // INTERNAL METHODS
    // ====================================================

    void FindAndCacheComponents();
    void InitialiseDefaultBones();
    void InitialiseFACSUnits();
    void InitialiseExpressionPADMap();

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

    /** Record DTE cognitive telemetry (load, flow) and detect shifts that ask the model for a reaction. Never sets PAD. */
    void SampleCognitiveExpression();

    // Model reactions
    FVector4 GetCognitiveVector() const;
    FString GetReactionCharacterNameResolved() const;
    FString BuildReactionPrompt(const FString& Trigger) const;
    void HandleModelReactionReply(bool bSuccess, const FString& TextOrError);
    const FVector* FindPADForExpression(const FString& Name, FString& OutKey) const;
};
