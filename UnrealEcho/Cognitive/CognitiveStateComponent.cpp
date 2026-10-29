// CognitiveStateComponent.cpp

#include "CognitiveStateComponent.h"

UCognitiveStateComponent::UCognitiveStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCognitiveStateComponent::SetCognitiveLoad(float NewLoad)
{
	const float Clamped = FMath::Clamp(NewLoad, 0.0f, 1.0f);
	if (!FMath::IsNearlyEqual(Clamped, CognitiveLoad))
	{
		CognitiveLoad = Clamped;
		OnCognitiveLoadChanged.Broadcast(CognitiveLoad);
	}
}

void UCognitiveStateComponent::SetAttentionLevel(float NewLevel)
{
	AttentionLevel = FMath::Clamp(NewLevel, 0.0f, 1.0f);
}
