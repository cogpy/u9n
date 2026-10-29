// CognitiveStateComponent.h
// Holds the character's current cognitive load and attention level for the character and
// its components. State only: values are set by whichever cognitive system measures them.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CognitiveStateComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCognitiveLoadChanged, float, NewLoad);

UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class UNREALENGINE_API UCognitiveStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCognitiveStateComponent();

	/** Broadcast when the cognitive load changes */
	UPROPERTY(BlueprintAssignable, Category = "Cognitive State")
	FOnCognitiveLoadChanged OnCognitiveLoadChanged;

	/** Set the current cognitive load (0-1) */
	UFUNCTION(BlueprintCallable, Category = "Cognitive State")
	void SetCognitiveLoad(float NewLoad);

	/** Set the current attention level (0-1) */
	UFUNCTION(BlueprintCallable, Category = "Cognitive State")
	void SetAttentionLevel(float NewLevel);

	UFUNCTION(BlueprintPure, Category = "Cognitive State")
	float GetCurrentCognitiveLoad() const { return CognitiveLoad; }

	UFUNCTION(BlueprintPure, Category = "Cognitive State")
	float GetAttentionLevel() const { return AttentionLevel; }

protected:
	/** Current cognitive load (0-1) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cognitive State")
	float CognitiveLoad = 0.0f;

	/** Current attention level (0-1) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cognitive State")
	float AttentionLevel = 0.0f;
};
