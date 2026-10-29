// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - Advanced Character Implementation

#include "DeepTreeEchoCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "AbilitySystemComponent.h"

// Deep Tree Echo components
#include "Personality/PersonalityTraitSystem.h"
#include "Cognitive/CognitiveStateComponent.h"
#include "Neurochemical/NeurochemicalSimulationComponent.h"
#include "Avatar/Avatar3DComponentEnhanced.h"
#include "Character/DeepTreeEchoMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Audio/AudioManagerComponent.h"
#include "Interaction/DeepTreeEchoInteractionComponent.h"
#include "Cosmetics/DeepTreeEchoCosmeticsComponent.h"

//////////////////////////////////////////////////////////////////////////
// FDeepTreeEchoReplicatedState

bool FDeepTreeEchoReplicatedState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bOutSuccess = true;

	Ar << Valence;
	Ar << Arousal;
	Ar << Dominance;
	Ar << CognitiveLoad;
	Ar << Openness;
	Ar << Conscientiousness;
	Ar << Extraversion;
	Ar << Agreeableness;
	Ar << Neuroticism;
	Ar << BehaviorFlags;

	return true;
}

//////////////////////////////////////////////////////////////////////////
// ADeepTreeEchoCharacter

ADeepTreeEchoCharacter::ADeepTreeEchoCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UDeepTreeEchoMovementComponent>(ACharacter::CharacterMovementComponentName))
	, CachedCognitiveLoad(0.0f)
	, CachedEmotionalState(FVector::ZeroVector)
	, ReplicationUpdateTimer(0.0f)
	, VisualizationUpdateTimer(0.0f)
{
	// Set this character to call Tick() every frame
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// Configure character mesh
	GetMesh()->SetCollisionProfileName(TEXT("CharacterMesh"));
	GetMesh()->SetGenerateOverlapEvents(true);
	GetMesh()->SetNotifyRigidBodyCollision(true);

	// Create personality system
	PersonalitySystem = CreateDefaultSubobject<UPersonalityTraitSystem>(TEXT("PersonalitySystem"));

	// Create cognitive state component
	CognitiveState = CreateDefaultSubobject<UCognitiveStateComponent>(TEXT("CognitiveState"));

	// Create neurochemical simulation
	NeurochemicalSystem = CreateDefaultSubobject<UNeurochemicalSimulationComponent>(TEXT("NeurochemicalSystem"));

	// Create avatar 3D component
	AvatarComponent = CreateDefaultSubobject<UAvatar3DComponentEnhanced>(TEXT("AvatarComponent"));

	// Create interaction component
	InteractionComponent = CreateDefaultSubobject<UDeepTreeEchoInteractionComponent>(TEXT("InteractionComponent"));

	// Create cosmetics component
	CosmeticsComponent = CreateDefaultSubobject<UDeepTreeEchoCosmeticsComponent>(TEXT("CosmeticsComponent"));

	// Create audio manager
	AudioManager = CreateDefaultSubobject<UAudioManagerComponent>(TEXT("AudioManager"));

	// Create ability system component
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// Configure replication
	bReplicates = true;
	SetReplicatingMovement(true);
}

void ADeepTreeEchoCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Input will be configured via Enhanced Input System in blueprints
	// or through a separate input configuration component
}

void ADeepTreeEchoCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Initialize ability system for server
	InitializeAbilitySystem();

	// Bind to cognitive events
	if (CognitiveState)
	{
		CognitiveState->OnCognitiveLoadChanged.AddDynamic(this, &ADeepTreeEchoCharacter::OnCognitiveLoadChanged);
	}
}

void ADeepTreeEchoCharacter::UnPossessed()
{
	// Unbind events
	if (CognitiveState)
	{
		CognitiveState->OnCognitiveLoadChanged.RemoveDynamic(this, &ADeepTreeEchoCharacter::OnCognitiveLoadChanged);
	}

	Super::UnPossessed();
}

void ADeepTreeEchoCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Initialize all components
	InitializeComponents();

	// No emotional state is pushed to the avatar here: the avatar, expression and audio
	// components subscribe to FEchoReactionBus themselves and show only model-chosen reactions.
}

void ADeepTreeEchoCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void ADeepTreeEchoCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update component synchronization
	UpdateComponentSynchronization();

	// Update cognitive visualization
	UpdateCognitiveVisualization();

	// Update replicated state (server only, throttled)
	if (HasAuthority())
	{
		ReplicationUpdateTimer += DeltaTime;
		if (ReplicationUpdateTimer >= 0.1f) // Update 10 times per second
		{
			UpdateReplicatedState();
			ReplicationUpdateTimer = 0.0f;
		}
	}
}

UAbilitySystemComponent* ADeepTreeEchoCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

//////////////////////////////////////////////////////////////////////////
// Emotional State Accessors

float ADeepTreeEchoCharacter::GetEmotionalValence() const
{
	if (PersonalitySystem)
	{
		return PersonalitySystem->GetEmotionalTendency().X;
	}
	return 0.5f;
}

float ADeepTreeEchoCharacter::GetEmotionalArousal() const
{
	if (PersonalitySystem)
	{
		return PersonalitySystem->GetEmotionalTendency().Y;
	}
	return 0.5f;
}

//////////////////////////////////////////////////////////////////////////
// Cognitive State Accessors

float ADeepTreeEchoCharacter::GetCognitiveLoad() const
{
	if (CognitiveState)
	{
		return CognitiveState->GetCurrentCognitiveLoad();
	}
	return 0.0f;
}

float ADeepTreeEchoCharacter::GetAttentionLevel() const
{
	if (CognitiveState)
	{
		return CognitiveState->GetAttentionLevel();
	}
	return 1.0f;
}

//////////////////////////////////////////////////////////////////////////
// Personality Accessors

float ADeepTreeEchoCharacter::GetPersonalityTrait(FName TraitName) const
{
	if (PersonalitySystem)
	{
		if (const UEnum* TraitEnum = StaticEnum<EPersonalityTraitType>())
		{
			const int64 TraitValue = TraitEnum->GetValueByName(TraitName);
			if (TraitValue != INDEX_NONE)
			{
				return PersonalitySystem->GetTraitIntensity(static_cast<EPersonalityTraitType>(TraitValue));
			}
		}
	}
	return 0.5f;
}

//////////////////////////////////////////////////////////////////////////
// Behavioral Control

void ADeepTreeEchoCharacter::TriggerEmergentBehavior(FName BehaviorName)
{
	// Trigger corresponding animation or effect
	if (AvatarComponent)
	{
		AvatarComponent->PlayGesture(BehaviorName.ToString());
	}

	// Play audio feedback
	if (AudioManager)
	{
		AudioManager->PlayGestureSound(BehaviorName);
	}
}

void ADeepTreeEchoCharacter::InterruptCurrentBehavior()
{
	// Stop current animations
	if (AvatarComponent)
	{
		if (UAnimInstance* AnimInstance = AvatarComponent->GetAnimInstance())
		{
			AnimInstance->Montage_Stop(0.2f);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Environmental Interaction

void ADeepTreeEchoCharacter::InteractWithObject(AActor* TargetObject)
{
	if (!TargetObject || !InteractionComponent)
	{
		return;
	}

	// Delegate to interaction component
	InteractionComponent->InteractWith(TargetObject);
}

TArray<AActor*> ADeepTreeEchoCharacter::GetNearbyInteractableObjects(float Radius) const
{
	TArray<AActor*> InteractableObjects;

	if (!InteractionComponent)
	{
		return InteractableObjects;
	}

	// Query interaction component for nearby objects
	InteractableObjects = InteractionComponent->FindInteractableObjects(Radius);

	return InteractableObjects;
}

//////////////////////////////////////////////////////////////////////////
// Visual Effects Control

void ADeepTreeEchoCharacter::SetEmotionalAuraColor(FLinearColor Color, float Intensity)
{
	if (AvatarComponent)
	{
		AvatarComponent->SetEmotionalAuraColor(Color, Intensity);
	}
}

//////////////////////////////////////////////////////////////////////////
// Internal Update Functions

void ADeepTreeEchoCharacter::UpdateReplicatedState()
{
	if (!HasAuthority())
	{
		return;
	}

	// Update emotional tendency (valence, arousal) from the personality system
	if (PersonalitySystem)
	{
		const FVector2D Tendency = PersonalitySystem->GetEmotionalTendency();
		ReplicatedState.Valence = FMath::Clamp(FMath::RoundToInt(Tendency.X * 255.0f), 0, 255);
		ReplicatedState.Arousal = FMath::Clamp(FMath::RoundToInt(Tendency.Y * 255.0f), 0, 255);
	}

	// Update cognitive load
	float CogLoad = CognitiveState ? CognitiveState->GetCurrentCognitiveLoad() : 0.0f;
	ReplicatedState.CognitiveLoad = FMath::Clamp(FMath::RoundToInt(CogLoad * 255.0f), 0, 255);

}

void ADeepTreeEchoCharacter::UpdateComponentSynchronization()
{
	// Synchronize cognitive state with avatar visuals
	if (CognitiveState && AvatarComponent)
	{
		float CogLoad = CognitiveState->GetCurrentCognitiveLoad();
		AvatarComponent->ApplyCognitiveLoadVisualization(CogLoad);
	}

	// The personality system's emotional state is internal persona data (model input); it is not
	// pushed to movement, avatar, cosmetics or audio as the character's displayed reaction.
}

void ADeepTreeEchoCharacter::UpdateCognitiveVisualization()
{
	VisualizationUpdateTimer += GetWorld()->GetDeltaSeconds();
	
	if (VisualizationUpdateTimer < 0.5f) // Update twice per second
	{
		return;
	}

	VisualizationUpdateTimer = 0.0f;

	if (!AvatarComponent || !CognitiveState)
	{
		return;
	}

	// Update cognitive visualization effects
	float CogLoad = CognitiveState->GetCurrentCognitiveLoad();
	AvatarComponent->ApplyCognitiveLoadVisualization(CogLoad);
}

//////////////////////////////////////////////////////////////////////////
// Network Replication

void ADeepTreeEchoCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADeepTreeEchoCharacter, ReplicatedState);
}

//////////////////////////////////////////////////////////////////////////
// Component Initialization

void ADeepTreeEchoCharacter::InitializeComponents()
{
	// Initialize interaction component
	if (InteractionComponent)
	{
		InteractionComponent->Initialize(this);
	}

	// Initialize cosmetics component
	if (CosmeticsComponent)
	{
		CosmeticsComponent->Initialize(GetMesh());
	}
}

void ADeepTreeEchoCharacter::InitializeAbilitySystem()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	// Initialize ability system component
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
}

//////////////////////////////////////////////////////////////////////////
// Event Handlers

void ADeepTreeEchoCharacter::OnCognitiveLoadChanged(float NewLoad)
{
	CachedCognitiveLoad = NewLoad;

	// Update visualization
	if (AvatarComponent)
	{
		AvatarComponent->ApplyCognitiveLoadVisualization(NewLoad);
	}

	// Affect movement speed
	if (GetCharacterMovement())
	{
		UDeepTreeEchoMovementComponent* DTEMovement = Cast<UDeepTreeEchoMovementComponent>(GetCharacterMovement());
		if (DTEMovement)
		{
			DTEMovement->SetCognitiveLoad(NewLoad);
		}
	}
}
