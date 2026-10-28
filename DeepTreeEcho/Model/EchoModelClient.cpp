// Copyright Deep Tree Echo. All Rights Reserved.

#include "EchoModelClient.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "HAL/PlatformMisc.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

UEchoModelClient* UEchoModelClient::Instance = nullptr;

UEchoModelClient* UEchoModelClient::Get()
{
	if (!Instance)
	{
		Instance = NewObject<UEchoModelClient>();
		Instance->AddToRoot();
		Instance->ApplyEnvironment();
	}
	return Instance;
}

void UEchoModelClient::ApplyEnvironment()
{
	const FString EnvUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("ECHO_MODEL_BASE_URL"));
	const FString EnvModel = FPlatformMisc::GetEnvironmentVariable(TEXT("ECHO_MODEL_NAME"));
	const FString EnvKey = FPlatformMisc::GetEnvironmentVariable(TEXT("ECHO_MODEL_API_KEY"));
	if (!EnvUrl.IsEmpty()) BaseUrl = EnvUrl;
	if (!EnvModel.IsEmpty()) ModelName = EnvModel;
	if (!EnvKey.IsEmpty()) ApiKey = EnvKey;
	BaseUrl.RemoveFromEnd(TEXT("/"));
}

bool UEchoModelClient::IsConfigured() const
{
	return ConfigurationError().IsEmpty();
}

FString UEchoModelClient::ConfigurationError() const
{
	if (BaseUrl.IsEmpty()) return TEXT("Echo model not configured: set ECHO_MODEL_BASE_URL");
	if (ModelName.IsEmpty()) return TEXT("Echo model not configured: set ECHO_MODEL_NAME");
	return FString();
}

void UEchoModelClient::Chat(const TArray<FEchoModelMessage>& Messages, FOnEchoModelReply OnComplete, float Temperature)
{
	const FString ConfigError = ConfigurationError();
	if (!ConfigError.IsEmpty())
	{
		OnComplete.ExecuteIfBound(false, ConfigError);
		return;
	}

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("model"), ModelName);
	Body->SetNumberField(TEXT("temperature"), Temperature);
	TArray<TSharedPtr<FJsonValue>> JsonMessages;
	for (const FEchoModelMessage& M : Messages)
	{
		TSharedRef<FJsonObject> Msg = MakeShared<FJsonObject>();
		Msg->SetStringField(TEXT("role"), M.Role);
		Msg->SetStringField(TEXT("content"), M.Content);
		JsonMessages.Add(MakeShared<FJsonValueObject>(Msg));
	}
	Body->SetArrayField(TEXT("messages"), JsonMessages);

	FString Payload;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
	FJsonSerializer::Serialize(Body, Writer);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(BaseUrl + TEXT("/chat/completions"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	if (!ApiKey.IsEmpty())
	{
		Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + ApiKey);
	}
	Request->SetTimeout(TimeoutSeconds);
	Request->SetContentAsString(Payload);
	Request->OnProcessRequestComplete().BindLambda(
		[OnComplete](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
		{
			if (!bConnected || !Response.IsValid())
			{
				OnComplete.ExecuteIfBound(false, TEXT("Echo model request failed: no response from endpoint"));
				return;
			}
			const int32 Code = Response->GetResponseCode();
			const FString Content = Response->GetContentAsString();
			if (Code < 200 || Code >= 300)
			{
				OnComplete.ExecuteIfBound(false, FString::Printf(TEXT("Echo model request failed: HTTP %d: %s"), Code, *Content.Left(512)));
				return;
			}
			TSharedPtr<FJsonObject> Json;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
			const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
			if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid()
				|| !Json->TryGetArrayField(TEXT("choices"), Choices) || Choices->Num() == 0)
			{
				OnComplete.ExecuteIfBound(false, TEXT("Echo model request failed: response has no choices"));
				return;
			}
			const TSharedPtr<FJsonObject>* Choice = nullptr;
			const TSharedPtr<FJsonObject>* Message = nullptr;
			FString Text;
			if (!(*Choices)[0]->TryGetObject(Choice) || !(*Choice)->TryGetObjectField(TEXT("message"), Message)
				|| !(*Message)->TryGetStringField(TEXT("content"), Text) || Text.TrimStartAndEnd().IsEmpty())
			{
				OnComplete.ExecuteIfBound(false, TEXT("Echo model request failed: empty message content"));
				return;
			}
			OnComplete.ExecuteIfBound(true, Text.TrimStartAndEnd());
		});
	if (!Request->ProcessRequest())
	{
		OnComplete.ExecuteIfBound(false, TEXT("Echo model request failed: could not start HTTP request"));
	}
}

bool UEchoModelClient::ParseJsonObject(const FString& Text, TSharedPtr<FJsonObject>& OutObject)
{
	const int32 Start = Text.Find(TEXT("{"));
	const int32 End = Text.Find(TEXT("}"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	if (Start == INDEX_NONE || End == INDEX_NONE || End <= Start) return false;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text.Mid(Start, End - Start + 1));
	return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
}
