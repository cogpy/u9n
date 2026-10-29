// Copyright Deep Tree Echo. All Rights Reserved.
//
// EchoCharacterReaction — the one channel through which a character's visible reaction
// (what it says, which expression it shows, which gesture it makes) reaches the avatar.
//
// Producers: code that has just received a model reply (via UEchoModelClient) and parsed the
// model's chosen utterance / expression / gesture. Consumers: expression, gesture, animation,
// audio and caption systems, which apply only what arrives here.
//
// There is no other source: no timers, dice, clock steps or keyword tables may synthesise a
// reaction. When no model reply arrives, nothing is broadcast and the avatar shows nothing new.
#pragma once

#include "CoreMinimal.h"

struct FEchoCharacterReaction
{
	/** Which character reacted (e.g. "Deep Tree Echo"). */
	FString CharacterName;

	/** The model's utterance; empty when the model chose not to speak. */
	FString Utterance;

	/** Expression name chosen by the model from the allowed vocabulary; empty = no change. */
	FString Expression;

	/** Gesture name chosen by the model from the allowed vocabulary; empty = none. */
	FString Gesture;

	/** Model-chosen intensity in [0, 1]. */
	float Intensity = 0.0f;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnEchoCharacterReaction, const FEchoCharacterReaction&);

struct FEchoReactionBus
{
	/** Broadcast only with content parsed from a model reply. */
	static FOnEchoCharacterReaction& OnReaction()
	{
		static FOnEchoCharacterReaction Delegate;
		return Delegate;
	}
};
