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
	const FString EnvLocal = FPlatformMisc::GetEnvironmentVariable(TEXT("ECHO_MODEL_LOCAL_URLS"));
	if (!EnvUrl.IsEmpty()) BaseUrl = EnvUrl;
	if (!EnvModel.IsEmpty()) ModelName = EnvModel;
	if (!EnvKey.IsEmpty()) ApiKey = EnvKey;
	if (!EnvLocal.IsEmpty())
	{
		LocalUrls.Reset();
		EnvLocal.ParseIntoArray(LocalUrls, TEXT(","), true);
	}
	BaseUrl.RemoveFromEnd(TEXT("/"));
	for (FString& Url : LocalUrls)
	{
		Url.TrimStartAndEndInline();
		Url.RemoveFromEnd(TEXT("/"));
	}
	bResolved = false;
	ResolvedBaseUrl.Reset();
	ResolvedModel.Reset();
}

void UEchoModelClient::BuildProbes(TArray<FProbe>& Out) const
{
	if (!BaseUrl.IsEmpty())
	{
		// Explicit endpoint: verify it and learn its models.
		Out.Add({BaseUrl + TEXT("/models"), BaseUrl, false});
		return;
	}
	for (const FString& Url : LocalUrls)
	{
		if (!Url.IsEmpty()) Out.Add({Url + TEXT("/models"), Url, false});
	}
	Out.Add({TEXT("http://127.0.0.1:11434/api/tags"), TEXT("http://127.0.0.1:11434/v1"), true});
	for (const int32 Port : {8080, 1234, 8000, 5000})
	{
		const FString Base = FString::Printf(TEXT("http://127.0.0.1:%d/v1"), Port);
		Out.Add({Base + TEXT("/models"), Base, false});
	}
}

bool UEchoModelClient::ExtractModelIds(const FString& Body, bool bOllamaTags, TArray<FString>& OutIds)
{
	TSharedPtr<FJsonObject> Json;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid()) return false;
	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	if (!Json->TryGetArrayField(bOllamaTags ? TEXT("models") : TEXT("data"), Items)) return false;
	for (const TSharedPtr<FJsonValue>& Item : *Items)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		FString Id;
		if (Item->TryGetObject(Obj) && (*Obj)->TryGetStringField(bOllamaTags ? TEXT("name") : TEXT("id"), Id) && !Id.IsEmpty())
		{
			OutIds.Add(Id);
		}
	}
	return OutIds.Num() > 0;
}

void UEchoModelClient::Resolve(FOnEchoModelReply OnDone)
{
	if (bResolved)
	{
		OnDone.ExecuteIfBound(true, ResolvedModel + TEXT(" @ ") + ResolvedBaseUrl);
		return;
	}
	// A hosted API may not list its models; an explicit endpoint plus model name is used as given.
	if (!BaseUrl.IsEmpty() && !ModelName.IsEmpty())
	{
		FinishResolve(true, BaseUrl, ModelName);
		OnDone.ExecuteIfBound(true, ResolvedModel + TEXT(" @ ") + ResolvedBaseUrl);
		return;
	}
	PendingResolves.Add(OnDone);
	if (bResolving) return;
	bResolving = true;

	TSharedRef<TArray<FProbe>> Probes = MakeShared<TArray<FProbe>>();
	BuildProbes(*Probes);
	RunProbe(Probes, 0, MakeShared<TArray<FString>>());
}

void UEchoModelClient::RunProbe(TSharedRef<TArray<FProbe>> Probes, int32 Index, TSharedRef<TArray<FString>> Tried)
{
	if (Index >= Probes->Num())
	{
		const FString Error = BaseUrl.IsEmpty()
			? FString::Printf(TEXT("No model available: ECHO_MODEL_BASE_URL is not set and no local model server is serving a model (tried %s). Serve one locally, e.g. `ollama serve` and `ollama pull <model>`, or set ECHO_MODEL_BASE_URL."), *FString::Join(*Tried, TEXT(", ")))
			: FString::Printf(TEXT("No model available at %s (%s). Set ECHO_MODEL_NAME or check the endpoint."), *BaseUrl, *FString::Join(*Tried, TEXT(", ")));
		FinishResolve(false, Error, FString());
		return;
	}

	const FProbe Probe = (*Probes)[Index];
	TWeakObjectPtr<UEchoModelClient> WeakThis(this);
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Probe.Url);
	Request->SetVerb(TEXT("GET"));
	if (!ApiKey.IsEmpty() && !BaseUrl.IsEmpty())
	{
		Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + ApiKey);
	}
	Request->SetTimeout(BaseUrl.IsEmpty() ? ProbeTimeoutSeconds : TimeoutSeconds);
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, Probes, Index, Tried, Probe](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
		{
			UEchoModelClient* Self = WeakThis.Get();
			if (!Self) return;
			TArray<FString> Ids;
			if (!bConnected || !Response.IsValid())
			{
				Tried->Add(Probe.Url + TEXT(": no server"));
			}
			else if (Response->GetResponseCode() < 200 || Response->GetResponseCode() >= 300)
			{
				Tried->Add(FString::Printf(TEXT("%s: HTTP %d"), *Probe.Url, Response->GetResponseCode()));
			}
			else if (!ExtractModelIds(Response->GetContentAsString(), Probe.bOllamaTags, Ids))
			{
				Tried->Add(Probe.Url + TEXT(": serves no model"));
			}
			else if (!Self->ModelName.IsEmpty() && !Ids.Contains(Self->ModelName))
			{
				Tried->Add(FString::Printf(TEXT("%s: model '%s' not served"), *Probe.Url, *Self->ModelName));
			}
			else
			{
				Self->FinishResolve(true, Probe.BaseUrl, Self->ModelName.IsEmpty() ? Ids[0] : Self->ModelName);
				return;
			}
			Self->RunProbe(Probes, Index + 1, Tried);
		});
	if (!Request->ProcessRequest())
	{
		Tried->Add(Probe.Url + TEXT(": request not started"));
		RunProbe(Probes, Index + 1, Tried);
	}
}

void UEchoModelClient::FinishResolve(bool bOk, const FString& BaseOrError, const FString& Model)
{
	bResolving = false;
	bResolved = bOk;
	if (bOk)
	{
		ResolvedBaseUrl = BaseOrError;
		ResolvedModel = Model;
		LastError.Reset();
		UE_LOG(LogTemp, Log, TEXT("EchoModelClient: using model %s @ %s"), *ResolvedModel, *ResolvedBaseUrl);
	}
	else
	{
		LastError = BaseOrError;
		UE_LOG(LogTemp, Warning, TEXT("EchoModelClient: %s"), *LastError);
	}
	TArray<FOnEchoModelReply> Waiting = MoveTemp(PendingResolves);
	PendingResolves.Reset();
	for (FOnEchoModelReply& Callback : Waiting)
	{
		Callback.ExecuteIfBound(bOk, bOk ? ResolvedModel + TEXT(" @ ") + ResolvedBaseUrl : LastError);
	}
}

void UEchoModelClient::Chat(const TArray<FEchoModelMessage>& Messages, FOnEchoModelReply OnComplete, float Temperature)
{
	if (bResolved)
	{
		SendChat(Messages, OnComplete, Temperature);
		return;
	}
	TWeakObjectPtr<UEchoModelClient> WeakThis(this);
	Resolve(FOnEchoModelReply::CreateLambda([WeakThis, Messages, OnComplete, Temperature](bool bOk, const FString& Info)
	{
		if (!bOk)
		{
			OnComplete.ExecuteIfBound(false, Info);
			return;
		}
		if (UEchoModelClient* Self = WeakThis.Get())
		{
			Self->SendChat(Messages, OnComplete, Temperature);
		}
	}));
}

void UEchoModelClient::SendChat(const TArray<FEchoModelMessage>& Messages, FOnEchoModelReply OnComplete, float Temperature)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("model"), ResolvedModel);
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

	TWeakObjectPtr<UEchoModelClient> WeakThis(this);
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(ResolvedBaseUrl + TEXT("/chat/completions"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	if (!ApiKey.IsEmpty() && !BaseUrl.IsEmpty())
	{
		Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + ApiKey);
	}
	Request->SetTimeout(TimeoutSeconds);
	Request->SetContentAsString(Payload);
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, OnComplete](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
		{
			if (!bConnected || !Response.IsValid())
			{
				// The server went away: resolve again on the next request (a local model may have moved).
				if (UEchoModelClient* Self = WeakThis.Get()) Self->bResolved = false;
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

FString UEchoModelClient::FillPrompt(const FString& Template, const TMap<FString, FString>& Slots)
{
	FString Out = Template;
	for (const TPair<FString, FString>& Slot : Slots)
	{
		Out.ReplaceInline(*(TEXT("{") + Slot.Key + TEXT("}")), *Slot.Value, ESearchCase::CaseSensitive);
	}
	return Out;
}

bool UEchoModelClient::ParseJsonObject(const FString& Text, TSharedPtr<FJsonObject>& OutObject)
{
	const int32 Start = Text.Find(TEXT("{"));
	const int32 End = Text.Find(TEXT("}"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	if (Start == INDEX_NONE || End == INDEX_NONE || End <= Start) return false;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text.Mid(Start, End - Start + 1));
	return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
}
