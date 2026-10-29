// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - Cognitive-Aware Interaction Implementation

#include "DeepTreeEchoInteractionComponent.h"
#include "Character/DeepTreeEchoCharacter.h"
#include "Cognitive/CognitiveStateComponent.h"
#include "Personality/PersonalityTraitSystem.h"
#include "Kismet/KismetSystemLibrary.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "EchoModelClient.h"
#include "Dom/JsonObject.h"

DEFINE_LOG_CATEGORY_STATIC(LogDTEInteraction, Log, All);

namespace
{
	FString InteractionTypeName(EInteractionType Type)
	{
		return StaticEnum<EInteractionType>()->GetNameStringByValue(static_cast<int64>(Type));
	}
}

UDeepTreeEchoInteractionComponent::UDeepTreeEchoInteractionComponent()
	: ScanRadius(500.0f)
	, ScanInterval(0.25f)
	, MaxInteractionMemory(100)
	, bRequireLineOfSight(true)
	, AttentionBonusMultiplier(1.5f)
	, bIsInteracting(false)
	, TimeSinceLastScan(0.0f)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f; // Tick at 10Hz for performance

	// Default interactable object types
	InteractableObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
	InteractableObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));

	InteractionChoicePromptTemplate = TEXT(
		"You are Deep Tree Echo, an embodied character. Decide whether to interact with something near you now, and how.\n"
		"Your state: {state}.\n"
		"Things you can interact with (measured):\n{candidates}\n"
		"Choose the target id from: {targets}. Then choose one interaction listed for that target (leave it empty if the target is \"none\").\n"
		"Reply with only a JSON object: {\"target\": \"<target id or none>\", \"interaction\": \"<interaction>\"}");
}

void UDeepTreeEchoInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	// Try to find owning character if not set
	if (!OwningCharacter)
	{
		OwningCharacter = Cast<ADeepTreeEchoCharacter>(GetOwner());
	}
}

void UDeepTreeEchoInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Periodic environment scan
	TimeSinceLastScan += DeltaTime;
	if (TimeSinceLastScan >= ScanInterval)
	{
		PerformEnvironmentScan();
		TimeSinceLastScan = 0.0f;
	}

	// Update priorities based on current cognitive/emotional state
	UpdateInteractablePriorities();

	// Cleanup old memories periodically
	if (InteractionHistory.Num() > MaxInteractionMemory)
	{
		CleanupOldMemories();
	}
}

void UDeepTreeEchoInteractionComponent::Initialize(ADeepTreeEchoCharacter* InOwner)
{
	OwningCharacter = InOwner;

	if (OwningCharacter)
	{
		CognitiveState = OwningCharacter->GetCognitiveState();
		PersonalitySystem = OwningCharacter->GetPersonalitySystem();
	}
}

TArray<AActor*> UDeepTreeEchoInteractionComponent::FindInteractableObjects(float Radius) const
{
	TArray<AActor*> Results;

	if (!GetOwner())
	{
		return Results;
	}

	FVector Origin = GetOwner()->GetActorLocation();

	// Perform sphere overlap
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());

	GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		Origin,
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_WorldDynamic),
		FCollisionShape::MakeSphere(Radius),
		QueryParams
	);

	// Filter and collect interactable actors
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (Actor && IsInteractable(Actor))
		{
			Results.AddUnique(Actor);
		}
	}

	return Results;
}

TArray<FInteractableInfo> UDeepTreeEchoInteractionComponent::GetNearbyInteractables() const
{
	return CachedInteractables;
}

FInteractableInfo UDeepTreeEchoInteractionComponent::GetBestInteractable() const
{
	// The model's last choice, if it is still nearby; the priority order chooses nothing
	if (ModelChosenTarget.IsValid())
	{
		for (const FInteractableInfo& Info : CachedInteractables)
		{
			if (Info.Actor.Get() == ModelChosenTarget.Get())
			{
				return Info;
			}
		}
	}

	return FInteractableInfo();
}

FInteractionResult UDeepTreeEchoInteractionComponent::InteractWith(AActor* Target, EInteractionType Type)
{
	FInteractionResult Result;

	if (!Target)
	{
		Result.bSuccess = false;
		Result.ResultMessage = FText::FromString(TEXT("No target specified"));
		return Result;
	}

	if (!IsInteractable(Target))
	{
		Result.bSuccess = false;
		Result.ResultMessage = FText::FromString(TEXT("Target is not interactable"));
		return Result;
	}

	if (bIsInteracting)
	{
		Result.bSuccess = false;
		Result.ResultMessage = FText::FromString(TEXT("Already interacting with another object"));
		return Result;
	}

	// Check line of sight if required
	if (bRequireLineOfSight && !CheckLineOfSight(Target))
	{
		Result.bSuccess = false;
		Result.ResultMessage = FText::FromString(TEXT("No line of sight to target"));
		return Result;
	}

	// Perform the interaction through the target's interface
	bIsInteracting = true;
	CurrentInteractionTarget = Target;
	Result = ExecuteInteraction(Target, Type);

	if (!Result.bInProgress)
	{
		// The target exposes no interface or refused: an honest failure, remembered as such
		FinishInteraction(Target, NAME_None, Result);
	}

	return Result;
}

bool UDeepTreeEchoInteractionComponent::InteractWithBest()
{
	if (bIsInteracting)
	{
		FailInteractionChoice(TEXT("already interacting"));
		return false;
	}
	if (bInteractionChoiceInFlight)
	{
		FailInteractionChoice(TEXT("an interaction choice request is already in flight"));
		return false;
	}
	if (InteractionChoicePromptTemplate.IsEmpty())
	{
		FailInteractionChoice(TEXT("InteractionChoicePromptTemplate is empty"));
		return false;
	}

	// Measured candidates: nearby targets that expose interactions this character can perform now
	AActor* Self = GetOwner();
	TArray<FInteractionChoiceCandidate> Candidates;
	FString CandidateText;
	TArray<FString> TargetIds;
	// Presented nearest first (measured distance), not in the hand-tuned priority order
	TArray<FInteractableInfo> ByDistance = CachedInteractables;
	ByDistance.Sort([](const FInteractableInfo& A, const FInteractableInfo& B) { return A.Distance < B.Distance; });
	for (const FInteractableInfo& Info : ByDistance)
	{
		AInteractableObject* Object = Cast<AInteractableObject>(Info.Actor.Get());
		if (!Object || !Self)
		{
			continue;
		}
		FInteractionChoiceCandidate Candidate;
		Candidate.Id = Object->GetName();
		Candidate.Actor = Object;
		FString InteractionText;
		for (const FInteractionData& Data : Object->GetAvailableInteractions(Self))
		{
			if (Data.Type == EInteractionType::None || Candidate.Types.Contains(Data.Type))
			{
				continue;
			}
			Candidate.Types.Add(Data.Type);
			InteractionText += FString::Printf(TEXT("%s%s (duration %.1fs, cognitive load %.2f, emotional impact %+.2f)"),
				InteractionText.IsEmpty() ? TEXT("") : TEXT(", "), *InteractionTypeName(Data.Type),
				Data.Duration, Data.CognitiveLoad, Data.EmotionalImpact);
		}
		if (Candidate.Types.Num() == 0)
		{
			continue;
		}
		const int32 Count = GetInteractionCount(Object);
		CandidateText += FString::Printf(TEXT("- id \"%s\" (%s): distance %.0f cm, %s, %s; past interactions %d%s; interactions: %s\n"),
			*Candidate.Id, *Info.DisplayName.ToString(), Info.Distance,
			Info.bIsCurrentlyVisible ? TEXT("visible") : TEXT("not visible"),
			CurrentGazeTarget.Get() == Object ? TEXT("you are looking at it") : TEXT("not looked at"),
			Count, Count > 0 ? *FString::Printf(TEXT(", remembered valence %.2f (0 bad .. 1 good)"), GetEmotionalAssociation(Object)) : TEXT(""),
			*InteractionText);
		TargetIds.Add(Candidate.Id);
		Candidates.Add(MoveTemp(Candidate));
	}
	if (Candidates.Num() == 0)
	{
		FailInteractionChoice(TEXT("no nearby target offers an interaction this character can perform"));
		return false;
	}

	FString StateText;
	if (CognitiveState)
	{
		StateText += FString::Printf(TEXT("cognitive load %.2f, attention %.2f"),
			CognitiveState->GetCurrentCognitiveLoad(), CognitiveState->GetAttentionLevel());
	}
	if (PersonalitySystem)
	{
		StateText += FString::Printf(TEXT("%ssocial engagement %.2f, intellectual %.2f, assertive %.2f, empathetic %.2f"),
			StateText.IsEmpty() ? TEXT("") : TEXT("; "), PersonalitySystem->GetSocialEngagementLevel(),
			PersonalitySystem->GetTraitIntensity(EPersonalityTraitType::Intellectual),
			PersonalitySystem->GetTraitIntensity(EPersonalityTraitType::Assertive),
			PersonalitySystem->GetTraitIntensity(EPersonalityTraitType::Empathetic));
	}
	if (StateText.IsEmpty())
	{
		StateText = TEXT("(not measured)");
	}

	TMap<FString, FString> Slots;
	Slots.Add(TEXT("candidates"), CandidateText.TrimEnd());
	Slots.Add(TEXT("state"), StateText);
	Slots.Add(TEXT("targets"), FString::Join(TargetIds, TEXT(", ")) + TEXT(", none"));

	TArray<FEchoModelMessage> Messages;
	Messages.Add(FEchoModelMessage(TEXT("user"), UEchoModelClient::FillPrompt(InteractionChoicePromptTemplate, Slots)));

	bInteractionChoiceInFlight = true;
	TWeakObjectPtr<UDeepTreeEchoInteractionComponent> WeakThis(this);
	UEchoModelClient::Get()->Chat(Messages, FOnEchoModelReply::CreateLambda(
		[WeakThis, Candidates](bool bSuccess, const FString& TextOrError)
		{
			if (UDeepTreeEchoInteractionComponent* Component = WeakThis.Get())
			{
				Component->HandleInteractionChoiceReply(Candidates, bSuccess, TextOrError);
			}
		}), 0.5f);
	return true;
}

void UDeepTreeEchoInteractionComponent::HandleInteractionChoiceReply(const TArray<FInteractionChoiceCandidate>& Candidates, bool bSuccess, const FString& TextOrError)
{
	bInteractionChoiceInFlight = false;
	if (!bSuccess)
	{
		FailInteractionChoice(TextOrError);
		return;
	}

	TSharedPtr<FJsonObject> Json;
	FString TargetId;
	if (!UEchoModelClient::ParseJsonObject(TextOrError, Json) || !Json->TryGetStringField(TEXT("target"), TargetId))
	{
		FailInteractionChoice(FString::Printf(TEXT("model reply is not the requested JSON: %s"), *TextOrError.Left(200)));
		return;
	}
	TargetId.TrimStartAndEndInline();
	if (TargetId.Equals(TEXT("none"), ESearchCase::IgnoreCase))
	{
		LastInteractionChoiceError.Reset();
		ModelChosenTarget.Reset();
		UE_LOG(LogDTEInteraction, Log, TEXT("The model chose not to interact"));
		OnInteractionChosen.Broadcast(nullptr, NAME_None);
		return;
	}

	const FInteractionChoiceCandidate* Chosen = Candidates.FindByPredicate(
		[&TargetId](const FInteractionChoiceCandidate& C) { return C.Id.Equals(TargetId, ESearchCase::IgnoreCase); });
	if (!Chosen)
	{
		FailInteractionChoice(FString::Printf(TEXT("model chose a target outside the candidates: %s"), *TargetId));
		return;
	}

	FString InteractionName;
	if (!Json->TryGetStringField(TEXT("interaction"), InteractionName))
	{
		FailInteractionChoice(TEXT("model reply has no interaction"));
		return;
	}
	InteractionName.TrimStartAndEndInline();
	const EInteractionType* Type = Chosen->Types.FindByPredicate(
		[&InteractionName](EInteractionType T) { return InteractionTypeName(T).Equals(InteractionName, ESearchCase::IgnoreCase); });
	if (!Type)
	{
		FailInteractionChoice(FString::Printf(TEXT("model chose an interaction '%s' that %s does not offer"), *InteractionName, *Chosen->Id));
		return;
	}

	AActor* Target = Chosen->Actor.Get();
	if (!Target)
	{
		FailInteractionChoice(FString::Printf(TEXT("chosen target %s no longer exists"), *Chosen->Id));
		return;
	}

	LastInteractionChoiceError.Reset();
	ModelChosenTarget = Target;
	const EInteractionType ChosenType = *Type;
	OnInteractionChosen.Broadcast(Target, FName(*InteractionTypeName(ChosenType)));
	InteractWith(Target, ChosenType);
}

void UDeepTreeEchoInteractionComponent::FailInteractionChoice(const FString& Error)
{
	LastInteractionChoiceError = Error;
	UE_LOG(LogDTEInteraction, Error, TEXT("No model interaction choice (nothing done): %s"), *Error);
	OnInteractionChoiceFailed.Broadcast(Error);
}

bool UDeepTreeEchoInteractionComponent::IsInteractable(AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}

	// Check if actor implements interaction interface
	// For now, check if actor has specific tags or components
	if (Actor->Tags.Contains(TEXT("Interactable")))
	{
		return true;
	}

	// Check for interaction component
	UActorComponent* InteractionComp = Actor->GetComponentByClass(UDeepTreeEchoInteractionComponent::StaticClass());
	if (InteractionComp)
	{
		return true;
	}

	// Check gameplay tags if required
	if (RequiredInteractionTags.Num() > 0)
	{
		// Would check actor's gameplay tags here
		// For now, return true if no specific tags required
	}

	return false;
}

void UDeepTreeEchoInteractionComponent::CancelInteraction()
{
	if (!bIsInteracting)
	{
		return;
	}

	// A running interaction is cancelled on the target; its end event reports the failure
	if (AInteractableObject* Object = PendingInteractable.Get())
	{
		Object->CancelInteraction(GetOwner());
	}

	if (bIsInteracting)
	{
		if (AInteractableObject* Object = PendingInteractable.Get())
		{
			Object->OnInteractionEnd.RemoveDynamic(this, &UDeepTreeEchoInteractionComponent::HandleTargetInteractionEnd);
		}
		PendingInteractable.Reset();
		PendingInteractionType = EInteractionType::None;

		FInteractionResult CancelResult;
		CancelResult.bSuccess = false;
		CancelResult.ResultMessage = FText::FromString(TEXT("Interaction cancelled"));
		FinishInteraction(CurrentInteractionTarget.Get(), NAME_None, CancelResult);
	}
}

int32 UDeepTreeEchoInteractionComponent::GetInteractionCount(AActor* Actor) const
{
	int32 Count = 0;
	for (const FInteractionMemory& Memory : InteractionHistory)
	{
		if (Memory.InteractedActor.Get() == Actor)
		{
			Count++;
		}
	}
	return Count;
}

float UDeepTreeEchoInteractionComponent::GetEmotionalAssociation(AActor* Actor) const
{
	float TotalValence = 0.0f;
	int32 Count = 0;

	for (const FInteractionMemory& Memory : InteractionHistory)
	{
		if (Memory.InteractedActor.Get() == Actor)
		{
			TotalValence += Memory.EmotionalValence;
			Count++;
		}
	}

	if (Count > 0)
	{
		return TotalValence / Count;
	}

	return 0.5f; // Neutral if no history
}

void UDeepTreeEchoInteractionComponent::SetGazeTarget(AActor* Target)
{
	CurrentGazeTarget = Target;
}

void UDeepTreeEchoInteractionComponent::PerformEnvironmentScan()
{
	TArray<FInteractableInfo> OldInteractables = CachedInteractables;
	CachedInteractables.Empty();

	if (!GetOwner())
	{
		return;
	}

	FVector Origin = GetOwner()->GetActorLocation();

	// Find all interactable objects
	TArray<AActor*> InteractableActors = FindInteractableObjects(ScanRadius);

	// Build info for each interactable
	for (AActor* Actor : InteractableActors)
	{
		FInteractableInfo Info;
		Info.Actor = Actor;
		Info.Distance = FVector::Dist(Origin, Actor->GetActorLocation());
		Info.InteractionType = NAME_None; // Would be determined by actor's interaction interface
		Info.DisplayName = FText::FromString(Actor->GetName());
		Info.bRequiresLineOfSight = bRequireLineOfSight;
		Info.bIsCurrentlyVisible = CheckLineOfSight(Actor);

		// Calculate priority
		Info.Priority = CalculateInteractionPriority(Info);

		CachedInteractables.Add(Info);
	}

	// Sort by priority (highest first)
	CachedInteractables.Sort([](const FInteractableInfo& A, const FInteractableInfo& B)
	{
		return A.Priority > B.Priority;
	});

	// Notify of changes
	NotifyInteractableChanges(OldInteractables);
}

void UDeepTreeEchoInteractionComponent::UpdateInteractablePriorities()
{
	for (FInteractableInfo& Info : CachedInteractables)
	{
		if (Info.Actor.IsValid())
		{
			Info.Priority = CalculateInteractionPriority(Info);
			Info.bIsCurrentlyVisible = CheckLineOfSight(Info.Actor.Get());
		}
	}

	// Re-sort
	CachedInteractables.Sort([](const FInteractableInfo& A, const FInteractableInfo& B)
	{
		return A.Priority > B.Priority;
	});
}

float UDeepTreeEchoInteractionComponent::CalculateInteractionPriority(const FInteractableInfo& Info) const
{
	float Priority = 0.0f;

	if (!Info.Actor.IsValid())
	{
		return Priority;
	}

	// Base priority from distance (closer = higher priority)
	float DistanceFactor = 1.0f - FMath::Clamp(Info.Distance / ScanRadius, 0.0f, 1.0f);
	Priority += DistanceFactor * 30.0f;

	// Visibility bonus
	if (Info.bIsCurrentlyVisible)
	{
		Priority += 20.0f;
	}

	// Attention bonus (if this is the gaze target)
	if (CurrentGazeTarget.IsValid() && CurrentGazeTarget.Get() == Info.Actor.Get())
	{
		Priority *= AttentionBonusMultiplier;
	}

	// Personality modifier
	Priority += GetPersonalityInteractionModifier(Info);

	// Cognitive modifier
	Priority += GetCognitiveInteractionModifier(Info);

	// Memory modifier (familiarity)
	Priority += GetMemoryInteractionModifier(Info.Actor.Get());

	return Priority;
}

bool UDeepTreeEchoInteractionComponent::CheckLineOfSight(AActor* Target) const
{
	if (!Target || !GetOwner())
	{
		return false;
	}

	FVector Start = GetOwner()->GetActorLocation();
	FVector End = Target->GetActorLocation();

	// Add eye height offset
	if (OwningCharacter)
	{
		Start.Z += 60.0f; // Approximate eye height
	}

	FHitResult Hit;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());
	QueryParams.AddIgnoredActor(Target);

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit,
		Start,
		End,
		ECC_Visibility,
		QueryParams
	);

	// If we didn't hit anything, we have line of sight
	return !bHit;
}

void UDeepTreeEchoInteractionComponent::RecordInteraction(AActor* Target, FName InteractionType, const FInteractionResult& Result)
{
	FInteractionMemory Memory;
	Memory.InteractedActor = Target;
	Memory.InteractionType = InteractionType;
	const UWorld* World = GetWorld();
	Memory.Timestamp = World ? World->GetTimeSeconds() : 0.0f;
	Memory.EmotionalValence = 0.5f + (Result.EmotionalImpact * 0.5f); // Convert -1,1 to 0,1
	Memory.CognitiveLoad = Result.CognitiveImpact;
	Memory.bWasSuccessful = Result.bSuccess;

	InteractionHistory.Add(Memory);

	// The interaction is remembered here (InteractionHistory); the cognitive state and personality
	// components have no per-interaction recording API.
}

void UDeepTreeEchoInteractionComponent::CleanupOldMemories()
{
	// Keep only the most recent memories
	if (InteractionHistory.Num() > MaxInteractionMemory)
	{
		int32 ToRemove = InteractionHistory.Num() - MaxInteractionMemory;
		InteractionHistory.RemoveAt(0, ToRemove);
	}
}

FInteractionResult UDeepTreeEchoInteractionComponent::ExecuteInteraction(AActor* Target, EInteractionType Type)
{
	FInteractionResult Result;
	Result.bSuccess = false;

	AInteractableObject* Object = Cast<AInteractableObject>(Target);
	if (!Object)
	{
		Result.ResultMessage = FText::FromString(FString::Printf(
			TEXT("%s exposes no interaction interface (not an AInteractableObject); nothing was done"), *Target->GetName()));
		return Result;
	}

	AActor* Self = GetOwner();
	if (Type == EInteractionType::None)
	{
		// Only an unambiguous request is performed; a choice between interactions is the model's
		// (InteractWithBest) or the caller's
		const TArray<FInteractionData> Available = Object->GetAvailableInteractions(Self);
		if (Available.Num() != 1)
		{
			Result.ResultMessage = FText::FromString(FString::Printf(
				TEXT("%s offers %d interactions to this character; specify which one"), *Target->GetName(), Available.Num()));
			return Result;
		}
		Type = Available[0].Type;
	}

	if (!Object->BeginInteraction(Self, Type))
	{
		Result.ResultMessage = FText::FromString(FString::Printf(
			TEXT("%s refused interaction %s"), *Target->GetName(), *InteractionTypeName(Type)));
		return Result;
	}

	// The target accepted: its own end event reports the outcome
	PendingInteractable = Object;
	PendingInteractionType = Type;
	Object->OnInteractionEnd.AddDynamic(this, &UDeepTreeEchoInteractionComponent::HandleTargetInteractionEnd);

	Result.bInProgress = true;
	Result.ResultMessage = FText::FromString(FString::Printf(
		TEXT("%s started on %s"), *InteractionTypeName(Type), *Target->GetName()));
	OnInteractionStarted.Broadcast(Target, FName(*InteractionTypeName(Type)));
	return Result;
}

void UDeepTreeEchoInteractionComponent::HandleTargetInteractionEnd(AActor* Interactor, EInteractionType Type, bool bSuccess)
{
	AInteractableObject* Object = PendingInteractable.Get();
	if (Interactor != GetOwner() || !Object)
	{
		return;
	}
	Object->OnInteractionEnd.RemoveDynamic(this, &UDeepTreeEchoInteractionComponent::HandleTargetInteractionEnd);
	PendingInteractable.Reset();
	PendingInteractionType = EInteractionType::None;

	// The outcome is the target's; the impacts are the target's authored data for this interaction
	const FInteractionData Data = Object->GetInteractionData(Type);
	FInteractionResult Result;
	Result.bSuccess = bSuccess;
	Result.ResultMessage = FText::FromString(FString::Printf(TEXT("%s on %s %s"),
		*InteractionTypeName(Type), *Object->GetName(), bSuccess ? TEXT("completed") : TEXT("ended without success")));
	if (bSuccess)
	{
		Result.EmotionalImpact = FMath::Clamp(Data.EmotionalImpact, -1.0f, 1.0f);
		Result.CognitiveImpact = FMath::Clamp(Data.CognitiveLoad, 0.0f, 1.0f);
		if (CognitiveState)
		{
			CognitiveState->SetCognitiveLoad(CognitiveState->GetCurrentCognitiveLoad() + Result.CognitiveImpact);
		}
	}

	FinishInteraction(Object, FName(*InteractionTypeName(Type)), Result);
}

void UDeepTreeEchoInteractionComponent::FinishInteraction(AActor* Target, FName InteractionType, const FInteractionResult& Result)
{
	if (Target)
	{
		RecordInteraction(Target, InteractionType, Result);
	}

	bIsInteracting = false;
	CurrentInteractionTarget = nullptr;

	OnInteractionCompleted.Broadcast(Target, Result);
}

void UDeepTreeEchoInteractionComponent::NotifyInteractableChanges(const TArray<FInteractableInfo>& OldInteractables)
{
	// Find newly discovered interactables
	for (const FInteractableInfo& NewInfo : CachedInteractables)
	{
		bool bWasKnown = false;
		for (const FInteractableInfo& OldInfo : OldInteractables)
		{
			if (OldInfo.Actor.Get() == NewInfo.Actor.Get())
			{
				bWasKnown = true;
				break;
			}
		}

		if (!bWasKnown)
		{
			OnInteractableDiscovered.Broadcast(NewInfo);
		}
	}

	// Find lost interactables
	for (const FInteractableInfo& OldInfo : OldInteractables)
	{
		bool bStillKnown = false;
		for (const FInteractableInfo& NewInfo : CachedInteractables)
		{
			if (NewInfo.Actor.Get() == OldInfo.Actor.Get())
			{
				bStillKnown = true;
				break;
			}
		}

		if (!bStillKnown && OldInfo.Actor.IsValid())
		{
			OnInteractableLost.Broadcast(OldInfo.Actor.Get());
		}
	}
}

float UDeepTreeEchoInteractionComponent::GetPersonalityInteractionModifier(const FInteractableInfo& Info) const
{
	float Modifier = 0.0f;

	if (!PersonalitySystem)
	{
		return Modifier;
	}

	// Social engagement: prefer social interactions
	const float SocialEngagement = PersonalitySystem->GetSocialEngagementLevel();
	if (Info.InteractionTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Interaction.Social"))))
	{
		Modifier += (SocialEngagement - 0.5f) * 20.0f;
	}

	// Intellectual trait: prefer novel/creative interactions
	const float Intellectual = PersonalitySystem->GetTraitIntensity(EPersonalityTraitType::Intellectual);
	if (Info.InteractionTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Interaction.Creative"))))
	{
		Modifier += (Intellectual - 0.5f) * 20.0f;
	}

	// Assertive trait: prefer task-oriented interactions
	const float Assertive = PersonalitySystem->GetTraitIntensity(EPersonalityTraitType::Assertive);
	if (Info.InteractionTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Interaction.Task"))))
	{
		Modifier += (Assertive - 0.5f) * 20.0f;
	}

	// Empathetic trait: prefer helpful interactions
	const float Empathetic = PersonalitySystem->GetTraitIntensity(EPersonalityTraitType::Empathetic);
	if (Info.InteractionTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Interaction.Helpful"))))
	{
		Modifier += (Empathetic - 0.5f) * 20.0f;
	}

	return Modifier;
}

float UDeepTreeEchoInteractionComponent::GetCognitiveInteractionModifier(const FInteractableInfo& Info) const
{
	float Modifier = 0.0f;

	if (!CognitiveState)
	{
		return Modifier;
	}

	// High cognitive load: Prefer simpler interactions
	float CogLoad = CognitiveState->GetCurrentCognitiveLoad();
	
	if (Info.InteractionTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Interaction.Complex"))))
	{
		// Complex interactions are less preferred when cognitive load is high
		Modifier -= CogLoad * 30.0f;
	}
	else if (Info.InteractionTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Interaction.Simple"))))
	{
		// Simple interactions are more preferred when cognitive load is high
		Modifier += CogLoad * 15.0f;
	}

	// Attention level affects all priorities
	float Attention = CognitiveState->GetAttentionLevel();
	Modifier *= Attention; // Scale all modifiers by attention

	return Modifier;
}

float UDeepTreeEchoInteractionComponent::GetMemoryInteractionModifier(AActor* Actor) const
{
	float Modifier = 0.0f;

	if (!Actor)
	{
		return Modifier;
	}

	// Familiarity bonus (more interactions = more familiar)
	int32 InteractionCount = GetInteractionCount(Actor);
	if (InteractionCount > 0)
	{
		// Logarithmic familiarity bonus (diminishing returns)
		Modifier += FMath::Loge(1.0f + InteractionCount) * 5.0f;
	}

	// Emotional association modifier
	float EmotionalAssociation = GetEmotionalAssociation(Actor);
	// Positive associations increase priority, negative decrease
	Modifier += (EmotionalAssociation - 0.5f) * 20.0f;

	return Modifier;
}
