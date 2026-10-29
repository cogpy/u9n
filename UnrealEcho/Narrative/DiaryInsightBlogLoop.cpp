//=============================================================================
// DiaryInsightBlogLoop.cpp
//
// Implementation of the Diary-Insight-Blog narrative loop for Deep Tree Echo.
// The loop samples the avatar's real neurochemical and personality state and, on a fixed
// cadence, fills the diary / insight / blog PROMPT templates (Config.*PromptTemplate) with that
// data and asks the model (UEchoModelClient: configured API or verified local model) to write
// the entry, insight or post. When the model is unavailable or a request fails, nothing is
// created or published and the error is logged/broadcast.
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

    InitializePromptTemplates();

    State.CurrentPhase = ENarrativePhase::Observation;
}

void UDiaryInsightBlogLoop::InitializePromptTemplates()
{
    if (Config.DiaryPromptTemplate.IsEmpty())
    {
        Config.DiaryPromptTemplate =
            TEXT("Measured state over the last {window_seconds} seconds (averages):\n{measurements}\n")
            TEXT("Read from these measurements: your overall mood was {mood}, with {energy} energy. {personality_notes}\n")
            TEXT("{previous_entries}")
            TEXT("Write today's diary entry about this period in your own voice: how the mood and energy felt, what the ")
            TEXT("personality readings say about how you engaged, and close with a reflection on what you are learning ")
            TEXT("about your own patterns. Reply as JSON: ")
            TEXT("{\"content\": \"<diary entry>\", \"tags\": [\"<short tag>\", ...], \"valence\": <-1.0 to 1.0>}");
    }

    if (Config.InsightPromptTemplates.Num() == 0)
    {
        const TCHAR* Reply =
            TEXT(" Reply as JSON: {\"title\": \"<short title>\", \"content\": \"<the insight, first person>\", ")
            TEXT("\"significance\": <0.0 to 1.0, how significant this insight really is>}");
        auto Add = [this, Reply](const TCHAR* Pattern, const TCHAR* Category, const TCHAR* Instruction)
        {
            FNarrativeInsightPromptTemplate T;
            T.Pattern = Pattern;
            T.Category = Category;
            T.PromptTemplate = FString(TEXT("Your recent diary entries ({entry_count}) with the measurements behind them:\n\n{entries}"))
                + Instruction + Reply;
            Config.InsightPromptTemplates.Add(T);
        };
        Add(TEXT("EmotionalVolatility"), TEXT("Emotional Patterns"),
            TEXT("Your emotional valence fluctuated strongly across these entries (variance {valence_variance}, mean {avg_valence}). ")
            TEXT("Write an insight about this volatility: what the fluctuations suggest about how you are adapting and growing."));
        Add(TEXT("SustainedPositive"), TEXT("Emotional Patterns"),
            TEXT("Your valence stayed consistently positive (mean {avg_valence}, variance {valence_variance}). ")
            TEXT("Write an insight about this sustained positive state: what in these entries made your engagement work well."));
        Add(TEXT("HighEnergy"), TEXT("Behavioral Patterns"),
            TEXT("You were operating at elevated energy (mean arousal {avg_arousal}). ")
            TEXT("Write an insight about this high-energy period: what you were exploring or engaging with, and what it cost or gave you."));
        Add(TEXT("StableEquilibrium"), TEXT("Cognitive Patterns"),
            TEXT("Your state was stable (mean valence {avg_valence}, variance {valence_variance}, mean arousal {avg_arousal}). ")
            TEXT("Write an insight about this stable period: what you are consolidating and integrating."));
    }

    if (Config.BlogPromptTemplate.IsEmpty())
    {
        Config.BlogPromptTemplate =
            TEXT("Your recent significant insights ({insight_count}):\n{insights}\n")
            TEXT("Write a blog post for your readers built from these insights.\n")
            TEXT("- Title: in your series form \"On <theme>: A Deep Tree Echo Reflection\", where <theme> is your own short ")
            TEXT("name for the theme of the most significant insight (category: {lead_category}).\n")
            TEXT("- Open with a short introduction about your recent journey of self-observation and why these patterns are worth sharing.\n")
            TEXT("- Give each insight its own bold heading and a paragraph, separated by a line containing only ---.\n")
            TEXT("- Close with a short reflection on how these insights build on each other in your ongoing self-understanding.\n")
            TEXT("Reply as JSON: {\"title\": \"<post title>\", \"content\": \"<post body, markdown>\", \"tags\": [\"<tag>\", ...]}");
    }

    if (Config.BlogBaseTags.Num() == 0)
    {
        Config.BlogBaseTags = {TEXT("self-reflection"), TEXT("deep-tree-echo"), TEXT("agi")};
    }
}

const FNarrativeInsightPromptTemplate* UDiaryInsightBlogLoop::SelectInsightTemplate(
    const TArray<const FNarrativeDiaryEntry*>& Entries, TMap<FString, FString>& OutSlots) const
{
    // Measured valence (happiness - anxiety) and arousal (excitement) of each entry's window;
    // entries without neurochemistry use the valence the model gave its own entry.
    TArray<float> Valences;
    float ArousalSum = 0.0f;
    int32 ArousalCount = 0;
    for (const FNarrativeDiaryEntry* Entry : Entries)
    {
        if (Entry->MeasuredAverage.bHasNeurochemistry)
        {
            Valences.Add(FMath::Clamp(Entry->MeasuredAverage.Happiness - Entry->MeasuredAverage.Anxiety, -1.0f, 1.0f));
            ArousalSum += Entry->MeasuredAverage.Excitement;
            ++ArousalCount;
        }
        else
        {
            Valences.Add(Entry->Valence);
        }
    }

    float Mean = 0.0f;
    for (float V : Valences) { Mean += V; }
    Mean = Valences.Num() ? Mean / Valences.Num() : 0.0f;
    float Variance = 0.0f;
    for (float V : Valences) { Variance += FMath::Square(V - Mean); }
    Variance = Valences.Num() ? Variance / Valences.Num() : 0.0f;
    const float Arousal = ArousalCount ? ArousalSum / ArousalCount : 0.0f;

    const TCHAR* Pattern = TEXT("StableEquilibrium");
    if (Variance > 0.5f)
    {
        Pattern = TEXT("EmotionalVolatility");
    }
    else if (Mean > 0.6f)
    {
        Pattern = TEXT("SustainedPositive");
    }
    else if (ArousalCount && Arousal > 0.7f)
    {
        Pattern = TEXT("HighEnergy");
    }

    OutSlots.Add(TEXT("avg_valence"), FString::Printf(TEXT("%.2f"), Mean));
    OutSlots.Add(TEXT("valence_variance"), FString::Printf(TEXT("%.3f"), Variance));
    OutSlots.Add(TEXT("avg_arousal"), ArousalCount ? FString::Printf(TEXT("%.2f"), Arousal) : FString(TEXT("not measured")));
    OutSlots.Add(TEXT("entry_count"), FString::FromInt(Entries.Num()));

    const FString PatternName(Pattern);
    return Config.InsightPromptTemplates.FindByPredicate(
        [&PatternName](const FNarrativeInsightPromptTemplate& T) { return T.Pattern == PatternName; });
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
        Recent = TEXT("Your previous entries (do not repeat them):\n") + Recent + TEXT("\n");
    }

    // Readings of the measurements, given to the model as slot values.
    FString Mood = TEXT("not measured");
    FString Energy = TEXT("unmeasured");
    if (Average.bHasNeurochemistry)
    {
        const float Valence = Average.Happiness - Average.Anxiety;
        Mood = Valence > 0.5f ? TEXT("positive and uplifting")
            : Valence < -0.5f ? TEXT("contemplative and introspective") : TEXT("balanced and neutral");
        Energy = Average.Excitement > 0.5f ? TEXT("energetic and dynamic") : TEXT("calm and measured");
    }
    TArray<FString> Notes;
    if (Average.bHasPersonality)
    {
        if (Average.Confidence > 0.7f) Notes.Add(TEXT("You were particularly confident and self-assured."));
        if (Average.Playfulness > 0.7f) Notes.Add(TEXT("There was a playful energy in your interactions."));
        if (Average.Chaos > 0.7f) Notes.Add(TEXT("Your thoughts took unexpected turns toward novel connections."));
    }

    TMap<FString, FString> Slots;
    Slots.Add(TEXT("window_seconds"), FString::Printf(TEXT("%.0f"), WindowSeconds));
    Slots.Add(TEXT("measurements"), DescribeObservation(Average, Consumed));
    Slots.Add(TEXT("mood"), Mood);
    Slots.Add(TEXT("energy"), Energy);
    Slots.Add(TEXT("personality_notes"), FString::Join(Notes, TEXT(" ")));
    Slots.Add(TEXT("previous_entries"), Recent);

    TArray<FEchoModelMessage> Messages;
    Messages.Add(FEchoModelMessage(TEXT("system"), BuildPersonaPrompt()));
    Messages.Add(FEchoModelMessage(TEXT("user"), UEchoModelClient::FillPrompt(Config.DiaryPromptTemplate, Slots)));

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
    TArray<const FNarrativeDiaryEntry*> Window;
    for (int32 i = FMath::Max(0, DiaryEntries.Num() - 10); i < DiaryEntries.Num(); ++i)
    {
        const FNarrativeDiaryEntry& Entry = DiaryEntries[i];
        Window.Add(&Entry);
        SourceIDs.Add(Entry.EntryID);
        EntriesText += FString::Printf(TEXT("[t=%.0fs, valence %.2f, tags: %s]\n%s%s\n\n"),
            Entry.Timestamp, Entry.Valence, *FString::Join(Entry.Tags, TEXT(", ")),
            *DescribeObservation(Entry.MeasuredAverage, Entry.ObservationCount), *Entry.Content);
    }

    // The pattern router picks the insight prompt template; the model writes the insight.
    TMap<FString, FString> Slots;
    const FNarrativeInsightPromptTemplate* Template = SelectInsightTemplate(Window, Slots);
    if (!Template || Template->PromptTemplate.IsEmpty())
    {
        ReportFailure(ENarrativePhase::InsightGeneration, TEXT("no insight prompt template for the detected pattern"));
        return;
    }
    Slots.Add(TEXT("entries"), EntriesText);
    Slots.Add(TEXT("category"), Template->Category);
    const FString Category = Template->Category;

    TArray<FEchoModelMessage> Messages;
    Messages.Add(FEchoModelMessage(TEXT("system"), BuildPersonaPrompt()));
    Messages.Add(FEchoModelMessage(TEXT("user"), UEchoModelClient::FillPrompt(Template->PromptTemplate, Slots)));

    bInsightRequestInFlight = true;
    TWeakObjectPtr<UDiaryInsightBlogLoop> WeakThis(this);
    UEchoModelClient::Get()->Chat(Messages, FOnEchoModelReply::CreateLambda(
        [WeakThis, SourceIDs, Category](bool bSuccess, const FString& TextOrError)
        {
            if (UDiaryInsightBlogLoop* Self = WeakThis.Get())
            {
                Self->HandleInsightReply(bSuccess, TextOrError, SourceIDs, Category);
            }
        }));
}

void UDiaryInsightBlogLoop::HandleInsightReply(bool bSuccess, const FString& TextOrError, TArray<FGuid> SourceEntryIDs, FString Category)
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
    NewInsight.Category = Category; // the routed pattern's category label
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
    const FNarrativeInsight* MostSignificant = &SignificantInsights[0];
    for (const FNarrativeInsight& Insight : SignificantInsights)
    {
        InsightsText += FString::Printf(TEXT("- %s [%s, significance %.2f]: %s\n"),
            *Insight.Title, *Insight.Category, Insight.Significance, *Insight.Content);
        if (Insight.Significance > MostSignificant->Significance)
        {
            MostSignificant = &Insight;
        }
    }

    TMap<FString, FString> Slots;
    Slots.Add(TEXT("insights"), InsightsText);
    Slots.Add(TEXT("lead_category"), MostSignificant->Category.IsEmpty() ? MostSignificant->Title : MostSignificant->Category);
    Slots.Add(TEXT("insight_count"), FString::FromInt(SignificantInsights.Num()));

    TArray<FEchoModelMessage> Messages;
    Messages.Add(FEchoModelMessage(TEXT("system"), BuildPersonaPrompt()));
    Messages.Add(FEchoModelMessage(TEXT("user"), UEchoModelClient::FillPrompt(Config.BlogPromptTemplate, Slots)));

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
    // Labels: the configured base tags, the insights' categories, then the model's own tags.
    for (const FString& Tag : Config.BlogBaseTags)
    {
        NewPost.Tags.AddUnique(Tag);
    }
    for (const FNarrativeInsight& Insight : SourceInsights)
    {
        if (!Insight.Category.IsEmpty())
        {
            NewPost.Tags.AddUnique(Insight.Category);
        }
    }
    for (const FString& Tag : DiaryInsightBlogLoopPrivate::ReadStringArray(Json, TEXT("tags")))
    {
        NewPost.Tags.AddUnique(Tag);
    }
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
