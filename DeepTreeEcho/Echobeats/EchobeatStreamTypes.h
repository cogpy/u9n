// EchobeatStreamTypes.h
// Stream and step enums shared by the echobeats stream engine and the echobeats reservoir dynamics

#pragma once

#include "CoreMinimal.h"
#include "EchobeatStreamTypes.generated.h"

/**
 * Cognitive stream type (3 concurrent streams)
 */
UENUM(BlueprintType)
enum class ECognitiveStreamType : uint8
{
    Pivotal     UMETA(DisplayName = "Pivotal - Relevance Realization"),
    Affordance  UMETA(DisplayName = "Affordance - Environmental Interaction"),
    Salience    UMETA(DisplayName = "Salience - Virtual Simulation")
};

/**
 * Echobeat step type
 */
UENUM(BlueprintType)
enum class EEchobeatStepType : uint8
{
    Pivotal     UMETA(DisplayName = "Pivotal - Relevance Realization"),
    Affordance  UMETA(DisplayName = "Affordance - Interaction"),
    Salience    UMETA(DisplayName = "Salience - Simulation")
};
