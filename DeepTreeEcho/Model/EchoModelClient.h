// Copyright Deep Tree Echo. All Rights Reserved.
//
// EchoModelClient — the single path by which Deep Tree Echo's characters produce language.
//
// Every utterance, reply, diary line, insight or chosen expression that is presented as a
// character's own must come from a model. This client calls an OpenAI-compatible
// chat-completions endpoint (OpenAI, Ollama /v1, echo9llama, a NanEcho server, ...):
//
//   POST {BaseUrl}/chat/completions  { model, messages, temperature }
//
// Configuration (environment variables override the config properties):
//   ECHO_MODEL_BASE_URL   e.g. https://api.openai.com/v1  or  http://127.0.0.1:11434/v1
//   ECHO_MODEL_NAME       model identifier understood by that endpoint
//   ECHO_MODEL_API_KEY    bearer token (optional for local servers)
//
// There is deliberately no offline, mock or fallback mode: when the client is not configured
// or a request fails, the callback receives bSuccess = false and an error description, and
// the caller must produce no character output for that turn.
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Dom/JsonObject.h"
#include "EchoModelClient.generated.h"

USTRUCT(BlueprintType)
struct DEEPTREEECHO_API FEchoModelMessage
{
	GENERATED_BODY()

	/** "system", "user" or "assistant". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo Model")
	FString Role;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo Model")
	FString Content;

	FEchoModelMessage() {}
	FEchoModelMessage(const FString& InRole, const FString& InContent) : Role(InRole), Content(InContent) {}
};

/** bSuccess = true: Text is the model's reply. bSuccess = false: Text is an error description (never shown as the character's words). */
DECLARE_DELEGATE_TwoParams(FOnEchoModelReply, bool /*bSuccess*/, const FString& /*TextOrError*/);

UCLASS(BlueprintType, Config = Game)
class DEEPTREEECHO_API UEchoModelClient : public UObject
{
	GENERATED_BODY()

public:
	/** Endpoint base URL; ECHO_MODEL_BASE_URL overrides it. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Echo Model")
	FString BaseUrl;

	/** Model name; ECHO_MODEL_NAME overrides it. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Echo Model")
	FString ModelName;

	/** Bearer token; ECHO_MODEL_API_KEY overrides it. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Echo Model")
	FString ApiKey;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Echo Model")
	float TimeoutSeconds = 60.0f;

	/** Shared instance (rooted), configured from config + environment on first use. */
	static UEchoModelClient* Get();

	/** True when a base URL and a model name are set. */
	UFUNCTION(BlueprintPure, Category = "Echo Model")
	bool IsConfigured() const;

	/** Why the client is not usable, or empty when it is. */
	FString ConfigurationError() const;

	/**
	 * Sends a chat request. OnComplete fires on the game thread exactly once:
	 * (true, reply text) on success, (false, error) when unconfigured, on transport/HTTP
	 * errors, or when the response carries no message content.
	 */
	void Chat(const TArray<FEchoModelMessage>& Messages, FOnEchoModelReply OnComplete, float Temperature = 0.8f);

	/**
	 * Extracts the first JSON object from model text (models sometimes wrap JSON in prose or
	 * code fences). Returns false when no parseable object is present.
	 */
	static bool ParseJsonObject(const FString& Text, TSharedPtr<FJsonObject>& OutObject);

	/** Loads environment overrides into the properties. */
	void ApplyEnvironment();

private:
	static UEchoModelClient* Instance;
};
