// E4ECognitionMode.h
// Shared 4E cognition mode enum (Embodied, Embedded, Enacted, Extended)

#pragma once

#include "CoreMinimal.h"
#include "E4ECognitionMode.generated.h"

/**
 * 4E cognition mode (Embodied, Embedded, Enacted, Extended)
 */
UENUM(BlueprintType)
enum class E4ECognitionMode : uint8
{
    Embodied        UMETA(DisplayName = "Embodied"),
    Embedded        UMETA(DisplayName = "Embedded"),
    Enacted         UMETA(DisplayName = "Enacted"),
    Extended        UMETA(DisplayName = "Extended")
};
