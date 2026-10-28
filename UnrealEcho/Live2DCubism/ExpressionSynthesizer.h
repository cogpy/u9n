#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Cognitive/EmotionalState.h"
#include "ExpressionSynthesizer.generated.h"

// The emotional state rendered by the synthesizer (FEmotionalState) must come from the
// character model's reactions; the synthesizer only renders it.

/**
 * Renders an emotional state as Live2D Cubism parameters (plus blinking/breathing idle
 * motion). Adds no expressions of its own.
 */
UCLASS(Blueprintable)
class DEEPTREECHO_API UExpressionSynthesizer : public UObject
{
    GENERATED_BODY()

public:
    UExpressionSynthesizer();

    /** Synthesizes Live2D parameters from an emotional state */
    UFUNCTION(BlueprintCallable, Category = "Live2D")
        void SynthesizeExpression(const FEmotionalState& EmotionalState, TMap<FName, float>& OutParameters, float DeltaTime);

    UFUNCTION(BlueprintCallable, Category = "Live2D")
    void SetExpressionIntensity(float Intensity);

    UFUNCTION(BlueprintCallable, Category = "Live2D")
    void SetSmoothingSpeed(float Speed);

    UFUNCTION(BlueprintCallable, Category = "Live2D")
    void ResetExpression();

    UFUNCTION(BlueprintCallable, Category = "Live2D")
    TMap<FName, float> GetCurrentParameters() const;

    UFUNCTION(BlueprintCallable, Category = "Live2D")
    float GetParameterValue(const FName& ParameterName) const;

    UFUNCTION(BlueprintCallable, Category = "Live2D")
    void SetParameterValue(const FName& ParameterName, float Value);

    UFUNCTION(BlueprintCallable, Category = "Live2D")
    void BlendExpressions(const TMap<FName, float>& ExpressionA, const TMap<FName, float>& ExpressionB,
                          float BlendWeight, TMap<FName, float>& OutBlendedExpression);

    /** Mouth opening from the amplitude of the speech audio being played */
    UFUNCTION(BlueprintCallable, Category = "Live2D")
    void ApplyEmotionalLipSync(const TArray<float>& AudioData, float EmotionalIntensity, TMap<FName, float>& OutParameters);

private:
    /** Maps emotional dimensions to facial parameters */
    void MapEmotionToParameters(const FEmotionalState& EmotionalState, TMap<FName, float>& OutParameters);

    /** Physiological idle motion: blinking, breathing, idle head sway */
    void GenerateAutonomousBehaviors(TMap<FName, float>& Parameters, float DeltaTime);

    /** Applies temporal smoothing to parameter changes */
    void ApplyParametersSmoothed(TMap<FName, float>& TargetParameters, float DeltaTime);

    /** The current smoothed parameters */
    UPROPERTY()
    TMap<FName, float> CurrentParameters;

    float BlinkTimer;
    float BlinkInterval;
    bool bIsBlinking;
    float BlinkDuration;
    float ExpressionIntensityMultiplier;
    float SmoothingSpeed;
};
