// EmotionalState.h
// Shared emotional and personality state data for UnrealEcho components.
// Data containers only: the values are supplied by the character model's reactions and the
// personality/neurochemical systems; components here only read and render them.

#pragma once

#include "CoreMinimal.h"
#include "EmotionalState.generated.h"

/**
 * Emotional state: dimensional (valence/arousal/dominance) plus categorical intensities.
 */
USTRUCT(BlueprintType)
struct FEmotionalState
{
    GENERATED_BODY()

    /** Valence (-1 negative .. 1 positive) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emotions|Dimensional")
    float Valence = 0.0f;

    /** Arousal (0 calm .. 1 excited) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emotions|Dimensional")
    float Arousal = 0.0f;

    /** Dominance (0 submissive .. 1 dominant) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emotions|Dimensional")
    float Dominance = 0.0f;

    /** Overall intensity (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emotions|Dimensional")
    float Intensity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emotions")
    float Happiness = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emotions")
    float Surprise = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emotions")
    float Sadness = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emotions")
    float Anger = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emotions")
    float Fear = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emotions")
    float Disgust = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emotions")
    float Excitement = 0.0f;
};

/**
 * Big Five personality state (each 0-1).
 */
USTRUCT(BlueprintType)
struct FPersonalityState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    float Openness = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    float Conscientiousness = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    float Extraversion = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    float Agreeableness = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    float Neuroticism = 0.5f;
};
