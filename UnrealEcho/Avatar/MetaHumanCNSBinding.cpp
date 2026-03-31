// MetaHumanCNSBinding.cpp
// MetaHuman → Virtual CNS Binding Implementation

#include "Avatar/MetaHumanCNSBinding.h"
#include "CNS/VirtualCNS.h"
#include "Core/DeepTreeEchoCore.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Math/UnrealMathUtility.h"

// -------------------------------------------------------
// Constructor
// -------------------------------------------------------

UMetaHumanCNSBinding::UMetaHumanCNSBinding()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

// -------------------------------------------------------
// BeginPlay
// -------------------------------------------------------

void UMetaHumanCNSBinding::BeginPlay()
{
    Super::BeginPlay();
    FindAndCacheComponents();
    InitialiseDefaultBones();
    InitialiseFACSUnits();
    bInitialised = true;
}

// -------------------------------------------------------
// TickComponent
// -------------------------------------------------------

void UMetaHumanCNSBinding::TickComponent(float DeltaTime,
                                          ELevelTick TickType,
                                          FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bInitialised) { return; }

    // ── Body schema reading ───────────────────────────────
    if (bEnableBodySchemaReading)
    {
        BodySchemaTimer += DeltaTime;
        if (BodySchemaHz > 0.0f && BodySchemaTimer >= 1.0f / BodySchemaHz)
        {
            BodySchemaTimer = 0.0f;
            ReadBodySchema(DeltaTime);
        }
    }

    // ── Expression writing ────────────────────────────────
    if (bEnableExpressionWriting)
    {
        ExpressionTimer += DeltaTime;
        if (ExpressionHz > 0.0f && ExpressionTimer >= 1.0f / ExpressionHz)
        {
            ExpressionTimer = 0.0f;
            SampleCognitiveExpression();
            WriteExpressions(DeltaTime);
        }
    }

    // ── Emotion override countdown ────────────────────────
    if (EmotionOverrideTimer > 0.0f)
    {
        EmotionOverrideTimer -= DeltaTime;
        if (EmotionOverrideTimer <= 0.0f)
        {
            EmotionOverrideTimer = 0.0f;
        }
    }
}

// -------------------------------------------------------
// Body Schema
// -------------------------------------------------------

FTransform UMetaHumanCNSBinding::GetBoneWorldTransform(const FString& BoneName) const
{
    if (!SkeletalMesh) { return FTransform::Identity; }

    const FName Bone(*BoneName);
    return SkeletalMesh->GetSocketTransform(Bone, RTS_World);
}

void UMetaHumanCNSBinding::SetBodySchema(const TArray<FKinematicJoint>& Joints)
{
    KinematicChain = Joints;
    LatestSnapshot = BuildProprioceptiveSnapshot();
    if (VirtualCNS)
    {
        VirtualCNS->UpdateProprioception(LatestSnapshot);
    }
    OnBodySchemaUpdated.Broadcast(LatestSnapshot);
}

// -------------------------------------------------------
// Expression
// -------------------------------------------------------

void UMetaHumanCNSBinding::SetExpressionState(const FCognitiveExpressionState& State)
{
    ExpressionState = State;
    MapPADToFACS(State);
    ApplyFACSToBlendShapes();
    OnExpressionStateUpdated.Broadcast(ExpressionState);
}

void UMetaHumanCNSBinding::SetAUActivation(int32 AUNumber, float Activation)
{
    for (FFACSActionUnit& AU : FACSUnits)
    {
        if (AU.AUNumber == AUNumber)
        {
            AU.Activation = FMath::Clamp(Activation, 0.0f, 1.0f);
            ExpressionState.AUActivations.Add(AUNumber, AU.Activation);
            return;
        }
    }
}

float UMetaHumanCNSBinding::GetAUActivation(int32 AUNumber) const
{
    const float* Val = ExpressionState.AUActivations.Find(AUNumber);
    return Val ? *Val : 0.0f;
}

void UMetaHumanCNSBinding::TriggerEmotion(float Valence, float Arousal,
                                           float Dominance, float Duration)
{
    EmotionOverride.Valence   = Valence;
    EmotionOverride.Arousal   = Arousal;
    EmotionOverride.Dominance = Dominance;
    EmotionOverrideTimer = FMath::Max(0.0f, Duration);
    SetExpressionState(EmotionOverride);
}

// -------------------------------------------------------
// Wiring
// -------------------------------------------------------

void UMetaHumanCNSBinding::SetVirtualCNS(UVirtualCNS* CNS)
{
    VirtualCNS = CNS;
}

void UMetaHumanCNSBinding::SetDTECore(UDeepTreeEchoCore* Core)
{
    DTECore = Core;
}

// -------------------------------------------------------
// PRIVATE — FindAndCacheComponents
// -------------------------------------------------------

void UMetaHumanCNSBinding::FindAndCacheComponents()
{
    AActor* Owner = GetOwner();
    if (!Owner) { return; }

    if (!VirtualCNS) { VirtualCNS = Owner->FindComponentByClass<UVirtualCNS>(); }
    if (!DTECore)    { DTECore    = Owner->FindComponentByClass<UDeepTreeEchoCore>(); }
    // DNABridge is a UObject (not a component) — must be set explicitly via SetDTECore
    // if needed; skip FindComponentByClass for it

    if (!SkeletalMesh)
    {
        SkeletalMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
    }

    // Link the binding back to the VirtualCNS
    if (VirtualCNS)
    {
        VirtualCNS->SetMetaHumanBinding(this);
    }
}

// -------------------------------------------------------
// PRIVATE — InitialiseDefaultBones
// -------------------------------------------------------

void UMetaHumanCNSBinding::InitialiseDefaultBones()
{
    if (!TrackedBones.IsEmpty()) { return; }

    // Key MetaHuman bones for proprioception
    TrackedBones = {
        // Spine
        TEXT("pelvis"),
        TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"),
        TEXT("spine_04"), TEXT("spine_05"),
        // Head
        TEXT("neck_01"), TEXT("neck_02"), TEXT("head"),
        // Right arm
        TEXT("clavicle_r"),
        TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"),
        // Left arm
        TEXT("clavicle_l"),
        TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"),
        // Right leg
        TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r"), TEXT("ball_r"),
        // Left leg
        TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"), TEXT("ball_l"),
        // Facial root
        TEXT("FACIAL_C_FacialRoot")
    };

    KinematicChain.SetNum(TrackedBones.Num());
    for (int32 i = 0; i < TrackedBones.Num(); ++i)
    {
        KinematicChain[i].BoneName = TrackedBones[i];
    }

    BindingTelemetry.TrackedBones = TrackedBones.Num();
}

// -------------------------------------------------------
// PRIVATE — InitialiseFACSUnits
// -------------------------------------------------------

void UMetaHumanCNSBinding::InitialiseFACSUnits()
{
    if (!FACSUnits.IsEmpty()) { return; }

    // Minimal FACS set — upper and lower face groups
    // Each AU maps to MetaHuman blend shape names

    auto MakeAU = [](int32 Num, const FString& Desc,
                     const TArray<FString>& Shapes) -> FFACSActionUnit
    {
        FFACSActionUnit AU;
        AU.AUNumber   = Num;
        AU.Description = Desc;
        for (const FString& S : Shapes)
        {
            FExpressionBlendTarget BT;
            BT.BlendShapeName = S;
            BT.InterpSpeed    = 6.0f;
            AU.BlendTargets.Add(BT);
        }
        return AU;
    };

    FACSUnits.Add(MakeAU(1,  TEXT("Inner Brow Raise"),
        { TEXT("CTRL_expressions_browInnerUp_L"),
          TEXT("CTRL_expressions_browInnerUp_R") }));

    FACSUnits.Add(MakeAU(2,  TEXT("Outer Brow Raise"),
        { TEXT("CTRL_expressions_browOuterUp_L"),
          TEXT("CTRL_expressions_browOuterUp_R") }));

    FACSUnits.Add(MakeAU(4,  TEXT("Brow Lowerer"),
        { TEXT("CTRL_expressions_browDown_L"),
          TEXT("CTRL_expressions_browDown_R") }));

    FACSUnits.Add(MakeAU(6,  TEXT("Cheek Raiser"),
        { TEXT("CTRL_expressions_cheekRaiser_L"),
          TEXT("CTRL_expressions_cheekRaiser_R") }));

    FACSUnits.Add(MakeAU(7,  TEXT("Lid Tightener"),
        { TEXT("CTRL_expressions_eyeSquintInner_L"),
          TEXT("CTRL_expressions_eyeSquintInner_R") }));

    FACSUnits.Add(MakeAU(12, TEXT("Lip Corner Puller (Smile)"),
        { TEXT("CTRL_expressions_mouthSmile_L"),
          TEXT("CTRL_expressions_mouthSmile_R") }));

    FACSUnits.Add(MakeAU(15, TEXT("Lip Corner Depressor"),
        { TEXT("CTRL_expressions_mouthFrown_L"),
          TEXT("CTRL_expressions_mouthFrown_R") }));

    FACSUnits.Add(MakeAU(17, TEXT("Chin Raiser"),
        { TEXT("CTRL_expressions_mouthShrugLower") }));

    FACSUnits.Add(MakeAU(20, TEXT("Lip Stretcher"),
        { TEXT("CTRL_expressions_mouthStretch_L"),
          TEXT("CTRL_expressions_mouthStretch_R") }));

    FACSUnits.Add(MakeAU(23, TEXT("Lip Tightener (Anger)"),
        { TEXT("CTRL_expressions_mouthPress_L"),
          TEXT("CTRL_expressions_mouthPress_R") }));

    FACSUnits.Add(MakeAU(25, TEXT("Lips Part"),
        { TEXT("CTRL_expressions_jawOpen") }));

    FACSUnits.Add(MakeAU(43, TEXT("Eye Closure"),
        { TEXT("CTRL_expressions_eyesClosed_L"),
          TEXT("CTRL_expressions_eyesClosed_R") }));

    FACSUnits.Add(MakeAU(61, TEXT("Eyes Left"),
        { TEXT("CTRL_C_eyesAimLeft") }));

    FACSUnits.Add(MakeAU(62, TEXT("Eyes Right"),
        { TEXT("CTRL_C_eyesAimRight") }));
}

// -------------------------------------------------------
// PRIVATE — ReadBodySchema
// -------------------------------------------------------

void UMetaHumanCNSBinding::ReadBodySchema(float DeltaTime)
{
    if (!SkeletalMesh)
    {
        // If no skeletal mesh, still push actor transform
        LatestSnapshot = BuildProprioceptiveSnapshot();
    }
    else
    {
        for (int32 i = 0; i < TrackedBones.Num(); ++i)
        {
            if (!KinematicChain.IsValidIndex(i)) { continue; }

            FKinematicJoint& Joint = KinematicChain[i];
            const FName BoneFName(*TrackedBones[i]);

            if (!SkeletalMesh->DoesBoneExist(BoneFName)) { continue; }

            const FTransform BoneWorld =
                SkeletalMesh->GetSocketTransform(BoneFName, RTS_World);
            const FTransform BoneLocal =
                SkeletalMesh->GetSocketTransform(BoneFName, RTS_Component);

            // Compute angular velocity
            const FRotator PrevLocal = Joint.LocalRotation;
            const FRotator NewLocal  = BoneLocal.Rotator();
            const float DeltaPitch   = (NewLocal.Pitch - PrevLocal.Pitch) / FMath::Max(DeltaTime, 1e-5f);

            Joint.WorldPosition  = BoneWorld.GetTranslation();
            Joint.LocalPosition  = BoneLocal.GetTranslation();
            Joint.LocalRotation  = NewLocal;
            Joint.JointAngle     = NewLocal.Pitch;
            Joint.AngularVelocity = DeltaPitch;
        }

        LatestSnapshot = BuildProprioceptiveSnapshot();
    }

    // Push to Virtual CNS
    if (VirtualCNS)
    {
        VirtualCNS->UpdateProprioception(LatestSnapshot);
    }

    OnBodySchemaUpdated.Broadcast(LatestSnapshot);

    // Update telemetry
    BindingTelemetry.ProprioceptiveHz = BodySchemaHz;
    BindingTelemetry.TrackedBones = KinematicChain.Num();
    BindingTelemetry.BodySchemaCoherence =
        VirtualCNS ? VirtualCNS->GetBodySchemaCoherence() : 1.0f;
}

// -------------------------------------------------------
// PRIVATE — WriteExpressions
// -------------------------------------------------------

void UMetaHumanCNSBinding::WriteExpressions(float DeltaTime)
{
    if (!SkeletalMesh) { return; }

    MapPADToFACS(ExpressionState);

    if (bEnableMicroExpressions)
    {
        ApplyMicroExpression(ExpressionState.CognitiveLoad, DeltaTime);
    }

    ApplyFACSToBlendShapes();

    BindingTelemetry.ExpressionHz = ExpressionHz;
    BindingTelemetry.ActiveAUs    = 0;
    for (const FFACSActionUnit& AU : FACSUnits)
    {
        if (AU.Activation > 0.01f) { BindingTelemetry.ActiveAUs++; }
    }

    OnExpressionStateUpdated.Broadcast(ExpressionState);
}

// -------------------------------------------------------
// PRIVATE — BuildProprioceptiveSnapshot
// -------------------------------------------------------

FProprioceptiveSnapshot UMetaHumanCNSBinding::BuildProprioceptiveSnapshot() const
{
    FProprioceptiveSnapshot Snap;

    const AActor* Owner = GetOwner();
    if (Owner)
    {
        Snap.WorldLocation = Owner->GetActorLocation();
        Snap.WorldRotation = Owner->GetActorRotation();

        if (const UPrimitiveComponent* Prim =
                Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
        {
            Snap.Velocity = Prim->GetPhysicsLinearVelocity();
        }

        Snap.bIsGrounded = !Snap.bIsAirborne; // simplified; full impl uses CharacterMovement
    }

    // Pack joint angles into EfferenceCopy for prediction error
    Snap.EfferenceCopy.Reserve(KinematicChain.Num());
    for (const FKinematicJoint& Joint : KinematicChain)
    {
        Snap.EfferenceCopy.Add(Joint.JointAngle / 180.0f); // normalise to ±1
    }

    // Centre of mass from pelvis position
    const FKinematicJoint* Pelvis = nullptr;
    for (const FKinematicJoint& J : KinematicChain)
    {
        if (J.BoneName == TEXT("pelvis"))
        {
            Pelvis = &J;
            break;
        }
    }
    if (Pelvis)
    {
        Snap.CentreOfMass = Pelvis->WorldPosition;
    }

    Snap.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    // Populate BoneRotations for key joints
    for (const FKinematicJoint& Joint : KinematicChain)
    {
        Snap.BoneRotations.Add(Joint.BoneName, Joint.LocalRotation);
    }

    return Snap;
}

// -------------------------------------------------------
// PRIVATE — MapPADToFACS
// -------------------------------------------------------

void UMetaHumanCNSBinding::MapPADToFACS(const FCognitiveExpressionState& State)
{
    // PAD → FACS mapping based on Russell's circumplex model
    // Happiness: high valence, mid-high arousal
    const float HappyIntensity = FMath::Clamp(State.Valence * 0.8f + State.Arousal * 0.2f, 0.0f, 1.0f);
    // Surprise: high arousal, neutral valence
    const float SurpriseIntensity = FMath::Clamp(FMath::Abs(State.Arousal) - 0.3f, 0.0f, 1.0f) *
                                     (1.0f - FMath::Abs(State.Valence) * 0.5f);
    // Fear: high arousal, negative valence
    const float FearIntensity = FMath::Clamp(
        (-State.Valence + State.Arousal) * 0.5f, 0.0f, 1.0f);
    // Anger: high arousal, negative valence, high dominance
    const float AngerIntensity = FMath::Clamp(
        (-State.Valence * 0.6f + State.Arousal * 0.4f) * State.Dominance, 0.0f, 1.0f);
    // Sadness: negative valence, low arousal
    const float SadIntensity = FMath::Clamp(
        (-State.Valence) * (1.0f - State.Arousal) * 0.8f, 0.0f, 1.0f);
    // Concentration (cognitive load)
    const float ConcentrationIntensity = State.CognitiveLoad;

    // Set AUs from emotional intensities
    // AU 1+2 = surprise brow raise
    SetAUActivation(1, FMath::Max(SurpriseIntensity, FearIntensity * 0.7f));
    SetAUActivation(2, SurpriseIntensity);
    // AU 4 = brow lowerer (anger, concentration)
    SetAUActivation(4, FMath::Max(AngerIntensity, ConcentrationIntensity * 0.5f));
    // AU 6 = cheek raise (happiness)
    SetAUActivation(6, HappyIntensity * 0.8f);
    // AU 7 = lid tightener (anger)
    SetAUActivation(7, AngerIntensity * 0.7f);
    // AU 12 = lip corner pull (smile)
    SetAUActivation(12, HappyIntensity);
    // AU 15 = lip corner depress (sadness)
    SetAUActivation(15, SadIntensity);
    // AU 17 = chin raise (contempt / concentration)
    SetAUActivation(17, ConcentrationIntensity * 0.4f);
    // AU 20 = lip stretch (fear)
    SetAUActivation(20, FearIntensity * 0.8f);
    // AU 23 = lip tightener (anger)
    SetAUActivation(23, AngerIntensity * 0.9f);
    // AU 25 = lips part (surprise, speech)
    SetAUActivation(25, SurpriseIntensity * 0.6f);
    // AU 43 = blink (involuntary — handled by animation blueprint)

    // Sync to expression state AU map
    for (const FFACSActionUnit& AU : FACSUnits)
    {
        ExpressionState.AUActivations.Add(AU.AUNumber, AU.Activation);
    }
}

// -------------------------------------------------------
// PRIVATE — ApplyFACSToBlendShapes
// -------------------------------------------------------

void UMetaHumanCNSBinding::ApplyFACSToBlendShapes()
{
    if (!SkeletalMesh) { return; }

    for (FFACSActionUnit& AU : FACSUnits)
    {
        for (FExpressionBlendTarget& BT : AU.BlendTargets)
        {
            // Smooth interpolation toward target
            BT.TargetWeight = AU.Activation;
            BT.CurrentWeight = FMath::FInterpTo(
                BT.CurrentWeight,
                BT.TargetWeight,
                GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f,
                BT.InterpSpeed);

            const FName MorphTarget(*BT.BlendShapeName);
            SkeletalMesh->SetMorphTarget(MorphTarget,
                                          FMath::Clamp(BT.CurrentWeight, 0.0f, 1.0f));
        }
    }
}

// -------------------------------------------------------
// PRIVATE — ApplyMicroExpression
// -------------------------------------------------------

void UMetaHumanCNSBinding::ApplyMicroExpression(float CognitiveLoad,
                                                  float DeltaTime)
{
    // At high cognitive load, add subtle brow tension and lid tightening
    const float LoadThreshold = 0.7f;
    if (CognitiveLoad < LoadThreshold) { return; }

    const float Intensity = (CognitiveLoad - LoadThreshold) / (1.0f - LoadThreshold);
    const float Noise = FMath::Sin(GetWorld() ? GetWorld()->GetTimeSeconds() * 3.7f : 0.0f)
                        * 0.05f * Intensity;

    // Add subtle brow knit and lid squint as cognitive load spikes
    SetAUActivation(4, FMath::Min(1.0f, GetAUActivation(4) + Intensity * 0.3f + Noise));
    SetAUActivation(7, FMath::Min(1.0f, GetAUActivation(7) + Intensity * 0.2f + Noise));

    (void)DeltaTime;
}

// -------------------------------------------------------
// PRIVATE — SampleCognitiveExpression
// -------------------------------------------------------

void UMetaHumanCNSBinding::SampleCognitiveExpression()
{
    if (!DTECore) { return; }

    // Skip if emotion override is active
    if (EmotionOverrideTimer > 0.0f) { return; }

    // Read somatic markers from 4E cognition state for valence/arousal
    const F4ECognitionState& Cog4E = DTECore->CognitionState4E;

    // Map motor readiness to arousal
    ExpressionState.Arousal = (Cog4E.MotorReadiness - 0.5f) * 2.0f;

    // Map enactive engagement to valence (proxy)
    ExpressionState.Valence = (Cog4E.EnactiveEngagement - 0.5f) * 2.0f;

    // Map extension integration to dominance
    ExpressionState.Dominance = (Cog4E.ExtensionIntegration - 0.5f) * 2.0f;

    // Cognitive load from relevance coherence (high coherence = focused)
    ExpressionState.CognitiveLoad =
        1.0f - DTECore->RelevanceState.RelevanceCoherence;

    // Flow index from gestalt coherence
    ExpressionState.FlowIndex = DTECore->GetGestaltCoherence();

    // Clamp all values
    ExpressionState.Valence   = FMath::Clamp(ExpressionState.Valence,   -1.0f, 1.0f);
    ExpressionState.Arousal   = FMath::Clamp(ExpressionState.Arousal,   -1.0f, 1.0f);
    ExpressionState.Dominance = FMath::Clamp(ExpressionState.Dominance, -1.0f, 1.0f);
}
