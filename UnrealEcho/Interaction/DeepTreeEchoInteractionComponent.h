// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - Cognitive-Aware Interaction Component

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "InteractableObject.h"
#include "DeepTreeEchoInteractionComponent.generated.h"

class ADeepTreeEchoCharacter;
class UCognitiveStateComponent;
class UPersonalityTraitSystem;

/**
 * FInteractionMemory
 * 
 * Records a past interaction for cognitive processing and learning.
 */
USTRUCT(BlueprintType)
struct FInteractionMemory
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TWeakObjectPtr<AActor> InteractedActor;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FName InteractionType;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	float Timestamp;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	float EmotionalValence; // How positive/negative the interaction was

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	float CognitiveLoad; // How mentally demanding it was

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bWasSuccessful;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FGameplayTagContainer InteractionTags;

	FInteractionMemory()
		: InteractionType(NAME_None)
		, Timestamp(0.0f)
		, EmotionalValence(0.5f)
		, CognitiveLoad(0.0f)
		, bWasSuccessful(true)
	{}
};

/**
 * FInteractableInfo
 * 
 * Information about a nearby interactable object.
 */
USTRUCT(BlueprintType)
struct FInteractableInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TWeakObjectPtr<AActor> Actor;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	float Distance;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	float Priority; // Calculated based on cognitive state and personality

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FName InteractionType;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FGameplayTagContainer InteractionTags;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bRequiresLineOfSight;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bIsCurrentlyVisible;

	FInteractableInfo()
		: Distance(0.0f)
		, Priority(0.0f)
		, InteractionType(NAME_None)
		, bRequiresLineOfSight(true)
		, bIsCurrentlyVisible(false)
	{}
};

/**
 * FInteractionResult
 * 
 * Result of an interaction attempt.
 */
USTRUCT(BlueprintType)
struct FInteractionResult
{
	GENERATED_BODY()

	/** True only when the target reported the interaction as completed successfully */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bSuccess;

	/**
	 * True when the target accepted the interaction and it is still running; the outcome arrives
	 * on OnInteractionCompleted when the target ends it. bSuccess is false while in progress.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bInProgress;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FText ResultMessage;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	float EmotionalImpact; // -1 to 1, negative to positive

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	float CognitiveImpact; // 0 to 1, how much cognitive load was added

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TArray<FName> UnlockedInteractions; // New interactions now available

	FInteractionResult()
		: bSuccess(false)
		, bInProgress(false)
		, EmotionalImpact(0.0f)
		, CognitiveImpact(0.0f)
	{}
};

// Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractionStarted, AActor*, Target, FName, InteractionType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractionCompleted, AActor*, Target, const FInteractionResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractableDiscovered, const FInteractableInfo&, Info);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractableLost, AActor*, Actor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractionChosen, AActor*, Target, FName, InteractionType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractionChoiceFailed, const FString&, Error);

/** One candidate offered to the model in InteractWithBest (measured, not scored) */
struct FInteractionChoiceCandidate
{
	FString Id;
	TWeakObjectPtr<AActor> Actor;
	TArray<EInteractionType> Types;
};

/**
 * UDeepTreeEchoInteractionComponent
 *
 * Manages environmental awareness and object interactions for the Deep Tree Echo avatar.
 * Integrates with cognitive and personality systems to provide intelligent, context-aware interactions.
 *
 * Key Features:
 * - Environmental scanning for interactable objects
 * - Priority-based interaction selection based on cognitive state
 * - Interaction memory for learning and adaptation
 * - Personality-influenced interaction preferences
 * - Line-of-sight and attention-based filtering
 */
UCLASS(ClassGroup = (DeepTreeEcho), meta = (BlueprintSpawnableComponent))
class UNREALENGINE_API UDeepTreeEchoInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeepTreeEchoInteractionComponent();

	//~UActorComponent interface
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~End of UActorComponent interface

	// Initialize with owning character
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Interaction")
	void Initialize(ADeepTreeEchoCharacter* InOwner);

	// Find all interactable objects within radius
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Interaction")
	TArray<AActor*> FindInteractableObjects(float Radius = 500.0f) const;

	// Get detailed info about nearby interactables (sorted by priority)
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Interaction")
	TArray<FInteractableInfo> GetNearbyInteractables() const;

	/**
	 * The interactable the model last chose in InteractWithBest, if it is still nearby; otherwise an
	 * empty info. The priority order of GetNearbyInteractables chooses nothing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Interaction")
	FInteractableInfo GetBestInteractable() const;

	/**
	 * Interact with a specific actor through its interaction interface (AInteractableObject):
	 * BeginInteraction(owner, Type) on the target. Type None is accepted only when the target offers
	 * exactly one interaction to this character. The returned result is a failure (also recorded and
	 * broadcast on OnInteractionCompleted) when the target exposes no interface or refuses; when the
	 * target accepts, bInProgress is set and the target's own outcome is reported on
	 * OnInteractionCompleted (and remembered) when it ends the interaction.
	 */
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Interaction")
	FInteractionResult InteractWith(AActor* Target, EInteractionType Type = EInteractionType::None);

	/**
	 * Ask the model to choose which nearby interactable to interact with, and how, from the
	 * measured candidates (InteractionChoicePromptTemplate; vocabulary = candidate ids and each
	 * candidate's available interactions, or "none"). A valid choice is broadcast on
	 * OnInteractionChosen and performed with InteractWith. Failures: Error log,
	 * GetLastInteractionChoiceError, OnInteractionChoiceFailed, no interaction.
	 * Returns false when nothing was requested.
	 */
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Interaction")
	bool InteractWithBest();

	/** Why the last model interaction choice failed, or empty */
	UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Interaction")
	FString GetLastInteractionChoiceError() const { return LastInteractionChoiceError; }

	/**
	 * Instructions to the model for choosing an interaction. Slots: {candidates}, {state},
	 * {targets} (the candidate ids and "none").
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deep Tree Echo|Interaction|Model", meta = (MultiLine = true))
	FString InteractionChoicePromptTemplate;

	/** The model chose an interaction (Target is null when it chose not to interact) */
	UPROPERTY(BlueprintAssignable, Category = "Deep Tree Echo|Interaction")
	FOnInteractionChosen OnInteractionChosen;

	/** A model interaction choice failed; nothing was done */
	UPROPERTY(BlueprintAssignable, Category = "Deep Tree Echo|Interaction")
	FOnInteractionChoiceFailed OnInteractionChoiceFailed;

	// Check if an actor is interactable
	UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Interaction")
	bool IsInteractable(AActor* Actor) const;

	// Check if currently interacting
	UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Interaction")
	bool IsInteracting() const { return bIsInteracting; }

	// Cancel current interaction
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Interaction")
	void CancelInteraction();

	// Get interaction memory
	UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Interaction")
	TArray<FInteractionMemory> GetInteractionHistory() const { return InteractionHistory; }

	// Get interaction count with specific actor
	UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Interaction")
	int32 GetInteractionCount(AActor* Actor) const;

	// Get emotional association with actor (based on past interactions)
	UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Interaction")
	float GetEmotionalAssociation(AActor* Actor) const;

	// Set gaze target for attention-based interactions
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Interaction")
	void SetGazeTarget(AActor* Target);

	// Events
	UPROPERTY(BlueprintAssignable, Category = "Deep Tree Echo|Interaction")
	FOnInteractionStarted OnInteractionStarted;

	UPROPERTY(BlueprintAssignable, Category = "Deep Tree Echo|Interaction")
	FOnInteractionCompleted OnInteractionCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Deep Tree Echo|Interaction")
	FOnInteractableDiscovered OnInteractableDiscovered;

	UPROPERTY(BlueprintAssignable, Category = "Deep Tree Echo|Interaction")
	FOnInteractableLost OnInteractableLost;

protected:
	// Configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deep Tree Echo|Interaction|Config")
	float ScanRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deep Tree Echo|Interaction|Config")
	float ScanInterval;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deep Tree Echo|Interaction|Config")
	int32 MaxInteractionMemory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deep Tree Echo|Interaction|Config")
	TArray<TEnumAsByte<EObjectTypeQuery>> InteractableObjectTypes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deep Tree Echo|Interaction|Config")
	FGameplayTagContainer RequiredInteractionTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deep Tree Echo|Interaction|Config")
	bool bRequireLineOfSight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deep Tree Echo|Interaction|Config")
	float AttentionBonusMultiplier;

	// References
	UPROPERTY()
	TObjectPtr<ADeepTreeEchoCharacter> OwningCharacter;

	UPROPERTY()
	TObjectPtr<UCognitiveStateComponent> CognitiveState;

	UPROPERTY()
	TObjectPtr<UPersonalityTraitSystem> PersonalitySystem;

	// State
	UPROPERTY()
	TArray<FInteractableInfo> CachedInteractables;

	UPROPERTY()
	TArray<FInteractionMemory> InteractionHistory;

	UPROPERTY()
	TWeakObjectPtr<AActor> CurrentInteractionTarget;

	UPROPERTY()
	TWeakObjectPtr<AActor> CurrentGazeTarget;

	UPROPERTY()
	bool bIsInteracting;

	UPROPERTY()
	float TimeSinceLastScan;

	// Interaction running on an AInteractableObject target (outcome pending)
	UPROPERTY()
	TWeakObjectPtr<AInteractableObject> PendingInteractable;

	EInteractionType PendingInteractionType = EInteractionType::None;

	// Model interaction choice
	TWeakObjectPtr<AActor> ModelChosenTarget;
	FString LastInteractionChoiceError;
	bool bInteractionChoiceInFlight = false;

	void HandleInteractionChoiceReply(const TArray<FInteractionChoiceCandidate>& Candidates, bool bSuccess, const FString& TextOrError);
	void FailInteractionChoice(const FString& Error);

	/** The target ended the interaction this component began */
	UFUNCTION()
	void HandleTargetInteractionEnd(AActor* Interactor, EInteractionType Type, bool bSuccess);
	void FinishInteraction(AActor* Target, FName InteractionType, const FInteractionResult& Result);

	// Internal functions
	void PerformEnvironmentScan();
	void UpdateInteractablePriorities();
	float CalculateInteractionPriority(const FInteractableInfo& Info) const;
	bool CheckLineOfSight(AActor* Target) const;
	void RecordInteraction(AActor* Target, FName InteractionType, const FInteractionResult& Result);
	void CleanupOldMemories();
	FInteractionResult ExecuteInteraction(AActor* Target, EInteractionType Type);
	void NotifyInteractableChanges(const TArray<FInteractableInfo>& OldInteractables);

	// Personality-based modifiers
	float GetPersonalityInteractionModifier(const FInteractableInfo& Info) const;
	float GetCognitiveInteractionModifier(const FInteractableInfo& Info) const;
	float GetMemoryInteractionModifier(AActor* Actor) const;
};
