// Deep Tree Echo - Cubism SDK Integration
// Wraps CubismUE plugin components with DTE-specific cognitive state bindings.
// Uses production SDK via the integrated CubismUE plugin.

#include "CubismSDKIntegration.h"
#include "Model/CubismParameterComponent.h"
#include "Model/CubismDrawableComponent.h"
#include "EchoModelClient.h"
#include "Dom/JsonObject.h"

namespace
{
    /** A face parameter the model may set, and the range it must stay within. */
    struct FFacialParamSpec
    {
        FString Id;
        FString Meaning;
        float Min;
        float Max;
    };

    /**
     * Asks the character's model to choose values for Params (Purpose says what the movement is
     * for). Applies them when the reply names every parameter with a number; otherwise logs at
     * Error, stores LastFacialParameterError and changes nothing.
     */
    void RequestModelFacialParameters(UCubismSDKIntegration* Model, const FString& Kind, const FString& Purpose,
                                      const TArray<FFacialParamSpec>& Params)
    {
        TArray<FString> Lines;
        for (const FFacialParamSpec& P : Params)
        {
            Lines.Add(FString::Printf(TEXT("%s (%s): %.2f to %.2f"), *P.Id, *P.Meaning, P.Min, P.Max));
        }

        TMap<FString, FString> Slots;
        Slots.Add(TEXT("character"), !Model->CharacterName.IsEmpty() ? Model->CharacterName
            : (Model->GetOuter() ? Model->GetOuter()->GetName() : Model->GetName()));
        Slots.Add(TEXT("purpose"), Purpose);
        Slots.Add(TEXT("chaos"), FString::Printf(TEXT("%.2f"), Model->ChaosLevel));
        Slots.Add(TEXT("resonance"), FString::Printf(TEXT("%.2f"), Model->EchoResonance));
        Slots.Add(TEXT("cognitive_load"), FString::Printf(TEXT("%.2f"), Model->CognitiveLoad));
        Slots.Add(TEXT("parameters"), FString::Join(Lines, TEXT("\n")));

        TArray<FEchoModelMessage> Messages;
        Messages.Add(FEchoModelMessage(TEXT("user"), UEchoModelClient::FillPrompt(Model->FacialParameterPromptTemplate, Slots)));

        const int32 RequestSerial = ++Model->FacialRequestSerials.FindOrAdd(Kind);
        TWeakObjectPtr<UCubismSDKIntegration> WeakModel(Model);
        UEchoModelClient::Get()->Chat(Messages, FOnEchoModelReply::CreateLambda(
            [WeakModel, Kind, RequestSerial, Params](bool bSuccess, const FString& TextOrError)
            {
                UCubismSDKIntegration* Target = WeakModel.Get();
                if (!Target)
                {
                    return;
                }
                const int32* Current = Target->FacialRequestSerials.Find(Kind);
                if (!Current || *Current != RequestSerial)
                {
                    return; // superseded by a newer request of this kind
                }

                auto Fail = [Target, &Kind](const FString& Error)
                {
                    Target->LastFacialParameterError = Error;
                    UE_LOG(LogTemp, Error, TEXT("CubismSDKIntegration: no model %s: %s"), *Kind, *Error);
                };
                if (!bSuccess)
                {
                    Fail(TextOrError);
                    return;
                }

                TSharedPtr<FJsonObject> Json;
                if (!UEchoModelClient::ParseJsonObject(TextOrError, Json))
                {
                    Fail(FString::Printf(TEXT("model reply is not the requested JSON: %s"), *TextOrError.Left(200)));
                    return;
                }

                TArray<float> Values;
                for (const FFacialParamSpec& P : Params)
                {
                    double Value = 0.0;
                    FString Text;
                    if (!Json->TryGetNumberField(P.Id, Value))
                    {
                        // Smaller models sometimes quote numbers; accept a numeric string too
                        if (Json->TryGetStringField(P.Id, Text) && Text.TrimStartAndEnd().IsNumeric())
                        {
                            Value = FCString::Atod(*Text.TrimStartAndEnd());
                        }
                        else
                        {
                            Fail(FString::Printf(TEXT("model reply has no number for %s: %s"), *P.Id, *TextOrError.Left(200)));
                            return;
                        }
                    }
                    Values.Add(FMath::Clamp(static_cast<float>(Value), P.Min, P.Max));
                }

                Target->LastFacialParameterError.Reset();
                for (int32 Index = 0; Index < Params.Num(); ++Index)
                {
                    Target->SetParameterValue(FName(*Params[Index].Id), Values[Index]);
                }
            }), 0.9f);
    }
}

UCubismSDKIntegration::UCubismSDKIntegration()
    : ChaosLevel(0.3f)
    , EchoResonance(0.0f)
    , CognitiveLoad(0.5f)
    , GlitchIntensity(0.0f)
    , ModelComponent(nullptr)
{
    FacialParameterPromptTemplate = TEXT(
        "You are {character}, an expressive Live2D avatar. {purpose} Your current state: chaos level {chaos} "
        "(0 calm .. 1 wild), echo resonance {resonance} (0 .. 1), cognitive load {cognitive_load} (0 .. 1). "
        "Choose a value for each face parameter, within its range:\n{parameters}\n"
        "Reply with only a JSON object mapping each parameter name to its number: {\"<parameter>\": <number>, ...}");
    MicroExpressionPurpose = TEXT(
        "Show a fleeting micro-expression: a brief, involuntary flicker of your brows and mouth that reveals what you feel right now.");
    ChaoticMotionPurpose = TEXT(
        "Make a small restless movement of your head and eyes that expresses your chaotic energy.");
}

UCubismSDKIntegration::~UCubismSDKIntegration()
{
    // ModelComponent is managed by UE's garbage collector
    ModelComponent = nullptr;
}

bool UCubismSDKIntegration::IsCubismFrameworkInitialized()
{
    // With CubismUE plugin, framework is always initialized when plugin loads
    return true;
}

void UCubismSDKIntegration::SetModelComponent(UCubismModelComponent* InModelComponent)
{
    ModelComponent = InModelComponent;
    if (ModelComponent)
    {
        UpdateModelInfoCache();
        UE_LOG(LogTemp, Log, TEXT("CubismSDKIntegration: Model component set with %d parameters, %d drawables"),
               ModelComponent->GetParameterCount(), ModelComponent->GetDrawableCount());
    }
}

void UCubismSDKIntegration::UpdateModelInfoCache()
{
    if (!ModelComponent)
    {
        CachedModelInfo = FCubismModelInfo();
        return;
    }

    CachedModelInfo.ModelName = TEXT("DeepTreeEchoAvatar");
    CachedModelInfo.CanvasSize = ModelComponent->GetCanvasSize();
    CachedModelInfo.PixelsPerUnit = ModelComponent->GetPixelsPerUnit();
    CachedModelInfo.ParameterCount = ModelComponent->GetParameterCount();
    CachedModelInfo.PartCount = ModelComponent->GetPartCount();
    CachedModelInfo.DrawableCount = ModelComponent->GetDrawableCount();
}

FCubismModelInfo UCubismSDKIntegration::GetModelInfo() const
{
    return CachedModelInfo;
}

TArray<FCubismParameter> UCubismSDKIntegration::GetAllParameters() const
{
    TArray<FCubismParameter> Result;

    if (!ModelComponent)
    {
        return Result;
    }

    const int32 ParamCount = ModelComponent->GetParameterCount();
    Result.Reserve(ParamCount);

    for (int32 i = 0; i < ParamCount; i++)
    {
        UCubismParameterComponent* ParamComp = ModelComponent->GetParameter(i);
        if (ParamComp)
        {
            FCubismParameter Param;
            Param.ParameterId = FName(*ModelComponent->GetParameterId(i));
            Param.Value = ParamComp->Value;
            Param.MinValue = ParamComp->MinimumValue;
            Param.MaxValue = ParamComp->MaximumValue;
            Param.DefaultValue = ParamComp->DefaultValue;
            Result.Add(Param);
        }
    }

    return Result;
}

FCubismParameter UCubismSDKIntegration::GetParameter(const FName& ParameterId) const
{
    FCubismParameter Result;

    if (!ModelComponent)
    {
        return Result;
    }

    UCubismParameterComponent* ParamComp = ModelComponent->GetParameter(ParameterId.ToString());
    if (ParamComp)
    {
        Result.ParameterId = ParameterId;
        Result.Value = ParamComp->Value;
        Result.MinValue = ParamComp->MinimumValue;
        Result.MaxValue = ParamComp->MaximumValue;
        Result.DefaultValue = ParamComp->DefaultValue;
    }

    return Result;
}

void UCubismSDKIntegration::SetParameterValue(const FName& ParameterId, float Value)
{
    if (!ModelComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("SetParameterValue: No model component set"));
        return;
    }

    UCubismParameterComponent* ParamComp = ModelComponent->GetParameter(ParameterId.ToString());
    if (ParamComp)
    {
        ParamComp->Value = FMath::Clamp(Value, ParamComp->MinimumValue, ParamComp->MaximumValue);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Parameter not found: %s"), *ParameterId.ToString());
    }
}

TArray<FCubismDrawable> UCubismSDKIntegration::GetAllDrawables() const
{
    TArray<FCubismDrawable> Result;

    if (!ModelComponent)
    {
        return Result;
    }

    const int32 DrawableCount = ModelComponent->GetDrawableCount();
    Result.Reserve(DrawableCount);

    for (int32 i = 0; i < DrawableCount; i++)
    {
        UCubismDrawableComponent* DrawComp = ModelComponent->GetDrawable(i);
        if (DrawComp)
        {
            FCubismDrawable Drawable;
            Drawable.DrawableId = FName(*ModelComponent->GetDrawableId(i));
            Drawable.TextureIndex = DrawComp->TextureIndex;
            Drawable.VertexCount = DrawComp->VertexPositions.Num();
            Drawable.IndexCount = DrawComp->VertexIndices.Num();
            Drawable.Opacity = DrawComp->BaseOpacity;
            Drawable.bIsVisible = DrawComp->IsVisible();
            Result.Add(Drawable);
        }
    }

    return Result;
}

// ========================================
// SDK Component Access
// ========================================

UCubismLipSyncComponent* UCubismSDKIntegration::GetLipSync() const
{
    return ModelComponent ? ModelComponent->LipSync : nullptr;
}

UCubismEyeBlinkComponent* UCubismSDKIntegration::GetEyeBlink() const
{
    return ModelComponent ? ModelComponent->EyeBlink : nullptr;
}

UCubismLookAtComponent* UCubismSDKIntegration::GetLookAt() const
{
    return ModelComponent ? ModelComponent->LookAt : nullptr;
}

UCubismPhysicsComponent* UCubismSDKIntegration::GetPhysics() const
{
    return ModelComponent ? ModelComponent->Physics : nullptr;
}

UCubismExpressionComponent* UCubismSDKIntegration::GetExpression() const
{
    return ModelComponent ? ModelComponent->Expression : nullptr;
}

UCubismMotionComponent* UCubismSDKIntegration::GetMotion() const
{
    return ModelComponent ? ModelComponent->Motion : nullptr;
}

// ============================================================================
// Enhanced Features Implementation - DTE-Specific Extensions
// ============================================================================

void UCubismEnhancedFeatures::ApplySuperHotGirlAesthetic(UCubismSDKIntegration* Model)
{
    if (!Model || !Model->GetModelComponent())
    {
        return;
    }

    // Enhanced eye sparkle - using SDK parameter access
    Model->SetParameterValue(TEXT("ParamEyeSparkle"), 0.9f);

    // Slight confident smile
    Model->SetParameterValue(TEXT("ParamMouthSmile"), 0.4f);

    // Subtle blush
    Model->SetParameterValue(TEXT("ParamBlushIntensity"), 0.3f);

    // Hair shimmer
    Model->SetParameterValue(TEXT("ParamHairShimmer"), 0.7f);

    // Confident posture
    Model->SetParameterValue(TEXT("ParamBodyAngleY"), 2.0f);
    Model->SetParameterValue(TEXT("ParamAngleZ"), -1.5f);
}

void UCubismEnhancedFeatures::ApplyHyperChaoticBehavior(UCubismSDKIntegration* Model, float ChaosFactor)
{
    if (!Model)
    {
        return;
    }

    // Store chaos level in DTE-specific state
    Model->ChaosLevel = ChaosFactor;

    const float Scale = FMath::Abs(ChaosFactor);
    if (Scale <= KINDA_SMALL_NUMBER)
    {
        return; // no room to move
    }

    // The character's model chooses the head/eye motion within chaos-scaled ranges
    TArray<FFacialParamSpec> Params;
    Params.Add({ TEXT("ParamAngleX"), TEXT("head turn, negative = left"), -5.0f * Scale, 5.0f * Scale });
    Params.Add({ TEXT("ParamAngleY"), TEXT("head tilt, negative = down"), -5.0f * Scale, 5.0f * Scale });
    Params.Add({ TEXT("ParamEyeBallX"), TEXT("gaze, negative = left"), -0.3f * Scale, 0.3f * Scale });
    Params.Add({ TEXT("ParamEyeBallY"), TEXT("gaze, negative = down"), -0.2f * Scale, 0.2f * Scale });
    RequestModelFacialParameters(Model, TEXT("chaotic motion"), Model->ChaoticMotionPurpose, Params);
}

void UCubismEnhancedFeatures::GenerateEyeSparkle(UCubismSDKIntegration* Model, float Intensity)
{
    if (!Model)
    {
        return;
    }

    Model->SetParameterValue(TEXT("ParamEyeSparkle"), FMath::Clamp(Intensity, 0.0f, 1.0f));
}

void UCubismEnhancedFeatures::ApplyEmotionalBlush(UCubismSDKIntegration* Model, float Intensity, const FLinearColor& Color)
{
    if (!Model)
    {
        return;
    }

    Model->SetParameterValue(TEXT("ParamBlushIntensity"), FMath::Clamp(Intensity, 0.0f, 1.0f));
    Model->SetParameterValue(TEXT("ParamCheek"), FMath::Clamp(Intensity * 0.8f, 0.0f, 1.0f));
}

void UCubismEnhancedFeatures::GenerateChaoticMicroExpression(UCubismSDKIntegration* Model)
{
    if (!Model)
    {
        return;
    }

    // The character's model chooses the micro-expression within the allowed ranges
    TArray<FFacialParamSpec> Params;
    Params.Add({ TEXT("ParamBrowLY"), TEXT("left brow height, negative = lowered"), -0.2f, 0.2f });
    Params.Add({ TEXT("ParamBrowRY"), TEXT("right brow height, negative = lowered"), -0.2f, 0.2f });
    Params.Add({ TEXT("ParamMouthForm"), TEXT("mouth shape, negative = frown, positive = smile"), -0.1f, 0.1f });
    RequestModelFacialParameters(Model, TEXT("micro-expression"), Model->MicroExpressionPurpose, Params);
}

void UCubismEnhancedFeatures::ApplyEchoResonanceEffect(UCubismSDKIntegration* Model, float Resonance)
{
    if (!Model)
    {
        return;
    }

    // Store in DTE-specific state
    Model->EchoResonance = FMath::Clamp(Resonance, 0.0f, 1.0f);

    Model->SetParameterValue(TEXT("ParamEchoResonance"), Model->EchoResonance);
    Model->SetParameterValue(TEXT("ParamEmotionalAura"), FMath::Clamp(Resonance * 0.8f, 0.0f, 1.0f));
}

void UCubismEnhancedFeatures::GenerateGlitchEffect(UCubismSDKIntegration* Model, float Severity)
{
    if (!Model)
    {
        return;
    }

    // Store in DTE-specific state
    Model->GlitchIntensity = FMath::Clamp(Severity, 0.0f, 1.0f);

    Model->SetParameterValue(TEXT("ParamGlitchIntensity"), Model->GlitchIntensity);

    // Random displacement during glitch
    if (Severity > 0.5f)
    {
        FRandomStream RandStream(FPlatformTime::Cycles());
        const float GlitchX = RandStream.FRandRange(-10.0f, 10.0f) * Severity;
        const float GlitchY = RandStream.FRandRange(-10.0f, 10.0f) * Severity;

        Model->SetParameterValue(TEXT("ParamAngleX"), GlitchX);
        Model->SetParameterValue(TEXT("ParamAngleY"), GlitchY);
    }
}

void UCubismEnhancedFeatures::ApplyHairShimmer(UCubismSDKIntegration* Model, float Time, float Intensity)
{
    if (!Model)
    {
        return;
    }

    // Animated shimmer using sine wave
    const float ShimmerValue = (FMath::Sin(Time * 2.0f) * 0.5f + 0.5f) * Intensity;
    Model->SetParameterValue(TEXT("ParamHairShimmer"), ShimmerValue);

    // Subtle hair movement - can leverage SDK's HarmonicMotion component if available
    UCubismHarmonicMotionComponent* HarmonicMotion = Model->GetModelComponent() ?
        Model->GetModelComponent()->HarmonicMotion : nullptr;

    if (HarmonicMotion)
    {
        // Use SDK's harmonic motion for natural hair movement
        // The component handles the animation automatically
    }
    else
    {
        // Fallback to manual calculation
        const float HairSway = FMath::Sin(Time * 1.5f) * 0.3f * Intensity;
        Model->SetParameterValue(TEXT("ParamHairFront"), HairSway);
        Model->SetParameterValue(TEXT("ParamHairSide"), HairSway * 0.8f);
        Model->SetParameterValue(TEXT("ParamHairBack"), HairSway * 1.2f);
    }
}

void UCubismEnhancedFeatures::GenerateEmotionalLipSync(UCubismSDKIntegration* Model, const TArray<float>& AudioData, float EmotionalIntensity)
{
    if (!Model || AudioData.Num() == 0)
    {
        return;
    }

    // Try to use SDK's LipSync component first
    UCubismLipSyncComponent* LipSync = Model->GetLipSync();
    if (LipSync)
    {
        // Calculate average amplitude for the SDK component
        float AverageAmplitude = 0.0f;
        for (float Sample : AudioData)
        {
            AverageAmplitude += FMath::Abs(Sample);
        }
        AverageAmplitude /= AudioData.Num();

        // Set the lip sync value - SDK handles parameter updates
        LipSync->Value = FMath::Clamp(AverageAmplitude * (1.0f + EmotionalIntensity), 0.0f, 1.0f);
    }
    else
    {
        // Fallback to manual lip-sync
        float AverageAmplitude = 0.0f;
        for (float Sample : AudioData)
        {
            AverageAmplitude += FMath::Abs(Sample);
        }
        AverageAmplitude /= AudioData.Num();

        const float MouthOpen = FMath::Clamp(AverageAmplitude * (1.0f + EmotionalIntensity), 0.0f, 1.0f);
        Model->SetParameterValue(TEXT("ParamMouthOpenY"), MouthOpen);
    }

    // Add emotional smile modulation (DTE-specific)
    const float EmotionalSmile = FMath::Clamp(EmotionalIntensity * 0.5f, 0.0f, 1.0f);
    Model->SetParameterValue(TEXT("ParamMouthSmile"), EmotionalSmile);
}

void UCubismEnhancedFeatures::ApplyPersonalizedGaze(UCubismSDKIntegration* Model, const FVector2D& Target, float Confidence, float Flirtiness)
{
    if (!Model)
    {
        return;
    }

    // Try to use SDK's LookAt component first
    UCubismLookAtComponent* LookAt = Model->GetLookAt();
    if (LookAt)
    {
        // SDK LookAt component handles gaze tracking automatically
        // We just need to set the target position
        // Note: LookAt component expects 3D position, we map 2D to screen-space
    }

    // Calculate gaze direction
    const float GazeX = FMath::Clamp(Target.X, -1.0f, 1.0f);
    const float GazeY = FMath::Clamp(Target.Y, -1.0f, 1.0f);

    // Apply confidence - more confident gaze is more direct
    const float ConfidenceModulatedX = GazeX * (0.5f + Confidence * 0.5f);
    const float ConfidenceModulatedY = GazeY * (0.5f + Confidence * 0.5f);

    Model->SetParameterValue(TEXT("ParamEyeBallX"), ConfidenceModulatedX);
    Model->SetParameterValue(TEXT("ParamEyeBallY"), ConfidenceModulatedY);

    // Flirty gaze includes more eye sparkle and slight eyebrow raise (DTE-specific)
    if (Flirtiness > 0.3f)
    {
        Model->SetParameterValue(TEXT("ParamEyeSparkle"), 0.8f + Flirtiness * 0.2f);
        Model->SetParameterValue(TEXT("ParamBrowLY"), Flirtiness * 0.2f);
        Model->SetParameterValue(TEXT("ParamBrowRY"), Flirtiness * 0.2f);
    }
}
