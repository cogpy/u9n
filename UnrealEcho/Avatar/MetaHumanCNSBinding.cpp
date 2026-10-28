// MetaHumanCNSBinding.cpp
// MetaHuman → Virtual CNS Binding Implementation

#include "Avatar/MetaHumanCNSBinding.h"
#include "CNS/VirtualCNS.h"
#include "Core/DeepTreeEchoCore.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Math/UnrealMathUtility.h"
#include "EchoCharacterReaction.h"
#include "EchoModelClient.h"
#include "Dom/JsonObject.h"

// -------------------------------------------------------
// Constructor
// -------------------------------------------------------

UMetaHumanCNSBinding::UMetaHumanCNSBinding()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;

    // Prompt template (model input): the model chooses the reaction, the slots carry the real state
    ReactionPromptTemplate = TEXT(
        "You are {character}, an embodied cognitive agent with a MetaHuman face. Event: {trigger}.\n"
        "4E state: motor readiness {motor_readiness}, enactive engagement {enactive_engagement}, "
        "extension integration {extension_integration}, environment coupling {environment_coupling}, niche {niche}. "
        "Relevance coherence {relevance_coherence}, gestalt coherence {gestalt_coherence}, "
        "body schema coherence {body_schema_coherence}. Your face currently shows: {current_expression}.\n"
        "Choose the facial expression you show now, from exactly this list: {expressions}. "
        "Reply with JSON only: {\"expression\": \"<one name from the list>\", \"intensity\": <number 0..1>}");
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
    InitialiseExpressionPADMap();

    // The displayed expression is chosen by the character model only
    TWeakObjectPtr<UMetaHumanCNSBinding> WeakThis(this);
    ReactionHandle = FEchoReactionBus::OnReaction().AddLambda([WeakThis](const FEchoCharacterReaction& Reaction)
    {
        if (UMetaHumanCNSBinding* Self = WeakThis.Get())
        {
            Self->HandleCharacterReaction(Reaction);
        }
    });

    bInitialised = true;
}

void UMetaHumanCNSBinding::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (ReactionHandle.IsValid())
    {
        FEchoReactionBus::OnReaction().Remove(ReactionHandle);
        ReactionHandle.Reset();
    }

    Super::EndPlay(EndPlayReason);
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
            SampleCognitiveExpression();   // telemetry + shift detection (never sets PAD)
            WriteExpressions(DeltaTime);   // renders the last model reaction
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

// -------------------------------------------------------
// Model reactions
// -------------------------------------------------------

void UMetaHumanCNSBinding::InitialiseExpressionPADMap()
{
    if (ExpressionPADMap.Num() > 0) { return; } // configured in the editor

    // Renderer map (PAD per expression name); the model picks the name
    ExpressionPADMap.Add(TEXT("Neutral"),       FVector( 0.0f,  0.0f,  0.0f));
    ExpressionPADMap.Add(TEXT("Joy"),           FVector( 0.8f,  0.4f,  0.3f));
    ExpressionPADMap.Add(TEXT("Serenity"),      FVector( 0.5f, -0.4f,  0.2f));
    ExpressionPADMap.Add(TEXT("Curiosity"),     FVector( 0.4f,  0.4f,  0.2f));
    ExpressionPADMap.Add(TEXT("Wonder"),        FVector( 0.5f,  0.7f,  0.0f));
    ExpressionPADMap.Add(TEXT("Surprise"),      FVector( 0.1f,  0.9f,  0.0f));
    ExpressionPADMap.Add(TEXT("Contemplation"), FVector( 0.1f, -0.3f,  0.1f));
    ExpressionPADMap.Add(TEXT("Focused"),       FVector( 0.1f,  0.2f,  0.4f));
    ExpressionPADMap.Add(TEXT("Determination"), FVector( 0.2f,  0.5f,  0.7f));
    ExpressionPADMap.Add(TEXT("Concern"),       FVector(-0.4f,  0.4f, -0.2f));
    ExpressionPADMap.Add(TEXT("Sadness"),       FVector(-0.7f, -0.4f, -0.4f));
    ExpressionPADMap.Add(TEXT("Fear"),          FVector(-0.7f,  0.8f, -0.6f));
    ExpressionPADMap.Add(TEXT("Anger"),         FVector(-0.6f,  0.7f,  0.7f));
}

const FVector* UMetaHumanCNSBinding::FindPADForExpression(const FString& Name, FString& OutKey) const
{
    const FString Trimmed = Name.TrimStartAndEnd();
    for (const auto& Pair : ExpressionPADMap)
    {
        if (Pair.Key.Equals(Trimmed, ESearchCase::IgnoreCase))
        {
            OutKey = Pair.Key;
            return &Pair.Value;
        }
    }
    return nullptr;
}

FVector4 UMetaHumanCNSBinding::GetCognitiveVector() const
{
    if (!DTECore) { return FVector4(0.5f, 0.5f, 0.5f, 1.0f); }
    const F4ECognitionState& Cog4E = DTECore->CognitionState4E;
    return FVector4(Cog4E.MotorReadiness, Cog4E.EnactiveEngagement, Cog4E.ExtensionIntegration,
                    DTECore->RelevanceState.RelevanceCoherence);
}

FString UMetaHumanCNSBinding::GetReactionCharacterNameResolved() const
{
    if (!ReactionCharacterName.IsEmpty())
    {
        return ReactionCharacterName;
    }
    return GetOwner() ? GetOwner()->GetName() : FString(TEXT("Deep Tree Echo"));
}

FString UMetaHumanCNSBinding::BuildReactionPrompt(const FString& Trigger) const
{
    auto Num = [](double Value) { return FString::Printf(TEXT("%.2f"), Value); };

    TArray<FString> Expressions;
    ExpressionPADMap.GetKeys(Expressions);

    TMap<FString, FString> Slots;
    Slots.Add(TEXT("character"), GetReactionCharacterNameResolved());
    Slots.Add(TEXT("trigger"), Trigger);
    Slots.Add(TEXT("body_schema_coherence"), Num(BindingTelemetry.BodySchemaCoherence));
    Slots.Add(TEXT("current_expression"), CurrentReactionExpression.IsEmpty()
        ? FString(TEXT("a resting face"))
        : FString::Printf(TEXT("%s at intensity %.2f"), *CurrentReactionExpression, CurrentReactionIntensity));
    Slots.Add(TEXT("expressions"), FString::Join(Expressions, TEXT(", ")));

    if (DTECore)
    {
        const F4ECognitionState& Cog4E = DTECore->CognitionState4E;
        Slots.Add(TEXT("motor_readiness"), Num(Cog4E.MotorReadiness));
        Slots.Add(TEXT("enactive_engagement"), Num(Cog4E.EnactiveEngagement));
        Slots.Add(TEXT("extension_integration"), Num(Cog4E.ExtensionIntegration));
        Slots.Add(TEXT("environment_coupling"), Num(Cog4E.EnvironmentCoupling));
        Slots.Add(TEXT("niche"), Cog4E.CurrentNiche.IsEmpty() ? FString(TEXT("(none)")) : Cog4E.CurrentNiche);
        Slots.Add(TEXT("relevance_coherence"), Num(DTECore->RelevanceState.RelevanceCoherence));
        Slots.Add(TEXT("gestalt_coherence"), Num(DTECore->GetGestaltCoherence()));
    }
    else
    {
        for (const TCHAR* Key : { TEXT("motor_readiness"), TEXT("enactive_engagement"), TEXT("extension_integration"),
                                  TEXT("environment_coupling"), TEXT("niche"), TEXT("relevance_coherence"),
                                  TEXT("gestalt_coherence") })
        {
            Slots.Add(Key, TEXT("(unknown)"));
        }
    }

    return UEchoModelClient::FillPrompt(ReactionPromptTemplate, Slots);
}

bool UMetaHumanCNSBinding::RequestModelReaction(const FString& Trigger)
{
    if (bReactionRequestPending) { return false; }

    bReactionRequestPending = true;
    LastReactionRequestTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    CognitiveBaseline = GetCognitiveVector();
    bHasCognitiveBaseline = true;

    TArray<FEchoModelMessage> Messages;
    Messages.Add(FEchoModelMessage(TEXT("user"), BuildReactionPrompt(Trigger)));

    TWeakObjectPtr<UMetaHumanCNSBinding> WeakThis(this);
    UEchoModelClient::Get()->Chat(Messages, FOnEchoModelReply::CreateLambda(
        [WeakThis](bool bSuccess, const FString& TextOrError)
        {
            if (UMetaHumanCNSBinding* Self = WeakThis.Get())
            {
                Self->HandleModelReactionReply(bSuccess, TextOrError);
            }
        }), 0.7f);
    return true;
}

void UMetaHumanCNSBinding::HandleModelReactionReply(bool bSuccess, const FString& TextOrError)
{
    bReactionRequestPending = false;

    // On any failure the face stays as it is; the error is a system event only
    if (!bSuccess)
    {
        LastReactionError = TextOrError;
        UE_LOG(LogTemp, Error, TEXT("MetaHumanCNSBinding: no model reaction: %s"), *TextOrError);
        return;
    }

    TSharedPtr<FJsonObject> Json;
    FString Expression;
    double Intensity = 0.0;
    if (!UEchoModelClient::ParseJsonObject(TextOrError, Json) ||
        !Json->TryGetStringField(TEXT("expression"), Expression) ||
        !Json->TryGetNumberField(TEXT("intensity"), Intensity))
    {
        LastReactionError = FString::Printf(TEXT("model reply is not the requested JSON: %s"), *TextOrError.Left(200));
        UE_LOG(LogTemp, Error, TEXT("MetaHumanCNSBinding: %s"), *LastReactionError);
        return;
    }

    FString Key;
    if (!FindPADForExpression(Expression, Key))
    {
        LastReactionError = FString::Printf(TEXT("model chose an expression outside the offered list: %s"), *Expression);
        UE_LOG(LogTemp, Error, TEXT("MetaHumanCNSBinding: %s"), *LastReactionError);
        return;
    }

    LastReactionError.Reset();

    // Publish the model's reaction; every subscriber (including this component) renders it
    FEchoCharacterReaction Reaction;
    Reaction.CharacterName = GetReactionCharacterNameResolved();
    Reaction.Expression = Key;
    Reaction.Intensity = FMath::Clamp(static_cast<float>(Intensity), 0.0f, 1.0f);
    FEchoReactionBus::OnReaction().Broadcast(Reaction);
}

void UMetaHumanCNSBinding::HandleCharacterReaction(const FEchoCharacterReaction& Reaction)
{
    if (!ReactionCharacterName.IsEmpty() &&
        !Reaction.CharacterName.Equals(ReactionCharacterName, ESearchCase::IgnoreCase))
    {
        return;
    }

    InitialiseExpressionPADMap(); // no-op once populated

    // Empty or unmapped expression names leave the face unchanged
    FString Key;
    const FVector* PAD = FindPADForExpression(Reaction.Expression, Key);
    if (!PAD) { return; }

    const float Intensity = FMath::Clamp(Reaction.Intensity, 0.0f, 1.0f);
    CurrentReactionExpression = Key;
    CurrentReactionIntensity = Intensity;

    FCognitiveExpressionState NewState = ExpressionState; // keep recorded telemetry
    NewState.Valence   = FMath::Clamp(static_cast<float>(PAD->X) * Intensity, -1.0f, 1.0f);
    NewState.Arousal   = FMath::Clamp(static_cast<float>(PAD->Y) * Intensity, -1.0f, 1.0f);
    NewState.Dominance = FMath::Clamp(static_cast<float>(PAD->Z) * Intensity, -1.0f, 1.0f);
    SetExpressionState(NewState);
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

    // Render the PAD of the last model reaction
    MapPADToFACS(ExpressionState);
    ApplyFACSToBlendShapes();
    (void)DeltaTime; // blend shapes interpolate with the world delta in ApplyFACSToBlendShapes

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

    // Set AUs from emotional intensities
    // AU 1+2 = surprise brow raise
    SetAUActivation(1, FMath::Max(SurpriseIntensity, FearIntensity * 0.7f));
    SetAUActivation(2, SurpriseIntensity);
    // AU 4 = brow lowerer (anger)
    SetAUActivation(4, AngerIntensity);
    // AU 6 = cheek raise (happiness)
    SetAUActivation(6, HappyIntensity * 0.8f);
    // AU 7 = lid tightener (anger)
    SetAUActivation(7, AngerIntensity * 0.7f);
    // AU 12 = lip corner pull (smile)
    SetAUActivation(12, HappyIntensity);
    // AU 15 = lip corner depress (sadness)
    SetAUActivation(15, SadIntensity);
    // AU 17 = chin raise (sadness / contempt)
    SetAUActivation(17, SadIntensity * 0.4f);
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
// PRIVATE — SampleCognitiveExpression
// -------------------------------------------------------

void UMetaHumanCNSBinding::SampleCognitiveExpression()
{
    if (!DTECore) { return; }

    // Telemetry only: cognitive load and flow are recorded, they do not set valence/arousal/dominance
    ExpressionState.CognitiveLoad = FMath::Clamp(1.0f - DTECore->RelevanceState.RelevanceCoherence, 0.0f, 1.0f);
    ExpressionState.FlowIndex = FMath::Clamp(DTECore->GetGestaltCoherence(), 0.0f, 1.0f);

    // A large shift in the 4E / coherence state is a cognition event: ask the model how the character reacts
    const FVector4 Current = GetCognitiveVector();
    if (!bHasCognitiveBaseline)
    {
        CognitiveBaseline = Current;
        bHasCognitiveBaseline = true;
        return;
    }

    if (!bRequestReactionOnCognitiveShift || bReactionRequestPending) { return; }

    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    if (Now - LastReactionRequestTime < MinSecondsBetweenReactionRequests) { return; }

    static const TCHAR* DimensionNames[4] = {
        TEXT("motor readiness"), TEXT("enactive engagement"), TEXT("extension integration"), TEXT("relevance coherence") };
    int32 Largest = 0;
    double LargestDelta = 0.0;
    for (int32 i = 0; i < 4; ++i)
    {
        const double Delta = FMath::Abs(Current[i] - CognitiveBaseline[i]);
        if (Delta > LargestDelta) { LargestDelta = Delta; Largest = i; }
    }

    if (LargestDelta >= CognitiveShiftThreshold)
    {
        RequestModelReaction(FString::Printf(TEXT("your %s shifted from %.2f to %.2f"),
            DimensionNames[Largest], CognitiveBaseline[Largest], Current[Largest]));
    }
}
