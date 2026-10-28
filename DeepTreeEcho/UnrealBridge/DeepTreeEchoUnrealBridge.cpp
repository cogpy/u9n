#include "DeepTreeEchoUnrealBridge.h"
#include "../Model/EchoCharacterReaction.h"
#include "../Model/EchoModelClient.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "../Avatar/AvatarEvolutionSystem.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetMathLibrary.h"

/**
 * Deep Tree Echo Unreal Bridge Implementation
 * 
 * Implements the comprehensive integration between Deep Tree Echo cognitive
 * framework and Unreal Engine systems for 4E embodied cognition.
 */

UDeepTreeEchoUnrealBridge::UDeepTreeEchoUnrealBridge()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;

    // Initialize default motor mapping
    MotorMapping.ModeToAnimationMap.Add(TEXT("Reactive"), TEXT("Idle_Alert"));
    MotorMapping.ModeToAnimationMap.Add(TEXT("Deliberative"), TEXT("Idle_Thinking"));
    MotorMapping.ModeToAnimationMap.Add(TEXT("Reflective"), TEXT("Idle_Contemplative"));
    MotorMapping.ModeToAnimationMap.Add(TEXT("Creative"), TEXT("Idle_Expressive"));
    MotorMapping.ModeToAnimationMap.Add(TEXT("Integrative"), TEXT("Idle_Focused"));

    MotorMapping.EmotionToGestureMap.Add(TEXT("Joy"), TEXT("Gesture_Happy"));
    MotorMapping.EmotionToGestureMap.Add(TEXT("Curiosity"), TEXT("Gesture_Interested"));
    MotorMapping.EmotionToGestureMap.Add(TEXT("Contemplation"), TEXT("Gesture_Thinking"));
    MotorMapping.EmotionToGestureMap.Add(TEXT("Wonder"), TEXT("Gesture_Amazed"));
    MotorMapping.EmotionToGestureMap.Add(TEXT("Concern"), TEXT("Gesture_Worried"));

    // Resting face until the model chooses an expression
    CurrentExpression.EmotionIntensities.Add(TEXT("Neutral"), 1.0f);

    // Prompt template (model input): the model chooses the reaction, the slots carry the real state
    ReactionPromptTemplate = TEXT(
        "You are {character}, an embodied cognitive agent. Your cognitive mode has just changed from "
        "{previous_mode} to {mode}. Relevance coherence: {coherence} (0..1). Current relevance frame: {frame}. "
        "Most salient items: {salience}.\n"
        "Choose the facial expression you show right now, from exactly this list: {expressions}. "
        "Reply with JSON only: {\"expression\": \"<one name from the list>\", \"intensity\": <number 0..1>}");

    // Narrative prompt templates (model input). These replace the fixed insight line and the
    // log-only diary / gesture stubs: the slots carry the real state, the model writes the words.
    const TCHAR* StateBlock =
        TEXT("Your current cognitive mode: {mode}. Relevance coherence: {coherence} (0..1). Relevance frame: {frame}. ")
        TEXT("Most salient items: {salience}. 4E integration score: {integration_4e} (0..1).\n")
        TEXT("Recent experiences (oldest first):\n{experiences}\nRecent diary entries:\n{diary}\n");
    NarrativePromptTemplates.Add(TEXT("insight"), FString(TEXT("You are {character}, an embodied cognitive agent reflecting on your recent experience.\n")) + StateBlock +
        TEXT("Write one insight (one or two sentences, first person) about a pattern you notice in these experiences. ")
        TEXT("Base it only on what is listed; if there is too little to see a pattern, say so plainly."));
    NarrativePromptTemplates.Add(TEXT("diary"), FString(TEXT("You are {character}, an embodied cognitive agent keeping a private diary.\n")) + StateBlock +
        TEXT("Context for this entry: {context}\nWrite today's diary entry (two to four sentences, first person) about this context and how ")
        TEXT("your current state relates to it. Use only the facts given."));
    NarrativePromptTemplates.Add(TEXT("gesture"), FString(TEXT("You are {character}, an embodied cognitive agent with a body.\n")) + StateBlock +
        TEXT("The animation system asks you to make the gesture \"{requested_gesture}\" at intensity {requested_intensity}. ")
        TEXT("Decide which gesture you actually make now, choosing from exactly this list: {gestures}, or \"none\". ")
        TEXT("Reply with JSON only: {\"gesture\": \"<name from the list or none>\", \"intensity\": <number 0..1>}"));
}

void UDeepTreeEchoUnrealBridge::BeginPlay()
{
    Super::BeginPlay();
    InitializeComponentReferences();

    // The displayed facial expression is chosen by the character model only
    TWeakObjectPtr<UDeepTreeEchoUnrealBridge> WeakThis(this);
    ReactionHandle = FEchoReactionBus::OnReaction().AddLambda([WeakThis](const FEchoCharacterReaction& Reaction)
    {
        if (UDeepTreeEchoUnrealBridge* Self = WeakThis.Get())
        {
            Self->HandleCharacterReaction(Reaction);
        }
    });
}

void UDeepTreeEchoUnrealBridge::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (ReactionHandle.IsValid())
    {
        FEchoReactionBus::OnReaction().Remove(ReactionHandle);
        ReactionHandle.Reset();
    }

    Super::EndPlay(EndPlayReason);
}

void UDeepTreeEchoUnrealBridge::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (IntegrationMode == EUnrealIntegrationMode::Minimal)
    {
        return;
    }

    // Process sensory inputs
    if (IntegrationMode == EUnrealIntegrationMode::Full || IntegrationMode == EUnrealIntegrationMode::CognitiveOnly)
    {
        ProcessAllSensoryInputs();
    }

    // Update motor outputs
    if (IntegrationMode == EUnrealIntegrationMode::Full || IntegrationMode == EUnrealIntegrationMode::AvatarOnly)
    {
        UpdateMotorOutputs(DeltaTime);
    }

    // Draw debug visualization if enabled
    if (bEnableCognitiveVisualization)
    {
        DrawCognitiveVisualization();
    }
}

void UDeepTreeEchoUnrealBridge::InitializeBridge()
{
    InitializeComponentReferences();

    if (CognitiveCore)
    {
        CognitiveCore->InitializeSystem();
    }

    if (ReservoirSystem)
    {
        ReservoirSystem->InitializeReservoirs();
    }

    if (EmbodiedCognition)
    {
        EmbodiedCognition->Initialize();
    }

    UE_LOG(LogTemp, Log, TEXT("DeepTreeEchoUnrealBridge: Bridge initialized successfully"));
}

void UDeepTreeEchoUnrealBridge::InitializeComponentReferences()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    // Find cognitive components
    CognitiveCore = Owner->FindComponentByClass<UDeepTreeEchoCore>();
    ReservoirSystem = Owner->FindComponentByClass<UDeepTreeEchoReservoir>();
    EmbodiedCognition = Owner->FindComponentByClass<UEmbodiedCognitionComponent>();

    // Find avatar components
    AvatarMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
    if (AvatarMesh)
    {
        AnimInstance = AvatarMesh->GetAnimInstance();
    }
}

void UDeepTreeEchoUnrealBridge::SetAvatarMesh(USkeletalMeshComponent* InMesh)
{
    AvatarMesh = InMesh;
    if (AvatarMesh)
    {
        AnimInstance = AvatarMesh->GetAnimInstance();
    }
}

// ========================================
// SENSORY INPUT PROCESSING
// ========================================

void UDeepTreeEchoUnrealBridge::ProcessAllSensoryInputs()
{
    if (SensoryConfig.bEnableProprioceptiveInput)
    {
        ProcessProprioceptiveInput();
    }
}

void UDeepTreeEchoUnrealBridge::ProcessVisualInput(const TArray<AActor*>& VisibleActors)
{
    if (!SensoryConfig.bEnableVisualInput || !CognitiveCore)
    {
        return;
    }

    // Convert visible actors to sensory data
    VisualSensoryBuffer.Reset();
    
    for (const AActor* Actor : VisibleActors)
    {
        if (!Actor) continue;

        FVector Direction = Actor->GetActorLocation() - GetOwner()->GetActorLocation();
        float Distance = Direction.Size();
        Direction.Normalize();

        // Encode: direction (3), distance (1), type hash (1)
        VisualSensoryBuffer.Add(Direction.X);
        VisualSensoryBuffer.Add(Direction.Y);
        VisualSensoryBuffer.Add(Direction.Z);
        VisualSensoryBuffer.Add(FMath::Clamp(1.0f - (Distance / 5000.0f), 0.0f, 1.0f));
        VisualSensoryBuffer.Add(FMath::Frac(GetTypeHash(Actor->GetClass()->GetName()) / 1000000.0f));
    }

    // Send to cognitive core
    CognitiveCore->ProcessSensoryInput(VisualSensoryBuffer, TEXT("Visual"));
}

void UDeepTreeEchoUnrealBridge::ProcessAuditoryInput(const TArray<FVector>& SoundSources, const TArray<float>& SoundIntensities)
{
    if (!SensoryConfig.bEnableAuditoryInput || !CognitiveCore)
    {
        return;
    }

    AuditorySensoryBuffer.Reset();

    for (int32 i = 0; i < SoundSources.Num() && i < SoundIntensities.Num(); ++i)
    {
        FVector Direction = SoundSources[i] - GetOwner()->GetActorLocation();
        float Distance = Direction.Size();

        if (Distance > SensoryConfig.AuditoryRange)
        {
            continue;
        }

        Direction.Normalize();

        // Encode: direction (3), intensity (1), distance factor (1)
        AuditorySensoryBuffer.Add(Direction.X);
        AuditorySensoryBuffer.Add(Direction.Y);
        AuditorySensoryBuffer.Add(Direction.Z);
        AuditorySensoryBuffer.Add(SoundIntensities[i]);
        AuditorySensoryBuffer.Add(1.0f - (Distance / SensoryConfig.AuditoryRange));
    }

    CognitiveCore->ProcessSensoryInput(AuditorySensoryBuffer, TEXT("Auditory"));
}

void UDeepTreeEchoUnrealBridge::ProcessProprioceptiveInput()
{
    if (!AvatarMesh || !CognitiveCore)
    {
        return;
    }

    ProprioceptiveBuffer.Reset();

    // Get bone transforms for proprioception
    TArray<FName> BoneNames = {
        TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"),
        TEXT("head"), TEXT("neck_01"),
        TEXT("upperarm_l"), TEXT("upperarm_r"),
        TEXT("lowerarm_l"), TEXT("lowerarm_r"),
        TEXT("hand_l"), TEXT("hand_r"),
        TEXT("thigh_l"), TEXT("thigh_r"),
        TEXT("calf_l"), TEXT("calf_r")
    };

    for (const FName& BoneName : BoneNames)
    {
        int32 BoneIndex = AvatarMesh->GetBoneIndex(BoneName);
        if (BoneIndex != INDEX_NONE)
        {
            FTransform BoneTransform = AvatarMesh->GetBoneTransform(BoneIndex);
            FRotator BoneRotation = BoneTransform.Rotator();

            // Encode rotation as normalized values
            ProprioceptiveBuffer.Add(BoneRotation.Pitch / 180.0f);
            ProprioceptiveBuffer.Add(BoneRotation.Yaw / 180.0f);
            ProprioceptiveBuffer.Add(BoneRotation.Roll / 180.0f);
        }
    }

    // Update embodied cognition component
    if (EmbodiedCognition)
    {
        TArray<float> InteroceptiveData; // Would come from physiological simulation
        EmbodiedCognition->UpdateEmbodiedState(ProprioceptiveBuffer, InteroceptiveData);
    }

    CognitiveCore->ProcessSensoryInput(ProprioceptiveBuffer, TEXT("Proprioceptive"));
}

void UDeepTreeEchoUnrealBridge::ProcessEnvironmentalInput(const TArray<FString>& Affordances)
{
    if (!SensoryConfig.bEnableEnvironmentalInput || !CognitiveCore)
    {
        return;
    }

    AffordanceBuffer = Affordances;

    const FString AffordanceText = FString::Join(Affordances, TEXT(", "));
    if (AffordanceText != LastExperienceAffordances)
    {
        LastExperienceAffordances = AffordanceText;
        RecordExperience(AffordanceText.IsEmpty()
            ? FString(TEXT("no environmental affordances perceived"))
            : FString::Printf(TEXT("environment affords: %s"), *AffordanceText));
    }

    // Update embedded state in 4E cognition
    if (EmbodiedCognition)
    {
        FString CurrentNiche = TEXT("Default");
        if (Affordances.Num() > 0)
        {
            // Determine niche from affordances
            if (Affordances.Contains(TEXT("Sit")) && Affordances.Contains(TEXT("Rest")))
            {
                CurrentNiche = TEXT("RestArea");
            }
            else if (Affordances.Contains(TEXT("Interact")) && Affordances.Contains(TEXT("Communicate")))
            {
                CurrentNiche = TEXT("SocialSpace");
            }
            else if (Affordances.Contains(TEXT("Explore")) && Affordances.Contains(TEXT("Navigate")))
            {
                CurrentNiche = TEXT("ExplorationZone");
            }
        }

        EmbodiedCognition->UpdateEmbeddedState(Affordances, CurrentNiche);
    }
}

void UDeepTreeEchoUnrealBridge::ProcessSocialInput(const TArray<AActor*>& NearbyCharacters)
{
    if (!SensoryConfig.bEnableSocialInput || !CognitiveCore)
    {
        return;
    }

    SocialContextBuffer.Reset();

    for (const AActor* Character : NearbyCharacters)
    {
        if (!Character) continue;

        // Extract social context
        FString CharacterName = Character->GetName();
        FVector RelativePosition = Character->GetActorLocation() - GetOwner()->GetActorLocation();
        float Distance = RelativePosition.Size();

        FString SocialContext = FString::Printf(TEXT("%s:%.0f"), *CharacterName, Distance);
        SocialContextBuffer.Add(SocialContext);
    }

    // Update extended cognition with social context
    if (EmbodiedCognition)
    {
        TArray<FString> EmptyTools;
        EmbodiedCognition->UpdateExtendedState(SocialContextBuffer, EmptyTools);
    }
}

// ========================================
// MOTOR OUTPUT
// ========================================

void UDeepTreeEchoUnrealBridge::UpdateMotorOutputs(float DeltaTime)
{
    ExpressionUpdateTimer += DeltaTime;

    if (ExpressionUpdateTimer >= ExpressionUpdateInterval)
    {
        ExpressionUpdateTimer = 0.0f;
        UpdateAvatarExpression();
        UpdateBodyPosture();
    }
}

void UDeepTreeEchoUnrealBridge::UpdateAvatarExpression()
{
    // Cognitive mode changes are experiences the narrative prompts draw on
    if (CognitiveCore)
    {
        const FString Mode = GetCurrentCognitiveModeString();
        if (Mode != LastExperienceMode)
        {
            if (!LastExperienceMode.IsEmpty())
            {
                RecordExperience(FString::Printf(TEXT("cognitive mode changed from %s to %s (relevance frame: %s)"),
                    *LastExperienceMode, *Mode, *GetCurrentRelevanceFrame()));
            }
            LastExperienceMode = Mode;
        }
    }

    // The facial expression itself is applied only in HandleCharacterReaction (model reactions).
    // A cognitive mode change is the moment to ask the model how the character reacts.
    if (CognitiveCore && bRequestReactionOnModeChange && !bReactionRequestPending)
    {
        const FString Mode = GetCurrentCognitiveModeString();
        const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
        if (Mode != LastObservedMode && Now - LastReactionRequestTime >= MinSecondsBetweenReactionRequests)
        {
            RequestModelReaction();
        }
    }

    // Procedural physiology (not a reaction): blink rate follows cognitive load
    if (CognitiveCore)
    {
        const float CognitiveLoad = 1.0f - CognitiveCore->RelevanceState.RelevanceCoherence;
        CurrentExpression.BlinkRateModifier = 1.0f + (CognitiveLoad * 0.5f);
    }

    // Update gaze
    FVector GazeTarget = ComputeGazeFromAttention();
    SetGazeTarget(GazeTarget);
}

FString UDeepTreeEchoUnrealBridge::GetReactionCharacterName() const
{
    if (!ReactionCharacterName.IsEmpty())
    {
        return ReactionCharacterName;
    }
    return GetOwner() ? GetOwner()->GetName() : FString(TEXT("Deep Tree Echo"));
}

const TArray<FString>& UDeepTreeEchoUnrealBridge::GetRenderableExpressions()
{
    static const TArray<FString> Expressions = {
        TEXT("Neutral"), TEXT("Joy"), TEXT("Curiosity"), TEXT("Contemplation"),
        TEXT("Wonder"), TEXT("Alert"), TEXT("Focused"), TEXT("Serenity")
    };
    return Expressions;
}

FString UDeepTreeEchoUnrealBridge::BuildReactionPrompt(const FString& PreviousMode) const
{
    TMap<FString, FString> Slots;
    Slots.Add(TEXT("character"), GetReactionCharacterName());
    Slots.Add(TEXT("mode"), GetCurrentCognitiveModeString());
    Slots.Add(TEXT("previous_mode"), PreviousMode.IsEmpty() ? FString(TEXT("(start)")) : PreviousMode);
    Slots.Add(TEXT("expressions"), FString::Join(GetRenderableExpressions(), TEXT(", ")));

    if (CognitiveCore)
    {
        const FRelevanceRealizationState& RelevanceState = CognitiveCore->RelevanceState;
        Slots.Add(TEXT("coherence"), FString::Printf(TEXT("%.2f"), RelevanceState.RelevanceCoherence));
        Slots.Add(TEXT("frame"), RelevanceState.RelevanceFrame.IsEmpty() ? FString(TEXT("(none)")) : RelevanceState.RelevanceFrame);

        // Top salient items, highest first
        TArray<TPair<FString, float>> Salient;
        for (const auto& Pair : RelevanceState.SalienceLandscape)
        {
            Salient.Add(TPair<FString, float>(Pair.Key, Pair.Value));
        }
        Salient.Sort([](const TPair<FString, float>& A, const TPair<FString, float>& B) { return A.Value > B.Value; });
        TArray<FString> SalienceText;
        for (int32 i = 0; i < Salient.Num() && i < 5; ++i)
        {
            SalienceText.Add(FString::Printf(TEXT("%s (%.2f)"), *Salient[i].Key, Salient[i].Value));
        }
        Slots.Add(TEXT("salience"), SalienceText.Num() > 0 ? FString::Join(SalienceText, TEXT(", ")) : FString(TEXT("(none)")));
    }
    else
    {
        Slots.Add(TEXT("coherence"), TEXT("(unknown)"));
        Slots.Add(TEXT("frame"), TEXT("(none)"));
        Slots.Add(TEXT("salience"), TEXT("(none)"));
    }

    return UEchoModelClient::FillPrompt(ReactionPromptTemplate, Slots);
}

bool UDeepTreeEchoUnrealBridge::RequestModelReaction()
{
    if (bReactionRequestPending)
    {
        return false;
    }

    const FString PreviousMode = LastObservedMode;
    LastObservedMode = GetCurrentCognitiveModeString();
    LastReactionRequestTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    bReactionRequestPending = true;

    TArray<FEchoModelMessage> Messages;
    Messages.Add(FEchoModelMessage(TEXT("user"), BuildReactionPrompt(PreviousMode)));

    TWeakObjectPtr<UDeepTreeEchoUnrealBridge> WeakThis(this);
    UEchoModelClient::Get()->Chat(Messages, FOnEchoModelReply::CreateLambda(
        [WeakThis](bool bSuccess, const FString& TextOrError)
        {
            if (UDeepTreeEchoUnrealBridge* Self = WeakThis.Get())
            {
                Self->HandleModelReactionReply(bSuccess, TextOrError);
            }
        }), 0.7f);
    return true;
}

void UDeepTreeEchoUnrealBridge::HandleModelReactionReply(bool bSuccess, const FString& TextOrError)
{
    bReactionRequestPending = false;

    // On any failure the face stays as it is; the error is reported as a system event only.
    if (!bSuccess)
    {
        LastReactionError = TextOrError;
        UE_LOG(LogTemp, Error, TEXT("DeepTreeEchoUnrealBridge: no model reaction: %s"), *TextOrError);
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
        UE_LOG(LogTemp, Error, TEXT("DeepTreeEchoUnrealBridge: %s"), *LastReactionError);
        return;
    }

    const FString* Allowed = GetRenderableExpressions().FindByPredicate(
        [&Expression](const FString& Name) { return Name.Equals(Expression.TrimStartAndEnd(), ESearchCase::IgnoreCase); });
    if (!Allowed)
    {
        LastReactionError = FString::Printf(TEXT("model chose an expression outside the offered list: %s"), *Expression);
        UE_LOG(LogTemp, Error, TEXT("DeepTreeEchoUnrealBridge: %s"), *LastReactionError);
        return;
    }

    LastReactionError.Reset();

    // Publish the model's reaction; every subscriber (including this bridge) renders it
    FEchoCharacterReaction Reaction;
    Reaction.CharacterName = GetReactionCharacterName();
    Reaction.Expression = *Allowed;
    Reaction.Intensity = FMath::Clamp(static_cast<float>(Intensity), 0.0f, 1.0f);
    FEchoReactionBus::OnReaction().Broadcast(Reaction);
}

void UDeepTreeEchoUnrealBridge::HandleCharacterReaction(const FEchoCharacterReaction& Reaction)
{
    if (!ReactionCharacterName.IsEmpty() &&
        !Reaction.CharacterName.Equals(ReactionCharacterName, ESearchCase::IgnoreCase))
    {
        return;
    }

    // A model-chosen gesture is played if this avatar has a montage for it
    if (!Reaction.Gesture.TrimStartAndEnd().IsEmpty())
    {
        PlayGestureMontage(Reaction.Gesture.TrimStartAndEnd(), Reaction.Intensity);
    }

    // Empty expression = the model chose no change
    if (Reaction.Expression.IsEmpty())
    {
        return;
    }

    RecordExperience(FString::Printf(TEXT("I showed %s (intensity %.2f)"), *Reaction.Expression, Reaction.Intensity));

    const float Intensity = FMath::Clamp(Reaction.Intensity, 0.0f, 1.0f);
    const TMap<FString, float> NewShapes = MapEmotionToBlendShapes(Reaction.Expression, Intensity);
    if (NewShapes.Num() == 0)
    {
        return; // not renderable here; leave the face unchanged
    }

    // Clear the previous model-chosen expression's morph targets, then apply the new ones
    TMap<FString, float> BlendShapes;
    for (const auto& Pair : CurrentExpression.FacialBlendShapes)
    {
        BlendShapes.Add(Pair.Key, 0.0f);
    }
    for (const auto& Pair : NewShapes)
    {
        BlendShapes.Add(Pair.Key, Pair.Value);
    }

    CurrentExpression.EmotionIntensities.Reset();
    CurrentExpression.EmotionIntensities.Add(Reaction.Expression, Intensity);
    CurrentExpression.FacialBlendShapes = NewShapes;
    CurrentExpression.ExpressionIntensity = Intensity;

    ApplyFacialBlendShapes(BlendShapes);
}

void UDeepTreeEchoUnrealBridge::ApplyFacialBlendShapes(const TMap<FString, float>& BlendShapes)
{
    if (!AvatarMesh)
    {
        return;
    }

    for (const auto& Pair : BlendShapes)
    {
        // Apply morph target
        AvatarMesh->SetMorphTarget(FName(*Pair.Key), Pair.Value);
    }
}

void UDeepTreeEchoUnrealBridge::SetGazeTarget(const FVector& TargetLocation)
{
    CurrentExpression.GazeDirection = (TargetLocation - GetOwner()->GetActorLocation()).GetSafeNormal();

    // Apply to animation instance if available
    if (AnimInstance)
    {
        // Set gaze target via animation blueprint variable
        // This would typically be done through a custom AnimInstance class
    }
}

bool UDeepTreeEchoUnrealBridge::TriggerGesture(const FString& GestureName, float Intensity)
{
    // A gesture request is a stimulus, not a script: the model decides what the body does.
    if (GestureMontages.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("DeepTreeEchoUnrealBridge: gesture '%s' not requested: no GestureMontages configured"), *GestureName);
        OnNarrativeFailed.Broadcast(0, TEXT("gesture"), TEXT("no GestureMontages configured; there is no gesture vocabulary to offer the model"));
        return false;
    }

    TArray<FString> Names;
    GestureMontages.GetKeys(Names);

    TMap<FString, FString> Slots = BuildNarrativeSlots();
    Slots.Add(TEXT("requested_gesture"), GestureName);
    Slots.Add(TEXT("requested_intensity"), FString::Printf(TEXT("%.2f"), FMath::Clamp(Intensity, 0.0f, 1.0f)));
    Slots.Add(TEXT("gestures"), FString::Join(Names, TEXT(", ")));

    TWeakObjectPtr<UDeepTreeEchoUnrealBridge> WeakThis(this);
    return SendNarrativeRequest(TEXT("gesture"), Slots, [WeakThis](int32 RequestId, const FString& Text)
    {
        if (UDeepTreeEchoUnrealBridge* Self = WeakThis.Get())
        {
            Self->HandleGestureReply(RequestId, Text);
        }
    }, 0.7f) != 0;
}

void UDeepTreeEchoUnrealBridge::HandleGestureReply(int32 RequestId, const FString& Text)
{
    TSharedPtr<FJsonObject> Json;
    FString Gesture;
    double Intensity = 0.0;
    if (!UEchoModelClient::ParseJsonObject(Text, Json) || !Json->TryGetStringField(TEXT("gesture"), Gesture))
    {
        const FString Error = FString::Printf(TEXT("model reply is not the requested gesture JSON: %s"), *Text.Left(200));
        UE_LOG(LogTemp, Error, TEXT("DeepTreeEchoUnrealBridge: %s"), *Error);
        OnNarrativeFailed.Broadcast(RequestId, TEXT("gesture"), Error);
        return;
    }
    Json->TryGetNumberField(TEXT("intensity"), Intensity);
    Gesture.TrimStartAndEndInline();

    if (Gesture.IsEmpty() || Gesture.Equals(TEXT("none"), ESearchCase::IgnoreCase))
    {
        return; // the model chose not to gesture
    }

    FString Canonical;
    for (const auto& Pair : GestureMontages)
    {
        if (Pair.Key.Equals(Gesture, ESearchCase::IgnoreCase))
        {
            Canonical = Pair.Key;
            break;
        }
    }
    if (Canonical.IsEmpty())
    {
        const FString Error = FString::Printf(TEXT("model chose a gesture outside the offered list: %s"), *Gesture);
        UE_LOG(LogTemp, Error, TEXT("DeepTreeEchoUnrealBridge: %s"), *Error);
        OnNarrativeFailed.Broadcast(RequestId, TEXT("gesture"), Error);
        return;
    }

    // Publish the model's gesture; HandleCharacterReaction (and other avatar systems) play it
    FEchoCharacterReaction Reaction;
    Reaction.CharacterName = GetReactionCharacterName();
    Reaction.Gesture = Canonical;
    Reaction.Intensity = FMath::Clamp(static_cast<float>(Intensity), 0.0f, 1.0f);
    FEchoReactionBus::OnReaction().Broadcast(Reaction);
}

void UDeepTreeEchoUnrealBridge::PlayGestureMontage(const FString& GestureName, float Intensity)
{
    UAnimMontage* Montage = nullptr;
    for (const auto& Pair : GestureMontages)
    {
        if (Pair.Key.Equals(GestureName, ESearchCase::IgnoreCase))
        {
            Montage = Pair.Value;
            break;
        }
    }

    if (!Montage || !AnimInstance)
    {
        UE_LOG(LogTemp, Verbose, TEXT("DeepTreeEchoUnrealBridge: gesture '%s' not played (%s)"), *GestureName,
            !Montage ? TEXT("no montage in GestureMontages") : TEXT("no anim instance"));
        return;
    }

    if (AnimInstance->Montage_Play(Montage) > 0.0f)
    {
        RecordExperience(FString::Printf(TEXT("I made the gesture %s (intensity %.2f)"), *GestureName, FMath::Clamp(Intensity, 0.0f, 1.0f)));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("DeepTreeEchoUnrealBridge: montage for gesture '%s' failed to play"), *GestureName);
    }
}

void UDeepTreeEchoUnrealBridge::UpdateBodyPosture()
{
    if (!CognitiveCore || !AnimInstance)
    {
        return;
    }

    // Map cognitive mode to posture
    FString ModeString = GetCurrentCognitiveModeString();
    if (MotorMapping.ModeToAnimationMap.Contains(ModeString))
    {
        FString AnimState = MotorMapping.ModeToAnimationMap[ModeString];
        // Apply animation state
    }
}

TMap<FString, float> UDeepTreeEchoUnrealBridge::MapEmotionToBlendShapes(const FString& Emotion, float Intensity) const
{
    TMap<FString, float> BlendShapes;

    if (Emotion == TEXT("Joy") || Emotion == TEXT("Happy"))
    {
        BlendShapes.Add(TEXT("Smile"), Intensity * 0.8f);
        BlendShapes.Add(TEXT("EyeSquint"), Intensity * 0.3f);
        BlendShapes.Add(TEXT("CheekRaise"), Intensity * 0.5f);
    }
    else if (Emotion == TEXT("Curiosity") || Emotion == TEXT("Interested"))
    {
        BlendShapes.Add(TEXT("BrowRaise"), Intensity * 0.6f);
        BlendShapes.Add(TEXT("EyeWiden"), Intensity * 0.4f);
        BlendShapes.Add(TEXT("LipsPart"), Intensity * 0.2f);
    }
    else if (Emotion == TEXT("Contemplation") || Emotion == TEXT("Thinking"))
    {
        BlendShapes.Add(TEXT("BrowFurrow"), Intensity * 0.3f);
        BlendShapes.Add(TEXT("EyeSquint"), Intensity * 0.2f);
        BlendShapes.Add(TEXT("LipsPurse"), Intensity * 0.3f);
    }
    else if (Emotion == TEXT("Wonder") || Emotion == TEXT("Amazed"))
    {
        BlendShapes.Add(TEXT("BrowRaise"), Intensity * 0.8f);
        BlendShapes.Add(TEXT("EyeWiden"), Intensity * 0.7f);
        BlendShapes.Add(TEXT("JawDrop"), Intensity * 0.4f);
    }
    else if (Emotion == TEXT("Alert"))
    {
        BlendShapes.Add(TEXT("EyeWiden"), Intensity * 0.5f);
        BlendShapes.Add(TEXT("BrowRaise"), Intensity * 0.3f);
    }
    else if (Emotion == TEXT("Focused") || Emotion == TEXT("Focus"))
    {
        BlendShapes.Add(TEXT("BrowFurrow"), Intensity * 0.4f);
        BlendShapes.Add(TEXT("EyeSquint"), Intensity * 0.3f);
    }
    else if (Emotion == TEXT("Serenity"))
    {
        BlendShapes.Add(TEXT("Smile"), Intensity * 0.3f);
        BlendShapes.Add(TEXT("EyeRelax"), Intensity * 0.5f);
    }
    else if (Emotion == TEXT("Neutral"))
    {
        BlendShapes.Add(TEXT("Neutral"), Intensity);
    }
    else
    {
        // Not in this renderer's vocabulary: render nothing rather than substitute another expression
        UE_LOG(LogTemp, Verbose, TEXT("DeepTreeEchoUnrealBridge: no blend-shape mapping for expression '%s'"), *Emotion);
    }

    return BlendShapes;
}

FVector UDeepTreeEchoUnrealBridge::ComputeGazeFromAttention() const
{
    if (!CognitiveCore)
    {
        return GetOwner()->GetActorForwardVector();
    }

    const FRelevanceRealizationState& RelevanceState = CognitiveCore->RelevanceState;

    // Find highest attention target
    FString HighestAttentionTarget;
    float HighestAttention = 0.0f;

    for (const auto& Pair : RelevanceState.AttentionAllocation)
    {
        if (Pair.Value > HighestAttention)
        {
            HighestAttention = Pair.Value;
            HighestAttentionTarget = Pair.Key;
        }
    }

    // Default to forward if no specific target
    if (HighestAttentionTarget.IsEmpty())
    {
        return GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() * 500.0f;
    }

    // Would need to resolve target name to world position
    // For now, return forward with slight variation based on attention
    FVector BaseDirection = GetOwner()->GetActorForwardVector();
    return GetOwner()->GetActorLocation() + BaseDirection * 500.0f;
}

// ========================================
// COGNITIVE INTEGRATION
// ========================================

FString UDeepTreeEchoUnrealBridge::GetCurrentCognitiveModeString() const
{
    if (!CognitiveCore)
    {
        return TEXT("Unknown");
    }

    switch (CognitiveCore->CurrentMode)
    {
    case ECognitiveMode::Reactive: return TEXT("Reactive");
    case ECognitiveMode::Deliberative: return TEXT("Deliberative");
    case ECognitiveMode::Reflective: return TEXT("Reflective");
    case ECognitiveMode::Creative: return TEXT("Creative");
    case ECognitiveMode::Integrative: return TEXT("Integrative");
    default: return TEXT("Unknown");
    }
}

TArray<FString> UDeepTreeEchoUnrealBridge::GetConsciousnessStreamStates() const
{
    TArray<FString> States;

    // Would integrate with RecursiveMutualAwarenessSystem
    States.Add(TEXT("Stream1:Perceiving"));
    States.Add(TEXT("Stream2:Acting"));
    States.Add(TEXT("Stream3:Reflecting"));

    return States;
}

float UDeepTreeEchoUnrealBridge::Get4EIntegrationScore() const
{
    if (!CognitiveCore)
    {
        return 0.0f;
    }

    return CognitiveCore->Get4EIntegrationScore();
}

FString UDeepTreeEchoUnrealBridge::GetCurrentRelevanceFrame() const
{
    if (!CognitiveCore)
    {
        return TEXT("None");
    }

    return CognitiveCore->RelevanceState.RelevanceFrame;
}

TArray<float> UDeepTreeEchoUnrealBridge::GetReservoirStateSummary() const
{
    TArray<float> Summary;

    if (ReservoirSystem)
    {
        Summary = ReservoirSystem->GetReservoirStateSummary();
    }

    return Summary;
}

// ========================================
// NARRATIVE INTEGRATION
// ========================================

void UDeepTreeEchoUnrealBridge::RecordExperience(const FString& Experience)
{
    const FString Trimmed = Experience.TrimStartAndEnd();
    if (Trimmed.IsEmpty())
    {
        return;
    }
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    RecentExperiences.Add(FString::Printf(TEXT("[t=%.1fs] %s"), Now, *Trimmed));
    const int32 Excess = RecentExperiences.Num() - FMath::Max(1, MaxRecentExperiences);
    if (Excess > 0)
    {
        RecentExperiences.RemoveAt(0, Excess);
    }
}

TMap<FString, FString> UDeepTreeEchoUnrealBridge::BuildNarrativeSlots() const
{
    TMap<FString, FString> Slots;
    Slots.Add(TEXT("character"), GetReactionCharacterName());
    Slots.Add(TEXT("mode"), GetCurrentCognitiveModeString());
    Slots.Add(TEXT("integration_4e"), CognitiveCore ? FString::Printf(TEXT("%.2f"), Get4EIntegrationScore()) : FString(TEXT("(unknown)")));

    if (CognitiveCore)
    {
        const FRelevanceRealizationState& RelevanceState = CognitiveCore->RelevanceState;
        Slots.Add(TEXT("coherence"), FString::Printf(TEXT("%.2f"), RelevanceState.RelevanceCoherence));
        Slots.Add(TEXT("frame"), RelevanceState.RelevanceFrame.IsEmpty() ? FString(TEXT("(none)")) : RelevanceState.RelevanceFrame);

        TArray<TPair<FString, float>> Salient;
        for (const auto& Pair : RelevanceState.SalienceLandscape)
        {
            Salient.Add(TPair<FString, float>(Pair.Key, Pair.Value));
        }
        Salient.Sort([](const TPair<FString, float>& A, const TPair<FString, float>& B) { return A.Value > B.Value; });
        TArray<FString> SalienceText;
        for (int32 i = 0; i < Salient.Num() && i < 5; ++i)
        {
            SalienceText.Add(FString::Printf(TEXT("%s (%.2f)"), *Salient[i].Key, Salient[i].Value));
        }
        Slots.Add(TEXT("salience"), SalienceText.Num() > 0 ? FString::Join(SalienceText, TEXT(", ")) : FString(TEXT("(none)")));
    }
    else
    {
        Slots.Add(TEXT("coherence"), TEXT("(unknown)"));
        Slots.Add(TEXT("frame"), TEXT("(none)"));
        Slots.Add(TEXT("salience"), TEXT("(none)"));
    }

    Slots.Add(TEXT("experiences"), RecentExperiences.Num() > 0 ? FString::Join(RecentExperiences, TEXT("\n")) : FString(TEXT("(none recorded)")));

    TArray<FString> DiaryText;
    for (int32 i = FMath::Max(0, DiaryEntries.Num() - 3); i < DiaryEntries.Num(); ++i)
    {
        DiaryText.Add(FString::Printf(TEXT("[t=%.1fs, %s] %s"), DiaryEntries[i].WorldTime, *DiaryEntries[i].Context, *DiaryEntries[i].Text));
    }
    Slots.Add(TEXT("diary"), DiaryText.Num() > 0 ? FString::Join(DiaryText, TEXT("\n")) : FString(TEXT("(none)")));
    return Slots;
}

int32 UDeepTreeEchoUnrealBridge::SendNarrativeRequest(const FString& Kind, const TMap<FString, FString>& Slots,
                                                      TFunction<void(int32, const FString&)> OnText, float Temperature)
{
    const FString* Template = NarrativePromptTemplates.Find(Kind);
    if (!Template || Template->IsEmpty())
    {
        const FString Error = FString::Printf(TEXT("no narrative prompt template '%s'"), *Kind);
        UE_LOG(LogTemp, Warning, TEXT("DeepTreeEchoUnrealBridge: %s"), *Error);
        OnNarrativeFailed.Broadcast(0, Kind, Error);
        return 0;
    }

    const int32 RequestId = NextNarrativeRequestId++;
    TArray<FEchoModelMessage> Messages;
    Messages.Add(FEchoModelMessage(TEXT("user"), UEchoModelClient::FillPrompt(*Template, Slots)));

    TWeakObjectPtr<UDeepTreeEchoUnrealBridge> WeakThis(this);
    UEchoModelClient::Get()->Chat(Messages, FOnEchoModelReply::CreateLambda(
        [WeakThis, RequestId, Kind, OnText = MoveTemp(OnText)](bool bSuccess, const FString& TextOrError)
        {
            UDeepTreeEchoUnrealBridge* Self = WeakThis.Get();
            if (!Self)
            {
                return;
            }
            const FString Text = TextOrError.TrimStartAndEnd();
            if (!bSuccess || Text.IsEmpty())
            {
                const FString Error = bSuccess ? FString(TEXT("model reply was empty")) : TextOrError;
                UE_LOG(LogTemp, Warning, TEXT("DeepTreeEchoUnrealBridge: %s request %d not produced: %s"), *Kind, RequestId, *Error);
                Self->OnNarrativeFailed.Broadcast(RequestId, Kind, Error);
                return;
            }
            OnText(RequestId, Text);
        }), Temperature);
    return RequestId;
}

int32 UDeepTreeEchoUnrealBridge::AddDiaryEntryFromCognitiveState(const FString& Context)
{
    FEchoBridgeDiaryEntry Pending;
    Pending.WorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    Pending.Context = Context.TrimStartAndEnd().IsEmpty() ? FString(TEXT("(no context given)")) : Context.TrimStartAndEnd();
    Pending.CognitiveMode = GetCurrentCognitiveModeString();

    TMap<FString, FString> Slots = BuildNarrativeSlots();
    Slots.Add(TEXT("context"), Pending.Context);

    TWeakObjectPtr<UDeepTreeEchoUnrealBridge> WeakThis(this);
    return SendNarrativeRequest(TEXT("diary"), Slots, [WeakThis, Pending](int32 RequestId, const FString& Text)
    {
        UDeepTreeEchoUnrealBridge* Self = WeakThis.Get();
        if (!Self)
        {
            return;
        }
        FEchoBridgeDiaryEntry Entry = Pending;
        Entry.Text = Text;
        Self->DiaryEntries.Add(Entry);
        Self->OnDiaryEntryAdded.Broadcast(RequestId, Entry);
    }, 0.8f);
}

int32 UDeepTreeEchoUnrealBridge::RequestInsight()
{
    if (RecentExperiences.Num() == 0 && DiaryEntries.Num() == 0)
    {
        const FString Error = TEXT("no recent experience or diary entries recorded; nothing to draw an insight from");
        UE_LOG(LogTemp, Log, TEXT("DeepTreeEchoUnrealBridge: insight not requested: %s"), *Error);
        OnNarrativeFailed.Broadcast(0, TEXT("insight"), Error);
        return 0;
    }

    TWeakObjectPtr<UDeepTreeEchoUnrealBridge> WeakThis(this);
    return SendNarrativeRequest(TEXT("insight"), BuildNarrativeSlots(), [WeakThis](int32 RequestId, const FString& Text)
    {
        if (UDeepTreeEchoUnrealBridge* Self = WeakThis.Get())
        {
            Self->LastInsight = Text;
            Self->OnInsightReady.Broadcast(RequestId, Text);
        }
    }, 0.7f);
}

float UDeepTreeEchoUnrealBridge::GetCurrentWisdomWeight() const
{
    // Would integrate with wisdom cultivation system
    if (CognitiveCore)
    {
        return CognitiveCore->RelevanceState.RelevanceCoherence;
    }
    return 0.5f;
}

// ========================================
// EVOLUTION
// ========================================

bool UDeepTreeEchoUnrealBridge::TriggerDevelopmentStep(float StepSeconds)
{
    UAvatarEvolutionSystem* Evolution = GetOwner() ? GetOwner()->FindComponentByClass<UAvatarEvolutionSystem>() : nullptr;
    if (!Evolution)
    {
        UE_LOG(LogTemp, Warning, TEXT("DeepTreeEchoUnrealBridge: development step not performed: owner has no AvatarEvolutionSystem"));
        return false;
    }

    const EDevelopmentalStage Before = Evolution->GetCurrentStage();
    Evolution->TriggerEvolutionStep(FMath::Max(0.0f, StepSeconds));
    if (Evolution->GetCurrentStage() != Before)
    {
        RecordExperience(FString::Printf(TEXT("developmental stage advanced to %s"),
            *UEnum::GetDisplayValueAsText(Evolution->GetCurrentStage()).ToString()));
    }
    return true;
}

int32 UDeepTreeEchoUnrealBridge::GetDevelopmentalStage() const
{
    const UAvatarEvolutionSystem* Evolution = GetOwner() ? GetOwner()->FindComponentByClass<UAvatarEvolutionSystem>() : nullptr;
    return Evolution ? static_cast<int32>(Evolution->GetCurrentStage()) + 1 : 0;
}

float UDeepTreeEchoUnrealBridge::GetEntelechyScore() const
{
    // Compute entelechy actualization score
    float Score = 0.0f;

    if (CognitiveCore)
    {
        Score += CognitiveCore->Get4EIntegrationScore() * 0.25f;
        Score += CognitiveCore->RelevanceState.RelevanceCoherence * 0.25f;
    }

    if (ReservoirSystem)
    {
        Score += 0.25f; // Reservoir presence
    }

    if (EmbodiedCognition)
    {
        Score += 0.25f; // 4E cognition presence
    }

    return Score;
}

// ========================================
// DEBUG VISUALIZATION
// ========================================

void UDeepTreeEchoUnrealBridge::DrawCognitiveVisualization() const
{
#if WITH_EDITOR
    if (!GetWorld() || !GetOwner())
    {
        return;
    }

    FVector OwnerLocation = GetOwner()->GetActorLocation();

    // Draw gaze direction
    DrawDebugLine(GetWorld(), OwnerLocation + FVector(0, 0, 100),
        OwnerLocation + FVector(0, 0, 100) + CurrentExpression.GazeDirection * 200.0f,
        FColor::Cyan, false, -1.0f, 0, 2.0f);

    // Draw cognitive mode indicator
    FColor ModeColor;
    switch (CognitiveCore ? CognitiveCore->CurrentMode : ECognitiveMode::Reactive)
    {
    case ECognitiveMode::Reactive: ModeColor = FColor::Red; break;
    case ECognitiveMode::Deliberative: ModeColor = FColor::Yellow; break;
    case ECognitiveMode::Reflective: ModeColor = FColor::Blue; break;
    case ECognitiveMode::Creative: ModeColor = FColor::Magenta; break;
    case ECognitiveMode::Integrative: ModeColor = FColor::Green; break;
    default: ModeColor = FColor::White; break;
    }

    DrawDebugSphere(GetWorld(), OwnerLocation + FVector(0, 0, 150), 20.0f, 8, ModeColor, false, -1.0f, 0, 1.0f);

    // Draw 4E integration score as vertical bar
    float IntegrationScore = Get4EIntegrationScore();
    DrawDebugLine(GetWorld(), OwnerLocation + FVector(50, 0, 0),
        OwnerLocation + FVector(50, 0, IntegrationScore * 100.0f),
        FColor::Green, false, -1.0f, 0, 5.0f);
#endif
}
