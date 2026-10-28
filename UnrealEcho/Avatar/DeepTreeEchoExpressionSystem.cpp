#include "DeepTreeEchoExpressionSystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
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

    template <typename TEnum>
    FString ReactionEnumName(TEnum Value)
    {
        const UEnum* Enum = StaticEnum<TEnum>();
        return Enum ? Enum->GetNameStringByValue(static_cast<int64>(Value)) : FString();
    }
}

// Static helper implementations
FDeepTreeEchoMorphTargets FDeepTreeEchoMorphTargets::Lerp(const FDeepTreeEchoMorphTargets& A, const FDeepTreeEchoMorphTargets& B, float Alpha)
{
    FDeepTreeEchoMorphTargets Result;
    
    // Brow
    Result.BrowRaiseL = FMath::Lerp(A.BrowRaiseL, B.BrowRaiseL, Alpha);
    Result.BrowRaiseR = FMath::Lerp(A.BrowRaiseR, B.BrowRaiseR, Alpha);
    Result.BrowFurrow = FMath::Lerp(A.BrowFurrow, B.BrowFurrow, Alpha);
    
    // Eye
    Result.EyeWideL = FMath::Lerp(A.EyeWideL, B.EyeWideL, Alpha);
    Result.EyeWideR = FMath::Lerp(A.EyeWideR, B.EyeWideR, Alpha);
    Result.EyeSquintL = FMath::Lerp(A.EyeSquintL, B.EyeSquintL, Alpha);
    Result.EyeSquintR = FMath::Lerp(A.EyeSquintR, B.EyeSquintR, Alpha);
    Result.EyeCloseL = FMath::Lerp(A.EyeCloseL, B.EyeCloseL, Alpha);
    Result.EyeCloseR = FMath::Lerp(A.EyeCloseR, B.EyeCloseR, Alpha);
    
    // Nose
    Result.NoseScrunch = FMath::Lerp(A.NoseScrunch, B.NoseScrunch, Alpha);
    
    // Cheek
    Result.CheekRaiseL = FMath::Lerp(A.CheekRaiseL, B.CheekRaiseL, Alpha);
    Result.CheekRaiseR = FMath::Lerp(A.CheekRaiseR, B.CheekRaiseR, Alpha);
    
    // Mouth
    Result.MouthSmileL = FMath::Lerp(A.MouthSmileL, B.MouthSmileL, Alpha);
    Result.MouthSmileR = FMath::Lerp(A.MouthSmileR, B.MouthSmileR, Alpha);
    Result.MouthOpen = FMath::Lerp(A.MouthOpen, B.MouthOpen, Alpha);
    Result.MouthWide = FMath::Lerp(A.MouthWide, B.MouthWide, Alpha);
    Result.LipPucker = FMath::Lerp(A.LipPucker, B.LipPucker, Alpha);
    Result.LipPart = FMath::Lerp(A.LipPart, B.LipPart, Alpha);
    
    // Visemes
    Result.VisemeAA = FMath::Lerp(A.VisemeAA, B.VisemeAA, Alpha);
    Result.VisemeEE = FMath::Lerp(A.VisemeEE, B.VisemeEE, Alpha);
    Result.VisemeOH = FMath::Lerp(A.VisemeOH, B.VisemeOH, Alpha);
    Result.VisemeOO = FMath::Lerp(A.VisemeOO, B.VisemeOO, Alpha);
    
    return Result;
}

FDeepTreeEchoMorphTargets FDeepTreeEchoMorphTargets::Add(const FDeepTreeEchoMorphTargets& Base, const FDeepTreeEchoMorphTargets& Additive, float Weight)
{
    FDeepTreeEchoMorphTargets Result = Base;
    
    Result.BrowRaiseL = FMath::Clamp(Base.BrowRaiseL + Additive.BrowRaiseL * Weight, 0.0f, 1.0f);
    Result.BrowRaiseR = FMath::Clamp(Base.BrowRaiseR + Additive.BrowRaiseR * Weight, 0.0f, 1.0f);
    Result.BrowFurrow = FMath::Clamp(Base.BrowFurrow + Additive.BrowFurrow * Weight, 0.0f, 1.0f);
    Result.EyeWideL = FMath::Clamp(Base.EyeWideL + Additive.EyeWideL * Weight, 0.0f, 1.0f);
    Result.EyeWideR = FMath::Clamp(Base.EyeWideR + Additive.EyeWideR * Weight, 0.0f, 1.0f);
    Result.EyeSquintL = FMath::Clamp(Base.EyeSquintL + Additive.EyeSquintL * Weight, 0.0f, 1.0f);
    Result.EyeSquintR = FMath::Clamp(Base.EyeSquintR + Additive.EyeSquintR * Weight, 0.0f, 1.0f);
    Result.EyeCloseL = FMath::Clamp(Base.EyeCloseL + Additive.EyeCloseL * Weight, 0.0f, 1.0f);
    Result.EyeCloseR = FMath::Clamp(Base.EyeCloseR + Additive.EyeCloseR * Weight, 0.0f, 1.0f);
    Result.NoseScrunch = FMath::Clamp(Base.NoseScrunch + Additive.NoseScrunch * Weight, 0.0f, 1.0f);
    Result.CheekRaiseL = FMath::Clamp(Base.CheekRaiseL + Additive.CheekRaiseL * Weight, 0.0f, 1.0f);
    Result.CheekRaiseR = FMath::Clamp(Base.CheekRaiseR + Additive.CheekRaiseR * Weight, 0.0f, 1.0f);
    Result.MouthSmileL = FMath::Clamp(Base.MouthSmileL + Additive.MouthSmileL * Weight, -1.0f, 1.0f);
    Result.MouthSmileR = FMath::Clamp(Base.MouthSmileR + Additive.MouthSmileR * Weight, -1.0f, 1.0f);
    Result.MouthOpen = FMath::Clamp(Base.MouthOpen + Additive.MouthOpen * Weight, 0.0f, 1.0f);
    Result.MouthWide = FMath::Clamp(Base.MouthWide + Additive.MouthWide * Weight, 0.0f, 1.0f);
    Result.LipPucker = FMath::Clamp(Base.LipPucker + Additive.LipPucker * Weight, 0.0f, 1.0f);
    Result.LipPart = FMath::Clamp(Base.LipPart + Additive.LipPart * Weight, 0.0f, 1.0f);
    Result.VisemeAA = FMath::Clamp(Base.VisemeAA + Additive.VisemeAA * Weight, 0.0f, 1.0f);
    Result.VisemeEE = FMath::Clamp(Base.VisemeEE + Additive.VisemeEE * Weight, 0.0f, 1.0f);
    Result.VisemeOH = FMath::Clamp(Base.VisemeOH + Additive.VisemeOH * Weight, 0.0f, 1.0f);
    Result.VisemeOO = FMath::Clamp(Base.VisemeOO + Additive.VisemeOO * Weight, 0.0f, 1.0f);
    
    return Result;
}

UDeepTreeEchoExpressionSystem::UDeepTreeEchoExpressionSystem()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;

    // Initialize states
    CurrentExpressionState = EExpressionState::Neutral;
    TargetExpressionState = EExpressionState::Neutral;
    CurrentCognitiveState = ECognitiveState::Idle;
    PreviousCognitiveState = ECognitiveState::Idle;

    ExpressionTransitionProgress = 1.0f;
    ExpressionTransitionDuration = 0.3f;
    TransitionStartEmissive = 1.0f;

    // Procedural animation defaults
    bBlinkingEnabled = true;
    bBreathingEnabled = true;
    bSaccadesEnabled = true;

    BlinkTimer = 0.0f;
    NextBlinkTime = 3.0f;
    bIsBlinking = false;
    BlinkProgress = 0.0f;

    BreathingPhase = 0.0f;

    CurrentSaccadeOffset = FVector2D::ZeroVector;
    TargetSaccadeOffset = FVector2D::ZeroVector;
    SaccadeTimer = 0.0f;

    // Gaze
    bHasGazeTarget = false;
    CurrentEyeRotation = FRotator::ZeroRotator;

    // Microexpression
    bMicroExpressionActive = false;
    ActiveMicroExpression = EMicroExpression::None;
    MicroExpressionTimer = 0.0f;
    MicroExpressionDuration = 0.2f;

    // Emissive
    CurrentEmissiveIntensity = 1.0f;
    TargetEmissiveIntensity = 1.0f;
    bEmissivePulseActive = false;
    EmissivePulseTimer = 0.0f;
    EmissivePulseDuration = 0.5f;
    EmissivePulsePeak = 2.0f;

    // Speech
    CurrentSpeakingAmplitude = 0.0f;

    // Prompt template (model input): the model chooses the reaction, the slots carry the real state
    ReactionPromptTemplate = TEXT(
        "You are {character}, an embodied cognitive agent rendered as a photorealistic avatar. "
        "Your cognitive state has just changed from {previous_state} to {state} "
        "(echobeats step {echobeats_step} of 12, phase {echobeats_phase}; currently speaking: {speaking}; "
        "face currently shows: {current_expression}).\n"
        "Choose the facial expression you show now, from exactly this list: {expressions}. "
        "You may keep the current expression. "
        "Reply with JSON only: {\"expression\": \"<one name from the list>\", \"intensity\": <number 0..1>}");
}

void UDeepTreeEchoExpressionSystem::BeginPlay()
{
    Super::BeginPlay();

    InitializeExpressionPresets();

    // Resting face until the model chooses an expression
    if (ExpressionPresets.Contains(EExpressionState::Neutral))
    {
        ExpressionMorphTargets = ExpressionPresets[EExpressionState::Neutral].MorphTargets;
        TargetMorphTargets = ExpressionMorphTargets;
        TransitionStartMorphTargets = ExpressionMorphTargets;
        CurrentMorphTargets = ExpressionMorphTargets;
    }

    // The displayed expression is chosen by the character model only
    TWeakObjectPtr<UDeepTreeEchoExpressionSystem> WeakThis(this);
    ReactionHandle = FEchoReactionBus::OnReaction().AddLambda([WeakThis](const FEchoCharacterReaction& Reaction)
    {
        if (UDeepTreeEchoExpressionSystem* Self = WeakThis.Get())
        {
            Self->HandleCharacterReaction(Reaction);
        }
    });

    // Randomize first blink time
    NextBlinkTime = FMath::RandRange(ProceduralSettings.MinBlinkInterval, ProceduralSettings.MaxBlinkInterval);

    UE_LOG(LogTemp, Log, TEXT("DeepTreeEchoExpressionSystem initialized with %d expression presets"), ExpressionPresets.Num());
}

void UDeepTreeEchoExpressionSystem::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (ReactionHandle.IsValid())
    {
        FEchoReactionBus::OnReaction().Remove(ReactionHandle);
        ReactionHandle.Reset();
    }

    Super::EndPlay(EndPlayReason);
}

void UDeepTreeEchoExpressionSystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Physiological layer is rebuilt every frame
    ProceduralMorphTargets = FDeepTreeEchoMorphTargets();

    // Update all systems
    UpdateExpressionTransition(DeltaTime);
    UpdateProceduralBlinking(DeltaTime);
    UpdateProceduralBreathing(DeltaTime);
    UpdateProceduralSaccades(DeltaTime);
    UpdateMicroExpression(DeltaTime);
    UpdateEmissivePulse(DeltaTime);
    UpdateTextLipSync(DeltaTime);

    // Output = model-chosen expression layer + physiology + speech
    CurrentMorphTargets = FDeepTreeEchoMorphTargets::Add(ExpressionMorphTargets, ProceduralMorphTargets, 1.0f);
    CurrentMorphTargets = FDeepTreeEchoMorphTargets::Add(CurrentMorphTargets, SpeechMorphTargets, 1.0f);
}

void UDeepTreeEchoExpressionSystem::InitializeExpressionPresets()
{
    // ========== PRIORITY 1: Neutral (Foundation) ==========
    {
        FExpressionPreset Neutral;
        Neutral.ExpressionState = EExpressionState::Neutral;
        Neutral.EmissiveIntensity = 1.0f;
        Neutral.HeadTilt = FVector2D(0.0f, 0.0f);
        // All morph targets at 0 (default)
        ExpressionPresets.Add(EExpressionState::Neutral, Neutral);
    }

    // ========== PRIORITY 2: Gentle Smile (JOY_03) ==========
    {
        FExpressionPreset GentleSmile;
        GentleSmile.ExpressionState = EExpressionState::GentleSmile;
        GentleSmile.MorphTargets.MouthSmileL = 0.4f;
        GentleSmile.MorphTargets.MouthSmileR = 0.4f;
        GentleSmile.MorphTargets.CheekRaiseL = 0.3f;
        GentleSmile.MorphTargets.CheekRaiseR = 0.3f;
        GentleSmile.MorphTargets.EyeSquintL = 0.15f;
        GentleSmile.MorphTargets.EyeSquintR = 0.15f;
        GentleSmile.MorphTargets.LipPart = 0.1f;
        GentleSmile.EmissiveIntensity = 1.1f;
        GentleSmile.HeadTilt = FVector2D(3.0f, 0.0f);  // Slight upward tilt
        ExpressionPresets.Add(EExpressionState::GentleSmile, GentleSmile);
    }

    // ========== PRIORITY 3: Speaking Vowel (SPEAK_01) ==========
    {
        FExpressionPreset SpeakingVowel;
        SpeakingVowel.ExpressionState = EExpressionState::SpeakingVowel;
        SpeakingVowel.MorphTargets.MouthOpen = 0.5f;
        SpeakingVowel.MorphTargets.LipPart = 0.4f;
        SpeakingVowel.MorphTargets.VisemeAA = 0.6f;
        SpeakingVowel.EmissiveIntensity = 1.0f;
        ExpressionPresets.Add(EExpressionState::SpeakingVowel, SpeakingVowel);
    }

    // ========== PRIORITY 4: Contemplative (WONDER_03) ==========
    {
        FExpressionPreset Contemplative;
        Contemplative.ExpressionState = EExpressionState::Contemplative;
        Contemplative.MorphTargets.BrowRaiseL = 0.2f;
        Contemplative.MorphTargets.BrowRaiseR = 0.2f;
        Contemplative.MorphTargets.EyeWideL = 0.1f;
        Contemplative.MorphTargets.EyeWideR = 0.1f;
        Contemplative.MorphTargets.LipPart = 0.15f;
        Contemplative.EmissiveIntensity = 0.9f;  // Slightly dimmer during thought
        Contemplative.HeadTilt = FVector2D(5.0f, -5.0f);  // Upward, slight left
        ExpressionPresets.Add(EExpressionState::Contemplative, Contemplative);
    }

    // ========== PRIORITY 5: Broad Smile (JOY_01) ==========
    {
        FExpressionPreset BroadSmile;
        BroadSmile.ExpressionState = EExpressionState::BroadSmile;
        BroadSmile.MorphTargets.MouthSmileL = 0.85f;
        BroadSmile.MorphTargets.MouthSmileR = 0.85f;
        BroadSmile.MorphTargets.MouthOpen = 0.3f;
        BroadSmile.MorphTargets.MouthWide = 0.4f;
        BroadSmile.MorphTargets.CheekRaiseL = 0.7f;
        BroadSmile.MorphTargets.CheekRaiseR = 0.7f;
        BroadSmile.MorphTargets.EyeSquintL = 0.4f;
        BroadSmile.MorphTargets.EyeSquintR = 0.4f;
        BroadSmile.MorphTargets.NoseScrunch = 0.2f;
        BroadSmile.EmissiveIntensity = 1.3f;  // Brighter during joy
        BroadSmile.HeadTilt = FVector2D(8.0f, 0.0f);  // Upward tilt
        ExpressionPresets.Add(EExpressionState::BroadSmile, BroadSmile);
    }

    // ========== Additional Expressions ==========

    // Laughing (JOY_02)
    {
        FExpressionPreset Laughing;
        Laughing.ExpressionState = EExpressionState::Laughing;
        Laughing.MorphTargets.MouthSmileL = 0.9f;
        Laughing.MorphTargets.MouthSmileR = 0.9f;
        Laughing.MorphTargets.MouthOpen = 0.6f;
        Laughing.MorphTargets.MouthWide = 0.5f;
        Laughing.MorphTargets.CheekRaiseL = 0.8f;
        Laughing.MorphTargets.CheekRaiseR = 0.8f;
        Laughing.MorphTargets.EyeSquintL = 0.6f;
        Laughing.MorphTargets.EyeSquintR = 0.6f;
        Laughing.MorphTargets.NoseScrunch = 0.3f;
        Laughing.EmissiveIntensity = 1.5f;
        Laughing.HeadTilt = FVector2D(10.0f, 5.0f);
        ExpressionPresets.Add(EExpressionState::Laughing, Laughing);
    }

    // Blissful (JOY_05)
    {
        FExpressionPreset Blissful;
        Blissful.ExpressionState = EExpressionState::Blissful;
        Blissful.MorphTargets.MouthSmileL = 0.5f;
        Blissful.MorphTargets.MouthSmileR = 0.5f;
        Blissful.MorphTargets.EyeCloseL = 0.7f;
        Blissful.MorphTargets.EyeCloseR = 0.7f;
        Blissful.MorphTargets.CheekRaiseL = 0.5f;
        Blissful.MorphTargets.CheekRaiseR = 0.5f;
        Blissful.EmissiveIntensity = 1.2f;
        Blissful.HeadTilt = FVector2D(15.0f, 0.0f);
        ExpressionPresets.Add(EExpressionState::Blissful, Blissful);
    }

    // Playful Grin (JOY_06)
    {
        FExpressionPreset PlayfulGrin;
        PlayfulGrin.ExpressionState = EExpressionState::PlayfulGrin;
        PlayfulGrin.MorphTargets.MouthSmileL = 0.7f;
        PlayfulGrin.MorphTargets.MouthSmileR = 0.5f;  // Asymmetric
        PlayfulGrin.MorphTargets.BrowRaiseL = 0.4f;
        PlayfulGrin.MorphTargets.BrowRaiseR = 0.1f;  // Asymmetric
        PlayfulGrin.MorphTargets.EyeSquintL = 0.2f;
        PlayfulGrin.MorphTargets.EyeSquintR = 0.1f;
        PlayfulGrin.EmissiveIntensity = 1.2f;
        PlayfulGrin.HeadTilt = FVector2D(5.0f, 8.0f);  // Tilted to side
        ExpressionPresets.Add(EExpressionState::PlayfulGrin, PlayfulGrin);
    }

    // Curious Gaze (WONDER_02)
    {
        FExpressionPreset CuriousGaze;
        CuriousGaze.ExpressionState = EExpressionState::CuriousGaze;
        CuriousGaze.MorphTargets.BrowRaiseL = 0.5f;
        CuriousGaze.MorphTargets.BrowRaiseR = 0.5f;
        CuriousGaze.MorphTargets.EyeWideL = 0.3f;
        CuriousGaze.MorphTargets.EyeWideR = 0.3f;
        CuriousGaze.MorphTargets.LipPart = 0.2f;
        CuriousGaze.EmissiveIntensity = 1.1f;
        CuriousGaze.HeadTilt = FVector2D(0.0f, 10.0f);  // Head tilt to side
        ExpressionPresets.Add(EExpressionState::CuriousGaze, CuriousGaze);
    }

    // Awe (WONDER_01)
    {
        FExpressionPreset Awe;
        Awe.ExpressionState = EExpressionState::Awe;
        Awe.MorphTargets.BrowRaiseL = 0.7f;
        Awe.MorphTargets.BrowRaiseR = 0.7f;
        Awe.MorphTargets.EyeWideL = 0.6f;
        Awe.MorphTargets.EyeWideR = 0.6f;
        Awe.MorphTargets.MouthOpen = 0.3f;
        Awe.MorphTargets.LipPart = 0.4f;
        Awe.EmissiveIntensity = 1.4f;
        Awe.HeadTilt = FVector2D(10.0f, 0.0f);
        ExpressionPresets.Add(EExpressionState::Awe, Awe);
    }

    // Speaking Consonant (SPEAK_02)
    {
        FExpressionPreset SpeakingConsonant;
        SpeakingConsonant.ExpressionState = EExpressionState::SpeakingConsonant;
        SpeakingConsonant.MorphTargets.MouthOpen = 0.2f;
        SpeakingConsonant.MorphTargets.LipPucker = 0.3f;
        SpeakingConsonant.MorphTargets.VisemeOO = 0.4f;
        SpeakingConsonant.EmissiveIntensity = 1.0f;
        ExpressionPresets.Add(EExpressionState::SpeakingConsonant, SpeakingConsonant);
    }

    // Alert Gaze (FOCUS_01)
    {
        FExpressionPreset AlertGaze;
        AlertGaze.ExpressionState = EExpressionState::AlertGaze;
        AlertGaze.MorphTargets.EyeWideL = 0.2f;
        AlertGaze.MorphTargets.EyeWideR = 0.2f;
        AlertGaze.MorphTargets.BrowRaiseL = 0.15f;
        AlertGaze.MorphTargets.BrowRaiseR = 0.15f;
        AlertGaze.EmissiveIntensity = 1.15f;
        AlertGaze.HeadTilt = FVector2D(-5.0f, 0.0f);  // Slight forward lean
        ExpressionPresets.Add(EExpressionState::AlertGaze, AlertGaze);
    }

    // Upward Gaze (FOCUS_02)
    {
        FExpressionPreset UpwardGaze;
        UpwardGaze.ExpressionState = EExpressionState::UpwardGaze;
        UpwardGaze.MorphTargets.BrowRaiseL = 0.3f;
        UpwardGaze.MorphTargets.BrowRaiseR = 0.3f;
        UpwardGaze.MorphTargets.EyeWideL = 0.15f;
        UpwardGaze.MorphTargets.EyeWideR = 0.15f;
        UpwardGaze.EmissiveIntensity = 1.0f;
        UpwardGaze.HeadTilt = FVector2D(12.0f, 0.0f);  // Looking up
        ExpressionPresets.Add(EExpressionState::UpwardGaze, UpwardGaze);
    }

    UE_LOG(LogTemp, Log, TEXT("Expression presets initialized: %d expressions"), ExpressionPresets.Num());
}

void UDeepTreeEchoExpressionSystem::SetExpressionState(EExpressionState NewState, float TransitionTime)
{
    if (NewState == CurrentExpressionState && ExpressionTransitionProgress >= 1.0f)
        return;

    TargetExpressionState = NewState;
    ExpressionTransitionDuration = FMath::Max(TransitionTime, 0.01f);
    ExpressionTransitionProgress = 0.0f;
    TransitionStartMorphTargets = ExpressionMorphTargets;
    TransitionStartEmissive = TargetEmissiveIntensity;

    if (ExpressionPresets.Contains(TargetExpressionState))
    {
        TargetMorphTargets = ExpressionPresets[TargetExpressionState].MorphTargets;
        TargetEmissiveIntensity = ExpressionPresets[TargetExpressionState].EmissiveIntensity;
    }

    UE_LOG(LogTemp, Log, TEXT("Expression transition: %d -> %d (%.2fs)"),
        (int32)CurrentExpressionState, (int32)TargetExpressionState, TransitionTime);
}

void UDeepTreeEchoExpressionSystem::ApplyReactionExpression(EExpressionState NewState, float Intensity, float TransitionTime)
{
    const float Clamped = FMath::Clamp(Intensity, 0.0f, 1.0f);

    // Always start a fresh transition: the preset may be unchanged but the intensity may differ
    TargetExpressionState = NewState;
    ExpressionTransitionDuration = FMath::Max(TransitionTime, 0.01f);
    ExpressionTransitionProgress = 0.0f;
    TransitionStartMorphTargets = ExpressionMorphTargets;
    TransitionStartEmissive = TargetEmissiveIntensity;

    if (const FExpressionPreset* Preset = ExpressionPresets.Find(NewState))
    {
        // Preset scaled by the model's intensity (neutral = all zero)
        TargetMorphTargets = FDeepTreeEchoMorphTargets::Lerp(FDeepTreeEchoMorphTargets(), Preset->MorphTargets, Clamped);
        TargetEmissiveIntensity = FMath::Lerp(1.0f, Preset->EmissiveIntensity, Clamped);
    }
    else
    {
        TargetMorphTargets = FDeepTreeEchoMorphTargets();
        TargetEmissiveIntensity = 1.0f;
    }

    ReactionIntensity = Clamped;
}

void UDeepTreeEchoExpressionSystem::SetCognitiveState(ECognitiveState NewState)
{
    if (NewState == CurrentCognitiveState)
    {
        return;
    }

    // Recorded only: the expression is not derived from the cognitive state
    PreviousCognitiveState = CurrentCognitiveState;
    CurrentCognitiveState = NewState;

    UE_LOG(LogTemp, Verbose, TEXT("Cognitive state recorded: %s"), *ReactionEnumName(NewState));

    // The state change is a moment to ask the model how the character reacts
    if (bRequestReactionOnCognitiveStateChange && !bReactionRequestPending)
    {
        const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
        if (Now - LastReactionRequestTime >= MinSecondsBetweenReactionRequests)
        {
            RequestModelReaction();
        }
    }
}

// ========== Model Reactions ==========

TArray<FString> UDeepTreeEchoExpressionSystem::GetReactionVocabulary()
{
    TArray<FString> Names;

    // Expression presets (speaking shapes come from the utterance's visemes, not from a choice)
    if (const UEnum* Enum = StaticEnum<EExpressionState>())
    {
        for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
        {
            const EExpressionState Value = static_cast<EExpressionState>(Enum->GetValueByIndex(Index));
            if (Value != EExpressionState::SpeakingVowel && Value != EExpressionState::SpeakingConsonant)
            {
                Names.Add(Enum->GetNameStringByIndex(Index));
            }
        }
    }

    // Micro-expressions (Blink is physiology; None is not a choice)
    if (const UEnum* Enum = StaticEnum<EMicroExpression>())
    {
        for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
        {
            const EMicroExpression Value = static_cast<EMicroExpression>(Enum->GetValueByIndex(Index));
            if (Value != EMicroExpression::None && Value != EMicroExpression::Blink)
            {
                Names.Add(Enum->GetNameStringByIndex(Index));
            }
        }
    }

    return Names;
}

FString UDeepTreeEchoExpressionSystem::GetReactionCharacterNameResolved() const
{
    if (!ReactionCharacterName.IsEmpty())
    {
        return ReactionCharacterName;
    }
    return GetOwner() ? GetOwner()->GetName() : FString(TEXT("Deep Tree Echo"));
}

FString UDeepTreeEchoExpressionSystem::GetEchobeatsPhaseName(int32 StepNumber)
{
    // Triadic groups of the 12-step loop (labels for the record and the prompt)
    switch (((StepNumber - 1) % 4 + 4) % 4)
    {
    case 0: return TEXT("Pivotal relevance realization");
    case 1: return TEXT("Affordance interaction");
    case 2: return TEXT("Salience processing");
    default: return TEXT("Integration");
    }
}

FString UDeepTreeEchoExpressionSystem::BuildReactionPrompt() const
{
    TMap<FString, FString> Slots;
    Slots.Add(TEXT("character"), GetReactionCharacterNameResolved());
    Slots.Add(TEXT("previous_state"), ReactionEnumName(PreviousCognitiveState));
    Slots.Add(TEXT("state"), ReactionEnumName(CurrentCognitiveState));
    Slots.Add(TEXT("echobeats_step"), CurrentEchobeatsStep.StepNumber > 0
        ? FString::FromInt(CurrentEchobeatsStep.StepNumber) : FString(TEXT("(unknown)")));
    Slots.Add(TEXT("echobeats_phase"), CurrentEchobeatsStep.CognitivePhase.IsEmpty()
        ? FString(TEXT("(unknown)")) : CurrentEchobeatsStep.CognitivePhase);
    Slots.Add(TEXT("speaking"), (bTextLipSyncActive || CurrentSpeakingAmplitude > 0.05f) ? TEXT("yes") : TEXT("no"));
    Slots.Add(TEXT("current_expression"), FString::Printf(TEXT("%s at intensity %.2f"),
        *ReactionEnumName(TargetExpressionState), ReactionIntensity));
    Slots.Add(TEXT("expressions"), FString::Join(GetReactionVocabulary(), TEXT(", ")));
    return UEchoModelClient::FillPrompt(ReactionPromptTemplate, Slots);
}

bool UDeepTreeEchoExpressionSystem::RequestModelReaction()
{
    if (bReactionRequestPending)
    {
        return false;
    }

    bReactionRequestPending = true;
    LastReactionRequestTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

    TArray<FEchoModelMessage> Messages;
    Messages.Add(FEchoModelMessage(TEXT("user"), BuildReactionPrompt()));

    TWeakObjectPtr<UDeepTreeEchoExpressionSystem> WeakThis(this);
    UEchoModelClient::Get()->Chat(Messages, FOnEchoModelReply::CreateLambda(
        [WeakThis](bool bSuccess, const FString& TextOrError)
        {
            if (UDeepTreeEchoExpressionSystem* Self = WeakThis.Get())
            {
                Self->HandleModelReactionReply(bSuccess, TextOrError);
            }
        }), 0.7f);
    return true;
}

void UDeepTreeEchoExpressionSystem::HandleModelReactionReply(bool bSuccess, const FString& TextOrError)
{
    bReactionRequestPending = false;

    // On any failure the face stays as it is; the error is a system event only
    if (!bSuccess)
    {
        LastReactionError = TextOrError;
        UE_LOG(LogTemp, Error, TEXT("DeepTreeEchoExpressionSystem: no model reaction: %s"), *TextOrError);
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
        UE_LOG(LogTemp, Error, TEXT("DeepTreeEchoExpressionSystem: %s"), *LastReactionError);
        return;
    }

    const FString Trimmed = Expression.TrimStartAndEnd();
    const TArray<FString> Vocabulary = GetReactionVocabulary();
    const FString* Allowed = Vocabulary.FindByPredicate(
        [&Trimmed](const FString& Name) { return Name.Equals(Trimmed, ESearchCase::IgnoreCase); });
    if (!Allowed)
    {
        LastReactionError = FString::Printf(TEXT("model chose an expression outside the offered list: %s"), *Expression);
        UE_LOG(LogTemp, Error, TEXT("DeepTreeEchoExpressionSystem: %s"), *LastReactionError);
        return;
    }

    LastReactionError.Reset();

    // Publish the model's reaction; every subscriber (including this component) renders it
    FEchoCharacterReaction Reaction;
    Reaction.CharacterName = GetReactionCharacterNameResolved();
    Reaction.Expression = *Allowed;
    Reaction.Intensity = FMath::Clamp(static_cast<float>(Intensity), 0.0f, 1.0f);
    FEchoReactionBus::OnReaction().Broadcast(Reaction);
}

void UDeepTreeEchoExpressionSystem::HandleCharacterReaction(const FEchoCharacterReaction& Reaction)
{
    if (!ReactionCharacterName.IsEmpty() &&
        !Reaction.CharacterName.Equals(ReactionCharacterName, ESearchCase::IgnoreCase))
    {
        return;
    }

    const float Intensity = FMath::Clamp(Reaction.Intensity, 0.0f, 1.0f);

    // Expression: a preset at the model's intensity, or a micro-expression the model named.
    // An empty or unrecognised name leaves the face unchanged.
    EExpressionState NewExpression;
    EMicroExpression NewMicro;
    if (ParseReactionEnum(Reaction.Expression, NewExpression))
    {
        ApplyReactionExpression(NewExpression, Intensity, 0.4f);
    }
    else if (ParseReactionEnum(Reaction.Expression, NewMicro) && NewMicro != EMicroExpression::None)
    {
        TriggerMicroExpression(NewMicro, FMath::Lerp(0.15f, 0.4f, Intensity));
    }

    // Mouth follows the model's utterance for its playback
    if (bDriveVisemesFromUtterance && !Reaction.Utterance.IsEmpty())
    {
        StartTextLipSync(Reaction.Utterance);
    }
}

// ========== Text-driven visemes ==========

void UDeepTreeEchoExpressionSystem::StartTextLipSync(const FString& Text, float CharactersPerSecond)
{
    if (Text.IsEmpty())
    {
        return;
    }

    LipSyncText = Text;
    LipSyncTime = 0.0f;
    LipSyncCharsPerSecond = FMath::Max(CharactersPerSecond, 1.0f);
    bTextLipSyncActive = true;
}

void UDeepTreeEchoExpressionSystem::StopLipSync()
{
    bTextLipSyncActive = false;
    LipSyncText.Reset();
    LipSyncTime = 0.0f;
    SpeechMorphTargets = FDeepTreeEchoMorphTargets();
}

void UDeepTreeEchoExpressionSystem::UpdateTextLipSync(float DeltaTime)
{
    if (!bTextLipSyncActive)
    {
        return;
    }

    LipSyncTime += DeltaTime;
    const float CharPosition = LipSyncTime * LipSyncCharsPerSecond;
    const int32 CharIndex = FMath::FloorToInt(CharPosition);
    if (CharIndex >= LipSyncText.Len())
    {
        StopLipSync();
        return;
    }

    // Target mouth shape for the current character of the utterance
    FDeepTreeEchoMorphTargets Target;
    const TCHAR C = FChar::ToLower(LipSyncText[CharIndex]);
    switch (C)
    {
    case TEXT('a'):
        Target.VisemeAA = 0.8f; Target.MouthOpen = 0.5f; Target.LipPart = 0.4f;
        break;
    case TEXT('e'): case TEXT('i'): case TEXT('y'):
        Target.VisemeEE = 0.7f; Target.MouthWide = 0.35f; Target.LipPart = 0.3f;
        break;
    case TEXT('o'):
        Target.VisemeOH = 0.75f; Target.MouthOpen = 0.4f; Target.LipPucker = 0.2f;
        break;
    case TEXT('u'): case TEXT('w'): case TEXT('q'):
        Target.VisemeOO = 0.75f; Target.LipPucker = 0.45f;
        break;
    case TEXT('m'): case TEXT('b'): case TEXT('p'):
        // Bilabial closure
        break;
    default:
        if (FChar::IsAlpha(C))
        {
            Target.MouthOpen = 0.15f; Target.LipPart = 0.2f;
        }
        // Spaces and punctuation: mouth at rest
        break;
    }

    // Smooth toward the target so shapes blend between characters
    const float Alpha = FMath::Clamp(DeltaTime * 18.0f, 0.0f, 1.0f);
    SpeechMorphTargets = FDeepTreeEchoMorphTargets::Lerp(SpeechMorphTargets, Target, Alpha);
}

void UDeepTreeEchoExpressionSystem::UpdateExpressionTransition(float DeltaTime)
{
    if (ExpressionTransitionProgress < 1.0f)
    {
        ExpressionTransitionProgress += DeltaTime / ExpressionTransitionDuration;
        ExpressionTransitionProgress = FMath::Clamp(ExpressionTransitionProgress, 0.0f, 1.0f);

        // Smooth step for more natural transitions
        float SmoothAlpha = FMath::SmoothStep(0.0f, 1.0f, ExpressionTransitionProgress);

        // Interpolate the expression layer from where the transition started
        ExpressionMorphTargets = FDeepTreeEchoMorphTargets::Lerp(TransitionStartMorphTargets, TargetMorphTargets, SmoothAlpha);

        // Interpolate emissive
        CurrentEmissiveIntensity = FMath::Lerp(TransitionStartEmissive, TargetEmissiveIntensity, SmoothAlpha);

        if (ExpressionTransitionProgress >= 1.0f)
        {
            CurrentExpressionState = TargetExpressionState;
        }
    }
}

void UDeepTreeEchoExpressionSystem::UpdateProceduralBlinking(float DeltaTime)
{
    if (!bBlinkingEnabled)
        return;

    if (bIsBlinking)
    {
        // Blink animation
        BlinkProgress += DeltaTime / ProceduralSettings.BlinkDuration;
        
        if (BlinkProgress >= 1.0f)
        {
            bIsBlinking = false;
            BlinkProgress = 0.0f;
            NextBlinkTime = FMath::RandRange(ProceduralSettings.MinBlinkInterval, ProceduralSettings.MaxBlinkInterval);
            BlinkTimer = 0.0f;
        }
        else
        {
            // Smooth blink curve (fast close, slower open)
            float BlinkValue;
            if (BlinkProgress < 0.3f)
            {
                // Fast close
                BlinkValue = FMath::Lerp(0.0f, 1.0f, BlinkProgress / 0.3f);
            }
            else
            {
                // Slower open
                BlinkValue = FMath::Lerp(1.0f, 0.0f, (BlinkProgress - 0.3f) / 0.7f);
            }
            
            ProceduralMorphTargets.EyeCloseL = BlinkValue;
            ProceduralMorphTargets.EyeCloseR = BlinkValue;
        }
    }
    else
    {
        // Wait for next blink
        BlinkTimer += DeltaTime;
        if (BlinkTimer >= NextBlinkTime)
        {
            TriggerBlink();
        }
    }
}

void UDeepTreeEchoExpressionSystem::UpdateProceduralBreathing(float DeltaTime)
{
    if (!bBreathingEnabled)
        return;

    BreathingPhase += DeltaTime * ProceduralSettings.BreathingRate * 2.0f * PI;
    if (BreathingPhase > 2.0f * PI)
    {
        BreathingPhase -= 2.0f * PI;
    }

    // Breathing affects subtle mouth/lip movement
    float BreathValue = FMath::Sin(BreathingPhase) * ProceduralSettings.BreathingAmplitude;
    ProceduralMorphTargets.LipPart += BreathValue * 0.5f;
}

void UDeepTreeEchoExpressionSystem::UpdateProceduralSaccades(float DeltaTime)
{
    if (!bSaccadesEnabled)
        return;

    SaccadeTimer += DeltaTime;

    // Check if we should start a new saccade
    if (SaccadeTimer >= 1.0f / ProceduralSettings.SaccadeFrequency)
    {
        SaccadeTimer = 0.0f;
        
        // Generate new random target
        TargetSaccadeOffset.X = FMath::RandRange(-ProceduralSettings.SaccadeAmplitude, ProceduralSettings.SaccadeAmplitude);
        TargetSaccadeOffset.Y = FMath::RandRange(-ProceduralSettings.SaccadeAmplitude, ProceduralSettings.SaccadeAmplitude);
    }

    // Interpolate towards target
    float SaccadeSpeed = ProceduralSettings.SaccadeSpeed * DeltaTime;
    CurrentSaccadeOffset = FMath::Vector2DInterpTo(CurrentSaccadeOffset, TargetSaccadeOffset, DeltaTime, SaccadeSpeed);

    // Apply to eye rotation (this would be used by the animation blueprint)
    CurrentEyeRotation.Pitch = CurrentSaccadeOffset.Y;
    CurrentEyeRotation.Yaw = CurrentSaccadeOffset.X;
}

void UDeepTreeEchoExpressionSystem::UpdateMicroExpression(float DeltaTime)
{
    if (!bMicroExpressionActive)
        return;

    MicroExpressionTimer += DeltaTime;

    if (MicroExpressionTimer >= MicroExpressionDuration)
    {
        bMicroExpressionActive = false;
        ActiveMicroExpression = EMicroExpression::None;
        MicroExpressionTimer = 0.0f;
        return;
    }

    // Apply microexpression based on type
    float MicroProgress = MicroExpressionTimer / MicroExpressionDuration;
    float MicroIntensity = FMath::Sin(MicroProgress * PI);  // Fade in and out

    switch (ActiveMicroExpression)
    {
    case EMicroExpression::Blink:
        ProceduralMorphTargets.EyeCloseL = MicroIntensity;
        ProceduralMorphTargets.EyeCloseR = MicroIntensity;
        break;
        
    case EMicroExpression::UnilateralBrowRaise:
        ProceduralMorphTargets.BrowRaiseL += MicroIntensity * 0.3f;
        break;
        
    case EMicroExpression::LipCornerTwitch:
        ProceduralMorphTargets.MouthSmileL += MicroIntensity * 0.2f;
        break;
        
    case EMicroExpression::NoseWrinkle:
        ProceduralMorphTargets.NoseScrunch += MicroIntensity * 0.4f;
        break;
        
    case EMicroExpression::EyelidFlutter:
        ProceduralMorphTargets.EyeCloseL = FMath::Sin(MicroExpressionTimer * 30.0f) * 0.3f * MicroIntensity;
        ProceduralMorphTargets.EyeCloseR = FMath::Sin(MicroExpressionTimer * 30.0f) * 0.3f * MicroIntensity;
        break;
        
    case EMicroExpression::LipPurse:
        ProceduralMorphTargets.LipPucker += MicroIntensity * 0.3f;
        break;
        
    case EMicroExpression::JawSlack:
        ProceduralMorphTargets.MouthOpen += MicroIntensity * 0.15f;
        break;
        
    default:
        break;
    }
}

void UDeepTreeEchoExpressionSystem::UpdateEmissivePulse(float DeltaTime)
{
    if (!bEmissivePulseActive)
        return;

    EmissivePulseTimer += DeltaTime;

    if (EmissivePulseTimer >= EmissivePulseDuration)
    {
        bEmissivePulseActive = false;
        EmissivePulseTimer = 0.0f;
        return;
    }

    // Pulse curve
    float PulseProgress = EmissivePulseTimer / EmissivePulseDuration;
    float PulseValue = FMath::Sin(PulseProgress * PI);  // Smooth pulse
    
    CurrentEmissiveIntensity = FMath::Lerp(TargetEmissiveIntensity, EmissivePulsePeak, PulseValue);
}

void UDeepTreeEchoExpressionSystem::TriggerMicroExpression(EMicroExpression MicroExp, float Duration)
{
    if (bMicroExpressionActive)
        return;

    ActiveMicroExpression = MicroExp;
    MicroExpressionDuration = Duration;
    MicroExpressionTimer = 0.0f;
    bMicroExpressionActive = true;

    UE_LOG(LogTemp, Log, TEXT("Microexpression triggered: %d for %.2fs"), (int32)MicroExp, Duration);
}

void UDeepTreeEchoExpressionSystem::SetBlinkingEnabled(bool bEnabled)
{
    bBlinkingEnabled = bEnabled;
    if (!bEnabled)
    {
        bIsBlinking = false;
        BlinkProgress = 0.0f;
        ProceduralMorphTargets.EyeCloseL = 0.0f;
        ProceduralMorphTargets.EyeCloseR = 0.0f;
    }
}

void UDeepTreeEchoExpressionSystem::SetBreathingEnabled(bool bEnabled)
{
    bBreathingEnabled = bEnabled;
}

void UDeepTreeEchoExpressionSystem::SetSaccadesEnabled(bool bEnabled)
{
    bSaccadesEnabled = bEnabled;
    if (!bEnabled)
    {
        CurrentSaccadeOffset = FVector2D::ZeroVector;
        TargetSaccadeOffset = FVector2D::ZeroVector;
        CurrentEyeRotation = FRotator::ZeroRotator;
    }
}

void UDeepTreeEchoExpressionSystem::TriggerBlink()
{
    if (!bIsBlinking)
    {
        bIsBlinking = true;
        BlinkProgress = 0.0f;
    }
}

void UDeepTreeEchoExpressionSystem::SetGazeTarget(const FVector& WorldTarget)
{
    GazeTarget = WorldTarget;
    bHasGazeTarget = true;
    
    // Disable procedural saccades when we have a specific target
    bSaccadesEnabled = false;
}

void UDeepTreeEchoExpressionSystem::ClearGazeTarget()
{
    bHasGazeTarget = false;
    bSaccadesEnabled = true;
}

void UDeepTreeEchoExpressionSystem::SetSpeakingAmplitude(float Amplitude)
{
    CurrentSpeakingAmplitude = FMath::Clamp(Amplitude, 0.0f, 1.0f);
    
    // Apply amplitude of the real speech audio to the mouth
    SpeechMorphTargets.MouthOpen = CurrentSpeakingAmplitude * 0.5f;
    SpeechMorphTargets.LipPart = CurrentSpeakingAmplitude * 0.3f;
}

void UDeepTreeEchoExpressionSystem::SetViseme(const FName& VisemeName, float Weight)
{
    Weight = FMath::Clamp(Weight, 0.0f, 1.0f);
    
    if (VisemeName == TEXT("AA") || VisemeName == TEXT("ah"))
    {
        SpeechMorphTargets.VisemeAA = Weight;
        SpeechMorphTargets.MouthOpen = Weight * 0.6f;
    }
    else if (VisemeName == TEXT("EE") || VisemeName == TEXT("ee"))
    {
        SpeechMorphTargets.VisemeEE = Weight;
        SpeechMorphTargets.MouthWide = Weight * 0.4f;
    }
    else if (VisemeName == TEXT("OH") || VisemeName == TEXT("oh"))
    {
        SpeechMorphTargets.VisemeOH = Weight;
        SpeechMorphTargets.MouthOpen = Weight * 0.5f;
    }
    else if (VisemeName == TEXT("OO") || VisemeName == TEXT("oo"))
    {
        SpeechMorphTargets.VisemeOO = Weight;
        SpeechMorphTargets.LipPucker = Weight * 0.5f;
    }
}

void UDeepTreeEchoExpressionSystem::SetEmissiveIntensity(float Intensity)
{
    TargetEmissiveIntensity = FMath::Max(Intensity, 0.0f);
}

void UDeepTreeEchoExpressionSystem::PulseEmissives(float Duration, float PeakIntensity)
{
    if (bEmissivePulseActive)
        return;

    bEmissivePulseActive = true;
    EmissivePulseTimer = 0.0f;
    EmissivePulseDuration = Duration;
    EmissivePulsePeak = PeakIntensity;

    UE_LOG(LogTemp, Log, TEXT("Emissive pulse triggered: %.2fs, peak %.2f"), Duration, PeakIntensity);
}

void UDeepTreeEchoExpressionSystem::OnEchobeatsStep(int32 StepNumber)
{
    // Cadence record only: the 12-step clock never selects an expression
    CurrentEchobeatsStep.StepNumber = StepNumber;
    CurrentEchobeatsStep.CognitivePhase = GetEchobeatsPhaseName(StepNumber);

    UE_LOG(LogTemp, Verbose, TEXT("Echobeats step %d (%s) recorded"), StepNumber, *CurrentEchobeatsStep.CognitivePhase);
}

void UDeepTreeEchoExpressionSystem::ApplyToSkeletalMesh(USkeletalMeshComponent* TargetMesh)
{
    if (!TargetMesh)
        return;

    // Apply morph targets to the skeletal mesh
    // These names should match the morph target names in the skeletal mesh asset
    
    TargetMesh->SetMorphTarget(TEXT("BrowRaiseL"), CurrentMorphTargets.BrowRaiseL);
    TargetMesh->SetMorphTarget(TEXT("BrowRaiseR"), CurrentMorphTargets.BrowRaiseR);
    TargetMesh->SetMorphTarget(TEXT("BrowFurrow"), CurrentMorphTargets.BrowFurrow);
    
    TargetMesh->SetMorphTarget(TEXT("EyeWideL"), CurrentMorphTargets.EyeWideL);
    TargetMesh->SetMorphTarget(TEXT("EyeWideR"), CurrentMorphTargets.EyeWideR);
    TargetMesh->SetMorphTarget(TEXT("EyeSquintL"), CurrentMorphTargets.EyeSquintL);
    TargetMesh->SetMorphTarget(TEXT("EyeSquintR"), CurrentMorphTargets.EyeSquintR);
    TargetMesh->SetMorphTarget(TEXT("EyeCloseL"), CurrentMorphTargets.EyeCloseL);
    TargetMesh->SetMorphTarget(TEXT("EyeCloseR"), CurrentMorphTargets.EyeCloseR);
    
    TargetMesh->SetMorphTarget(TEXT("NoseScrunch"), CurrentMorphTargets.NoseScrunch);
    
    TargetMesh->SetMorphTarget(TEXT("CheekRaiseL"), CurrentMorphTargets.CheekRaiseL);
    TargetMesh->SetMorphTarget(TEXT("CheekRaiseR"), CurrentMorphTargets.CheekRaiseR);
    
    TargetMesh->SetMorphTarget(TEXT("MouthSmileL"), CurrentMorphTargets.MouthSmileL);
    TargetMesh->SetMorphTarget(TEXT("MouthSmileR"), CurrentMorphTargets.MouthSmileR);
    TargetMesh->SetMorphTarget(TEXT("MouthOpen"), CurrentMorphTargets.MouthOpen);
    TargetMesh->SetMorphTarget(TEXT("MouthWide"), CurrentMorphTargets.MouthWide);
    TargetMesh->SetMorphTarget(TEXT("LipPucker"), CurrentMorphTargets.LipPucker);
    TargetMesh->SetMorphTarget(TEXT("LipPart"), CurrentMorphTargets.LipPart);
    
    TargetMesh->SetMorphTarget(TEXT("VisemeAA"), CurrentMorphTargets.VisemeAA);
    TargetMesh->SetMorphTarget(TEXT("VisemeEE"), CurrentMorphTargets.VisemeEE);
    TargetMesh->SetMorphTarget(TEXT("VisemeOH"), CurrentMorphTargets.VisemeOH);
    TargetMesh->SetMorphTarget(TEXT("VisemeOO"), CurrentMorphTargets.VisemeOO);
}

void UDeepTreeEchoExpressionSystem::ApplyToMaterial(UMaterialInstanceDynamic* TargetMaterial)
{
    if (!TargetMaterial)
        return;

    // Apply emissive intensity to material
    TargetMaterial->SetScalarParameterValue(TEXT("EmissiveIntensity"), CurrentEmissiveIntensity);
    
    // Could also set color parameters based on cognitive state
    // TargetMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), ...);
}
