// UnrealAvatarCognition.cpp
// Implementation of Unreal Engine avatar cognitive integration

#include "UnrealAvatarCognition.h"
#include "../Model/EchoCharacterReaction.h"
#include "../Model/EchoModelClient.h"
#include "Dom/JsonObject.h"
#include "Math/UnrealMathUtility.h"

namespace
{
    /** Map a model-chosen name to an enum value (by name or display name, case-insensitive). */
    template <typename TEnum>
    bool ParseAvatarCognitionEnum(const FString& Name, TEnum& OutValue)
    {
        const FString Trimmed = Name.TrimStartAndEnd();
        const UEnum* Enum = StaticEnum<TEnum>();
        if (Trimmed.IsEmpty() || !Enum)
        {
            return false;
        }

        // NumEnums() includes the generated _MAX entry
        for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
        {
            if (Enum->GetNameStringByIndex(Index).Equals(Trimmed, ESearchCase::IgnoreCase) ||
                Enum->GetDisplayNameTextByIndex(Index).ToString().Equals(Trimmed, ESearchCase::IgnoreCase))
            {
                OutValue = static_cast<TEnum>(Enum->GetValueByIndex(Index));
                return true;
            }
        }
        return false;
    }

    /** All names of an enum (without the generated _MAX). */
    template <typename TEnum>
    TArray<FString> AvatarCognitionEnumNames()
    {
        TArray<FString> Names;
        if (const UEnum* Enum = StaticEnum<TEnum>())
        {
            for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
            {
                Names.Add(Enum->GetNameStringByIndex(Index));
            }
        }
        return Names;
    }
}

UUnrealAvatarCognition::UUnrealAvatarCognition()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;

    // Prompt template (model input): the model chooses the reaction, the slots carry the real state
    ReactionPromptTemplate = TEXT(
        "You are {character}, an embodied cognitive agent. Event: {trigger}.\n"
        "Cognitive state: coherence {coherence}, entelechy (purposeful direction) {entelechy}, "
        "4E cognition embodied {embodied}, embedded {embedded}, enacted {enacted}, extended {extended}; "
        "latest relevance realization {relevance}; echobeat step {echobeat_step} of 12; sys6 step {sys6_step}. "
        "You currently show: {current_emotion}.\n"
        "Choose the emotion your face and body show now, from exactly this list: {emotions}. "
        "Reply with JSON only: {\"emotion\": \"<one name from the list>\", \"intensity\": <number 0..1>}");
}

void UUnrealAvatarCognition::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
    InitializeDefaultMappings();

    // Initialize 4E state
    State.E4State.Add(TEXT("Embodied"), 0.5f);
    State.E4State.Add(TEXT("Embedded"), 0.5f);
    State.E4State.Add(TEXT("Enacted"), 0.5f);
    State.E4State.Add(TEXT("Extended"), 0.5f);

    // Resting body until the model chooses an emotion
    ReactionBodySchema = GetEmotionBodySchema(EAvatarEmotionState::Neutral, 0.0f);
    TargetBodySchema = ReactionBodySchema;
    TargetFacialExpression = GetEmotionFacialExpression(EAvatarEmotionState::Neutral, 0.0f);
    CoherenceAtLastRequest = State.CognitiveCoherence;

    // The displayed emotion is chosen by the character model only
    TWeakObjectPtr<UUnrealAvatarCognition> WeakThis(this);
    ReactionHandle = FEchoReactionBus::OnReaction().AddLambda([WeakThis](const FEchoCharacterReaction& Reaction)
    {
        if (UUnrealAvatarCognition* Self = WeakThis.Get())
        {
            Self->HandleCharacterReaction(Reaction);
        }
    });
}

void UUnrealAvatarCognition::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (ReactionHandle.IsValid())
    {
        FEchoReactionBus::OnReaction().Remove(ReactionHandle);
        ReactionHandle.Reset();
    }

    if (EchobeatsEngine)
    {
        EchobeatsEngine->OnStepChanged.RemoveDynamic(this, &UUnrealAvatarCognition::HandleEchobeatStepChanged);
        EchobeatsEngine->OnRelevanceRealized.RemoveDynamic(this, &UUnrealAvatarCognition::HandleRelevanceRealized);
        EchobeatsEngine->OnStreamActivated.RemoveDynamic(this, &UUnrealAvatarCognition::HandleStreamActivated);
    }

    Super::EndPlay(EndPlayReason);
}

void UUnrealAvatarCognition::TickComponent(float DeltaTime, ELevelTick TickType,
                                            FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Postural/physiological tone from 4E state (never selects an emotion or facial expression)
    ExpressionTimer += DeltaTime;
    const float UpdateInterval = 1.0f / FMath::Max(ExpressionUpdateRate, 1.0f);

    if (ExpressionTimer >= UpdateInterval)
    {
        ExpressionTimer -= UpdateInterval;
        UpdateExpressionFromMappings();
    }

    // Render: blend toward the model-chosen targets and apply
    BlendExpressionStates(DeltaTime);
    ApplyExpressionToAvatar();

    // Handle emotion blending
    if (EmotionBlendTimer > 0.0f)
    {
        EmotionBlendTimer -= DeltaTime;
        if (EmotionBlendTimer <= 0.0f)
        {
            EmotionBlendTimer = 0.0f;
            const EAvatarEmotionState OldState = State.EmotionState;
            State.EmotionState = TargetEmotionState;
            State.EmotionIntensity = TargetEmotionIntensity;
            if (OldState != State.EmotionState)
            {
                OnEmotionStateChanged.Broadcast(OldState, State.EmotionState);
            }
        }
        else
        {
            // Interpolate emotion intensity
            float BlendAlpha = 1.0f - (EmotionBlendTimer / EmotionTransitionDuration);
            State.EmotionIntensity = FMath::Lerp(State.EmotionIntensity, TargetEmotionIntensity, BlendAlpha);
        }
    }
}

void UUnrealAvatarCognition::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        SkeletalMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
        EchobeatsEngine = Owner->FindComponentByClass<UEchobeatsStreamEngine>();
        Sys6Avatar = Owner->FindComponentByClass<USys6AvatarIntegration>();

        // Bind to echobeats events
        if (EchobeatsEngine)
        {
            EchobeatsEngine->OnStepChanged.AddDynamic(this, &UUnrealAvatarCognition::HandleEchobeatStepChanged);
            EchobeatsEngine->OnRelevanceRealized.AddDynamic(this, &UUnrealAvatarCognition::HandleRelevanceRealized);
            EchobeatsEngine->OnStreamActivated.AddDynamic(this, &UUnrealAvatarCognition::HandleStreamActivated);
        }
    }
}

void UUnrealAvatarCognition::InitializeDefaultMappings()
{
    ExpressionMappings.Empty();

    // Coherence -> Facial stability
    FCognitiveExpressionMapping CoherenceMapping;
    CoherenceMapping.CognitiveParameter = TEXT("Coherence");
    CoherenceMapping.ExpressionChannel = EAvatarExpressionChannel::Facial;
    CoherenceMapping.MappingWeight = 0.8f;
    CoherenceMapping.ActivationThreshold = 0.1f;
    ExpressionMappings.Add(CoherenceMapping);

    // Relevance -> Gaze focus
    FCognitiveExpressionMapping RelevanceMapping;
    RelevanceMapping.CognitiveParameter = TEXT("Relevance");
    RelevanceMapping.ExpressionChannel = EAvatarExpressionChannel::Gaze;
    RelevanceMapping.MappingWeight = 1.0f;
    RelevanceMapping.ActivationThreshold = 0.3f;
    ExpressionMappings.Add(RelevanceMapping);

    // Embodied -> Body tension
    FCognitiveExpressionMapping EmbodiedMapping;
    EmbodiedMapping.CognitiveParameter = TEXT("Embodied");
    EmbodiedMapping.ExpressionChannel = EAvatarExpressionChannel::Body;
    EmbodiedMapping.MappingWeight = 0.9f;
    EmbodiedMapping.ActivationThreshold = 0.2f;
    ExpressionMappings.Add(EmbodiedMapping);

    // Enacted -> Gesture
    FCognitiveExpressionMapping EnactedMapping;
    EnactedMapping.CognitiveParameter = TEXT("Enacted");
    EnactedMapping.ExpressionChannel = EAvatarExpressionChannel::Gesture;
    EnactedMapping.MappingWeight = 0.85f;
    EnactedMapping.ActivationThreshold = 0.25f;
    ExpressionMappings.Add(EnactedMapping);

    // Entelechy -> Posture
    FCognitiveExpressionMapping EntelechyMapping;
    EntelechyMapping.CognitiveParameter = TEXT("Entelechy");
    EntelechyMapping.ExpressionChannel = EAvatarExpressionChannel::Posture;
    EntelechyMapping.MappingWeight = 0.7f;
    EntelechyMapping.ActivationThreshold = 0.15f;
    ExpressionMappings.Add(EntelechyMapping);

    // Emotion -> Aura
    FCognitiveExpressionMapping EmotionMapping;
    EmotionMapping.CognitiveParameter = TEXT("EmotionIntensity");
    EmotionMapping.ExpressionChannel = EAvatarExpressionChannel::Aura;
    EmotionMapping.MappingWeight = 1.0f;
    EmotionMapping.ActivationThreshold = 0.1f;
    ExpressionMappings.Add(EmotionMapping);
}

void UUnrealAvatarCognition::SetEmotionState(EAvatarEmotionState NewState, float Intensity)
{
    EAvatarEmotionState OldState = State.EmotionState;
    State.EmotionState = NewState;
    State.EmotionIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);

    if (OldState != NewState)
    {
        OnEmotionStateChanged.Broadcast(OldState, NewState);

        // Update facial expression for new emotion
        TargetFacialExpression = GetEmotionFacialExpression(NewState, State.EmotionIntensity);
        ReactionBodySchema = GetEmotionBodySchema(NewState, State.EmotionIntensity);
        TargetBodySchema = ReactionBodySchema;
    }
}

void UUnrealAvatarCognition::BlendToEmotionState(EAvatarEmotionState TargetState, float TargetIntensity, float BlendTime)
{
    TargetEmotionState = TargetState;
    TargetEmotionIntensity = FMath::Clamp(TargetIntensity, 0.0f, 1.0f);
    EmotionBlendTimer = BlendTime;
    EmotionTransitionDuration = BlendTime;

    // Set target expressions
    TargetFacialExpression = GetEmotionFacialExpression(TargetState, TargetEmotionIntensity);
    ReactionBodySchema = GetEmotionBodySchema(TargetState, TargetEmotionIntensity);
    TargetBodySchema = ReactionBodySchema;
}

EAvatarEmotionState UUnrealAvatarCognition::GetEmotionState() const
{
    return State.EmotionState;
}

float UUnrealAvatarCognition::GetEmotionIntensity() const
{
    return State.EmotionIntensity;
}

void UUnrealAvatarCognition::UpdateFacialExpression(const FFacialExpressionState& NewExpression)
{
    TargetFacialExpression = NewExpression;
    OnExpressionUpdated.Broadcast(EAvatarExpressionChannel::Facial);
}

void UUnrealAvatarCognition::UpdateBodySchema(const FAvatarBodySchema& NewSchema)
{
    ReactionBodySchema = NewSchema;
    TargetBodySchema = NewSchema;
    OnExpressionUpdated.Broadcast(EAvatarExpressionChannel::Body);
}

void UUnrealAvatarCognition::UpdateVoiceModulation(const FVoiceModulationState& NewModulation)
{
    State.VoiceModulation = NewModulation;
    OnExpressionUpdated.Broadcast(EAvatarExpressionChannel::Voice);
}

void UUnrealAvatarCognition::SetGazeTarget(const FVector& WorldTarget)
{
    State.BodySchema.GazeTarget = WorldTarget;
    OnExpressionUpdated.Broadcast(EAvatarExpressionChannel::Gaze);
}

void UUnrealAvatarCognition::SetHeadOrientation(const FRotator& Orientation)
{
    State.BodySchema.HeadOrientation = Orientation;
}

void UUnrealAvatarCognition::ProcessEchobeatsState(const FEchobeatsFullState& EchobeatsState)
{
    State.CurrentEchobeatStep = EchobeatsState.CurrentStep;
    State.CognitiveCoherence = EchobeatsState.OverallCoherence;

    // Update 4E state based on stream activations
    for (const FCognitiveStreamState& StreamState : EchobeatsState.StreamStates)
    {
        switch (StreamState.StreamType)
        {
        case ECognitiveStreamType::Pivotal:
            State.E4State.Add(TEXT("Embodied"), StreamState.ActivationLevel);
            break;
        case ECognitiveStreamType::Affordance:
            State.E4State.Add(TEXT("Embedded"), StreamState.ActivationLevel);
            break;
        case ECognitiveStreamType::Salience:
            State.E4State.Add(TEXT("Enacted"), StreamState.ActivationLevel);
            break;
        }
    }

    OnCognitiveStateChanged.Broadcast(TEXT("EchobeatsStep"), (float)EchobeatsState.CurrentStep);

    // A large coherence shift is a cognition event: ask the model how the character reacts
    if (bRequestReactionOnCognitiveEvents &&
        FMath::Abs(State.CognitiveCoherence - CoherenceAtLastRequest) >= CoherenceShiftReactionThreshold &&
        CanRequestReaction())
    {
        RequestModelReaction(FString::Printf(TEXT("cognitive coherence shifted from %.2f to %.2f"),
            CoherenceAtLastRequest, State.CognitiveCoherence));
    }
}

void UUnrealAvatarCognition::ProcessSys6State(const FSys6FullState& Sys6State)
{
    State.CurrentSys6Step = Sys6State.CurrentStep;
    State.EntelechyLevel = Sys6State.Entelechy;

    // Update extended cognition based on sys6 state
    State.E4State.Add(TEXT("Extended"), Sys6State.Coherence);

    OnCognitiveStateChanged.Broadcast(TEXT("Sys6Step"), (float)Sys6State.CurrentStep);
}

void UUnrealAvatarCognition::Update4EState(const TMap<FString, float>& E4Values)
{
    for (const auto& Pair : E4Values)
    {
        State.E4State.Add(Pair.Key, Pair.Value);
        OnCognitiveStateChanged.Broadcast(Pair.Key, Pair.Value);
    }
}

bool UUnrealAvatarCognition::ComputeExpressionFromCognition()
{
    // The expression is the model's choice; this asks for it with the real cognitive data
    return RequestModelReaction(TEXT("an explicit check-in on your current cognitive state"));
}

void UUnrealAvatarCognition::UpdateExpressionFromMappings()
{
    // Start from the body schema of the model-chosen emotion, then apply 4E postural tone
    TargetBodySchema = ReactionBodySchema;

    if (const float* Embodied = State.E4State.Find(TEXT("Embodied")))
    {
        TargetBodySchema.ShoulderTension *= *Embodied;
        TargetBodySchema.BreathingDepth = FMath::Lerp(0.3f, 0.8f, *Embodied);
    }
    if (const float* Enacted = State.E4State.Find(TEXT("Enacted")))
    {
        TargetBodySchema.HandExpressiveness = FMath::Lerp(0.1f, 0.9f, *Enacted);
    }
    TargetBodySchema.SpineCurvature = FMath::Lerp(0.3f, 0.0f, State.EntelechyLevel);

    for (const FCognitiveExpressionMapping& Mapping : ExpressionMappings)
    {
        float CognitiveValue = 0.0f;

        // Get cognitive parameter value
        if (Mapping.CognitiveParameter == TEXT("Coherence"))
        {
            CognitiveValue = State.CognitiveCoherence;
        }
        else if (Mapping.CognitiveParameter == TEXT("Entelechy"))
        {
            CognitiveValue = State.EntelechyLevel;
        }
        else if (Mapping.CognitiveParameter == TEXT("EmotionIntensity"))
        {
            CognitiveValue = State.EmotionIntensity;
        }
        else if (float* E4Value = State.E4State.Find(Mapping.CognitiveParameter))
        {
            CognitiveValue = *E4Value;
        }

        // Check threshold
        if (CognitiveValue < Mapping.ActivationThreshold)
        {
            continue;
        }

        // Apply mapping weight
        float MappedValue = CognitiveValue * Mapping.MappingWeight;

        // Apply to body-tone channels. Facial expression is not driven by cognition values.
        switch (Mapping.ExpressionChannel)
        {
        case EAvatarExpressionChannel::Body:
            TargetBodySchema.ShoulderTension = FMath::Lerp(TargetBodySchema.ShoulderTension, MappedValue, 0.5f);
            break;
        case EAvatarExpressionChannel::Posture:
            TargetBodySchema.SpineCurvature = FMath::Lerp(TargetBodySchema.SpineCurvature, 1.0f - MappedValue, 0.5f);
            break;
        case EAvatarExpressionChannel::Gesture:
            TargetBodySchema.HandExpressiveness = FMath::Lerp(TargetBodySchema.HandExpressiveness, MappedValue, 0.5f);
            break;
        default:
            break;
        }
    }
}

void UUnrealAvatarCognition::BlendExpressionStates(float DeltaTime)
{
    float BlendSpeed = 1.0f - ExpressionSmoothingFactor;
    float BlendAlpha = FMath::Clamp(BlendSpeed * DeltaTime * 10.0f, 0.0f, 1.0f);

    // Blend facial expression
    State.FacialExpression.BrowRaise = FMath::Lerp(State.FacialExpression.BrowRaise, TargetFacialExpression.BrowRaise, BlendAlpha);
    State.FacialExpression.BrowFurrow = FMath::Lerp(State.FacialExpression.BrowFurrow, TargetFacialExpression.BrowFurrow, BlendAlpha);
    State.FacialExpression.EyeOpenness = FMath::Lerp(State.FacialExpression.EyeOpenness, TargetFacialExpression.EyeOpenness, BlendAlpha);
    State.FacialExpression.Smile = FMath::Lerp(State.FacialExpression.Smile, TargetFacialExpression.Smile, BlendAlpha);
    State.FacialExpression.MouthOpenness = FMath::Lerp(State.FacialExpression.MouthOpenness, TargetFacialExpression.MouthOpenness, BlendAlpha);
    State.FacialExpression.LipTension = FMath::Lerp(State.FacialExpression.LipTension, TargetFacialExpression.LipTension, BlendAlpha);
    State.FacialExpression.CheekRaise = FMath::Lerp(State.FacialExpression.CheekRaise, TargetFacialExpression.CheekRaise, BlendAlpha);
    State.FacialExpression.NoseWrinkle = FMath::Lerp(State.FacialExpression.NoseWrinkle, TargetFacialExpression.NoseWrinkle, BlendAlpha);
    State.FacialExpression.JawClench = FMath::Lerp(State.FacialExpression.JawClench, TargetFacialExpression.JawClench, BlendAlpha);

    // Blend body schema
    State.BodySchema.HeadOrientation = FMath::Lerp(State.BodySchema.HeadOrientation, TargetBodySchema.HeadOrientation, BlendAlpha);
    State.BodySchema.SpineCurvature = FMath::Lerp(State.BodySchema.SpineCurvature, TargetBodySchema.SpineCurvature, BlendAlpha);
    State.BodySchema.ShoulderTension = FMath::Lerp(State.BodySchema.ShoulderTension, TargetBodySchema.ShoulderTension, BlendAlpha);
    State.BodySchema.ArmOpenness = FMath::Lerp(State.BodySchema.ArmOpenness, TargetBodySchema.ArmOpenness, BlendAlpha);
    State.BodySchema.HandExpressiveness = FMath::Lerp(State.BodySchema.HandExpressiveness, TargetBodySchema.HandExpressiveness, BlendAlpha);
    State.BodySchema.StanceWidth = FMath::Lerp(State.BodySchema.StanceWidth, TargetBodySchema.StanceWidth, BlendAlpha);
    State.BodySchema.WeightDistribution = FMath::Lerp(State.BodySchema.WeightDistribution, TargetBodySchema.WeightDistribution, BlendAlpha);
    State.BodySchema.BreathingRate = FMath::Lerp(State.BodySchema.BreathingRate, TargetBodySchema.BreathingRate, BlendAlpha);
    State.BodySchema.BreathingDepth = FMath::Lerp(State.BodySchema.BreathingDepth, TargetBodySchema.BreathingDepth, BlendAlpha);
}

void UUnrealAvatarCognition::ApplyExpressionToAvatar()
{
    if (!SkeletalMesh) return;

    // Apply morph targets for facial expression
    // These would map to actual morph target names in the skeletal mesh
    SkeletalMesh->SetMorphTarget(TEXT("BrowRaise"), State.FacialExpression.BrowRaise);
    SkeletalMesh->SetMorphTarget(TEXT("BrowFurrow"), State.FacialExpression.BrowFurrow);
    SkeletalMesh->SetMorphTarget(TEXT("EyeOpenness"), State.FacialExpression.EyeOpenness);
    SkeletalMesh->SetMorphTarget(TEXT("Smile"), State.FacialExpression.Smile);
    SkeletalMesh->SetMorphTarget(TEXT("MouthOpen"), State.FacialExpression.MouthOpenness);
    SkeletalMesh->SetMorphTarget(TEXT("LipTension"), State.FacialExpression.LipTension);
    SkeletalMesh->SetMorphTarget(TEXT("CheekRaise"), State.FacialExpression.CheekRaise);
    SkeletalMesh->SetMorphTarget(TEXT("NoseWrinkle"), State.FacialExpression.NoseWrinkle);
    SkeletalMesh->SetMorphTarget(TEXT("JawClench"), State.FacialExpression.JawClench);

    // Body schema would be applied through animation blueprint parameters
    // This is a placeholder for the actual implementation
}

FFacialExpressionState UUnrealAvatarCognition::GetEmotionFacialExpression(EAvatarEmotionState Emotion, float Intensity)
{
    FFacialExpressionState Expression;

    switch (Emotion)
    {
    case EAvatarEmotionState::Joy:
        Expression.Smile = 0.8f * Intensity;
        Expression.CheekRaise = 0.6f * Intensity;
        Expression.EyeOpenness = 0.7f;
        Expression.BrowRaise = 0.3f * Intensity;
        break;

    case EAvatarEmotionState::Wonder:
        Expression.BrowRaise = 0.7f * Intensity;
        Expression.EyeOpenness = 0.9f * Intensity;
        Expression.MouthOpenness = 0.3f * Intensity;
        break;

    case EAvatarEmotionState::Curiosity:
        Expression.BrowRaise = 0.4f * Intensity;
        Expression.EyeOpenness = 0.8f;
        Expression.Smile = 0.2f * Intensity;
        break;

    case EAvatarEmotionState::Focus:
        Expression.BrowFurrow = 0.3f * Intensity;
        Expression.EyeOpenness = 0.75f;
        Expression.LipTension = 0.2f * Intensity;
        break;

    case EAvatarEmotionState::Concern:
        Expression.BrowFurrow = 0.5f * Intensity;
        Expression.EyeOpenness = 0.65f;
        Expression.LipTension = 0.4f * Intensity;
        break;

    case EAvatarEmotionState::Contemplation:
        Expression.EyeOpenness = 0.6f;
        Expression.BrowFurrow = 0.2f * Intensity;
        break;

    case EAvatarEmotionState::Excitement:
        Expression.Smile = 0.7f * Intensity;
        Expression.EyeOpenness = 0.85f;
        Expression.BrowRaise = 0.5f * Intensity;
        break;

    case EAvatarEmotionState::Serenity:
        Expression.EyeOpenness = 0.65f;
        Expression.Smile = 0.3f * Intensity;
        break;

    case EAvatarEmotionState::Determination:
        Expression.BrowFurrow = 0.4f * Intensity;
        Expression.JawClench = 0.3f * Intensity;
        Expression.EyeOpenness = 0.75f;
        break;

    default: // Neutral
        Expression.EyeOpenness = 0.7f;
        break;
    }

    return Expression;
}

FAvatarBodySchema UUnrealAvatarCognition::GetEmotionBodySchema(EAvatarEmotionState Emotion, float Intensity)
{
    FAvatarBodySchema Schema;

    switch (Emotion)
    {
    case EAvatarEmotionState::Joy:
        Schema.SpineCurvature = 0.0f;
        Schema.ShoulderTension = 0.1f;
        Schema.ArmOpenness = 0.7f * Intensity;
        Schema.HandExpressiveness = 0.6f * Intensity;
        Schema.BreathingRate = 14.0f;
        Schema.BreathingDepth = 0.6f;
        break;

    case EAvatarEmotionState::Wonder:
        Schema.SpineCurvature = -0.1f; // Slight lean forward
        Schema.ShoulderTension = 0.2f;
        Schema.ArmOpenness = 0.5f;
        Schema.HandExpressiveness = 0.4f * Intensity;
        Schema.BreathingRate = 10.0f;
        Schema.BreathingDepth = 0.7f;
        break;

    case EAvatarEmotionState::Curiosity:
        Schema.SpineCurvature = -0.15f; // Lean forward
        Schema.ShoulderTension = 0.15f;
        Schema.ArmOpenness = 0.4f;
        Schema.HandExpressiveness = 0.5f * Intensity;
        Schema.BreathingRate = 13.0f;
        Schema.BreathingDepth = 0.5f;
        break;

    case EAvatarEmotionState::Focus:
        Schema.SpineCurvature = 0.0f;
        Schema.ShoulderTension = 0.3f * Intensity;
        Schema.ArmOpenness = 0.3f;
        Schema.HandExpressiveness = 0.2f;
        Schema.BreathingRate = 11.0f;
        Schema.BreathingDepth = 0.6f;
        break;

    case EAvatarEmotionState::Concern:
        Schema.SpineCurvature = 0.1f; // Slight slouch
        Schema.ShoulderTension = 0.5f * Intensity;
        Schema.ArmOpenness = 0.3f;
        Schema.HandExpressiveness = 0.3f;
        Schema.BreathingRate = 14.0f;
        Schema.BreathingDepth = 0.4f;
        break;

    case EAvatarEmotionState::Contemplation:
        Schema.SpineCurvature = 0.05f;
        Schema.ShoulderTension = 0.1f;
        Schema.ArmOpenness = 0.4f;
        Schema.HandExpressiveness = 0.2f;
        Schema.BreathingRate = 10.0f;
        Schema.BreathingDepth = 0.7f;
        break;

    case EAvatarEmotionState::Excitement:
        Schema.SpineCurvature = -0.05f;
        Schema.ShoulderTension = 0.2f;
        Schema.ArmOpenness = 0.8f * Intensity;
        Schema.HandExpressiveness = 0.8f * Intensity;
        Schema.BreathingRate = 16.0f;
        Schema.BreathingDepth = 0.5f;
        break;

    case EAvatarEmotionState::Serenity:
        Schema.SpineCurvature = 0.0f;
        Schema.ShoulderTension = 0.05f;
        Schema.ArmOpenness = 0.5f;
        Schema.HandExpressiveness = 0.1f;
        Schema.BreathingRate = 8.0f;
        Schema.BreathingDepth = 0.8f;
        break;

    case EAvatarEmotionState::Determination:
        Schema.SpineCurvature = -0.05f;
        Schema.ShoulderTension = 0.4f * Intensity;
        Schema.ArmOpenness = 0.4f;
        Schema.HandExpressiveness = 0.3f;
        Schema.StanceWidth = 0.6f;
        Schema.BreathingRate = 12.0f;
        Schema.BreathingDepth = 0.65f;
        break;

    default: // Neutral
        Schema.SpineCurvature = 0.0f;
        Schema.ShoulderTension = 0.15f;
        Schema.ArmOpenness = 0.5f;
        Schema.HandExpressiveness = 0.3f;
        Schema.BreathingRate = 12.0f;
        Schema.BreathingDepth = 0.5f;
        break;
    }

    return Schema;
}

// State query implementations
FAvatarCognitiveState UUnrealAvatarCognition::GetCognitiveState() const
{
    return State;
}

FAvatarBodySchema UUnrealAvatarCognition::GetBodySchema() const
{
    return State.BodySchema;
}

FFacialExpressionState UUnrealAvatarCognition::GetFacialExpression() const
{
    return State.FacialExpression;
}

FVoiceModulationState UUnrealAvatarCognition::GetVoiceModulation() const
{
    return State.VoiceModulation;
}

float UUnrealAvatarCognition::GetCognitiveCoherence() const
{
    return State.CognitiveCoherence;
}

float UUnrealAvatarCognition::GetEntelechyLevel() const
{
    return State.EntelechyLevel;
}

void UUnrealAvatarCognition::HandleEchobeatStepChanged(int32 OldStep, int32 NewStep)
{
    State.CurrentEchobeatStep = NewStep;

    // Record the full echobeats state (cadence; the step never selects an emotion)
    if (EchobeatsEngine)
    {
        ProcessEchobeatsState(EchobeatsEngine->GetFullState());
    }
}

void UUnrealAvatarCognition::HandleRelevanceRealized(int32 Step, float RelevanceLevel)
{
    LastRelevanceLevel = RelevanceLevel;

    // Trigger relevance realization event
    OnRelevanceRealizationTriggered.Broadcast(RelevanceLevel);

    // Significant relevance realization: ask the model how the character reacts
    if (bRequestReactionOnCognitiveEvents && RelevanceLevel >= RelevanceReactionThreshold && CanRequestReaction())
    {
        RequestModelReaction(FString::Printf(TEXT("relevance realization at echobeat step %d with relevance %.2f"),
            Step, RelevanceLevel));
    }
}

// ========================================
// MODEL REACTIONS
// ========================================

bool UUnrealAvatarCognition::CanRequestReaction() const
{
    if (bReactionRequestPending)
    {
        return false;
    }
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    return Now - LastReactionRequestTime >= MinSecondsBetweenReactionRequests;
}

FString UUnrealAvatarCognition::GetReactionCharacterNameResolved() const
{
    if (!ReactionCharacterName.IsEmpty())
    {
        return ReactionCharacterName;
    }
    return GetOwner() ? GetOwner()->GetName() : FString(TEXT("Deep Tree Echo"));
}

FString UUnrealAvatarCognition::BuildReactionPrompt(const FString& Trigger) const
{
    auto E4 = [this](const TCHAR* Key) -> FString
    {
        const float* Value = State.E4State.Find(Key);
        return Value ? FString::Printf(TEXT("%.2f"), *Value) : FString(TEXT("(unknown)"));
    };

    const UEnum* EmotionEnum = StaticEnum<EAvatarEmotionState>();
    const FString CurrentEmotion = EmotionEnum
        ? EmotionEnum->GetNameStringByValue(static_cast<int64>(State.EmotionState))
        : FString(TEXT("(unknown)"));

    TMap<FString, FString> Slots;
    Slots.Add(TEXT("character"), GetReactionCharacterNameResolved());
    Slots.Add(TEXT("trigger"), Trigger);
    Slots.Add(TEXT("coherence"), FString::Printf(TEXT("%.2f"), State.CognitiveCoherence));
    Slots.Add(TEXT("entelechy"), FString::Printf(TEXT("%.2f"), State.EntelechyLevel));
    Slots.Add(TEXT("embodied"), E4(TEXT("Embodied")));
    Slots.Add(TEXT("embedded"), E4(TEXT("Embedded")));
    Slots.Add(TEXT("enacted"), E4(TEXT("Enacted")));
    Slots.Add(TEXT("extended"), E4(TEXT("Extended")));
    Slots.Add(TEXT("relevance"), FString::Printf(TEXT("%.2f"), LastRelevanceLevel));
    Slots.Add(TEXT("echobeat_step"), FString::FromInt(State.CurrentEchobeatStep));
    Slots.Add(TEXT("sys6_step"), FString::FromInt(State.CurrentSys6Step));
    Slots.Add(TEXT("current_emotion"), FString::Printf(TEXT("%s at intensity %.2f"), *CurrentEmotion, State.EmotionIntensity));
    Slots.Add(TEXT("emotions"), FString::Join(AvatarCognitionEnumNames<EAvatarEmotionState>(), TEXT(", ")));
    return UEchoModelClient::FillPrompt(ReactionPromptTemplate, Slots);
}

bool UUnrealAvatarCognition::RequestModelReaction(const FString& Trigger)
{
    if (bReactionRequestPending)
    {
        return false;
    }

    bReactionRequestPending = true;
    LastReactionRequestTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    CoherenceAtLastRequest = State.CognitiveCoherence;

    TArray<FEchoModelMessage> Messages;
    Messages.Add(FEchoModelMessage(TEXT("user"), BuildReactionPrompt(Trigger)));

    TWeakObjectPtr<UUnrealAvatarCognition> WeakThis(this);
    UEchoModelClient::Get()->Chat(Messages, FOnEchoModelReply::CreateLambda(
        [WeakThis](bool bSuccess, const FString& TextOrError)
        {
            if (UUnrealAvatarCognition* Self = WeakThis.Get())
            {
                Self->HandleModelReactionReply(bSuccess, TextOrError);
            }
        }), 0.7f);
    return true;
}

void UUnrealAvatarCognition::HandleModelReactionReply(bool bSuccess, const FString& TextOrError)
{
    bReactionRequestPending = false;

    // On any failure the avatar stays as it is; the error is a system event only
    if (!bSuccess)
    {
        LastReactionError = TextOrError;
        UE_LOG(LogTemp, Error, TEXT("UnrealAvatarCognition: no model reaction: %s"), *TextOrError);
        return;
    }

    TSharedPtr<FJsonObject> Json;
    FString EmotionName;
    double Intensity = 0.0;
    if (!UEchoModelClient::ParseJsonObject(TextOrError, Json) ||
        !Json->TryGetStringField(TEXT("emotion"), EmotionName) ||
        !Json->TryGetNumberField(TEXT("intensity"), Intensity))
    {
        LastReactionError = FString::Printf(TEXT("model reply is not the requested JSON: %s"), *TextOrError.Left(200));
        UE_LOG(LogTemp, Error, TEXT("UnrealAvatarCognition: %s"), *LastReactionError);
        return;
    }

    EAvatarEmotionState Emotion = EAvatarEmotionState::Neutral;
    if (!ParseAvatarCognitionEnum(EmotionName, Emotion))
    {
        LastReactionError = FString::Printf(TEXT("model chose an emotion outside the offered list: %s"), *EmotionName);
        UE_LOG(LogTemp, Error, TEXT("UnrealAvatarCognition: %s"), *LastReactionError);
        return;
    }

    LastReactionError.Reset();

    // Publish the model's reaction; every subscriber (including this component) renders it
    FEchoCharacterReaction Reaction;
    Reaction.CharacterName = GetReactionCharacterNameResolved();
    Reaction.Expression = StaticEnum<EAvatarEmotionState>()->GetNameStringByValue(static_cast<int64>(Emotion));
    Reaction.Intensity = FMath::Clamp(static_cast<float>(Intensity), 0.0f, 1.0f);
    FEchoReactionBus::OnReaction().Broadcast(Reaction);
}

void UUnrealAvatarCognition::HandleCharacterReaction(const FEchoCharacterReaction& Reaction)
{
    if (!ReactionCharacterName.IsEmpty() &&
        !Reaction.CharacterName.Equals(ReactionCharacterName, ESearchCase::IgnoreCase))
    {
        return;
    }

    // Render the model-chosen emotion; empty or unknown names leave the avatar unchanged
    EAvatarEmotionState Emotion = EAvatarEmotionState::Neutral;
    if (ParseAvatarCognitionEnum(Reaction.Expression, Emotion))
    {
        BlendToEmotionState(Emotion, FMath::Clamp(Reaction.Intensity, 0.0f, 1.0f), 0.5f);
        OnExpressionUpdated.Broadcast(EAvatarExpressionChannel::Facial);
    }
}

void UUnrealAvatarCognition::HandleStreamActivated(ECognitiveStreamType StreamType, float ActivationLevel)
{
    // Update 4E state based on stream activation
    switch (StreamType)
    {
    case ECognitiveStreamType::Pivotal:
        State.E4State.Add(TEXT("Embodied"), ActivationLevel);
        break;
    case ECognitiveStreamType::Affordance:
        State.E4State.Add(TEXT("Embedded"), ActivationLevel);
        break;
    case ECognitiveStreamType::Salience:
        State.E4State.Add(TEXT("Enacted"), ActivationLevel);
        break;
    }
}
