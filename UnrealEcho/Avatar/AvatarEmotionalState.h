// AvatarEmotionalState.h
// Shared avatar emotion vocabulary. The emotion shown is always one chosen by the character
// model (validated against this enum); this header only defines the vocabulary.

#pragma once

#include "CoreMinimal.h"
#include "AvatarEmotionalState.generated.h"

UENUM(BlueprintType)
enum class EAvatarEmotionalState : uint8
{
    Neutral UMETA(DisplayName = "Neutral"),
    Happy UMETA(DisplayName = "Happy"),
    Sad UMETA(DisplayName = "Sad"),
    Angry UMETA(DisplayName = "Angry"),
    Surprised UMETA(DisplayName = "Surprised"),
    Flirty UMETA(DisplayName = "Flirty"),
    Chaotic UMETA(DisplayName = "Chaotic"),
    Thoughtful UMETA(DisplayName = "Thoughtful"),
    Excited UMETA(DisplayName = "Excited"),
    Mysterious UMETA(DisplayName = "Mysterious")
};
