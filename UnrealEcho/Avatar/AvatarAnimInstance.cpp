// Avatar Animation Instance - Production Implementation
// Deep Tree Echo AGI Avatar System

#include "AvatarAnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "EchoCharacterReaction.h"

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

    /** Mouth openness for one character of the utterance being spoken. */
    float MouthOpenForCharacter(TCHAR Character)
    {
        switch (FChar::ToLower(Character))
        {
            case TEXT('a'): case TEXT('o'):
                return 0.8f;
            case TEXT('e'): case TEXT('i'): case TEXT('u'): case TEXT('y'):
                return 0.5f;
            default:
                return FChar::IsAlnum(Character) ? 0.25f : 0.0f;
        }
    }
}

UAvatarAnimInstance::UAvatarAnimInstance()
{
    // Initialize default values
    LocomotionState = ELocomotionState::Idle;
    Speed = 0.0f;
    Direction = 0.0f;
    bIsInAir = false;
    bIsOnGround = true;

    CurrentEmotion = EAvatarEmotionalState::Neutral;
    EmotionIntensity = 0.5f;
    EmotionalValence = 0.0f;
    EmotionalArousal = 0.5f;

    ConfidenceLevel = 0.7f;
    FlirtinessLevel = 0.5f;
    ChaosFactor = 0.3f;
    PlayfulnessLevel = 0.5f;

    CurrentGesture = EGestureState::None;
    GestureBlendWeight = 0.0f;
    GestureIntensity = 0.0f;
    GestureTimer = 0.0f;
    GestureDuration = 0.0f;

    bEnableGazeTracking = true;
    EyeLookAtAlpha = 1.0f;
    HeadLookAtAlpha = 0.7f;

    EchoResonance = 0.0f;
    CognitiveLoad = 0.0f;
    GlitchIntensity = 0.0f;

    BreathingCycle = 0.0f;
    BreathingRate = 15.0f; // 15 breaths per minute
    BreathingTime = 0.0f;

    IdleSway = FVector2D::ZeroVector;
    IdleSwayTime = 0.0f;

    FacialBlendSpeed = 5.0f;

    bIsSpeaking = false;
    SpeechMouthOpen = 0.0f;
    SpeechSecondsPerCharacter = 0.06f;
    ReactionGestureDuration = 2.0f;

    LipSyncDuration = 0.0f;
    LipSyncTime = 0.0f;
    bTextLipSyncActive = false;
}

void UAvatarAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

    // Get owning pawn
    OwningPawn = TryGetPawnOwner();
    
    if (OwningPawn)
    {
        // Get character movement component
        ACharacter* Character = Cast<ACharacter>(OwningPawn);
        if (Character)
        {
            MovementComponent = Character->GetCharacterMovement();
        }
    }

    // Apply only what the model chooses: subscribe to the character reaction bus
    UnbindReactionBus();
    TWeakObjectPtr<UAvatarAnimInstance> WeakThis(this);
    ReactionHandle = FEchoReactionBus::OnReaction().AddLambda([WeakThis](const FEchoCharacterReaction& Reaction)
    {
        if (UAvatarAnimInstance* Self = WeakThis.Get())
        {
            Self->ApplyCharacterReaction(Reaction);
        }
    });
}

void UAvatarAnimInstance::NativeUninitializeAnimation()
{
    UnbindReactionBus();
    Super::NativeUninitializeAnimation();
}

void UAvatarAnimInstance::BeginDestroy()
{
    UnbindReactionBus();
    Super::BeginDestroy();
}

void UAvatarAnimInstance::UnbindReactionBus()
{
    if (ReactionHandle.IsValid())
    {
        FEchoReactionBus::OnReaction().Remove(ReactionHandle);
        ReactionHandle.Reset();
    }
}

void UAvatarAnimInstance::ApplyCharacterReaction(const FEchoCharacterReaction& Reaction)
{
    if (!ReactionCharacterName.IsEmpty() &&
        !Reaction.CharacterName.Equals(ReactionCharacterName, ESearchCase::IgnoreCase))
    {
        return;
    }

    const float Intensity = FMath::Clamp(Reaction.Intensity, 0.0f, 1.0f);

    EAvatarEmotionalState Emotion{};
    if (ParseReactionEnum(Reaction.Expression, Emotion))
    {
        UpdateEmotionalState(Emotion, Intensity);
    }

    EGestureState Gesture{};
    if (ParseReactionEnum(Reaction.Gesture, Gesture) && Gesture != EGestureState::None)
    {
        TriggerGesture(Gesture, ReactionGestureDuration);
        GestureIntensity = Intensity;
    }

    // Speaking visuals follow the model's actual utterance
    if (!Reaction.Utterance.IsEmpty())
    {
        StartLipSync(Reaction.Utterance, Reaction.Utterance.Len() * SpeechSecondsPerCharacter);
    }
}

void UAvatarAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    if (!OwningPawn)
    {
        return;
    }

    // Update all animation systems
    UpdateLocomotionState(DeltaSeconds);
    UpdateGestureState(DeltaSeconds);
    UpdateFacialAnimation(DeltaSeconds);
    UpdateGazeTracking(DeltaSeconds);
    UpdateProceduralAnimation(DeltaSeconds);
    UpdateSpeech(DeltaSeconds);
}

void UAvatarAnimInstance::UpdateLocomotionState(float DeltaTime)
{
    if (!OwningPawn || !MovementComponent)
    {
        return;
    }

    // Calculate speed and direction
    FVector Velocity = OwningPawn->GetVelocity();
    Speed = Velocity.Size2D();
    
    FVector Forward = OwningPawn->GetActorForwardVector();
    FVector VelocityNormalized = Velocity.GetSafeNormal2D();
    Direction = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Forward, VelocityNormalized)));
    
    // Determine if moving left or right
    FVector Right = OwningPawn->GetActorRightVector();
    if (FVector::DotProduct(Right, VelocityNormalized) < 0.0f)
    {
        Direction *= -1.0f;
    }

    // Update air state
    bIsInAir = MovementComponent->IsFalling();
    bIsOnGround = !bIsInAir;

    // Determine locomotion state
    if (bIsInAir)
    {
        if (Velocity.Z < -100.0f)
        {
            LocomotionState = ELocomotionState::Fall;
        }
        else
        {
            LocomotionState = ELocomotionState::Jump;
        }
    }
    else
    {
        if (Speed > 300.0f)
        {
            LocomotionState = ELocomotionState::Run;
        }
        else if (Speed > 10.0f)
        {
            LocomotionState = ELocomotionState::Walk;
        }
        else
        {
            LocomotionState = ELocomotionState::Idle;
        }
    }
}

void UAvatarAnimInstance::UpdateGestureState(float DeltaTime)
{
    // Update gesture timer
    if (CurrentGesture != EGestureState::None)
    {
        GestureTimer += DeltaTime;
        
        // Calculate blend weight (fade in, hold, fade out)
        float FadeInTime = 0.3f;
        float FadeOutTime = 0.3f;
        float HoldTime = GestureDuration - FadeInTime - FadeOutTime;
        
        if (GestureTimer < FadeInTime)
        {
            // Fade in
            GestureBlendWeight = GestureTimer / FadeInTime;
        }
        else if (GestureTimer < FadeInTime + HoldTime)
        {
            // Hold
            GestureBlendWeight = 1.0f;
        }
        else if (GestureTimer < GestureDuration)
        {
            // Fade out
            float FadeOutProgress = (GestureTimer - FadeInTime - HoldTime) / FadeOutTime;
            GestureBlendWeight = 1.0f - FadeOutProgress;
        }
        else
        {
            // Gesture complete
            CurrentGesture = EGestureState::None;
            GestureBlendWeight = 0.0f;
            GestureTimer = 0.0f;
            GestureIntensity = 0.0f;
        }
    }
}

void UAvatarAnimInstance::UpdateFacialAnimation(float DeltaTime)
{
    // Calculate target blend shapes based on current emotion
    TargetFacialBlendShapes = CalculateEmotionalBlendShapes(CurrentEmotion);
    
    // Apply emotion intensity
    TargetFacialBlendShapes.BrowInnerUp *= EmotionIntensity;
    TargetFacialBlendShapes.BrowOuterUp *= EmotionIntensity;
    TargetFacialBlendShapes.BrowDown *= EmotionIntensity;
    TargetFacialBlendShapes.EyeWide *= EmotionIntensity;
    TargetFacialBlendShapes.EyeSquint *= EmotionIntensity;
    TargetFacialBlendShapes.MouthSmile *= EmotionIntensity;
    TargetFacialBlendShapes.MouthFrown *= EmotionIntensity;
    TargetFacialBlendShapes.MouthOpen *= EmotionIntensity;
    
    // Smooth blend to target
    BlendFacialShapes(TargetFacialBlendShapes, DeltaTime);

    // Speech drives the mouth on top of the emotional expression
    if (bIsSpeaking)
    {
        FacialBlendShapes.MouthOpen = FMath::Max(FacialBlendShapes.MouthOpen, SpeechMouthOpen);
        FacialBlendShapes.JawOpen = FMath::Max(FacialBlendShapes.JawOpen, SpeechMouthOpen * 0.6f);
    }
}

void UAvatarAnimInstance::UpdateGazeTracking(float DeltaTime)
{
    if (!bEnableGazeTracking || !OwningPawn)
    {
        return;
    }

    // Smooth interpolation of look-at alpha
    // This would be used by animation blueprint to drive eye and head bones
    EyeLookAtAlpha = FMath::FInterpTo(EyeLookAtAlpha, 1.0f, DeltaTime, 5.0f);
    HeadLookAtAlpha = FMath::FInterpTo(HeadLookAtAlpha, 0.7f, DeltaTime, 3.0f);
}

void UAvatarAnimInstance::UpdateProceduralAnimation(float DeltaTime)
{
    // Update breathing cycle
    BreathingTime += DeltaTime;
    float BreathsPerSecond = BreathingRate / 60.0f;
    BreathingCycle = (FMath::Sin(BreathingTime * BreathsPerSecond * 2.0f * PI) + 1.0f) * 0.5f;
    
    // Update idle sway
    IdleSwayTime += DeltaTime;
    IdleSway.X = FMath::Sin(IdleSwayTime * 0.3f) * 2.0f;
    IdleSway.Y = FMath::Cos(IdleSwayTime * 0.25f) * 1.5f;

    // Glitch effects requested via ApplyChaoticBehavior fade out
    GlitchIntensity = FMath::Max(0.0f, GlitchIntensity - DeltaTime * 0.5f);
}

void UAvatarAnimInstance::UpdateSpeech(float DeltaTime)
{
    if (!bTextLipSyncActive)
    {
        return;
    }

    LipSyncTime += DeltaTime;
    if (LipSyncTime >= LipSyncDuration || LipSyncText.IsEmpty())
    {
        StopLipSync();
        return;
    }

    // Step through the utterance being spoken
    const int32 CharIndex = FMath::Clamp(
        FMath::FloorToInt((LipSyncTime / LipSyncDuration) * LipSyncText.Len()), 0, LipSyncText.Len() - 1);
    const float TargetOpen = MouthOpenForCharacter(LipSyncText[CharIndex]);
    SpeechMouthOpen = FMath::FInterpTo(SpeechMouthOpen, TargetOpen, DeltaTime, 20.0f);
}

void UAvatarAnimInstance::StartLipSync(const FString& Text, float Duration)
{
    if (Text.IsEmpty() || Duration <= 0.0f)
    {
        return;
    }

    LipSyncText = Text;
    LipSyncDuration = Duration;
    LipSyncTime = 0.0f;
    bTextLipSyncActive = true;
    bIsSpeaking = true;
}

void UAvatarAnimInstance::SetSpeechAmplitude(float Amplitude)
{
    bTextLipSyncActive = false;
    SpeechMouthOpen = FMath::Clamp(Amplitude, 0.0f, 1.0f);
    bIsSpeaking = SpeechMouthOpen > 0.0f;
}

void UAvatarAnimInstance::StopLipSync()
{
    bTextLipSyncActive = false;
    LipSyncText.Reset();
    LipSyncTime = 0.0f;
    LipSyncDuration = 0.0f;
    bIsSpeaking = false;
    SpeechMouthOpen = 0.0f;
}

void UAvatarAnimInstance::UpdateEmotionalState(EAvatarEmotionalState Emotion, float Intensity)
{
    CurrentEmotion = Emotion;
    EmotionIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
    
    // Update emotional dimensions
    switch (Emotion)
    {
        case EAvatarEmotionalState::Happy:
        case EAvatarEmotionalState::Excited:
            EmotionalValence = 0.8f;
            EmotionalArousal = 0.7f;
            break;
            
        case EAvatarEmotionalState::Sad:
            EmotionalValence = -0.6f;
            EmotionalArousal = 0.3f;
            break;
            
        case EAvatarEmotionalState::Angry:
            EmotionalValence = -0.7f;
            EmotionalArousal = 0.9f;
            break;
            
        case EAvatarEmotionalState::Surprised:
            EmotionalValence = 0.2f;
            EmotionalArousal = 0.9f;
            break;
            
        case EAvatarEmotionalState::Flirty:
            EmotionalValence = 0.7f;
            EmotionalArousal = 0.6f;
            break;
            
        case EAvatarEmotionalState::Chaotic:
            EmotionalValence = 0.0f;
            EmotionalArousal = 1.0f;
            break;
            
        default:
            EmotionalValence = 0.0f;
            EmotionalArousal = 0.5f;
            break;
    }
    
    // Update breathing rate based on arousal
    BreathingRate = CalculateBreathingRate();
}

void UAvatarAnimInstance::SetPersonalityTraits(float Confidence, float Flirtiness, float Chaos)
{
    ConfidenceLevel = FMath::Clamp(Confidence, 0.0f, 1.0f);
    FlirtinessLevel = FMath::Clamp(Flirtiness, 0.0f, 1.0f);
    ChaosFactor = FMath::Clamp(Chaos, 0.0f, 1.0f);
}

void UAvatarAnimInstance::TriggerGesture(EGestureState Gesture, float Duration)
{
    CurrentGesture = Gesture;
    GestureDuration = Duration;
    GestureTimer = 0.0f;
    GestureBlendWeight = 0.0f;
}

void UAvatarAnimInstance::SetGazeTarget(const FVector& Target)
{
    GazeTargetLocation = Target;
}

void UAvatarAnimInstance::ApplyChaoticBehavior(float Intensity)
{
    ChaosFactor = FMath::Clamp(Intensity, 0.0f, 1.0f);
    GlitchIntensity = ChaosFactor;
}

FFacialBlendShapes UAvatarAnimInstance::CalculateEmotionalBlendShapes(EAvatarEmotionalState Emotion) const
{
    FFacialBlendShapes Shapes;

    switch (Emotion)
    {
        case EAvatarEmotionalState::Happy:
            Shapes.BrowInnerUp = 0.2f;
            Shapes.BrowOuterUp = 0.3f;
            Shapes.EyeWide = 0.3f;
            Shapes.EyeSquint = 0.2f;
            Shapes.MouthSmile = 0.9f;
            Shapes.MouthOpen = 0.2f;
            Shapes.CheekPuff = 0.3f;
            break;

        case EAvatarEmotionalState::Sad:
            Shapes.BrowInnerUp = 0.6f;
            Shapes.BrowDown = 0.4f;
            Shapes.EyeSquint = 0.3f;
            Shapes.MouthFrown = 0.8f;
            break;

        case EAvatarEmotionalState::Angry:
            Shapes.BrowDown = 0.8f;
            Shapes.EyeWide = 0.2f;
            Shapes.EyeSquint = 0.6f;
            Shapes.MouthFrown = 0.5f;
            Shapes.MouthOpen = 0.3f;
            break;

        case EAvatarEmotionalState::Surprised:
            Shapes.BrowInnerUp = 0.8f;
            Shapes.BrowOuterUp = 0.9f;
            Shapes.EyeWide = 0.9f;
            Shapes.MouthOpen = 0.7f;
            Shapes.JawOpen = 0.5f;
            break;

        case EAvatarEmotionalState::Flirty:
            Shapes.BrowInnerUp = 0.3f;
            Shapes.BrowOuterUp = 0.2f;
            Shapes.EyeWide = 0.4f;
            Shapes.EyeSquint = 0.3f;
            Shapes.MouthSmile = 0.7f;
            Shapes.MouthOpen = 0.2f;
            break;

        case EAvatarEmotionalState::Excited:
            Shapes.BrowOuterUp = 0.5f;
            Shapes.EyeWide = 0.7f;
            Shapes.MouthSmile = 0.8f;
            Shapes.MouthOpen = 0.4f;
            break;

        case EAvatarEmotionalState::Chaotic:
            // Wild, asymmetric-looking expression (deterministic rendering of the chosen emotion)
            Shapes.BrowInnerUp = 0.6f;
            Shapes.BrowOuterUp = 0.8f;
            Shapes.BrowDown = 0.2f;
            Shapes.EyeWide = 0.8f;
            Shapes.MouthSmile = 0.6f;
            Shapes.MouthOpen = 0.4f;
            break;

        default: // Neutral
            // All values remain at 0.0f
            break;
    }

    return Shapes;
}

void UAvatarAnimInstance::BlendFacialShapes(const FFacialBlendShapes& Target, float DeltaTime)
{
    // Smooth interpolation to target blend shapes
    FacialBlendShapes.BrowInnerUp = FMath::FInterpTo(FacialBlendShapes.BrowInnerUp, Target.BrowInnerUp, DeltaTime, FacialBlendSpeed);
    FacialBlendShapes.BrowOuterUp = FMath::FInterpTo(FacialBlendShapes.BrowOuterUp, Target.BrowOuterUp, DeltaTime, FacialBlendSpeed);
    FacialBlendShapes.BrowDown = FMath::FInterpTo(FacialBlendShapes.BrowDown, Target.BrowDown, DeltaTime, FacialBlendSpeed);
    FacialBlendShapes.EyeWide = FMath::FInterpTo(FacialBlendShapes.EyeWide, Target.EyeWide, DeltaTime, FacialBlendSpeed);
    FacialBlendShapes.EyeSquint = FMath::FInterpTo(FacialBlendShapes.EyeSquint, Target.EyeSquint, DeltaTime, FacialBlendSpeed);
    FacialBlendShapes.MouthSmile = FMath::FInterpTo(FacialBlendShapes.MouthSmile, Target.MouthSmile, DeltaTime, FacialBlendSpeed);
    FacialBlendShapes.MouthFrown = FMath::FInterpTo(FacialBlendShapes.MouthFrown, Target.MouthFrown, DeltaTime, FacialBlendSpeed);
    FacialBlendShapes.MouthOpen = FMath::FInterpTo(FacialBlendShapes.MouthOpen, Target.MouthOpen, DeltaTime, FacialBlendSpeed);
    FacialBlendShapes.CheekPuff = FMath::FInterpTo(FacialBlendShapes.CheekPuff, Target.CheekPuff, DeltaTime, FacialBlendSpeed);
    FacialBlendShapes.JawOpen = FMath::FInterpTo(FacialBlendShapes.JawOpen, Target.JawOpen, DeltaTime, FacialBlendSpeed);
}

float UAvatarAnimInstance::CalculateBreathingRate() const
{
    // Base breathing rate: 15 breaths per minute
    float BaseRate = 15.0f;
    
    // Arousal increases breathing rate
    float ArousalModifier = EmotionalArousal * 10.0f; // Up to +10 BPM
    
    return FMath::Clamp(BaseRate + ArousalModifier, 10.0f, 30.0f);
}
