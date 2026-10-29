// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - Interactable Object Implementation

#include "InteractableObject.h"
#include "Components/StaticMeshComponent.h"
#include "Personality/PersonalityTraitSystem.h"
#include "Cognitive/EmotionalState.h"
#include "EchoModelClient.h"
#include "Dom/JsonObject.h"

//////////////////////////////////////////////////////////////////////////
// AInteractableObject

AInteractableObject::AInteractableObject()
	: bCanInteractWhileBusy(false)
	, MaxSimultaneousInteractors(1)
	, InteractionProgress(0.0f)
{
	PrimaryActorTick.bCanEverTick = true;

	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(RootSceneComponent);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetupAttachment(RootSceneComponent);

	// Add interactable tag
	Tags.Add(TEXT("Interactable"));
}

void AInteractableObject::BeginPlay()
{
	Super::BeginPlay();
}

void AInteractableObject::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (ActiveInteractions.Num() > 0)
	{
		UpdateInteraction(DeltaTime);
	}
}

bool AInteractableObject::IsInteractionAvailable(AActor* Interactor, EInteractionType Type) const
{
	if (!Interactor || !InteractableData)
	{
		return false;
	}

	// Check if already at max interactors
	if (!bCanInteractWhileBusy && CurrentInteractors.Num() >= MaxSimultaneousInteractors)
	{
		return false;
	}

	// Find the interaction data
	const FInteractionData* Data = nullptr;
	for (const FInteractionData& InteractionData : InteractableData->AvailableInteractions)
	{
		if (InteractionData.Type == Type)
		{
			Data = &InteractionData;
			break;
		}
	}

	if (!Data)
	{
		return false;
	}

	return CheckInteractionRequirements(Interactor, *Data);
}

TArray<FInteractionData> AInteractableObject::GetAvailableInteractions(AActor* Interactor) const
{
	TArray<FInteractionData> Available;

	if (!Interactor || !InteractableData)
	{
		return Available;
	}

	for (const FInteractionData& Data : InteractableData->AvailableInteractions)
	{
		if (CheckInteractionRequirements(Interactor, Data))
		{
			Available.Add(Data);
		}
	}

	return Available;
}

bool AInteractableObject::BeginInteraction(AActor* Interactor, EInteractionType Type)
{
	if (!IsInteractionAvailable(Interactor, Type))
	{
		return false;
	}

	// Add to current interactors
	CurrentInteractors.Add(Interactor);
	ActiveInteractions.Add(Interactor, Type);

	// Notify
	OnInteractionStarted(Interactor, Type);
	OnInteractionBegin.Broadcast(Interactor, Type);

	return true;
}

void AInteractableObject::EndInteraction(AActor* Interactor, bool bSuccess)
{
	if (!Interactor)
	{
		return;
	}

	EInteractionType* TypePtr = ActiveInteractions.Find(Interactor);
	if (!TypePtr)
	{
		return;
	}

	EInteractionType Type = *TypePtr;

	// Remove from tracking
	CurrentInteractors.Remove(Interactor);
	ActiveInteractions.Remove(Interactor);

	// Notify
	OnInteractionCompleted(Interactor, Type, bSuccess);
	OnInteractionEnd.Broadcast(Interactor, Type, bSuccess);
}

void AInteractableObject::CancelInteraction(AActor* Interactor)
{
	EndInteraction(Interactor, false);
}

FInteractionData AInteractableObject::GetInteractionData(EInteractionType Type) const
{
	if (InteractableData)
	{
		for (const FInteractionData& Data : InteractableData->AvailableInteractions)
		{
			if (Data.Type == Type)
			{
				return Data;
			}
		}
	}

	return FInteractionData();
}

float AInteractableObject::GetPersonalityAffinity(const FPersonalityState& Personality) const
{
	if (!InteractableData)
	{
		return 0.5f;
	}

	float Affinity = 0.0f;

	// Calculate weighted affinity based on personality traits
	Affinity += Personality.Extraversion * InteractableData->ExtraversionAffinity;
	Affinity += Personality.Openness * InteractableData->OpennessAffinity;
	Affinity += Personality.Conscientiousness * InteractableData->ConscientiousnessAffinity;
	Affinity += Personality.Agreeableness * InteractableData->AgreeablenessAffinity;
	Affinity += (1.0f - Personality.Neuroticism) * InteractableData->NeuroticismAffinity; // Inverted

	// Normalize to 0-1 range
	Affinity /= 5.0f;

	return FMath::Clamp(Affinity, 0.0f, 1.0f);
}

void AInteractableObject::OnInteractionStarted(AActor* Interactor, EInteractionType Type)
{
	// Override in subclasses
	InteractionProgress = 0.0f;
}

void AInteractableObject::OnInteractionCompleted(AActor* Interactor, EInteractionType Type, bool bSuccess)
{
	// Override in subclasses
	InteractionProgress = 0.0f;
}

void AInteractableObject::UpdateInteraction(float DeltaTime)
{
	// Update progress for timed interactions. Finished ones are ended after the loop:
	// EndInteraction removes from ActiveInteractions, which must not change while iterating.
	TArray<AActor*> Finished;
	for (auto& Pair : ActiveInteractions)
	{
		AActor* Interactor = Pair.Key.Get();
		EInteractionType Type = Pair.Value;

		if (!Interactor)
		{
			continue;
		}

		FInteractionData Data = GetInteractionData(Type);
		if (Data.Duration > 0.0f)
		{
			InteractionProgress += DeltaTime / Data.Duration;

			if (InteractionProgress >= 1.0f)
			{
				Finished.Add(Interactor);
			}
		}
	}
	for (AActor* Interactor : Finished)
	{
		EndInteraction(Interactor, true);
	}
}

bool AInteractableObject::CheckInteractionRequirements(AActor* Interactor, const FInteractionData& Data) const
{
	if (!Interactor)
	{
		return false;
	}

	// Check distance
	float Distance = FVector::Dist(GetActorLocation(), Interactor->GetActorLocation());
	if (Distance > Data.MaxInteractionDistance)
	{
		return false;
	}

	// Check line of sight if required
	if (Data.bRequiresLineOfSight)
	{
		FHitResult Hit;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);
		QueryParams.AddIgnoredActor(Interactor);

		bool bHit = GetWorld()->LineTraceSingleByChannel(
			Hit,
			Interactor->GetActorLocation(),
			GetActorLocation(),
			ECC_Visibility,
			QueryParams
		);

		if (bHit)
		{
			return false; // Something is blocking line of sight
		}
	}

	// Check required tags (would need to get tags from interactor)
	// For now, return true if basic requirements are met

	return true;
}

//////////////////////////////////////////////////////////////////////////
// AInteractableContainer

AInteractableContainer::AInteractableContainer()
	: bIsOpen(false)
{
}

void AInteractableContainer::Open(AActor* Opener)
{
	if (!bIsOpen)
	{
		bIsOpen = true;

		// Spawn contained items
		for (TSubclassOf<AActor> ItemClass : ContainedItemClasses)
		{
			if (ItemClass)
			{
				FActorSpawnParameters SpawnParams;
				SpawnParams.Owner = this;
				AActor* SpawnedItem = GetWorld()->SpawnActor<AActor>(ItemClass, GetActorLocation(), GetActorRotation(), SpawnParams);
				if (SpawnedItem)
				{
					SpawnedContents.Add(SpawnedItem);
				}
			}
		}
	}
}

void AInteractableContainer::Close(AActor* Closer)
{
	if (bIsOpen)
	{
		bIsOpen = false;

		// Destroy spawned contents
		for (AActor* Item : SpawnedContents)
		{
			if (Item)
			{
				Item->Destroy();
			}
		}
		SpawnedContents.Empty();
	}
}

TArray<AActor*> AInteractableContainer::GetContents() const
{
	TArray<AActor*> Contents;
	for (AActor* Item : SpawnedContents)
	{
		if (Item)
		{
			Contents.Add(Item);
		}
	}
	return Contents;
}

void AInteractableContainer::OnInteractionStarted(AActor* Interactor, EInteractionType Type)
{
	Super::OnInteractionStarted(Interactor, Type);

	if (Type == EInteractionType::Open)
	{
		if (bIsOpen)
		{
			Close(Interactor);
		}
		else
		{
			Open(Interactor);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// AInteractableFurniture

AInteractableFurniture::AInteractableFurniture()
	: SitSocketName(TEXT("SitSocket"))
{
}

bool AInteractableFurniture::Occupy(AActor* Occupant)
{
	if (IsOccupied() || !Occupant)
	{
		return false;
	}

	CurrentOccupant = Occupant;
	return true;
}

void AInteractableFurniture::Vacate(AActor* Occupant)
{
	if (CurrentOccupant == Occupant)
	{
		CurrentOccupant = nullptr;
	}
}

FTransform AInteractableFurniture::GetSitTransform() const
{
	FTransform SitTransform = GetActorTransform();

	// Apply offset
	SitTransform = SitOffset * SitTransform;

	return SitTransform;
}

void AInteractableFurniture::OnInteractionStarted(AActor* Interactor, EInteractionType Type)
{
	Super::OnInteractionStarted(Interactor, Type);

	if (Type == EInteractionType::Sit)
	{
		Occupy(Interactor);
	}
}

void AInteractableFurniture::OnInteractionCompleted(AActor* Interactor, EInteractionType Type, bool bSuccess)
{
	Super::OnInteractionCompleted(Interactor, Type, bSuccess);

	if (Type == EInteractionType::Sit)
	{
		Vacate(Interactor);
	}
}

//////////////////////////////////////////////////////////////////////////
// AInteractableNPC

AInteractableNPC::AInteractableNPC()
	: RelationshipLevel(0.5f)
	, bInConversation(false)
{
	GreetingPromptTemplate = TEXT(
		"You are {name}, a character in a game world. {partner} has just walked up to talk to you. "
		"Your relationship with them is {relationship} on a scale from 0 (hostile stranger) to 1 (close friend). "
		"Write what you say to greet them, in your own words, at most {max_chars} characters.\n"
		"Style examples written by the designer (match their tone, do not copy them):\n{style_examples}\n"
		"Reply with only a JSON object: {\"greeting\": \"<what you say>\"}");
}

void AInteractableNPC::StartConversation(AActor* Initiator)
{
	if (!bInConversation && Initiator)
	{
		bInConversation = true;
		ConversationPartner = Initiator;
		LastGreeting = FText::GetEmpty();

		// The greeting is written by the model; until (and unless) it replies, the NPC says nothing
		RequestGreeting(Initiator);
	}
}

void AInteractableNPC::RequestGreeting(AActor* Partner)
{
	TArray<FString> Examples;
	for (const FText& Line : GreetingLines)
	{
		if (!Line.IsEmpty())
		{
			Examples.Add(FString::Printf(TEXT("- %s"), *Line.ToString()));
		}
	}

	TMap<FString, FString> Slots;
	Slots.Add(TEXT("name"), NPCName.IsEmpty() ? GetName() : NPCName.ToString());
	Slots.Add(TEXT("partner"), Partner->GetName());
	Slots.Add(TEXT("relationship"), FString::Printf(TEXT("%.2f"), FMath::Clamp(RelationshipLevel, 0.0f, 1.0f)));
	Slots.Add(TEXT("max_chars"), FString::FromInt(FMath::Max(20, MaxGreetingChars)));
	Slots.Add(TEXT("style_examples"), Examples.Num() ? FString::Join(Examples, TEXT("\n")) : FString(TEXT("(none)")));

	TArray<FEchoModelMessage> Messages;
	Messages.Add(FEchoModelMessage(TEXT("user"), UEchoModelClient::FillPrompt(GreetingPromptTemplate, Slots)));

	const int32 RequestSerial = ++GreetingRequestSerial;
	TWeakObjectPtr<AInteractableNPC> WeakThis(this);
	TWeakObjectPtr<AActor> WeakPartner(Partner);
	UEchoModelClient::Get()->Chat(Messages, FOnEchoModelReply::CreateLambda(
		[WeakThis, RequestSerial, WeakPartner](bool bSuccess, const FString& TextOrError)
		{
			if (AInteractableNPC* Self = WeakThis.Get())
			{
				Self->HandleGreetingReply(RequestSerial, WeakPartner, bSuccess, TextOrError);
			}
		}), 0.8f);
}

void AInteractableNPC::HandleGreetingReply(int32 RequestSerial, TWeakObjectPtr<AActor> Partner, bool bSuccess, const FString& TextOrError)
{
	// Superseded, or the conversation ended / changed partner while the model was thinking
	if (RequestSerial != GreetingRequestSerial || !bInConversation || !Partner.IsValid() || ConversationPartner != Partner)
	{
		return;
	}

	auto Fail = [this, &Partner](const FString& Error)
	{
		LastGreetingError = Error;
		UE_LOG(LogTemp, Error, TEXT("InteractableNPC %s: no model greeting: %s"), *GetName(), *Error);
		OnGreetingFailed.Broadcast(Partner.Get(), Error);
	};

	if (!bSuccess)
	{
		Fail(TextOrError);
		return;
	}

	TSharedPtr<FJsonObject> Json;
	FString Greeting;
	if (!UEchoModelClient::ParseJsonObject(TextOrError, Json) || !Json->TryGetStringField(TEXT("greeting"), Greeting))
	{
		Fail(FString::Printf(TEXT("model reply is not the requested JSON: %s"), *TextOrError.Left(200)));
		return;
	}

	Greeting.TrimStartAndEndInline();
	if (Greeting.IsEmpty())
	{
		Fail(TEXT("model greeting is empty"));
		return;
	}
	if (Greeting.Len() > FMath::Max(20, MaxGreetingChars))
	{
		Fail(FString::Printf(TEXT("model greeting is %d characters, more than %d"), Greeting.Len(), MaxGreetingChars));
		return;
	}
	for (const FText& Line : GreetingLines)
	{
		if (Greeting.Equals(Line.ToString().TrimStartAndEnd(), ESearchCase::IgnoreCase))
		{
			Fail(TEXT("model copied a designer style example verbatim"));
			return;
		}
	}

	LastGreetingError.Reset();
	LastGreeting = FText::FromString(Greeting);
	UE_LOG(LogTemp, Log, TEXT("NPC %s says: %s"), *NPCName.ToString(), *Greeting);
	OnGreeting.Broadcast(Partner.Get(), LastGreeting);
}

void AInteractableNPC::EndConversation()
{
	bInConversation = false;
	ConversationPartner = nullptr;
}

void AInteractableNPC::OnInteractionStarted(AActor* Interactor, EInteractionType Type)
{
	Super::OnInteractionStarted(Interactor, Type);

	if (Type == EInteractionType::Talk)
	{
		StartConversation(Interactor);
	}
}

void AInteractableNPC::OnInteractionCompleted(AActor* Interactor, EInteractionType Type, bool bSuccess)
{
	Super::OnInteractionCompleted(Interactor, Type, bSuccess);

	if (Type == EInteractionType::Talk)
	{
		EndConversation();

		// Update relationship based on success
		if (bSuccess)
		{
			RelationshipLevel = FMath::Clamp(RelationshipLevel + 0.05f, 0.0f, 1.0f);
		}
	}
}
