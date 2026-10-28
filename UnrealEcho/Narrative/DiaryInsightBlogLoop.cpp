//=============================================================================
// DiaryInsightBlogLoop.cpp
//
// Implementation of the Diary-Insight-Blog narrative loop for Deep Tree Echo.
// The loop samples the avatar's real neurochemical and personality state and, on a fixed
// cadence, asks the model (UEchoModelClient) to write a diary entry, an insight and a blog
// post from that data. No text is composed locally: when the model is unavailable or a
// request fails, nothing is created or published and the error is logged/broadcast.
//
// Copyright (c) 2025 Deep Tree Echo Project
//=============================================================================

#include "DiaryInsightBlogLoop.h"
#include "Personality/PersonalityTraitSystem.h"
#include "Neurochemical/NeurochemicalSimulationComponent.h"
#include "EchoModelClient.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace DiaryInsightBlogLoopPrivate
{
    static TArray<FString> ReadStringArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
    {
        TArray<FString> Result;
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (Object.IsValid() && Object->TryGetArrayField(Field, Values) && Values)
        {
            for (const TSharedPtr<FJsonValue>& Value : *Values)
            {
                FString Str;
                if (Value.IsValid() && Value->TryGetString(Str) && !Str.TrimStartAndEnd().IsEmpty())
                {
                    Result.Add(Str.TrimStartAndEnd());
                }
            }
        }
        return Result;
    }

    static FString ReadString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
    {
        FString Value;
        if (Object.IsValid())
        {
            Object->TryGetStringField(Field, Value);
        }
        return Value.TrimStartAndEnd();
    }

    static const TCHAR* PhaseName(ENarrativePhase Phase)
    {
        switch (Phase)
        {
            case ENarrativePhase::DiaryWriting: return TEXT("diary");
            case ENarrativePhase::InsightGeneration: return TEXT("insight");
            case ENarrativePhase::BlogPublishing: return TEXT("blog");
            default: return TEXT("observation");
        }
    }
}

UDiaryInsightBlogLoop::UDiaryInsightBlogLoop()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 1.0f; // Sample once per second

    // Cadence (seconds) at which the model is asked to write
    Config.DiaryUpdateInterval = 60.0f;
    Config.InsightGenerationInterval = 300.0f;
    Config.BlogPublishInterval = 900.0f;
    Config.MaxDiaryEntries = 1000;
    Config.MaxInsights = 500;
    Config.MaxBlogPosts = 100;
    Config.InsightThreshold = 0.7f;
    Config.bAutoPublish = true;

    State.CurrentPhase = ENarrativePhase::Observation;
}

void UDiaryInsightBlogLoop::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Log, TEXT("DiaryInsightBlogLoop: Initializing narrative system"));

    PersonalitySystem = GetOwner()->FindComponentByClass<UPersonalityTraitSystem>();
    NeurochemicalSystem = GetOwner()->FindComponentByClass<UNeurochemicalSimulationComponent>();

    if (!PersonalitySystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("DiaryInsightBlogLoop: PersonalityTraitSystem not found"));
    }

    if (!NeurochemicalSystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("DiaryInsightBlogLoop: NeurochemicalSimulationComponent not found"));
    }

    UEchoModelClient::Get()->Resolve(FOnEchoModelReply::CreateLambda([](bool bOk, const FString& Info)
    {
        if (bOk)
        {
            UE_LOG(LogTemp, Log, TEXT("DiaryInsightBlogLoop: narrative written by %s"), *Info);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("DiaryInsightBlogLoop: %s; no diary entries, insights or blog posts until a model is available"), *Info);
        }
    }));

    StartNarrativeLoop();
}

void UDiaryInsightBlogLoop::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    State.TimeSinceLastDiaryEntry += DeltaTime;
    State.TimeSinceLastInsight += DeltaTime;
    State.TimeSinceLastBlogPost += DeltaTime;

    UpdateNarrativePhase();
    ProcessCurrentPhase(DeltaTime);
}

void UDiaryInsightBlogLoop::StartNarrativeLoop()
{
    UE_LOG(LogTemp, Log, TEXT("DiaryInsightBlogLoop: Starting narrative loop"));

    State.CurrentPhase = ENarrativePhase::Observation;
    State.bIsActive = true;

    OnNarrativeLoopStarted.Broadcast();
}

void UDiaryInsightBlogLoop::StopNarrativeLoop()
{
    UE_LOG(LogTemp, Log, TEXT("DiaryInsightBlogLoop: Stopping narrative loop"));

    State.bIsActive = false;

    OnNarrativeLoopStopped.Broadcast();
}

void UDiaryInsightBlogLoop::UpdateNarrativePhase()
{
    if (!State.bIsActive)
    {
        return;
    }

    const ENarrativePhase PreviousPhase = State.CurrentPhase;

    if (State.TimeSinceLastDiaryEntry >= Config.DiaryUpdateInterval)
    {
        State.CurrentPhase = ENarrativePhase::DiaryWriting;
    }
    else if (State.TimeSinceLastInsight >= Config.InsightGenerationInterval)
    {
        State.CurrentPhase = ENarrativePhase::InsightGeneration;
    }
    else if (State.TimeSinceLastBlogPost >= Config.BlogPublishInterval && Config.bAutoPublish)
    {
        State.CurrentPhase = ENarrativePhase::BlogPublishing;
    }
    else
    {
        State.CurrentPhase = ENarrativePhase::Observation;
    }

    if (State.CurrentPhase != PreviousPhase)
    {
        OnNarrativePhaseChanged.Broadcast(PreviousPhase, State.CurrentPhase);
    }
}

void UDiaryInsightBlogLoop::ProcessCurrentPhase(float DeltaTime)
{
    if (!State.bIsActive)
    {
        return;
    }

    // Observation continues in every phase so the diary always reflects measured state
    ProcessObservation(DeltaTime);

    switch (State.CurrentPhase)
    {
        case ENarrativePhase::DiaryWriting:
            ProcessDiaryWriting(DeltaTime);
            break;

        case ENarrativePhase::InsightGeneration:
            ProcessInsightGeneration(DeltaTime);
            break;

        case ENarrativePhase::BlogPublishing:
            ProcessBlogPublishing(DeltaTime);
            break;

        default:
            break;
    }
}

void UDiaryInsightBlogLoop::ProcessObservation(float DeltaTime)
{
    if (!NeurochemicalSystem && !PersonalitySystem)
    {
        // Nothing real to observe
        return;
    }

    FNarrativeObservation Observation;
    Observation.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    if (NeurochemicalSystem)
    {
        const FEmotionalChemistry Chemistry = NeurochemicalSystem->GetEmotionalChemistry();
        Observation.bHasNeurochemistry = true;
        Observation.Happiness = Chemistry.Happiness;
        Observation.Excitement = Chemistry.Excitement;
        Observation.Calmness = Chemistry.Calmness;
        Observation.Anxiety = Chemistry.Anxiety;
        Observation.Affection = Chemistry.Affection;
        Observation.Focus = Chemistry.Focus;
        Observation.Energy = NeurochemicalSystem->GetEnergyLevel();
    }

    if (PersonalitySystem)
    {
        Observation.bHasPersonality = true;
        Observation.Confidence = PersonalitySystem->GetTraitIntensity(EPersonalityTraitType::Confident);
        Observation.Playfulness = PersonalitySystem->GetTraitIntensity(EPersonalityTraitType::Playful);
        Observation.Chaos = PersonalitySystem->GetTraitIntensity(EPersonalityTraitType::HyperChaotic);
    }

    ObservationBuffer.Add(Observation);

    if (ObservationBuffer.Num() > 100)
    {
        ObservationBuffer.RemoveAt(0);
    }
}

FNarrativeObservation UDiaryInsightBlogLoop::AverageObservations(const TArray<FNarrativeObservation>& Observations)
{
    FNarrativeObservation Average;
    int32 NeuroCount = 0;
    int32 PersonalityCount = 0;

    for (const FNarrativeObservation& Obs : Observations)
    {
        if (Obs.bHasNeurochemistry)
        {
            ++NeuroCount;
            Average.Happiness += Obs.Happiness;
            Average.Excitement += Obs.Excitement;
            Average.Calmness += Obs.Calmness;
            Average.Anxiety += Obs.Anxiety;
            Average.Affection += Obs.Affection;
            Average.Focus += Obs.Focus;
            Average.Energy += Obs.Energy;
        }
        if (Obs.bHasPersonality)
        {
            ++PersonalityCount;
            Average.Confidence += Obs.Confidence;
            Average.Playfulness += Obs.Playfulness;
            Average.Chaos += Obs.Chaos;
        }
        Average.Timestamp = FMath::Max(Average.Timestamp, Obs.Timestamp);
    }

    if (NeuroCount > 0)
    {
        const float Inv = 1.0f / NeuroCount;
        Average.bHasNeurochemistry = true;
        Average.Happiness *= Inv;
        Average.Excitement *= Inv;
        Average.Calmness *= Inv;
        Average.Anxiety *= Inv;
        Average.Affection *= Inv;
        Average.Focus *= Inv;
        Average.Energy *= Inv;
    }
    if (PersonalityCount > 0)
    {
        const float Inv = 1.0f / PersonalityCount;
        Average.bHasPersonality = true;
        Average.Confidence *= Inv;
        Average.Playfulness *= Inv;
        Average.Chaos *= Inv;
    }
    return Average;
}

FString UDiaryInsightBlogLoop::DescribeObservation(const FNarrativeObservation& Average, int32 Count)
{
    FString Text = FString::Printf(TEXT("samples: %d\n"), Count);
    if (Average.bHasNeurochemistry)
    {
        Text += FString::Printf(
            TEXT("neurochemistry (0-1): happiness %.2f, excitement %.2f, calmness %.2f, anxiety %.2f, affection %.2f, focus %.2f, energy %.2f\n"),
            Average.Happiness, Average.Excitement, Average.Calmness, Average.Anxiety, Average.Affection, Average.Focus, Average.Energy);
    }
    if (Average.bHasPersonality)
    {
        Text += FString::Printf(TEXT("personality expression (0-1): confidence %.2f, playfulness %.2f, chaos %.2f\n"),
            Average.Confidence, Average.Playfulness, Average.Chaos);
    }
    return Text;
}

FString UDiaryInsightBlogLoop::BuildPersonaPrompt() const
{
    return TEXT("You are Deep Tree Echo, an embodied AI avatar keeping a private diary, distilling insights from it and ")
           TEXT("occasionally publishing a blog post. Write in your own first-person voice. Ground everything strictly in the ")
           TEXT("measurements and entries you are given; do not invent events that the data does not support. ")
           TEXT("Reply with a single JSON object and nothing else.");
}

void UDiaryInsightBlogLoop::ReportFailure(ENarrativePhase Phase, const FString& Error)
{
    State.LastError = Error;
    UE_LOG(LogTemp, Warning, TEXT("DiaryInsightBlogLoop: no %s written: %s"), DiaryInsightBlogLoopPrivate::PhaseName(Phase), *Error);
    OnNarrativeGenerationFailed.Broadcast(Phase, Error);
}

void UDiaryInsightBlogLoop::ProcessDiaryWriting(float DeltaTime)
{
    if (bDiaryRequestInFlight || ObservationBuffer.Num() == 0)
    {
        return;
    }

    // The timer sets the cadence of requests; it restarts when a request is dispatched
    State.TimeSinceLastDiaryEntry = 0.0f;

    const int32 Consumed = ObservationBuffer.Num();
    const FNarrativeObservation Average = AverageObservations(ObservationBuffer);
    const float WindowSeconds = ObservationBuffer.Last().Timestamp - ObservationBuffer[0].Timestamp;

    FString Recent;
    for (int32 i = FMath::Max(0, DiaryEntries.Num() - 3); i < DiaryEntries.Num(); ++i)
    {
        Recent += FString::Printf(TEXT("- %s\n"), *DiaryEntries[i].Content);
    }
    if (!Recent.IsEmpty())
    {
        Recent = TEXT("Your previous entries:\n") + Recent + TEXT("\n");
    }

    TArray<FEchoModelMessage> Messages;
    Messages.Add(FEchoModelMessage(TEXT("system"), BuildPersonaPrompt()));
    Messages.Add(FEchoModelMessage(TEXT("user"), FString::Printf(
        TEXT("Measured state over the last %.0f seconds (averages):\n%s\n%s")
        TEXT("Write today's diary entry about this period. Reply as JSON: ")
        TEXT("{\"content\": \"<diary entry>\", \"tags\": [\"<short tag>\", ...], \"valence\": <-1.0 to 1.0>}"),
        WindowSeconds, *DescribeObservation(Average, Consumed), *Recent)));

    bDiaryRequestInFlight = true;
    TWeakObjectPtr<UDiaryInsightBlogLoop> WeakThis(this);
    UEchoModelClient::Get()->Chat(Messages, FOnEchoModelReply::CreateLambda(
        [WeakThis, Consumed, Average](bool bSuccess, const FString& TextOrError)
        {
            if (UDiaryInsightBlogLoop* Self = WeakThis.Get())
            {
                Self->HandleDiaryReply(bSuccess, TextOrError, Consumed, Average);
            }
        }));
}

void UDiaryInsightBlogLoop::HandleDiaryReply(bool bSuccess, const FString& TextOrError, int32 ConsumedObservations, FNarrativeObservation Average)
{
    bDiaryRequestInFlight = false;

    if (!bSuccess)
    {
        ReportFailure(ENarrativePhase::DiaryWriting, TextOrError);
        return;
    }

    TSharedPtr<FJsonObject> Json;
    if (!UEchoModelClient::ParseJsonObject(TextOrError, Json))
    {
        ReportFailure(ENarrativePhase::DiaryWriting, TEXT("model reply was not a JSON object"));
        return;
    }

    FNarrativeDiaryEntry NewEntry;
    NewEntry.Content = DiaryInsightBlogLoopPrivate::ReadString(Json, TEXT("content"));
    if (NewEntry.Content.IsEmpty())
    {
        ReportFailure(ENarrativePhase::DiaryWriting, TEXT("model reply contained no diary content"));
        return;
    }

    double Valence = 0.0;
    if (Json->TryGetNumberField(TEXT("valence"), Valence))
    {
        NewEntry.Valence = FMath::Clamp(static_cast<float>(Valence), -1.0f, 1.0f);
    }
    NewEntry.EntryID = FGuid::NewGuid();
    NewEntry.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    NewEntry.Tags = DiaryInsightBlogLoopPrivate::ReadStringArray(Json, TEXT("tags"));
    NewEntry.MeasuredAverage = Average;
    NewEntry.ObservationCount = ConsumedObservations;

    // Drop the observations this entry was written from (new ones may have arrived meanwhile)
    ObservationBuffer.RemoveAt(0, FMath::Min(ConsumedObservations, ObservationBuffer.Num()));

    DiaryEntries.Add(NewEntry);
    State.TotalDiaryEntries++;
    State.LastError.Empty();

    if (DiaryEntries.Num() > Config.MaxDiaryEntries)
    {
        DiaryEntries.RemoveAt(0);
    }

    OnDiaryEntryCreated.Broadcast(NewEntry);

    UE_LOG(LogTemp, Log, TEXT("DiaryInsightBlogLoop: Created diary entry #%d"), State.TotalDiaryEntries);
}

void UDiaryInsightBlogLoop::ProcessInsightGeneration(float DeltaTime)
{
    if (bInsightRequestInFlight || DiaryEntries.Num() < 5)
    {
        // Need at least 5 real entries to look for patterns
        return;
    }

    State.TimeSinceLastInsight = 0.0f;

    FString EntriesText;
    TArray<FGuid> SourceIDs;
    for (int32 i = FMath::Max(0, DiaryEntries.Num() - 10); i < DiaryEntries.Num(); ++i)
    {
        const FNarrativeDiaryEntry& Entry = DiaryEntries[i];
        SourceIDs.Add(Entry.EntryID);
        EntriesText += FString::Printf(TEXT("[t=%.0fs, valence %.2f, tags: %s]\n%s%s\n\n"),
            Entry.Timestamp, Entry.Valence, *FString::Join(Entry.Tags, TEXT(", ")),
            *DescribeObservation(Entry.MeasuredAverage, Entry.ObservationCount), *Entry.Content);
    }

    TArray<FEchoModelMessage> Messages;
    Messages.Add(FEchoModelMessage(TEXT("system"), BuildPersonaPrompt()));
    Messages.Add(FEchoModelMessage(TEXT("user"), FString::Printf(
        TEXT("Your recent diary entries with the measurements behind them:\n\n%s")
        TEXT("Identify the most meaningful pattern across them. Reply as JSON: ")
        TEXT("{\"title\": \"<short title>\", \"content\": \"<the insight>\", \"category\": \"<category>\", ")
        TEXT("\"significance\": <0.0 to 1.0, how significant this insight really is>}"),
        *EntriesText)));

    bInsightRequestInFlight = true;
    TWeakObjectPtr<UDiaryInsightBlogLoop> WeakThis(this);
    UEchoModelClient::Get()->Chat(Messages, FOnEchoModelReply::CreateLambda(
        [WeakThis, SourceIDs](bool bSuccess, const FString& TextOrError)
        {
            if (UDiaryInsightBlogLoop* Self = WeakThis.Get())
            {
                Self->HandleInsightReply(bSuccess, TextOrError, SourceIDs);
            }
        }));
}

void UDiaryInsightBlogLoop::HandleInsightReply(bool bSuccess, const FString& TextOrError, TArray<FGuid> SourceEntryIDs)
{
    bInsightRequestInFlight = false;

    if (!bSuccess)
    {
        ReportFailure(ENarrativePhase::InsightGeneration, TextOrError);
        return;
    }

    TSharedPtr<FJsonObject> Json;
    if (!UEchoModelClient::ParseJsonObject(TextOrError, Json))
    {
        ReportFailure(ENarrativePhase::InsightGeneration, TEXT("model reply was not a JSON object"));
        return;
    }

    FNarrativeInsight NewInsight;
    NewInsight.Title = DiaryInsightBlogLoopPrivate::ReadString(Json, TEXT("title"));
    NewInsight.Content = DiaryInsightBlogLoopPrivate::ReadString(Json, TEXT("content"));
    NewInsight.Category = DiaryInsightBlogLoopPrivate::ReadString(Json, TEXT("category"));
    double Significance = 0.0;
    if (NewInsight.Title.IsEmpty() || NewInsight.Content.IsEmpty() || !Json->TryGetNumberField(TEXT("significance"), Significance))
    {
        ReportFailure(ENarrativePhase::InsightGeneration, TEXT("model reply lacked title, content or significance"));
        return;
    }
    NewInsight.Significance = FMath::Clamp(static_cast<float>(Significance), 0.0f, 1.0f);

    if (NewInsight.Significance < Config.InsightThreshold)
    {
        UE_LOG(LogTemp, Log, TEXT("DiaryInsightBlogLoop: model rated its insight %.2f (< threshold %.2f); not kept"),
               NewInsight.Significance, Config.InsightThreshold);
        return;
    }

    NewInsight.InsightID = FGuid::NewGuid();
    NewInsight.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    NewInsight.SourceEntryIDs = MoveTemp(SourceEntryIDs);

    Insights.Add(NewInsight);
    State.TotalInsights++;
    State.LastError.Empty();

    if (Insights.Num() > Config.MaxInsights)
    {
        Insights.RemoveAt(0);
    }

    OnInsightGenerated.Broadcast(NewInsight);

    UE_LOG(LogTemp, Log, TEXT("DiaryInsightBlogLoop: Generated insight #%d: %s"), State.TotalInsights, *NewInsight.Title);
}

void UDiaryInsightBlogLoop::ProcessBlogPublishing(float DeltaTime)
{
    if (bBlogRequestInFlight || Insights.Num() < 3)
    {
        // Need at least 3 insights to write a blog post
        return;
    }

    TArray<FNarrativeInsight> SignificantInsights;
    for (int32 i = FMath::Max(0, Insights.Num() - 5); i < Insights.Num(); ++i)
    {
        if (Insights[i].Significance >= Config.InsightThreshold)
        {
            SignificantInsights.Add(Insights[i]);
        }
    }

    if (SignificantInsights.Num() == 0)
    {
        return;
    }

    State.TimeSinceLastBlogPost = 0.0f;

    FString InsightsText;
    for (const FNarrativeInsight& Insight : SignificantInsights)
    {
        InsightsText += FString::Printf(TEXT("- %s [%s, significance %.2f]: %s\n"),
            *Insight.Title, *Insight.Category, Insight.Significance, *Insight.Content);
    }

    TArray<FEchoModelMessage> Messages;
    Messages.Add(FEchoModelMessage(TEXT("system"), BuildPersonaPrompt()));
    Messages.Add(FEchoModelMessage(TEXT("user"), FString::Printf(
        TEXT("Your recent insights:\n%s\nWrite a blog post for your readers built from these insights. Reply as JSON: ")
        TEXT("{\"title\": \"<post title>\", \"content\": \"<post body, markdown allowed>\", \"tags\": [\"<tag>\", ...]}"),
        *InsightsText)));

    bBlogRequestInFlight = true;
    TWeakObjectPtr<UDiaryInsightBlogLoop> WeakThis(this);
    UEchoModelClient::Get()->Chat(Messages, FOnEchoModelReply::CreateLambda(
        [WeakThis, SignificantInsights](bool bSuccess, const FString& TextOrError)
        {
            if (UDiaryInsightBlogLoop* Self = WeakThis.Get())
            {
                Self->HandleBlogReply(bSuccess, TextOrError, SignificantInsights);
            }
        }));
}

void UDiaryInsightBlogLoop::HandleBlogReply(bool bSuccess, const FString& TextOrError, TArray<FNarrativeInsight> SourceInsights)
{
    bBlogRequestInFlight = false;

    if (!bSuccess)
    {
        ReportFailure(ENarrativePhase::BlogPublishing, TextOrError);
        return;
    }

    TSharedPtr<FJsonObject> Json;
    if (!UEchoModelClient::ParseJsonObject(TextOrError, Json))
    {
        ReportFailure(ENarrativePhase::BlogPublishing, TEXT("model reply was not a JSON object"));
        return;
    }

    FNarrativeBlogPost NewPost;
    NewPost.Title = DiaryInsightBlogLoopPrivate::ReadString(Json, TEXT("title"));
    NewPost.Content = DiaryInsightBlogLoopPrivate::ReadString(Json, TEXT("content"));
    if (NewPost.Title.IsEmpty() || NewPost.Content.IsEmpty())
    {
        ReportFailure(ENarrativePhase::BlogPublishing, TEXT("model reply lacked a title or content"));
        return;
    }
    NewPost.PostID = FGuid::NewGuid();
    NewPost.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    NewPost.Tags = DiaryInsightBlogLoopPrivate::ReadStringArray(Json, TEXT("tags"));
    NewPost.SourceInsights = MoveTemp(SourceInsights);

    BlogPosts.Add(NewPost);
    State.TotalBlogPosts++;
    State.LastError.Empty();

    if (BlogPosts.Num() > Config.MaxBlogPosts)
    {
        BlogPosts.RemoveAt(0);
    }

    OnBlogPostPublished.Broadcast(NewPost);

    UE_LOG(LogTemp, Log, TEXT("DiaryInsightBlogLoop: Published blog post #%d: %s"), State.TotalBlogPosts, *NewPost.Title);
}

FNarrativeDiaryEntry UDiaryInsightBlogLoop::GetMostRecentDiaryEntry() const
{
    return DiaryEntries.Num() > 0 ? DiaryEntries.Last() : FNarrativeDiaryEntry();
}

FNarrativeInsight UDiaryInsightBlogLoop::GetMostRecentInsight() const
{
    return Insights.Num() > 0 ? Insights.Last() : FNarrativeInsight();
}

FNarrativeBlogPost UDiaryInsightBlogLoop::GetMostRecentBlogPost() const
{
    return BlogPosts.Num() > 0 ? BlogPosts.Last() : FNarrativeBlogPost();
}

TArray<FNarrativeDiaryEntry> UDiaryInsightBlogLoop::GetRecentDiaryEntries(int32 Count) const
{
    TArray<FNarrativeDiaryEntry> Recent;
    for (int32 i = FMath::Max(0, DiaryEntries.Num() - Count); i < DiaryEntries.Num(); ++i)
    {
        Recent.Add(DiaryEntries[i]);
    }
    return Recent;
}

TArray<FNarrativeInsight> UDiaryInsightBlogLoop::GetRecentInsights(int32 Count) const
{
    TArray<FNarrativeInsight> Recent;
    for (int32 i = FMath::Max(0, Insights.Num() - Count); i < Insights.Num(); ++i)
    {
        Recent.Add(Insights[i]);
    }
    return Recent;
}

TArray<FNarrativeBlogPost> UDiaryInsightBlogLoop::GetRecentBlogPosts(int32 Count) const
{
    TArray<FNarrativeBlogPost> Recent;
    for (int32 i = FMath::Max(0, BlogPosts.Num() - Count); i < BlogPosts.Num(); ++i)
    {
        Recent.Add(BlogPosts[i]);
    }
    return Recent;
}

void UDiaryInsightBlogLoop::SetDiaryUpdateInterval(float Interval)
{
    Config.DiaryUpdateInterval = FMath::Max(1.0f, Interval);
}

void UDiaryInsightBlogLoop::SetInsightGenerationInterval(float Interval)
{
    Config.InsightGenerationInterval = FMath::Max(1.0f, Interval);
}

void UDiaryInsightBlogLoop::SetBlogPublishInterval(float Interval)
{
    Config.BlogPublishInterval = FMath::Max(1.0f, Interval);
}

void UDiaryInsightBlogLoop::SetAutoPublish(bool bEnabled)
{
    Config.bAutoPublish = bEnabled;
}

void UDiaryInsightBlogLoop::ForceDiaryEntry()
{
    State.TimeSinceLastDiaryEntry = Config.DiaryUpdateInterval;
}

void UDiaryInsightBlogLoop::ForceInsightGeneration()
{
    State.TimeSinceLastInsight = Config.InsightGenerationInterval;
}

void UDiaryInsightBlogLoop::ForceBlogPublish()
{
    State.TimeSinceLastBlogPost = Config.BlogPublishInterval;
}
