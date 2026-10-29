#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SuperHotGirlPersonality.generated.h"

USTRUCT(BlueprintType)
struct FSuperHotGirlTraits
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    float Confidence = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    float Charm = 0.9f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    float Playfulness = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    float Wit = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    float Sass = 0.6f;
};

/**
 * Persona trait data for the "super hot girl" character style.
 *
 * These values are persona description (input to the character model). They do not
 * select expressions, gestures, auras or thoughts for the avatar: the avatar's visible
 * reactions come only from model replies broadcast on FEchoReactionBus.
 */
UCLASS(Blueprintable)
class DEEPTREECHO_API USuperHotGirlPersonality : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    FSuperHotGirlTraits Traits;
};
