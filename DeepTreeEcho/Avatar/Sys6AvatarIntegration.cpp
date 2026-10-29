// Sys6AvatarIntegration.cpp
// Implementation of sys6 operad integration with Unreal Engine avatar

#include "Sys6AvatarIntegration.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Math/UnrealMathUtility.h"
#include "EchoModelClient.h"
#include "Dom/JsonObject.h"

/** One channel's validated model expression (local to this file) */
struct FSys6ChosenExpression { int32 Index; float Intensity; float Valence; float Arousal; };

namespace
{
    const TCHAR* ExpressionChannelName(EAvatarExpressionChannel Channel)
    {
        switch (Channel)
        {
        case EAvatarExpressionChannel::Facial:    return TEXT("facial");
        case EAvatarExpressionChannel::Body:      return TEXT("body");
        case EAvatarExpressionChannel::Gesture:   return TEXT("gesture");
        case EAvatarExpressionChannel::Gaze:      return TEXT("gaze");
        case EAvatarExpressionChannel::Posture:   return TEXT("posture");
        case EAvatarExpressionChannel::Breathing: return TEXT("breathing");
        case EAvatarExpressionChannel::Voice:     return TEXT("voice");
        case EAvatarExpressionChannel::Aura:      return TEXT("aura");
        default:                                  return TEXT("unknown");
        }
    }

    // Smaller models sometimes quote numbers ("0.5"); accept a numeric string as the same value.
    bool ReadLenientNumber(const TSharedPtr<FJsonObject>& Json, const TCHAR* Field, double& Out)
    {
        if (Json->TryGetNumberField(Field, Out))
        {
            return true;
        }
        FString Text;
        if (Json->TryGetStringField(Field, Text) && Text.TrimStartAndEnd().IsNumeric())
        {
            Out = FCString::Atod(*Text.TrimStartAndEnd());
            return true;
        }
        return false;
    }
}

USys6AvatarIntegration::USys6AvatarIntegration()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;

    ExpressionPromptTemplate = TEXT(
        "You are Deep Tree Echo, an embodied avatar. Decide how your body expresses your current state.\n"
        "What is happening: {context}\n"
        "Cognitive cycle: {sys6}. Active 4E dimension: {dimension}. Coherence {coherence}, purposefulness {entelechy}.\n"
        "Sensorimotor coupling strengths: {couplings}.\n"
        "Current expression per channel: {current}.\n"
        "For each of these channels: {channels} - choose intensity (0 to 1), valence (-1 negative to 1 positive) and arousal (0 calm to 1 excited).\n"
        "Reply with only a JSON object mapping each channel name to its values, for example "
        "{\"facial\": {\"intensity\": 0.4, \"valence\": 0.2, \"arousal\": 0.3}, ...}");
}

void USys6AvatarIntegration::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
    InitializeIntegration();
}

void USys6AvatarIntegration::TickComponent(float DeltaTime, ELevelTick TickType,
                                            FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bEnableIntegration)
    {
        return;
    }

    SecondsSinceExpressionRequest += DeltaTime;

    // Update expression states
    ExpressionTimer += DeltaTime;
    float ExpressionInterval = 1.0f / ExpressionUpdateRate;
    if (ExpressionTimer >= ExpressionInterval)
    {
        ExpressionTimer -= ExpressionInterval;
        UpdateExpressionStates(ExpressionInterval);
    }

    // Update 4E dimension
    if (bEnable4ECycling)
    {
        Update4EDimension(DeltaTime);
    }

    // Process current 4E dimension
    switch (CognitiveState.ActiveDimension)
    {
    case E4EAvatarDimension::Embodied:
        ProcessEmbodiedCognition(DeltaTime);
        break;
    case E4EAvatarDimension::Embedded:
        ProcessEmbeddedCognition(DeltaTime);
        break;
    case E4EAvatarDimension::Enacted:
        ProcessEnactedCognition(DeltaTime);
        break;
    case E4EAvatarDimension::Extended:
        ProcessExtendedCognition(DeltaTime);
        break;
    }

    // Sync with sys6
    SyncWithSys6();

    // Compute coherence and entelechy
    ComputeCoherence();
    ComputeEntelechy();
}

void USys6AvatarIntegration::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        Sys6Engine = Owner->FindComponentByClass<USys6OperadEngine>();
        CognitiveBridge = Owner->FindComponentByClass<USys6CognitiveBridge>();
        SkeletalMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();

        // Bind to sys6 events
        if (Sys6Engine)
        {
            Sys6Engine->OnStepAdvanced.AddDynamic(this, &USys6AvatarIntegration::HandleSys6StepAdvanced);
            Sys6Engine->OnStageChanged.AddDynamic(this, &USys6AvatarIntegration::HandleSys6StageChanged);
            Sys6Engine->OnDyadChanged.AddDynamic(this, &USys6AvatarIntegration::HandleSys6DyadChanged);
            Sys6Engine->OnTriadChanged.AddDynamic(this, &USys6AvatarIntegration::HandleSys6TriadChanged);
        }
    }
}

void USys6AvatarIntegration::InitializeIntegration()
{
    InitializeExpressionStates();
    InitializeBodySchema();
    InitializeSensorimotorCouplings();

    CognitiveState.Sys6Step = 1;
    CognitiveState.CognitiveStep = 1;
    CognitiveState.ActiveDimension = E4EAvatarDimension::Embodied;
    CognitiveState.Coherence = 1.0f;
    CognitiveState.EntelechyLevel = 0.5f;

    ExpressionTimer = 0.0f;
    DimensionTimer = 0.0f;
}

void USys6AvatarIntegration::InitializeExpressionStates()
{
    CognitiveState.ExpressionStates.Empty();

    // Initialize all expression channels
    TArray<EAvatarExpressionChannel> Channels = {
        EAvatarExpressionChannel::Facial,
        EAvatarExpressionChannel::Body,
        EAvatarExpressionChannel::Gesture,
        EAvatarExpressionChannel::Gaze,
        EAvatarExpressionChannel::Posture,
        EAvatarExpressionChannel::Breathing,
        EAvatarExpressionChannel::Aura
    };

    for (EAvatarExpressionChannel Channel : Channels)
    {
        FSys6ExpressionState State;
        State.Channel = Channel;
        // Neutral until the model chooses this channel's expression
        State.Intensity = 0.0f;
        State.Valence = 0.0f;
        State.Arousal = 0.0f;
        State.bModelChosen = false;
        State.DyadicInfluence = 0.0f;
        State.TriadicInfluence = 0.0f;
        State.StageInfluence = 0.0f;
        CognitiveState.ExpressionStates.Add(State);
    }
}

void USys6AvatarIntegration::InitializeBodySchema()
{
    BodySchema.Empty();

    if (!SkeletalMesh)
    {
        return;
    }

    // Get bone names from skeleton
    TArray<FName> BoneNames;
    SkeletalMesh->GetBoneNames(BoneNames);

    // Create body schema elements for key bones
    TArray<FName> KeyBones = {
        FName("root"),
        FName("pelvis"),
        FName("spine_01"),
        FName("spine_02"),
        FName("spine_03"),
        FName("neck_01"),
        FName("head"),
        FName("clavicle_l"),
        FName("clavicle_r"),
        FName("upperarm_l"),
        FName("upperarm_r"),
        FName("lowerarm_l"),
        FName("lowerarm_r"),
        FName("hand_l"),
        FName("hand_r"),
        FName("thigh_l"),
        FName("thigh_r"),
        FName("calf_l"),
        FName("calf_r"),
        FName("foot_l"),
        FName("foot_r")
    };

    int32 ThreadID = 0;
    for (const FName& BoneName : KeyBones)
    {
        if (BoneNames.Contains(BoneName))
        {
            FAvatarBodySchemaElement Element;
            Element.BoneName = BoneName;
            Element.CurrentTransform = SkeletalMesh->GetBoneTransform(SkeletalMesh->GetBoneIndex(BoneName));
            Element.TargetTransform = Element.CurrentTransform;
            Element.ProprioceptiveFeedback = FVector::ZeroVector;
            
            // Assign 4E dimension based on body part
            if (BoneName.ToString().Contains("head") || BoneName.ToString().Contains("neck"))
            {
                Element.Dimension = E4EAvatarDimension::Embodied;
            }
            else if (BoneName.ToString().Contains("spine"))
            {
                Element.Dimension = E4EAvatarDimension::Embedded;
            }
            else if (BoneName.ToString().Contains("arm") || BoneName.ToString().Contains("hand"))
            {
                Element.Dimension = E4EAvatarDimension::Extended;
            }
            else
            {
                Element.Dimension = E4EAvatarDimension::Enacted;
            }

            // Assign sys6 thread (cycle through 8 threads)
            Element.Sys6ThreadID = ThreadID % 8;
            ThreadID++;

            BodySchema.Add(Element);
        }
    }
}

void USys6AvatarIntegration::InitializeSensorimotorCouplings()
{
    SensorimotorCouplings.Empty();

    // Visual-Motor coupling
    FAvatarSensorimotorCoupling VisualMotor;
    VisualMotor.SensoryChannel = TEXT("Visual");
    VisualMotor.MotorChannel = TEXT("Gaze");
    VisualMotor.CouplingStrength = 0.8f;
    VisualMotor.LatencySteps = 1;
    VisualMotor.ConvolutionKernel = 0;
    SensorimotorCouplings.Add(VisualMotor);

    // Auditory-Orientation coupling
    FAvatarSensorimotorCoupling AuditoryOrientation;
    AuditoryOrientation.SensoryChannel = TEXT("Auditory");
    AuditoryOrientation.MotorChannel = TEXT("HeadTurn");
    AuditoryOrientation.CouplingStrength = 0.6f;
    AuditoryOrientation.LatencySteps = 2;
    AuditoryOrientation.ConvolutionKernel = 1;
    SensorimotorCouplings.Add(AuditoryOrientation);

    // Proprioceptive-Posture coupling
    FAvatarSensorimotorCoupling ProprioceptivePosture;
    ProprioceptivePosture.SensoryChannel = TEXT("Proprioceptive");
    ProprioceptivePosture.MotorChannel = TEXT("Posture");
    ProprioceptivePosture.CouplingStrength = 0.9f;
    ProprioceptivePosture.LatencySteps = 1;
    ProprioceptivePosture.ConvolutionKernel = 2;
    SensorimotorCouplings.Add(ProprioceptivePosture);

    // Interoceptive-Breathing coupling
    FAvatarSensorimotorCoupling InteroceptiveBreathing;
    InteroceptiveBreathing.SensoryChannel = TEXT("Interoceptive");
    InteroceptiveBreathing.MotorChannel = TEXT("Breathing");
    InteroceptiveBreathing.CouplingStrength = 0.7f;
    InteroceptiveBreathing.LatencySteps = 3;
    InteroceptiveBreathing.ConvolutionKernel = 3;
    SensorimotorCouplings.Add(InteroceptiveBreathing);

    // Emotional-Facial coupling
    FAvatarSensorimotorCoupling EmotionalFacial;
    EmotionalFacial.SensoryChannel = TEXT("Emotional");
    EmotionalFacial.MotorChannel = TEXT("Facial");
    EmotionalFacial.CouplingStrength = 0.85f;
    EmotionalFacial.LatencySteps = 1;
    EmotionalFacial.ConvolutionKernel = 4;
    SensorimotorCouplings.Add(EmotionalFacial);

    // Social-Gesture coupling
    FAvatarSensorimotorCoupling SocialGesture;
    SocialGesture.SensoryChannel = TEXT("Social");
    SocialGesture.MotorChannel = TEXT("Gesture");
    SocialGesture.CouplingStrength = 0.5f;
    SocialGesture.LatencySteps = 2;
    SocialGesture.ConvolutionKernel = 5;
    SensorimotorCouplings.Add(SocialGesture);
}

void USys6AvatarIntegration::UpdateExpressionStates(float DeltaTime)
{
    // Clock influences: measured sys6 state, kept as model input only
    if (Sys6Engine)
    {
        const FSys6FullState Sys6State = Sys6Engine->GetFullState();
        const float DyadicInfluence = MapDyadicToExpression(Sys6State.DyadicPhase);
        const float TriadicInfluence = MapTriadicToExpression(Sys6State.TriadicPhase);
        const float StageInfluence = static_cast<float>(static_cast<int32>(Sys6State.PentadicStage)) / 4.0f;
        for (FSys6ExpressionState& ExprState : CognitiveState.ExpressionStates)
        {
            ExprState.DyadicInfluence = FMath::Lerp(ExprState.DyadicInfluence, DyadicInfluence, ExpressionSmoothing);
            ExprState.TriadicInfluence = FMath::Lerp(ExprState.TriadicInfluence, TriadicInfluence, ExpressionSmoothing);
            ExprState.StageInfluence = FMath::Lerp(ExprState.StageInfluence, StageInfluence, ExpressionSmoothing);
        }
    }

    // The expression itself: ease each channel toward the model's choice. Channels the model has
    // not chosen stay neutral and broadcast nothing.
    for (FSys6ExpressionState& ExprState : CognitiveState.ExpressionStates)
    {
        if (!ExprState.bModelChosen)
        {
            continue;
        }
        const float OldIntensity = ExprState.Intensity;
        const float OldValence = ExprState.Valence;
        const float OldArousal = ExprState.Arousal;
        ExprState.Intensity = FMath::Lerp(ExprState.Intensity, ExprState.TargetIntensity, ExpressionSmoothing);
        ExprState.Valence = FMath::Lerp(ExprState.Valence, ExprState.TargetValence, ExpressionSmoothing);
        ExprState.Arousal = FMath::Lerp(ExprState.Arousal, ExprState.TargetArousal, ExpressionSmoothing);
        if (FMath::Abs(ExprState.Intensity - OldIntensity) > KINDA_SMALL_NUMBER
            || FMath::Abs(ExprState.Valence - OldValence) > KINDA_SMALL_NUMBER
            || FMath::Abs(ExprState.Arousal - OldArousal) > KINDA_SMALL_NUMBER)
        {
            OnExpressionChanged.Broadcast(ExprState.Channel, ExprState.Intensity);
        }
    }
}

bool USys6AvatarIntegration::HasModelExpression() const
{
    for (const FSys6ExpressionState& ExprState : CognitiveState.ExpressionStates)
    {
        if (ExprState.bModelChosen) { return true; }
    }
    return false;
}

bool USys6AvatarIntegration::RequestModelExpression()
{
    if (CognitiveState.ExpressionStates.Num() == 0)
    {
        FailExpressionRequest(TEXT("no expression channels (InitializeIntegration has not run)"));
        return false;
    }
    if (ExpressionPromptTemplate.IsEmpty())
    {
        FailExpressionRequest(TEXT("ExpressionPromptTemplate is empty"));
        return false;
    }
    if (bExpressionRequestInFlight)
    {
        return false; // the previous request will answer
    }

    FString Sys6Text = FString::Printf(TEXT("step %d, cognitive step %d"), CognitiveState.Sys6Step, CognitiveState.CognitiveStep);
    if (Sys6Engine)
    {
        const FSys6FullState Sys6State = Sys6Engine->GetFullState();
        Sys6Text += FString::Printf(TEXT(", dyadic phase %s, triadic phase %d, stage %d"),
            Sys6State.DyadicPhase == EDyadicPhase::A ? TEXT("A") : TEXT("B"),
            static_cast<int32>(Sys6State.TriadicPhase) + 1, static_cast<int32>(Sys6State.PentadicStage) + 1);
    }

    static const TCHAR* DimensionNames[] = { TEXT("embodied"), TEXT("embedded"), TEXT("enacted"), TEXT("extended") };
    const int32 DimIndex = FMath::Clamp(static_cast<int32>(CognitiveState.ActiveDimension), 0, 3);

    FString Couplings;
    for (const FAvatarSensorimotorCoupling& Coupling : SensorimotorCouplings)
    {
        Couplings += FString::Printf(TEXT("%s%s->%s %.2f"), Couplings.IsEmpty() ? TEXT("") : TEXT(", "),
            *Coupling.SensoryChannel, *Coupling.MotorChannel, Coupling.CouplingStrength);
    }

    TArray<FString> Channels;
    FString Current;
    for (const FSys6ExpressionState& ExprState : CognitiveState.ExpressionStates)
    {
        const FString Name = ExpressionChannelName(ExprState.Channel);
        Channels.Add(Name);
        Current += ExprState.bModelChosen
            ? FString::Printf(TEXT("%s%s intensity %.2f valence %+.2f arousal %.2f"), Current.IsEmpty() ? TEXT("") : TEXT("; "),
                *Name, ExprState.Intensity, ExprState.Valence, ExprState.Arousal)
            : FString::Printf(TEXT("%s%s neutral"), Current.IsEmpty() ? TEXT("") : TEXT("; "), *Name);
    }

    TMap<FString, FString> Slots;
    Slots.Add(TEXT("context"), ExpressionContext.IsEmpty() ? FString(TEXT("(nothing reported by the game)")) : ExpressionContext);
    Slots.Add(TEXT("sys6"), Sys6Text);
    Slots.Add(TEXT("dimension"), DimensionNames[DimIndex]);
    Slots.Add(TEXT("coherence"), FString::Printf(TEXT("%.2f"), CognitiveState.Coherence));
    Slots.Add(TEXT("entelechy"), FString::Printf(TEXT("%.2f"), CognitiveState.EntelechyLevel));
    Slots.Add(TEXT("couplings"), Couplings.IsEmpty() ? FString(TEXT("(none)")) : Couplings);
    Slots.Add(TEXT("current"), Current);
    Slots.Add(TEXT("channels"), FString::Join(Channels, TEXT(", ")));

    TArray<FEchoModelMessage> Messages;
    Messages.Add(FEchoModelMessage(TEXT("user"), UEchoModelClient::FillPrompt(ExpressionPromptTemplate, Slots)));

    bExpressionRequestInFlight = true;
    SecondsSinceExpressionRequest = 0.0f;
    TWeakObjectPtr<USys6AvatarIntegration> WeakThis(this);
    UEchoModelClient::Get()->Chat(Messages, FOnEchoModelReply::CreateLambda(
        [WeakThis](bool bSuccess, const FString& TextOrError)
        {
            if (USys6AvatarIntegration* Self = WeakThis.Get())
            {
                Self->HandleExpressionReply(bSuccess, TextOrError);
            }
        }), 0.6f);
    return true;
}

void USys6AvatarIntegration::HandleExpressionReply(bool bSuccess, const FString& TextOrError)
{
    bExpressionRequestInFlight = false;
    if (!bSuccess)
    {
        FailExpressionRequest(TextOrError);
        return;
    }

    TSharedPtr<FJsonObject> Json;
    if (!UEchoModelClient::ParseJsonObject(TextOrError, Json) || !Json.IsValid())
    {
        FailExpressionRequest(FString::Printf(TEXT("model reply is not the requested JSON: %s"), *TextOrError.Left(200)));
        return;
    }

    // Validate every channel first; apply only the channels the model answered validly
    TArray<FSys6ChosenExpression> Chosen;
    TArray<FString> Rejected;
    for (int32 i = 0; i < CognitiveState.ExpressionStates.Num(); ++i)
    {
        const FString Name = ExpressionChannelName(CognitiveState.ExpressionStates[i].Channel);
        const TSharedPtr<FJsonObject>* ChannelJson = nullptr;
        for (const auto& Field : Json->Values)
        {
            if (Field.Key.Equals(Name, ESearchCase::IgnoreCase) && Field.Value.IsValid())
            {
                Field.Value->TryGetObject(ChannelJson);
                break;
            }
        }
        if (!ChannelJson || !ChannelJson->IsValid())
        {
            continue;
        }
        double Intensity = 0.0, Valence = 0.0, Arousal = 0.0;
        if (!ReadLenientNumber(*ChannelJson, TEXT("intensity"), Intensity)
            || !ReadLenientNumber(*ChannelJson, TEXT("valence"), Valence)
            || !ReadLenientNumber(*ChannelJson, TEXT("arousal"), Arousal)
            || Intensity < 0.0 || Intensity > 1.0 || Valence < -1.0 || Valence > 1.0 || Arousal < 0.0 || Arousal > 1.0)
        {
            Rejected.Add(Name);
            continue;
        }
        Chosen.Add({ i, static_cast<float>(Intensity), static_cast<float>(Valence), static_cast<float>(Arousal) });
    }

    if (Chosen.Num() == 0)
    {
        FailExpressionRequest(FString::Printf(TEXT("model reply chose no valid channel expression: %s"), *TextOrError.Left(200)));
        return;
    }
    if (Rejected.Num() > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Sys6AvatarIntegration: model values out of range or missing for: %s"), *FString::Join(Rejected, TEXT(", ")));
    }

    LastExpressionError.Reset();
    for (const FSys6ChosenExpression& C : Chosen)
    {
        FSys6ExpressionState& ExprState = CognitiveState.ExpressionStates[C.Index];
        ExprState.bModelChosen = true;
        ExprState.TargetIntensity = C.Intensity;
        ExprState.TargetValence = C.Valence;
        ExprState.TargetArousal = C.Arousal;
    }
}

void USys6AvatarIntegration::FailExpressionRequest(const FString& Error)
{
    LastExpressionError = Error;
    UE_LOG(LogTemp, Error, TEXT("Sys6AvatarIntegration: no model expression (unchanged): %s"), *Error);
    OnExpressionRequestFailed.Broadcast(Error);
}

void USys6AvatarIntegration::Update4EDimension(float DeltaTime)
{
    DimensionTimer += DeltaTime;

    if (DimensionTimer >= DimensionCycleDuration / 4.0f)
    {
        DimensionTimer = 0.0f;

        // Cycle to next dimension
        E4EAvatarDimension OldDimension = CognitiveState.ActiveDimension;
        int32 NextDimension = (static_cast<int32>(OldDimension) + 1) % 4;
        CognitiveState.ActiveDimension = static_cast<E4EAvatarDimension>(NextDimension);

        On4EDimensionChanged.Broadcast(OldDimension, CognitiveState.ActiveDimension);
    }
}

void USys6AvatarIntegration::SyncWithSys6()
{
    if (Sys6Engine)
    {
        CognitiveState.Sys6Step = Sys6Engine->GetCurrentStep();
    }

    if (CognitiveBridge)
    {
        FSys6BridgeState BridgeState = CognitiveBridge->GetBridgeState();
        CognitiveState.CognitiveStep = BridgeState.CognitiveStep;
    }

    OnSys6Sync.Broadcast(CognitiveState.Sys6Step);
}

void USys6AvatarIntegration::ProcessEmbodiedCognition(float DeltaTime)
{
    // Update body schema from skeletal mesh
    if (!SkeletalMesh)
    {
        return;
    }

    for (FAvatarBodySchemaElement& Element : BodySchema)
    {
        if (Element.Dimension == E4EAvatarDimension::Embodied)
        {
            int32 BoneIndex = SkeletalMesh->GetBoneIndex(Element.BoneName);
            if (BoneIndex != INDEX_NONE)
            {
                FTransform NewTransform = SkeletalMesh->GetBoneTransform(BoneIndex);
                
                // Compute proprioceptive feedback (velocity)
                FVector PositionDelta = NewTransform.GetLocation() - Element.CurrentTransform.GetLocation();
                Element.ProprioceptiveFeedback = PositionDelta / DeltaTime;

                Element.CurrentTransform = NewTransform;
            }
        }
    }
}

void USys6AvatarIntegration::ProcessEmbeddedCognition(float DeltaTime)
{
    // Process environmental affordances
    // This would typically involve raycasting and spatial queries
    // For now, update body schema elements associated with embedded dimension

    for (FAvatarBodySchemaElement& Element : BodySchema)
    {
        if (Element.Dimension == E4EAvatarDimension::Embedded)
        {
            // Apply environmental influence to target transform
            // This is a placeholder for more sophisticated environmental processing
        }
    }
}

void USys6AvatarIntegration::ProcessEnactedCognition(float DeltaTime)
{
    // Process sensorimotor contingencies
    if (!Sys6Engine)
    {
        return;
    }

    FTriadicConvolutionState ConvState = Sys6Engine->GetConvolutionState();

    for (FAvatarSensorimotorCoupling& Coupling : SensorimotorCouplings)
    {
        // Apply convolution kernel influence
        if (Coupling.ConvolutionKernel < ConvState.PhaseStates.Num())
        {
            float KernelWeight = ConvState.PhaseStates[Coupling.ConvolutionKernel];
            Coupling.CouplingStrength = FMath::Lerp(Coupling.CouplingStrength, KernelWeight, 0.1f);
        }
    }
}

void USys6AvatarIntegration::ProcessExtendedCognition(float DeltaTime)
{
    // Process tool use and extended cognition
    // Update body schema elements associated with hands/tools

    for (FAvatarBodySchemaElement& Element : BodySchema)
    {
        if (Element.Dimension == E4EAvatarDimension::Extended)
        {
            // Apply tool integration influence
            // This would involve tracking tool state and integrating it into body schema
        }
    }
}

void USys6AvatarIntegration::ComputeCoherence()
{
    // Compute overall coherence from multiple factors
    float ExpressionCoherence = 0.0f;
    for (const FSys6ExpressionState& ExprState : CognitiveState.ExpressionStates)
    {
        ExpressionCoherence += ExprState.Intensity;
    }
    ExpressionCoherence /= FMath::Max(1, CognitiveState.ExpressionStates.Num());

    // Body schema coherence
    float BodyCoherence = 1.0f;
    if (BodySchema.Num() > 0)
    {
        float TotalFeedback = 0.0f;
        for (const FAvatarBodySchemaElement& Element : BodySchema)
        {
            TotalFeedback += Element.ProprioceptiveFeedback.Size();
        }
        // Lower feedback = higher coherence (stable body)
        BodyCoherence = 1.0f / (1.0f + TotalFeedback * 0.001f);
    }

    // Bridge coherence
    float BridgeCoherence = 1.0f;
    if (CognitiveBridge)
    {
        BridgeCoherence = CognitiveBridge->GetBridgeCoherence();
    }

    // Combine coherences
    CognitiveState.Coherence = (ExpressionCoherence + BodyCoherence + BridgeCoherence) / 3.0f;
}

void USys6AvatarIntegration::ComputeEntelechy()
{
    // Entelechy = purposeful direction
    // Based on alignment between sys6 state and avatar expression

    if (!Sys6Engine)
    {
        CognitiveState.EntelechyLevel = 0.5f;
        return;
    }

    FSys6FullState Sys6State = Sys6Engine->GetFullState();

    // Entelechy increases with:
    // - Higher stage (more developed purpose)
    // - Lower entanglement (clearer direction)
    // - Higher coherence

    float StageContribution = static_cast<float>(static_cast<int32>(Sys6State.PentadicStage) + 1) / 5.0f;
    float EntanglementContribution = 1.0f - Sys6State.CubicState.EntanglementLevel;
    float CoherenceContribution = CognitiveState.Coherence;

    CognitiveState.EntelechyLevel = (StageContribution + EntanglementContribution + CoherenceContribution) / 3.0f;
}

// Expression state queries
FSys6ExpressionState USys6AvatarIntegration::GetExpressionState(EAvatarExpressionChannel Channel) const
{
    for (const FSys6ExpressionState& State : CognitiveState.ExpressionStates)
    {
        if (State.Channel == Channel)
        {
            return State;
        }
    }
    return FSys6ExpressionState();
}

void USys6AvatarIntegration::SetExpressionIntensity(EAvatarExpressionChannel Channel, float Intensity)
{
    for (FSys6ExpressionState& State : CognitiveState.ExpressionStates)
    {
        if (State.Channel == Channel)
        {
            State.Intensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
            OnExpressionChanged.Broadcast(Channel, State.Intensity);
            return;
        }
    }
}

void USys6AvatarIntegration::SetExpressionValence(EAvatarExpressionChannel Channel, float Valence)
{
    for (FSys6ExpressionState& State : CognitiveState.ExpressionStates)
    {
        if (State.Channel == Channel)
        {
            State.Valence = FMath::Clamp(Valence, -1.0f, 1.0f);
            return;
        }
    }
}

void USys6AvatarIntegration::ApplySys6ToExpression()
{
    // Refresh clock influences and ease toward the model's chosen expression
    UpdateExpressionStates(1.0f / ExpressionUpdateRate);
}

// 4E dimension methods
E4EAvatarDimension USys6AvatarIntegration::GetActive4EDimension() const
{
    return CognitiveState.ActiveDimension;
}

void USys6AvatarIntegration::SetActive4EDimension(E4EAvatarDimension Dimension)
{
    E4EAvatarDimension OldDimension = CognitiveState.ActiveDimension;
    CognitiveState.ActiveDimension = Dimension;
    On4EDimensionChanged.Broadcast(OldDimension, Dimension);
}

// Sys6 mapping methods
float USys6AvatarIntegration::MapDyadicToExpression(EDyadicPhase Phase) const
{
    // A = 0.7 (positive/active), B = 0.3 (negative/passive)
    return (Phase == EDyadicPhase::A) ? 0.7f : 0.3f;
}

float USys6AvatarIntegration::MapTriadicToExpression(ETriadicPhase Phase) const
{
    // Phase 1 = 0.33, Phase 2 = 0.66, Phase 3 = 1.0
    return (static_cast<float>(static_cast<int32>(Phase)) + 1.0f) / 3.0f;
}

E4EAvatarDimension USys6AvatarIntegration::MapStageToE4Dimension(EPentadicStage Stage) const
{
    // Map 5 stages to 4 dimensions (stage 5 returns to embodied)
    switch (Stage)
    {
    case EPentadicStage::Stage1: return E4EAvatarDimension::Embodied;
    case EPentadicStage::Stage2: return E4EAvatarDimension::Embedded;
    case EPentadicStage::Stage3: return E4EAvatarDimension::Enacted;
    case EPentadicStage::Stage4: return E4EAvatarDimension::Extended;
    case EPentadicStage::Stage5: return E4EAvatarDimension::Embodied;
    default: return E4EAvatarDimension::Embodied;
    }
}

void USys6AvatarIntegration::MapCubicToBodySchema(const FCubicConcurrencyState& CubicState)
{
    // Map 8 cubic threads to body schema elements
    for (FAvatarBodySchemaElement& Element : BodySchema)
    {
        if (Element.Sys6ThreadID < CubicState.ThreadStates.Num())
        {
            float ThreadWeight = CubicState.ThreadStates[Element.Sys6ThreadID];
            // Apply thread weight to target transform (scale influence)
            // This is a simplified mapping - full implementation would involve
            // more sophisticated transform blending
        }
    }
}

void USys6AvatarIntegration::MapConvolutionToSensorimotor(const FTriadicConvolutionState& ConvolutionState)
{
    // Map 9 convolution kernels to sensorimotor couplings
    for (FAvatarSensorimotorCoupling& Coupling : SensorimotorCouplings)
    {
        if (Coupling.ConvolutionKernel < ConvolutionState.PhaseStates.Num())
        {
            Coupling.CouplingStrength = ConvolutionState.PhaseStates[Coupling.ConvolutionKernel];
        }
    }
}

// State queries
FSys6AvatarCognitiveState USys6AvatarIntegration::GetCognitiveState() const
{
    return CognitiveState;
}

FAvatarBodySchemaElement USys6AvatarIntegration::GetBodySchemaElement(FName BoneName) const
{
    for (const FAvatarBodySchemaElement& Element : BodySchema)
    {
        if (Element.BoneName == BoneName)
        {
            return Element;
        }
    }
    return FAvatarBodySchemaElement();
}

float USys6AvatarIntegration::GetCoherence() const
{
    return CognitiveState.Coherence;
}

float USys6AvatarIntegration::GetEntelechyLevel() const
{
    return CognitiveState.EntelechyLevel;
}

// Event handlers
void USys6AvatarIntegration::HandleSys6StepAdvanced(int32 OldStep, int32 NewStep)
{
    CognitiveState.Sys6Step = NewStep;
    ApplySys6ToExpression();

    // The clock schedules when to ask; the model chooses what the expression is
    if (!bExpressionRequestInFlight && SecondsSinceExpressionRequest >= ExpressionRequestInterval)
    {
        RequestModelExpression();
    }
}

void USys6AvatarIntegration::HandleSys6StageChanged(EPentadicStage OldStage, EPentadicStage NewStage)
{
    // Map stage to 4E dimension
    E4EAvatarDimension NewDimension = MapStageToE4Dimension(NewStage);
    if (NewDimension != CognitiveState.ActiveDimension)
    {
        SetActive4EDimension(NewDimension);
    }
}

void USys6AvatarIntegration::HandleSys6DyadChanged(EDyadicPhase OldPhase, EDyadicPhase NewPhase)
{
    // The dyadic phase is clock state (model input via UpdateExpressionStates); it sets no expression
}

void USys6AvatarIntegration::HandleSys6TriadChanged(ETriadicPhase OldPhase, ETriadicPhase NewPhase)
{
    // The triadic phase is clock state (model input via UpdateExpressionStates); it sets no expression
}
