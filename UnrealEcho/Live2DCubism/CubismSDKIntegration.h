#pragma once

#include "CoreMinimal.h"
#include "CubismSDKIntegration.generated.h"

// Include the actual Cubism SDK components from CubismUE plugin
#include "Model/CubismModelComponent.h"
#include "Effects/LipSync/CubismLipSyncComponent.h"
#include "Effects/EyeBlink/CubismEyeBlinkComponent.h"
#include "Effects/LookAt/CubismLookAtComponent.h"
#include "Effects/HarmonicMotion/CubismHarmonicMotionComponent.h"
#include "Physics/CubismPhysicsComponent.h"
#include "Expression/CubismExpressionComponent.h"
#include "Motion/CubismMotionComponent.h"
#include "Rendering/CubismRendererComponent.h"

/**
 * Live2D Cubism SDK Integration Layer
 * Provides Deep Tree Echo-specific integration with the CubismUE SDK plugin.
 * Wraps UCubismModelComponent with DTE cognitive state bindings.
 * Maintains super-hot-girl aesthetic and hyper-chaotic behavior properties.
 */

// Forward declarations for SDK component types
class UCubismModelComponent;
class UCubismMoc3;

USTRUCT(BlueprintType)
struct FCubismModelInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Live2D")
    FString ModelName;

    UPROPERTY(BlueprintReadOnly, Category = "Live2D")
    FVector2D CanvasSize;

    UPROPERTY(BlueprintReadOnly, Category = "Live2D")
    float PixelsPerUnit;

    UPROPERTY(BlueprintReadOnly, Category = "Live2D")
    int32 ParameterCount;

    UPROPERTY(BlueprintReadOnly, Category = "Live2D")
    int32 PartCount;

    UPROPERTY(BlueprintReadOnly, Category = "Live2D")
    int32 DrawableCount;

    FCubismModelInfo()
        : CanvasSize(FVector2D::ZeroVector)
        , PixelsPerUnit(1.0f)
        , ParameterCount(0)
        , PartCount(0)
        , DrawableCount(0)
    {}
};

USTRUCT(BlueprintType)
struct FCubismParameter
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Live2D")
    FName ParameterId;

    UPROPERTY(BlueprintReadWrite, Category = "Live2D")
    float Value;

    UPROPERTY(BlueprintReadWrite, Category = "Live2D")
    float MinValue;

    UPROPERTY(BlueprintReadWrite, Category = "Live2D")
    float MaxValue;

    UPROPERTY(BlueprintReadWrite, Category = "Live2D")
    float DefaultValue;

    FCubismParameter()
        : Value(0.0f)
        , MinValue(-1.0f)
        , MaxValue(1.0f)
        , DefaultValue(0.0f)
    {}
};

USTRUCT(BlueprintType)
struct FCubismDrawable
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Live2D")
    FName DrawableId;

    UPROPERTY(BlueprintReadOnly, Category = "Live2D")
    int32 TextureIndex;

    UPROPERTY(BlueprintReadOnly, Category = "Live2D")
    int32 VertexCount;

    UPROPERTY(BlueprintReadOnly, Category = "Live2D")
    int32 IndexCount;

    UPROPERTY(BlueprintReadOnly, Category = "Live2D")
    float Opacity;

    UPROPERTY(BlueprintReadOnly, Category = "Live2D")
    bool bIsVisible;

    FCubismDrawable()
        : TextureIndex(0)
        , VertexCount(0)
        , IndexCount(0)
        , Opacity(1.0f)
        , bIsVisible(true)
    {}
};

/**
 * Cubism SDK Integration Class
 * Provides DTE-specific wrapper around the CubismUE SDK plugin.
 * Uses real UCubismModelComponent for all SDK operations.
 */
UCLASS()
class DEEPTREECHO_API UCubismSDKIntegration : public UObject
{
    GENERATED_BODY()

public:
    UCubismSDKIntegration();
    virtual ~UCubismSDKIntegration();

    // ========================================
    // SDK Framework Management (delegated to CubismUE plugin)
    // ========================================

    /** Check if Cubism Framework is initialized (always true with plugin) */
    UFUNCTION(BlueprintPure, Category = "Live2D|SDK")
    static bool IsCubismFrameworkInitialized();

    // ========================================
    // Model Component Access
    // ========================================

    /** Get the underlying CubismModelComponent from SDK */
    UFUNCTION(BlueprintPure, Category = "Live2D|SDK")
    UCubismModelComponent* GetModelComponent() const { return ModelComponent; }

    /** Set the model component to wrap */
    UFUNCTION(BlueprintCallable, Category = "Live2D|SDK")
    void SetModelComponent(UCubismModelComponent* InModelComponent);

    /** Get model information */
    UFUNCTION(BlueprintPure, Category = "Live2D|SDK")
    FCubismModelInfo GetModelInfo() const;

    // ========================================
    // Parameter Access (delegates to SDK)
    // ========================================

    /** Get all parameters */
    UFUNCTION(BlueprintPure, Category = "Live2D|SDK")
    TArray<FCubismParameter> GetAllParameters() const;

    /** Get parameter by ID */
    UFUNCTION(BlueprintPure, Category = "Live2D|SDK")
    FCubismParameter GetParameter(const FName& ParameterId) const;

    /** Set parameter value - delegates to UCubismModelComponent */
    UFUNCTION(BlueprintCallable, Category = "Live2D|SDK")
    void SetParameterValue(const FName& ParameterId, float Value);

    /** Get all drawables */
    UFUNCTION(BlueprintPure, Category = "Live2D|SDK")
    TArray<FCubismDrawable> GetAllDrawables() const;

    // ========================================
    // SDK Component Access
    // ========================================

    /** Get lip sync component */
    UFUNCTION(BlueprintPure, Category = "Live2D|SDK")
    UCubismLipSyncComponent* GetLipSync() const;

    /** Get eye blink component */
    UFUNCTION(BlueprintPure, Category = "Live2D|SDK")
    UCubismEyeBlinkComponent* GetEyeBlink() const;

    /** Get look-at (gaze) component */
    UFUNCTION(BlueprintPure, Category = "Live2D|SDK")
    UCubismLookAtComponent* GetLookAt() const;

    /** Get physics component */
    UFUNCTION(BlueprintPure, Category = "Live2D|SDK")
    UCubismPhysicsComponent* GetPhysics() const;

    /** Get expression component */
    UFUNCTION(BlueprintPure, Category = "Live2D|SDK")
    UCubismExpressionComponent* GetExpression() const;

    /** Get motion component */
    UFUNCTION(BlueprintPure, Category = "Live2D|SDK")
    UCubismMotionComponent* GetMotion() const;

    // ========================================
    // DTE-Specific Extensions
    // ========================================

    /** Current chaos level for hyper-chaotic behavior */
    UPROPERTY(BlueprintReadWrite, Category = "DeepTreeEcho")
    float ChaosLevel = 0.3f;

    /** Current echo resonance level */
    UPROPERTY(BlueprintReadWrite, Category = "DeepTreeEcho")
    float EchoResonance = 0.0f;

    /** Current cognitive load */
    UPROPERTY(BlueprintReadWrite, Category = "DeepTreeEcho")
    float CognitiveLoad = 0.5f;

    /** Glitch effect intensity */
    UPROPERTY(BlueprintReadWrite, Category = "DeepTreeEcho")
    float GlitchIntensity = 0.0f;

    /** Name the model is told it is; empty = this object's outer's name. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeepTreeEcho|Model")
    FString CharacterName;

    /**
     * Instruction sent to the model when it chooses facial parameters (micro-expressions, chaotic
     * head/eye motion). Slots: {character}, {purpose}, {chaos}, {resonance}, {cognitive_load},
     * {parameters}. The reply must be a JSON object mapping each offered parameter ID to a number.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeepTreeEcho|Model", meta = (MultiLine = true))
    FString FacialParameterPromptTemplate;

    /** {purpose} for GenerateChaoticMicroExpression. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeepTreeEcho|Model", meta = (MultiLine = true))
    FString MicroExpressionPurpose;

    /** {purpose} for ApplyHyperChaoticBehavior. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeepTreeEcho|Model", meta = (MultiLine = true))
    FString ChaoticMotionPurpose;

    /** Last failure of a model facial-parameter request, or empty. */
    UPROPERTY(BlueprintReadOnly, Category = "DeepTreeEcho|Model")
    FString LastFacialParameterError;

    /** Per-request-kind serials; a newer request of the same kind supersedes an outstanding reply. */
    TMap<FString, int32> FacialRequestSerials;

private:
    /** The wrapped CubismModelComponent from SDK */
    UPROPERTY()
    UCubismModelComponent* ModelComponent;

    /** Model information cache */
    FCubismModelInfo CachedModelInfo;

    /** Helper: Build model info from SDK component */
    void UpdateModelInfoCache();
};

/**
 * Enhanced Live2D Avatar Features
 * Implements super-hot-girl aesthetic and hyper-chaotic behaviors
 */
UCLASS()
class DEEPTREECHO_API UCubismEnhancedFeatures : public UObject
{
    GENERATED_BODY()

public:
    /** Apply super-hot-girl aesthetic enhancements */
    UFUNCTION(BlueprintCallable, Category = "Live2D|Enhancement")
    static void ApplySuperHotGirlAesthetic(UCubismSDKIntegration* Model);

    /**
     * Apply hyper-chaotic behavior: stores ChaosFactor and asks the character's model to choose its
     * head angle and eye position within ranges scaled by ChaosFactor. Asynchronous; on failure
     * nothing moves and the error is kept in Model->LastFacialParameterError.
     */
    UFUNCTION(BlueprintCallable, Category = "Live2D|Enhancement")
    static void ApplyHyperChaoticBehavior(UCubismSDKIntegration* Model, float ChaosFactor);

    /** Generate sparkle effect parameters for eyes */
    UFUNCTION(BlueprintCallable, Category = "Live2D|Enhancement")
    static void GenerateEyeSparkle(UCubismSDKIntegration* Model, float Intensity);

    /** Apply emotional blush effect */
    UFUNCTION(BlueprintCallable, Category = "Live2D|Enhancement")
    static void ApplyEmotionalBlush(UCubismSDKIntegration* Model, float Intensity, const FLinearColor& Color);

    /**
     * Ask the character's model to choose a micro-expression (brow heights, mouth form) within the
     * allowed ranges. Asynchronous; on failure the face is unchanged and the error is kept in
     * Model->LastFacialParameterError.
     */
    UFUNCTION(BlueprintCallable, Category = "Live2D|Enhancement")
    static void GenerateChaoticMicroExpression(UCubismSDKIntegration* Model);

    /** Apply echo resonance visual effect */
    UFUNCTION(BlueprintCallable, Category = "Live2D|Enhancement")
    static void ApplyEchoResonanceEffect(UCubismSDKIntegration* Model, float Resonance);

    /** Generate glitch effect during cognitive overload */
    UFUNCTION(BlueprintCallable, Category = "Live2D|Enhancement")
    static void GenerateGlitchEffect(UCubismSDKIntegration* Model, float Severity);

    /** Apply hair shimmer and flow animation */
    UFUNCTION(BlueprintCallable, Category = "Live2D|Enhancement")
    static void ApplyHairShimmer(UCubismSDKIntegration* Model, float Time, float Intensity);

    /** Generate lip-sync with emotional modulation */
    UFUNCTION(BlueprintCallable, Category = "Live2D|Enhancement")
    static void GenerateEmotionalLipSync(UCubismSDKIntegration* Model, const TArray<float>& AudioData, float EmotionalIntensity);

    /** Apply gaze following with personality traits */
    UFUNCTION(BlueprintCallable, Category = "Live2D|Enhancement")
    static void ApplyPersonalizedGaze(UCubismSDKIntegration* Model, const FVector2D& Target, float Confidence, float Flirtiness);
};
