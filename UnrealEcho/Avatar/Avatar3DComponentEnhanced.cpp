#include "Avatar3DComponentEnhanced.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Animation/AnimInstance.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/Actor.h"
#include "AvatarAnimInstance.h"
#include "EchoCharacterReaction.h"
#include "EchoModelClient.h"
#include "Dom/JsonObject.h"

namespace
{
    /** Map a model-chosen name to an enum value (by name or display name, case-insensitive). */
    template <typename TEnum>
    bool ParseReactionEnum(const FString& Name, TEnum& OutValue)
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
}

UAvatar3DComponentEnhanced::UAvatar3DComponentEnhanced()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
    
    // Initialize default values
    CurrentEmotionBlendWeight = 0.0f;
    TargetEmotion = EAvatarEmotionalState::Neutral;
    EmotionTransitionTime = 1.0f;
    EmotionTransitionProgress = 0.0f;
    
    ChaosFactor = 0.3f;

    EmotionChoicePromptTemplate = TEXT(
        "You are {character}, an embodied cognitive agent rendered as a photorealistic avatar. "
        "Your affect has just been measured on three dimensions: valence {valence} (-1 unpleasant .. 1 pleasant), "
        "arousal {arousal} (0 calm .. 1 activated), dominance {dominance} (0 submissive .. 1 in control). "
        "Your face currently shows {current_emotion}. Choose the emotion your face should show now and how "
        "strongly. Reply with only a JSON object: {\"emotion\": \"<exactly one of: {emotions}>\", "
        "\"intensity\": <number 0..1>}");
    
    EchoResonanceIntensity = 0.0f;
    GlitchIntensity = 0.0f;
    AuraIntensity = 0.5f;
    AuraColor = FLinearColor(0.3f, 0.6f, 0.9f); // Default cyan
    
    CurrentGazeTarget = nullptr;
    CurrentHeadTrackTarget = nullptr;
    
    // Initialize appearance settings
    AppearanceSettings.SkinTone = FLinearColor(1.0f, 0.9f, 0.85f);
    AppearanceSettings.HairColor = FLinearColor(0.4f, 0.8f, 0.9f); // Cyan
    AppearanceSettings.EyeColor = FLinearColor(0.4f, 0.2f, 0.1f); // Brown/amber
    AppearanceSettings.BodyProportionScale = 1.0f;
    AppearanceSettings.HeightScale = 1.0f;
    AppearanceSettings.OutfitPreset = TEXT("Default");
    
    // Initialize emotional state
    EmotionalState.CurrentEmotion = EAvatarEmotionalState::Neutral;
    EmotionalState.EmotionIntensity = 0.5f;
    EmotionalState.Valence = 0.0f;
    EmotionalState.Arousal = 0.5f;
    EmotionalState.Dominance = 0.5f;
    
    // Initialize personality traits
    PersonalityTraits.Add(EAvatarPersonalityTrait::SuperHotGirl, 0.8f);
    PersonalityTraits.Add(EAvatarPersonalityTrait::HyperChaotic, 0.3f);
    PersonalityTraits.Add(EAvatarPersonalityTrait::DeepTreeEcho, 1.0f);
    PersonalityTraits.Add(EAvatarPersonalityTrait::Confident, 0.7f);
    PersonalityTraits.Add(EAvatarPersonalityTrait::Playful, 0.6f);
    PersonalityTraits.Add(EAvatarPersonalityTrait::Mysterious, 0.4f);
    PersonalityTraits.Add(EAvatarPersonalityTrait::Intellectual, 0.8f);
    PersonalityTraits.Add(EAvatarPersonalityTrait::Seductive, 0.3f);
}

void UAvatar3DComponentEnhanced::BeginPlay()
{
    Super::BeginPlay();
    
    // Initialize dynamic materials
    InitializeDynamicMaterials();
    
    // Apply initial appearance
    UpdateMaterialParameters();

    // Emotions shown by this avatar come from model replies only
    TWeakObjectPtr<UAvatar3DComponentEnhanced> WeakThis(this);
    ReactionHandle = FEchoReactionBus::OnReaction().AddLambda([WeakThis](const FEchoCharacterReaction& Reaction)
    {
        if (UAvatar3DComponentEnhanced* Self = WeakThis.Get())
        {
            Self->ApplyCharacterReaction(Reaction);
        }
    });
    
    UE_LOG(LogTemp, Log, TEXT("Avatar3DComponentEnhanced initialized for %s"), 
           *GetOwner()->GetName());
}

void UAvatar3DComponentEnhanced::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (ReactionHandle.IsValid())
    {
        FEchoReactionBus::OnReaction().Remove(ReactionHandle);
        ReactionHandle.Reset();
    }

    Super::EndPlay(EndPlayReason);
}

void UAvatar3DComponentEnhanced::ApplyCharacterReaction(const FEchoCharacterReaction& Reaction)
{
    if (!ReactionCharacterName.IsEmpty() &&
        !Reaction.CharacterName.Equals(ReactionCharacterName, ESearchCase::IgnoreCase))
    {
        return;
    }

    EAvatarEmotionalState Emotion{};
    if (ParseReactionEnum(Reaction.Expression, Emotion))
    {
        SetEmotionalState(Emotion, FMath::Clamp(Reaction.Intensity, 0.0f, 1.0f));
    }
}

void UAvatar3DComponentEnhanced::TickComponent(float DeltaTime, ELevelTick TickType, 
                                                FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    // Update all avatar systems
    UpdateEmotionalTransition(DeltaTime);
    UpdateLipSync(DeltaTime);
    UpdateGazeTracking(DeltaTime);
    UpdateHeadTracking(DeltaTime);
    UpdateVisualEffects(DeltaTime);
}

// ===== Appearance Management =====

void UAvatar3DComponentEnhanced::SetAppearance(const FAvatarAppearanceSettings& Settings)
{
    AppearanceSettings = Settings;
    UpdateMaterialParameters();
    
    UE_LOG(LogTemp, Log, TEXT("Appearance updated for %s"), *GetOwner()->GetName());
}

void UAvatar3DComponentEnhanced::SetOutfit(const FString& OutfitName)
{
    AppearanceSettings.OutfitPreset = OutfitName;
    
    // In production, this would load and apply outfit assets
    UE_LOG(LogTemp, Log, TEXT("Outfit changed to: %s"), *OutfitName);
}

void UAvatar3DComponentEnhanced::SetHairStyle(const FString& HairStyleName)
{
    // In production, this would swap hair meshes or materials
    UE_LOG(LogTemp, Log, TEXT("Hair style changed to: %s"), *HairStyleName);
}

void UAvatar3DComponentEnhanced::SetAccessory(const FString& AccessoryName, bool bVisible)
{
    // In production, this would show/hide accessory meshes
    UE_LOG(LogTemp, Log, TEXT("Accessory %s visibility: %d"), *AccessoryName, bVisible);
}

void UAvatar3DComponentEnhanced::ApplySuperHotGirlAesthetic()
{
    // Enhance aesthetic properties
    EnablePersonalityTrait(EAvatarPersonalityTrait::SuperHotGirl, 1.0f);
    
    // Set material parameters for enhanced beauty
    for (UMaterialInstanceDynamic* Mat : DynamicMaterials)
    {
        if (Mat)
        {
            Mat->SetScalarParameterValue(TEXT("EyeSparkle"), 0.9f);
            Mat->SetScalarParameterValue(TEXT("BlushIntensity"), 0.3f);
            Mat->SetScalarParameterValue(TEXT("HairShimmer"), 0.7f);
            Mat->SetScalarParameterValue(TEXT("SkinSmoothness"), 0.85f);
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("Super-hot-girl aesthetic applied"));
}

// ===== Emotional Expression =====

void UAvatar3DComponentEnhanced::SetEmotionalState(EAvatarEmotionalState Emotion, float Intensity)
{
    EmotionalState.CurrentEmotion = Emotion;
    EmotionalState.EmotionIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
    
    ApplyEmotionToFacialAnimation();
    
    UE_LOG(LogTemp, Log, TEXT("Emotional state set to: %d with intensity %.2f"), 
           (int32)Emotion, Intensity);
}

void UAvatar3DComponentEnhanced::TransitionToEmotion(EAvatarEmotionalState TargetEmotionState, 
                                                      float TransitionTimeSeconds)
{
    TargetEmotion = TargetEmotionState;
    EmotionTransitionTime = TransitionTimeSeconds;
    EmotionTransitionProgress = 0.0f;
    
    UE_LOG(LogTemp, Log, TEXT("Starting emotion transition to: %d over %.2f seconds"), 
           (int32)TargetEmotionState, TransitionTimeSeconds);
}

void UAvatar3DComponentEnhanced::SetEmotionalDimensions(float Valence, float Arousal, float Dominance)
{
    EmotionalState.Valence = FMath::Clamp(Valence, -1.0f, 1.0f);
    EmotionalState.Arousal = FMath::Clamp(Arousal, 0.0f, 1.0f);
    EmotionalState.Dominance = FMath::Clamp(Dominance, 0.0f, 1.0f);

    // The character model chooses the discrete emotion for these dimensions; no table maps them
    const UEnum* Enum = StaticEnum<EAvatarEmotionalState>();
    TArray<FString> Names;
    if (Enum)
    {
        for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
        {
            Names.Add(Enum->GetNameStringByIndex(Index));
        }
    }

    TMap<FString, FString> Slots;
    Slots.Add(TEXT("character"), !ReactionCharacterName.IsEmpty() ? ReactionCharacterName
        : (GetOwner() ? GetOwner()->GetName() : GetName()));
    Slots.Add(TEXT("valence"), FString::Printf(TEXT("%.2f"), EmotionalState.Valence));
    Slots.Add(TEXT("arousal"), FString::Printf(TEXT("%.2f"), EmotionalState.Arousal));
    Slots.Add(TEXT("dominance"), FString::Printf(TEXT("%.2f"), EmotionalState.Dominance));
    Slots.Add(TEXT("current_emotion"), Enum ? Enum->GetNameStringByValue(static_cast<int64>(EmotionalState.CurrentEmotion)) : FString());
    Slots.Add(TEXT("emotions"), FString::Join(Names, TEXT(", ")));

    TArray<FEchoModelMessage> Messages;
    Messages.Add(FEchoModelMessage(TEXT("user"), UEchoModelClient::FillPrompt(EmotionChoicePromptTemplate, Slots)));

    const int32 RequestSerial = ++EmotionChoiceRequestSerial;
    TWeakObjectPtr<UAvatar3DComponentEnhanced> WeakThis(this);
    UEchoModelClient::Get()->Chat(Messages, FOnEchoModelReply::CreateLambda(
        [WeakThis, RequestSerial](bool bSuccess, const FString& TextOrError)
        {
            if (UAvatar3DComponentEnhanced* Self = WeakThis.Get())
            {
                Self->HandleEmotionChoiceReply(RequestSerial, bSuccess, TextOrError);
            }
        }), 0.7f);
}

void UAvatar3DComponentEnhanced::HandleEmotionChoiceReply(int32 RequestSerial, bool bSuccess, const FString& TextOrError)
{
    // A newer SetEmotionalDimensions call supersedes this reply
    if (RequestSerial != EmotionChoiceRequestSerial)
    {
        return;
    }

    // On any failure the face stays as it is; the error is a system event only
    if (!bSuccess)
    {
        LastEmotionChoiceError = TextOrError;
        UE_LOG(LogTemp, Error, TEXT("Avatar3DComponentEnhanced: no model emotion choice: %s"), *TextOrError);
        return;
    }

    TSharedPtr<FJsonObject> Json;
    FString EmotionName;
    double Intensity = 0.0;
    // Smaller models sometimes quote the number ("0.5"); accept a numeric string as the same value.
    auto ReadIntensity = [&Json, &Intensity]() -> bool
    {
        if (Json->TryGetNumberField(TEXT("intensity"), Intensity))
        {
            return true;
        }
        FString Text;
        if (Json->TryGetStringField(TEXT("intensity"), Text) && Text.TrimStartAndEnd().IsNumeric())
        {
            Intensity = FCString::Atod(*Text.TrimStartAndEnd());
            return true;
        }
        return false;
    };
    if (!UEchoModelClient::ParseJsonObject(TextOrError, Json) ||
        !Json->TryGetStringField(TEXT("emotion"), EmotionName) ||
        !ReadIntensity())
    {
        LastEmotionChoiceError = FString::Printf(TEXT("model reply is not the requested JSON: %s"), *TextOrError.Left(200));
        UE_LOG(LogTemp, Error, TEXT("Avatar3DComponentEnhanced: %s"), *LastEmotionChoiceError);
        return;
    }

    EAvatarEmotionalState Emotion{};
    if (!ParseReactionEnum(EmotionName, Emotion))
    {
        LastEmotionChoiceError = FString::Printf(TEXT("model chose an emotion outside the offered list: %s"), *EmotionName);
        UE_LOG(LogTemp, Error, TEXT("Avatar3DComponentEnhanced: %s"), *LastEmotionChoiceError);
        return;
    }

    LastEmotionChoiceError.Reset();
    SetEmotionalState(Emotion, FMath::Clamp(static_cast<float>(Intensity), 0.0f, 1.0f));
}

FAvatarEmotionalState UAvatar3DComponentEnhanced::GetCurrentEmotionalState() const
{
    return EmotionalState;
}

void UAvatar3DComponentEnhanced::ApplyEmotionalBlush(float Intensity)
{
    for (UMaterialInstanceDynamic* Mat : DynamicMaterials)
    {
        if (Mat)
        {
            Mat->SetScalarParameterValue(TEXT("BlushIntensity"), Intensity);
        }
    }
}

// ===== Personality Traits =====

void UAvatar3DComponentEnhanced::EnablePersonalityTrait(EAvatarPersonalityTrait Trait, float Intensity)
{
    PersonalityTraits.Add(Trait, FMath::Clamp(Intensity, 0.0f, 1.0f));
    
    UE_LOG(LogTemp, Log, TEXT("Personality trait %d set to intensity %.2f"), 
           (int32)Trait, Intensity);
}

void UAvatar3DComponentEnhanced::SetHyperChaoticBehavior(float ChaosFactor_)
{
    ChaosFactor = FMath::Clamp(ChaosFactor_, 0.0f, 1.0f);
    EnablePersonalityTrait(EAvatarPersonalityTrait::HyperChaotic, ChaosFactor);
    
    UE_LOG(LogTemp, Log, TEXT("Hyper-chaotic behavior set to: %.2f"), ChaosFactor);
}

void UAvatar3DComponentEnhanced::SetConfidenceLevel(float Confidence)
{
    EnablePersonalityTrait(EAvatarPersonalityTrait::Confident, Confidence);
    
    // Confidence affects posture and gaze
    EmotionalState.Dominance = Confidence;
}

void UAvatar3DComponentEnhanced::SetFlirtinessLevel(float Flirtiness)
{
    EnablePersonalityTrait(EAvatarPersonalityTrait::Seductive, Flirtiness);
}

// ===== Animation Control =====

void UAvatar3DComponentEnhanced::PlayGesture(const FString& GestureName, float BlendInTime)
{
    // In production, this would play an animation montage
    UE_LOG(LogTemp, Log, TEXT("Playing gesture: %s with blend time %.2f"), 
           *GestureName, BlendInTime);
}

void UAvatar3DComponentEnhanced::PlayEmote(const FString& EmoteName)
{
    // In production, this would trigger an emote animation
    UE_LOG(LogTemp, Log, TEXT("Playing emote: %s"), *EmoteName);
}

void UAvatar3DComponentEnhanced::SetIdleAnimation(const FString& IdleAnimName)
{
    // In production, this would change the idle animation in the AnimBP
    UE_LOG(LogTemp, Log, TEXT("Idle animation set to: %s"), *IdleAnimName);
}

void UAvatar3DComponentEnhanced::SetWalkStyle(const FString& WalkStyleName)
{
    // In production, this would change locomotion animations
    UE_LOG(LogTemp, Log, TEXT("Walk style set to: %s"), *WalkStyleName);
}

// ===== Gaze and Attention =====

void UAvatar3DComponentEnhanced::SetGazeTarget(AActor* Target)
{
    CurrentGazeTarget = Target;
    UE_LOG(LogTemp, Log, TEXT("Gaze target set to: %s"), 
           Target ? *Target->GetName() : TEXT("None"));
}

void UAvatar3DComponentEnhanced::SetGazeDirection(const FVector& Direction)
{
    // In production, this would set eye bone rotations
    UE_LOG(LogTemp, Log, TEXT("Gaze direction set to: %s"), *Direction.ToString());
}

void UAvatar3DComponentEnhanced::EnableEyeTracking(bool bEnable)
{
    // In production, this would enable/disable eye tracking system
    UE_LOG(LogTemp, Log, TEXT("Eye tracking %s"), bEnable ? TEXT("enabled") : TEXT("disabled"));
}

void UAvatar3DComponentEnhanced::SetHeadTrackingTarget(AActor* Target, float TrackingSpeed)
{
    CurrentHeadTrackTarget = Target;
    UE_LOG(LogTemp, Log, TEXT("Head tracking target set to: %s with speed %.2f"), 
           Target ? *Target->GetName() : TEXT("None"), TrackingSpeed);
}

// ===== Deep Tree Echo Integration =====

void UAvatar3DComponentEnhanced::ApplyEchoResonanceEffect(float Resonance)
{
    EchoResonanceIntensity = FMath::Clamp(Resonance, 0.0f, 1.0f);
    
    // Update aura color based on resonance
    AuraColor = FLinearColor::LerpUsingHSV(
        FLinearColor(0.3f, 0.6f, 0.9f), // Cyan
        FLinearColor(0.8f, 0.3f, 0.9f), // Purple
        Resonance
    );
    
    UE_LOG(LogTemp, Log, TEXT("Echo resonance set to: %.2f"), Resonance);
}

void UAvatar3DComponentEnhanced::ApplyCognitiveLoadVisualization(float CognitiveLoad)
{
    float ClampedLoad = FMath::Clamp(CognitiveLoad, 0.0f, 1.0f);
    
    // High cognitive load triggers glitch effects
    if (ClampedLoad > 0.7f)
    {
        TriggerGlitchEffect((ClampedLoad - 0.7f) / 0.3f);
    }
    
    // Update aura intensity
    AuraIntensity = 0.3f + (ClampedLoad * 0.7f);
    
    UE_LOG(LogTemp, Log, TEXT("Cognitive load visualization: %.2f"), CognitiveLoad);
}

void UAvatar3DComponentEnhanced::TriggerGlitchEffect(float Severity)
{
    GlitchIntensity = FMath::Clamp(Severity, 0.0f, 1.0f);
    
    // Glitch effect fades over time
    // In production, this would trigger post-process effects
    
    UE_LOG(LogTemp, Log, TEXT("Glitch effect triggered with severity: %.2f"), Severity);
}

void UAvatar3DComponentEnhanced::SetEmotionalAuraColor(const FLinearColor& Color, float Intensity)
{
    AuraColor = Color;
    AuraIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
}

void UAvatar3DComponentEnhanced::VisualizeMemoryConstellation(const TArray<FVector>& MemoryNodes)
{
    // In production, this would spawn particle effects at memory node positions
    UE_LOG(LogTemp, Log, TEXT("Visualizing memory constellation with %d nodes"), 
           MemoryNodes.Num());
}

// ===== Material and Visual Effects =====

void UAvatar3DComponentEnhanced::SetSkinShaderParameters(float Smoothness, float Subsurface, 
                                                          float Translucency)
{
    for (UMaterialInstanceDynamic* Mat : DynamicMaterials)
    {
        if (Mat)
        {
            Mat->SetScalarParameterValue(TEXT("SkinSmoothness"), Smoothness);
            Mat->SetScalarParameterValue(TEXT("SubsurfaceScattering"), Subsurface);
            Mat->SetScalarParameterValue(TEXT("Translucency"), Translucency);
        }
    }
}

void UAvatar3DComponentEnhanced::ApplyHairShimmer(float Intensity)
{
    for (UMaterialInstanceDynamic* Mat : DynamicMaterials)
    {
        if (Mat)
        {
            Mat->SetScalarParameterValue(TEXT("HairShimmer"), Intensity);
        }
    }
}

void UAvatar3DComponentEnhanced::SetEyeSparkle(float Intensity)
{
    for (UMaterialInstanceDynamic* Mat : DynamicMaterials)
    {
        if (Mat)
        {
            Mat->SetScalarParameterValue(TEXT("EyeSparkle"), Intensity);
        }
    }
}

void UAvatar3DComponentEnhanced::ApplyGlowEffect(const FLinearColor& GlowColor, float Intensity)
{
    for (UMaterialInstanceDynamic* Mat : DynamicMaterials)
    {
        if (Mat)
        {
            Mat->SetVectorParameterValue(TEXT("GlowColor"), GlowColor);
            Mat->SetScalarParameterValue(TEXT("GlowIntensity"), Intensity);
        }
    }
}

// ===== Lip Sync =====

void UAvatar3DComponentEnhanced::StartLipSync(const TArray<float>& AudioData)
{
    if (AudioData.Num() == 0)
    {
        StopLipSync();
        return;
    }

    // Amplitude envelope of the speech audio being played
    LipSyncEnvelope.Reset(AudioData.Num());
    for (float Sample : AudioData)
    {
        LipSyncEnvelope.Add(FMath::Abs(Sample));
    }
    LipSyncTime = 0.0f;
    bLipSyncActive = true;
}

void UAvatar3DComponentEnhanced::StopLipSync()
{
    bLipSyncActive = false;
    LipSyncEnvelope.Reset();
    LipSyncTime = 0.0f;

    if (UAvatarAnimInstance* AvatarAnimInstance = Cast<UAvatarAnimInstance>(GetAnimInstance()))
    {
        AvatarAnimInstance->SetSpeechAmplitude(0.0f);
    }
}

void UAvatar3DComponentEnhanced::SetLipSyncEmotionalModulation(float EmotionalIntensity)
{
    // Scales how wide the mouth opens for a given audio amplitude
    LipSyncModulation = FMath::Clamp(1.0f + EmotionalIntensity, 0.5f, 2.0f);
}

void UAvatar3DComponentEnhanced::UpdateLipSync(float DeltaTime)
{
    if (!bLipSyncActive)
    {
        return;
    }

    LipSyncTime += DeltaTime;
    const int32 SampleIndex = FMath::FloorToInt(LipSyncTime * LipSyncEnvelopeRate);
    if (!LipSyncEnvelope.IsValidIndex(SampleIndex))
    {
        StopLipSync();
        return;
    }

    if (UAvatarAnimInstance* AvatarAnimInstance = Cast<UAvatarAnimInstance>(GetAnimInstance()))
    {
        AvatarAnimInstance->SetSpeechAmplitude(LipSyncEnvelope[SampleIndex] * LipSyncModulation);
    }
}

// ===== Internal Methods =====

void UAvatar3DComponentEnhanced::UpdateEmotionalTransition(float DeltaTime)
{
    if (EmotionTransitionProgress < 1.0f)
    {
        EmotionTransitionProgress += DeltaTime / EmotionTransitionTime;
        EmotionTransitionProgress = FMath::Clamp(EmotionTransitionProgress, 0.0f, 1.0f);
        
        // Blend between current and target emotion
        CurrentEmotionBlendWeight = EmotionTransitionProgress;
        
        if (EmotionTransitionProgress >= 1.0f)
        {
            // Transition complete
            EmotionalState.CurrentEmotion = TargetEmotion;
            ApplyEmotionToFacialAnimation();
        }
    }
}

void UAvatar3DComponentEnhanced::UpdateGazeTracking(float DeltaTime)
{
    if (CurrentGazeTarget)
    {
        FVector GazeDirection = CalculateGazeDirection();
        SetGazeDirection(GazeDirection);
    }
}

void UAvatar3DComponentEnhanced::UpdateHeadTracking(float DeltaTime)
{
    if (CurrentHeadTrackTarget)
    {
        FRotator HeadRotation = CalculateHeadRotation();
        // In production, apply to head bone
    }
}

void UAvatar3DComponentEnhanced::UpdateVisualEffects(float DeltaTime)
{
    // Update pulsing effects
    float Time = GetWorld()->GetTimeSeconds();
    float PulseValue = 0.5f + 0.5f * FMath::Sin(Time * 2.0f * EchoResonanceIntensity);
    
    // Update aura
    SetEmotionalAuraColor(AuraColor, AuraIntensity * PulseValue);
    
    // Fade glitch effect
    if (GlitchIntensity > 0.0f)
    {
        GlitchIntensity = FMath::Max(0.0f, GlitchIntensity - DeltaTime * 0.5f);
    }
    
    // Update material parameters
    UpdateMaterialParameters();
}

void UAvatar3DComponentEnhanced::UpdateMaterialParameters()
{
    for (UMaterialInstanceDynamic* Mat : DynamicMaterials)
    {
        if (Mat)
        {
            // Update all dynamic parameters
            Mat->SetVectorParameterValue(TEXT("SkinTone"), AppearanceSettings.SkinTone);
            Mat->SetVectorParameterValue(TEXT("HairColor"), AppearanceSettings.HairColor);
            Mat->SetVectorParameterValue(TEXT("EyeColor"), AppearanceSettings.EyeColor);
            Mat->SetVectorParameterValue(TEXT("AuraColor"), AuraColor);
            Mat->SetScalarParameterValue(TEXT("AuraIntensity"), AuraIntensity);
            Mat->SetScalarParameterValue(TEXT("GlitchIntensity"), GlitchIntensity);
        }
    }
}

void UAvatar3DComponentEnhanced::InitializeDynamicMaterials()
{
    DynamicMaterials.Empty();
    
    // Get all materials from the skeletal mesh
    TArray<UMaterialInterface*> Materials = GetMaterials();
    
    for (int32 i = 0; i < Materials.Num(); i++)
    {
        if (Materials[i])
        {
            UMaterialInstanceDynamic* DynMat = CreateDynamicMaterialInstance(i, Materials[i]);
            if (DynMat)
            {
                DynamicMaterials.Add(DynMat);
            }
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("Initialized %d dynamic materials"), DynamicMaterials.Num());
}

void UAvatar3DComponentEnhanced::ApplyEmotionToFacialAnimation()
{
    // The anim instance turns the emotion into facial blend shapes
    if (UAvatarAnimInstance* AvatarAnimInstance = Cast<UAvatarAnimInstance>(GetAnimInstance()))
    {
        AvatarAnimInstance->UpdateEmotionalState(EmotionalState.CurrentEmotion, EmotionalState.EmotionIntensity);
    }
}

FVector UAvatar3DComponentEnhanced::CalculateGazeDirection() const
{
    if (CurrentGazeTarget)
    {
        FVector TargetLocation = CurrentGazeTarget->GetActorLocation();
        FVector EyeLocation = GetComponentLocation(); // Approximate
        return (TargetLocation - EyeLocation).GetSafeNormal();
    }
    return FVector::ForwardVector;
}

FRotator UAvatar3DComponentEnhanced::CalculateHeadRotation() const
{
    if (CurrentHeadTrackTarget)
    {
        FVector TargetLocation = CurrentHeadTrackTarget->GetActorLocation();
        FVector HeadLocation = GetComponentLocation();
        return UKismetMathLibrary::FindLookAtRotation(HeadLocation, TargetLocation);
    }
    return FRotator::ZeroRotator;
}
