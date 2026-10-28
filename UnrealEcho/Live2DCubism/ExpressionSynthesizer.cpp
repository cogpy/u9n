// Enhanced Expression Synthesizer - Production Implementation
// Deep Tree Echo AGI Avatar System
// Maps emotional states to Live2D Cubism parameters.
//
// This is a renderer: it turns the emotional state it is given into parameters. It adds
// no expressions of its own (no random micro-expressions, personality "smile floors" or
// chaotic jitter); the emotional state must come from the character model's reactions.
// Blinking, breathing and idle head sway are physiological idle motion.

#include "ExpressionSynthesizer.h"
#include "Math/UnrealMathUtility.h"
#include "GenericPlatform/GenericPlatformMath.h"

UExpressionSynthesizer::UExpressionSynthesizer()
{
    // Initialize default parameters
    CurrentParameters.Empty();
    BlinkTimer = 0.0f;
    BlinkInterval = 3.0f; // Blink every 3 seconds on average
    bIsBlinking = false;
    BlinkDuration = 0.15f;
    ExpressionIntensityMultiplier = 1.0f;
    SmoothingSpeed = 5.0f;
}

void UExpressionSynthesizer::SynthesizeExpression(const FEmotionalState& EmotionalState, 
                                                  TMap<FName, float>& OutParameters, 
                                                  float DeltaTime)
{
    TMap<FName, float> TargetParameters;
    
    // Map emotional dimensions to facial parameters
    MapEmotionToParameters(EmotionalState, TargetParameters);
    
    // Add physiological idle motion (blinking, breathing)
    GenerateAutonomousBehaviors(TargetParameters, DeltaTime);
    
    // Smooth parameter transitions
    ApplyParametersSmoothed(TargetParameters, DeltaTime);
    
    // Output final parameters
    OutParameters = CurrentParameters;
}

void UExpressionSynthesizer::MapEmotionToParameters(const FEmotionalState& EmotionalState, 
                                                    TMap<FName, float>& OutParameters)
{
    // Eyes - influenced by happiness, surprise, and sadness
    float EyeOpenness = 1.0f;
    EyeOpenness += EmotionalState.Happiness * 0.2f;      // Wider eyes when happy
    EyeOpenness += EmotionalState.Surprise * 0.5f;       // Very wide eyes when surprised
    EyeOpenness -= EmotionalState.Sadness * 0.4f;        // Narrower eyes when sad
    EyeOpenness -= EmotionalState.Disgust * 0.3f;        // Squinted eyes when disgusted
    EyeOpenness = FMath::Clamp(EyeOpenness, 0.0f, 1.5f);
    
    OutParameters.Add(TEXT("ParamEyeLOpen"), EyeOpenness);
    OutParameters.Add(TEXT("ParamEyeROpen"), EyeOpenness);
    
    // Eye sparkle - enhanced for super-hot-girl aesthetic
    float EyeSparkle = 0.5f;
    EyeSparkle += EmotionalState.Happiness * 0.4f;
    EyeSparkle += EmotionalState.Excitement * 0.3f;
    EyeSparkle = FMath::Clamp(EyeSparkle, 0.0f, 1.0f);
    OutParameters.Add(TEXT("ParamEyeSparkle"), EyeSparkle);
    
    // Eyebrows - influenced by surprise, anger, and sadness
    float BrowY = 0.0f;
    BrowY += EmotionalState.Surprise * 0.8f;             // Raised brows when surprised
    BrowY -= EmotionalState.Sadness * 0.5f;              // Lowered brows when sad
    BrowY -= EmotionalState.Anger * 0.3f;                // Lowered brows when angry
    BrowY = FMath::Clamp(BrowY, -1.0f, 1.0f);
    
    OutParameters.Add(TEXT("ParamBrowLY"), BrowY);
    OutParameters.Add(TEXT("ParamBrowRY"), BrowY);
    
    // Brow angle - influenced by anger and concentration
    float BrowAngle = 0.0f;
    BrowAngle -= EmotionalState.Anger * 0.7f;            // Furrowed brows when angry
    BrowAngle -= EmotionalState.Fear * 0.5f;             // Raised inner brows when fearful
    BrowAngle = FMath::Clamp(BrowAngle, -1.0f, 1.0f);
    
    OutParameters.Add(TEXT("ParamBrowLAngle"), BrowAngle);
    OutParameters.Add(TEXT("ParamBrowRAngle"), BrowAngle);
    
    // Mouth smile - influenced by happiness
    float MouthSmile = 0.0f;
    MouthSmile += EmotionalState.Happiness * 0.9f;       // Big smile when happy
    MouthSmile += EmotionalState.Excitement * 0.6f;      // Smile when excited
    MouthSmile -= EmotionalState.Sadness * 0.7f;         // Frown when sad
    MouthSmile -= EmotionalState.Disgust * 0.5f;         // Grimace when disgusted
    MouthSmile = FMath::Clamp(MouthSmile, -1.0f, 1.0f);
    
    OutParameters.Add(TEXT("ParamMouthSmile"), MouthSmile);
    
    // Mouth form - influenced by various emotions
    float MouthForm = 0.0f;
    MouthForm -= EmotionalState.Sadness * 0.6f;          // Downturned mouth when sad
    MouthForm += EmotionalState.Surprise * 0.4f;         // Open mouth when surprised
    MouthForm -= EmotionalState.Disgust * 0.7f;          // Pursed lips when disgusted
    MouthForm = FMath::Clamp(MouthForm, -1.0f, 1.0f);
    
    OutParameters.Add(TEXT("ParamMouthForm"), MouthForm);
    
    // Mouth openness - influenced by surprise and speech
    float MouthOpen = 0.0f;
    MouthOpen += EmotionalState.Surprise * 0.7f;         // Open mouth when surprised
    MouthOpen += EmotionalState.Excitement * 0.3f;       // Slightly open when excited
    MouthOpen = FMath::Clamp(MouthOpen, 0.0f, 1.0f);
    
    OutParameters.Add(TEXT("ParamMouthOpenY"), MouthOpen);
    
    // Cheeks - influenced by happiness and embarrassment
    float Cheek = 0.0f;
    Cheek += EmotionalState.Happiness * 0.5f;            // Raised cheeks when smiling
    Cheek += EmotionalState.Excitement * 0.3f;
    Cheek = FMath::Clamp(Cheek, 0.0f, 1.0f);
    
    OutParameters.Add(TEXT("ParamCheek"), Cheek);
    
    // Blush - influenced by embarrassment, happiness, and excitement
    float BlushIntensity = 0.0f;
    BlushIntensity += EmotionalState.Happiness * 0.3f;
    BlushIntensity += EmotionalState.Excitement * 0.4f;
    BlushIntensity = FMath::Clamp(BlushIntensity, 0.0f, 1.0f);
    
    OutParameters.Add(TEXT("ParamBlushIntensity"), BlushIntensity);
}

void UExpressionSynthesizer::GenerateAutonomousBehaviors(TMap<FName, float>& Parameters, float DeltaTime)
{
    // Blinking behavior
    BlinkTimer += DeltaTime;
    
    if (!bIsBlinking && BlinkTimer >= BlinkInterval)
    {
        // Start blink
        bIsBlinking = true;
        BlinkTimer = 0.0f;
        BlinkInterval = FMath::FRandRange(2.0f, 5.0f); // Random interval between blinks
    }
    
    if (bIsBlinking)
    {
        float BlinkProgress = BlinkTimer / BlinkDuration;
        
        if (BlinkProgress < 1.0f)
        {
            // Close eyes (sine wave for smooth motion)
            float BlinkValue = FMath::Sin(BlinkProgress * PI);
            float EyeClose = 1.0f - BlinkValue;
            
            Parameters.Add(TEXT("ParamEyeLOpen"), 
                         Parameters.FindRef(TEXT("ParamEyeLOpen")) * EyeClose);
            Parameters.Add(TEXT("ParamEyeROpen"), 
                         Parameters.FindRef(TEXT("ParamEyeROpen")) * EyeClose);
        }
        else
        {
            // End blink
            bIsBlinking = false;
            BlinkTimer = 0.0f;
        }
    }
    
    // Breathing motion (subtle body movement)
    float BreathCycle = FMath::Sin(GetWorld()->GetTimeSeconds() * 0.5f) * 0.5f + 0.5f;
    Parameters.Add(TEXT("ParamBreath"), BreathCycle);
    
    // Subtle idle head movement
    float IdleHeadX = FMath::Sin(GetWorld()->GetTimeSeconds() * 0.3f) * 2.0f;
    float IdleHeadY = FMath::Cos(GetWorld()->GetTimeSeconds() * 0.25f) * 1.5f;
    
    Parameters.Add(TEXT("ParamAngleX"), 
                 Parameters.FindRef(TEXT("ParamAngleX")) + IdleHeadX);
    Parameters.Add(TEXT("ParamAngleY"), 
                 Parameters.FindRef(TEXT("ParamAngleY")) + IdleHeadY);
}

void UExpressionSynthesizer::ApplyParametersSmoothed(TMap<FName, float>& TargetParameters, float DeltaTime)
{
    for (auto& Pair : TargetParameters)
    {
        const FName& ParamName = Pair.Key;
        float TargetValue = Pair.Value;
        
        // Clamp target value
        TargetValue = FMath::Clamp(TargetValue, -1.0f, 1.0f);
        
        float* CurrentValue = CurrentParameters.Find(ParamName);
        if (CurrentValue)
        {
            // Smooth interpolation to target
            *CurrentValue = FMath::FInterpTo(*CurrentValue, TargetValue, DeltaTime, SmoothingSpeed);
        }
        else
        {
            // Initialize parameter
            CurrentParameters.Add(ParamName, TargetValue);
        }
    }
}

void UExpressionSynthesizer::SetExpressionIntensity(float Intensity)
{
    ExpressionIntensityMultiplier = FMath::Clamp(Intensity, 0.0f, 2.0f);
}

void UExpressionSynthesizer::SetSmoothingSpeed(float Speed)
{
    SmoothingSpeed = FMath::Max(Speed, 0.1f);
}

void UExpressionSynthesizer::ResetExpression()
{
    // Reset all parameters to neutral
    for (auto& Pair : CurrentParameters)
    {
        Pair.Value = 0.0f;
    }
    
    // Reset timers
    BlinkTimer = 0.0f;
    bIsBlinking = false;
}

TMap<FName, float> UExpressionSynthesizer::GetCurrentParameters() const
{
    return CurrentParameters;
}

float UExpressionSynthesizer::GetParameterValue(const FName& ParameterName) const
{
    if (CurrentParameters.Contains(ParameterName))
    {
        return CurrentParameters[ParameterName];
    }
    return 0.0f;
}

void UExpressionSynthesizer::SetParameterValue(const FName& ParameterName, float Value)
{
    CurrentParameters.Add(ParameterName, FMath::Clamp(Value, -1.0f, 1.0f));
}

void UExpressionSynthesizer::BlendExpressions(const TMap<FName, float>& ExpressionA, 
                                              const TMap<FName, float>& ExpressionB, 
                                              float BlendWeight, 
                                              TMap<FName, float>& OutBlendedExpression)
{
    OutBlendedExpression.Empty();
    
    // Blend all parameters from both expressions
    TSet<FName> AllKeys;
    ExpressionA.GetKeys(AllKeys);
    
    TSet<FName> KeysB;
    ExpressionB.GetKeys(KeysB);
    AllKeys.Append(KeysB);
    
    for (const FName& Key : AllKeys)
    {
        float ValueA = ExpressionA.FindRef(Key);
        float ValueB = ExpressionB.FindRef(Key);
        
        float BlendedValue = FMath::Lerp(ValueA, ValueB, BlendWeight);
        OutBlendedExpression.Add(Key, BlendedValue);
    }
}

void UExpressionSynthesizer::ApplyEmotionalLipSync(const TArray<float>& AudioData, 
                                                   float EmotionalIntensity, 
                                                   TMap<FName, float>& OutParameters)
{
    if (AudioData.Num() == 0)
    {
        return;
    }
    
    // Calculate average amplitude
    float AverageAmplitude = 0.0f;
    for (float Sample : AudioData)
    {
        AverageAmplitude += FMath::Abs(Sample);
    }
    AverageAmplitude /= AudioData.Num();
    
    // Modulate mouth opening with emotional intensity
    float MouthOpen = FMath::Clamp(AverageAmplitude * (1.0f + EmotionalIntensity), 0.0f, 1.0f);
    OutParameters.Add(TEXT("ParamMouthOpenY"), MouthOpen);
}
