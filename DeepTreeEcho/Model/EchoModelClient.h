// Copyright Deep Tree Echo. All Rights Reserved.
//
// EchoModelClient — the single path by which Deep Tree Echo's characters produce language.
//
// Every utterance, reply, diary line, insight or chosen expression that is presented as a
// character's own comes from a model through this client. It speaks the OpenAI-compatible
// chat-completions protocol (POST {BaseUrl}/chat/completions), which hosted APIs and local
// servers (Ollama, llama.cpp server, LM Studio, vLLM, ...) all serve.
//
// Model resolution, in order:
//   1. Explicit configuration (environment overrides config properties):
//        ECHO_MODEL_BASE_URL  e.g. https://api.openai.com/v1  or  http://127.0.0.1:11434/v1
//        ECHO_MODEL_NAME      optional when the endpoint lists its models (the first is used)
//        ECHO_MODEL_API_KEY   optional; local servers need none
//   2. Otherwise a locally served model, verified before use: the server must answer AND list at
//      least one model. Probed: ECHO_MODEL_LOCAL_URLS (comma-separated base URLs), Ollama
//      (127.0.0.1:11434/api/tags), then OpenAI-compatible /v1/models on ports 8080 (llama.cpp),
//      1234 (LM Studio), 8000 (vLLM) and 5000 (text-generation-webui).
//   3. Nothing configured and no local model served: requests fail with an explanation.
//
// There is no offline, mock or fallback mode: on failure the callback receives bSuccess = false
// and an error description, and the caller produces no character output for that turn.
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
	/** Endpoint base URL; ECHO_MODEL_BASE_URL overrides it. Empty = discover a local model. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Echo Model")
	FString BaseUrl;

	/** Model name; ECHO_MODEL_NAME overrides it. Empty = first model the endpoint lists. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Echo Model")
	FString ModelName;

	/** Bearer token; ECHO_MODEL_API_KEY overrides it. Not needed for local servers. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Echo Model")
	FString ApiKey;

	/** Extra local base URLs probed first; ECHO_MODEL_LOCAL_URLS (comma-separated) overrides it. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Echo Model")
	TArray<FString> LocalUrls;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Echo Model")
	float TimeoutSeconds = 60.0f;

	/** Timeout for each local-server probe. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Echo Model")
	float ProbeTimeoutSeconds = 1.5f;

	/** Shared instance (rooted), configured from config + environment on first use. */
	static UEchoModelClient* Get();

	/** True once an endpoint and model have been resolved (explicitly or by local discovery). */
	UFUNCTION(BlueprintPure, Category = "Echo Model")
	bool IsResolved() const { return bResolved; }

	/** Resolved endpoint and model (empty until resolved). */
	UFUNCTION(BlueprintPure, Category = "Echo Model")
	FString GetResolvedEndpoint() const { return ResolvedBaseUrl; }
	UFUNCTION(BlueprintPure, Category = "Echo Model")
	FString GetResolvedModel() const { return ResolvedModel; }

	/** Last resolution or request error, or empty. */
	UFUNCTION(BlueprintPure, Category = "Echo Model")
	FString GetLastError() const { return LastError; }

	/**
	 * Resolves the endpoint and model (explicit configuration, else a verified local server).
	 * OnDone fires on the game thread: (true, "<model> @ <endpoint>") or (false, error).
	 */
	void Resolve(FOnEchoModelReply OnDone);

	/**
	 * Sends a chat request, resolving the model first if needed. OnComplete fires on the game
	 * thread exactly once: (true, reply text) on success, (false, error) when no model is
	 * available, on transport/HTTP errors, or when the response carries no message content.
	 */
	void Chat(const TArray<FEchoModelMessage>& Messages, FOnEchoModelReply OnComplete, float Temperature = 0.8f);

	/**
	 * Fills a prompt template: every "{key}" in Template is replaced by Slots[key]. Templates are
	 * instructions to the model, never text shown as the character's own words.
	 */
	static FString FillPrompt(const FString& Template, const TMap<FString, FString>& Slots);

	/**
	 * Extracts the first JSON object from model text (models sometimes wrap JSON in prose or
	 * code fences). Returns false when no parseable object is present.
	 */
	static bool ParseJsonObject(const FString& Text, TSharedPtr<FJsonObject>& OutObject);

	/** Loads environment overrides into the properties and forgets any previous resolution. */
	void ApplyEnvironment();

private:
	struct FProbe
	{
		FString Url;      // URL to GET
		FString BaseUrl;  // chat base URL if the probe succeeds
		bool bOllamaTags; // response format: Ollama /api/tags vs OpenAI /models
	};

	void BuildProbes(TArray<FProbe>& Out) const;
	void RunProbe(TSharedRef<TArray<FProbe>> Probes, int32 Index, TSharedRef<TArray<FString>> Tried);
	void FinishResolve(bool bOk, const FString& BaseOrError, const FString& Model);
	void SendChat(const TArray<FEchoModelMessage>& Messages, FOnEchoModelReply OnComplete, float Temperature);
	static bool ExtractModelIds(const FString& Body, bool bOllamaTags, TArray<FString>& OutIds);

	bool bResolved = false;
	bool bResolving = false;
	FString ResolvedBaseUrl;
	FString ResolvedModel;
	FString LastError;
	TArray<FOnEchoModelReply> PendingResolves;

	static UEchoModelClient* Instance;
};
