#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiaryInsightBlogLoop.generated.h"

class UPersonalityTraitSystem;
class UNeurochemicalSimulationComponent;

/**
 * Phase of the Diary-Insight-Blog loop. The timers only set the cadence at which the model is
 * asked to write; every diary entry, insight and blog post is written by the model
 * (UEchoModelClient) from the real observed state.
 */
UENUM(BlueprintType)
enum class ENarrativePhase : uint8
{
    Observation UMETA(DisplayName = "Observation"),
    DiaryWriting UMETA(DisplayName = "Diary Writing"),
    InsightGeneration UMETA(DisplayName = "Insight Generation"),
    BlogPublishing UMETA(DisplayName = "Blog Publishing")
};

/**
 * One measured sample of the avatar's state. Only values actually read from the owner's
 * components are stored; the bHas* flags say which sources were present.
 */
USTRUCT(BlueprintType)
struct FNarrativeObservation
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    float Timestamp = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    bool bHasNeurochemistry = false;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    float Happiness = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    float Excitement = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    float Calmness = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    float Anxiety = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    float Affection = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    float Focus = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    float Energy = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    bool bHasPersonality = false;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    float Confidence = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    float Playfulness = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    float Chaos = 0.0f;
};

/** A diary entry written by the model from a window of observations. */
USTRUCT(BlueprintType)
struct FNarrativeDiaryEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    FGuid EntryID;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    float Timestamp = 0.0f;

    /** Model-written diary text. */
    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    FString Content;

    /** Model-chosen tags. */
    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    TArray<FString> Tags;

    /** Emotional valence (-1..1) the model assigned to its own entry. */
    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    float Valence = 0.0f;

    /** Measured averages of the observations the entry was written from. */
    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    FNarrativeObservation MeasuredAverage;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    int32 ObservationCount = 0;
};

/** An insight written by the model from recent diary entries. */
USTRUCT(BlueprintType)
struct FNarrativeInsight
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    FGuid InsightID;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    float Timestamp = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    FString Title;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    FString Content;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    FString Category;

    /** Significance (0..1) assigned by the model. */
    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    float Significance = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    TArray<FGuid> SourceEntryIDs;
};

/** A blog post written by the model from significant insights. */
USTRUCT(BlueprintType)
struct FNarrativeBlogPost
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    FGuid PostID;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    float Timestamp = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    FString Title;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    FString Content;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    TArray<FString> Tags;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    TArray<FNarrativeInsight> SourceInsights;
};

/**
 * Insight prompt template for one pattern the router can detect in the diary's measurements.
 * The router picks the template; the model writes the insight.
 */
USTRUCT(BlueprintType)
struct FNarrativeInsightPromptTemplate
{
    GENERATED_BODY()

    /** Pattern key: "EmotionalVolatility", "SustainedPositive", "HighEnergy" or "StableEquilibrium". */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative")
    FString Pattern;

    /** Category recorded on the insight (a classification label). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative")
    FString Category;

    /**
     * Instruction to the model. Slots: {entries}, {avg_valence}, {valence_variance},
     * {avg_arousal}, {entry_count}, {category}.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative", meta = (MultiLine = true))
    FString PromptTemplate;
};

USTRUCT(BlueprintType)
struct FNarrativeLoopConfig
{
    GENERATED_BODY()

    /** Seconds between requests to the model for a diary entry. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative")
    float DiaryUpdateInterval = 60.0f;

    /** Seconds between requests to the model for an insight. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative")
    float InsightGenerationInterval = 300.0f;

    /** Seconds between requests to the model for a blog post. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative")
    float BlogPublishInterval = 900.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative")
    int32 MaxDiaryEntries = 1000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative")
    int32 MaxInsights = 500;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative")
    int32 MaxBlogPosts = 100;

    /** Model-assigned significance an insight needs to be kept. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative")
    float InsightThreshold = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative")
    bool bAutoPublish = true;

    // ===== Prompt templates (instructions to the model; the model writes every word) =====

    /**
     * Diary prompt. Slots: {window_seconds}, {measurements}, {mood}, {energy},
     * {personality_notes}, {previous_entries}.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Prompts", meta = (MultiLine = true))
    FString DiaryPromptTemplate;

    /** Insight prompts, one per detectable pattern. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Prompts")
    TArray<FNarrativeInsightPromptTemplate> InsightPromptTemplates;

    /**
     * Blog prompt (title series form, introduction, one section per insight, closing).
     * Slots: {insights}, {lead_category}, {insight_count}.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Prompts", meta = (MultiLine = true))
    FString BlogPromptTemplate;

    /** Tags every post carries in addition to its insights' categories and the model's tags. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative|Prompts")
    TArray<FString> BlogBaseTags;
};

USTRUCT(BlueprintType)
struct FNarrativeLoopState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    ENarrativePhase CurrentPhase = ENarrativePhase::Observation;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    bool bIsActive = false;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    float TimeSinceLastDiaryEntry = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    float TimeSinceLastInsight = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    float TimeSinceLastBlogPost = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    int32 TotalDiaryEntries = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    int32 TotalInsights = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    int32 TotalBlogPosts = 0;

    /** Last model/configuration error (system-level, never shown as the character's words). */
    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    FString LastError;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNarrativeLoopStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNarrativeLoopStopped);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNarrativePhaseChanged, ENarrativePhase, OldPhase, ENarrativePhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDiaryEntryCreated, const FNarrativeDiaryEntry&, Entry);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNarrativeInsightGenerated, const FNarrativeInsight&, Insight);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBlogPostPublished, const FNarrativeBlogPost&, Post);
/** A model request failed; nothing was created for that phase. Error is a system message. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNarrativeGenerationFailed, ENarrativePhase, Phase, const FString&, Error);

/**
 * Diary-Insight-Blog Narrative Loop Component.
 *
 * Samples the owner's real neurochemical/personality state, and on a fixed cadence asks the
 * model to write a diary entry from those measurements, an insight from recent diary entries,
 * and a blog post from significant insights. Requests are asynchronous. When the model is not
 * configured or a request fails, nothing is created or published and the error is logged and
 * broadcast through OnNarrativeGenerationFailed.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALENGINE_API UDiaryInsightBlogLoop : public UActorComponent
{
    GENERATED_BODY()

public:
    UDiaryInsightBlogLoop();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ===== Loop Management =====

    UFUNCTION(BlueprintCallable, Category = "Narrative|Loop")
    void StartNarrativeLoop();

    UFUNCTION(BlueprintCallable, Category = "Narrative|Loop")
    void StopNarrativeLoop();

    UFUNCTION(BlueprintPure, Category = "Narrative|Loop")
    FNarrativeLoopState GetNarrativeState() const { return State; }

    UFUNCTION(BlueprintCallable, Category = "Narrative|Config")
    void SetDiaryUpdateInterval(float Interval);

    UFUNCTION(BlueprintCallable, Category = "Narrative|Config")
    void SetInsightGenerationInterval(float Interval);

    UFUNCTION(BlueprintCallable, Category = "Narrative|Config")
    void SetBlogPublishInterval(float Interval);

    UFUNCTION(BlueprintCallable, Category = "Narrative|Config")
    void SetAutoPublish(bool bEnabled);

    /** Request a diary entry from the model on the next tick. */
    UFUNCTION(BlueprintCallable, Category = "Narrative|Loop")
    void ForceDiaryEntry();

    /** Request an insight from the model on the next tick. */
    UFUNCTION(BlueprintCallable, Category = "Narrative|Loop")
    void ForceInsightGeneration();

    /** Request a blog post from the model on the next tick. */
    UFUNCTION(BlueprintCallable, Category = "Narrative|Loop")
    void ForceBlogPublish();

    // ===== Queries =====

    UFUNCTION(BlueprintPure, Category = "Narrative|Query")
    FNarrativeDiaryEntry GetMostRecentDiaryEntry() const;

    UFUNCTION(BlueprintPure, Category = "Narrative|Query")
    FNarrativeInsight GetMostRecentInsight() const;

    UFUNCTION(BlueprintPure, Category = "Narrative|Query")
    FNarrativeBlogPost GetMostRecentBlogPost() const;

    UFUNCTION(BlueprintPure, Category = "Narrative|Query")
    TArray<FNarrativeDiaryEntry> GetRecentDiaryEntries(int32 Count = 10) const;

    UFUNCTION(BlueprintPure, Category = "Narrative|Query")
    TArray<FNarrativeInsight> GetRecentInsights(int32 Count = 5) const;

    UFUNCTION(BlueprintPure, Category = "Narrative|Query")
    TArray<FNarrativeBlogPost> GetRecentBlogPosts(int32 Count = 5) const;

    // ===== Events =====

    UPROPERTY(BlueprintAssignable, Category = "Narrative|Events")
    FOnNarrativeLoopStarted OnNarrativeLoopStarted;

    UPROPERTY(BlueprintAssignable, Category = "Narrative|Events")
    FOnNarrativeLoopStopped OnNarrativeLoopStopped;

    UPROPERTY(BlueprintAssignable, Category = "Narrative|Events")
    FOnNarrativePhaseChanged OnNarrativePhaseChanged;

    UPROPERTY(BlueprintAssignable, Category = "Narrative|Events")
    FOnDiaryEntryCreated OnDiaryEntryCreated;

    UPROPERTY(BlueprintAssignable, Category = "Narrative|Events")
    FOnNarrativeInsightGenerated OnInsightGenerated;

    UPROPERTY(BlueprintAssignable, Category = "Narrative|Events")
    FOnBlogPostPublished OnBlogPostPublished;

    UPROPERTY(BlueprintAssignable, Category = "Narrative|Events")
    FOnNarrativeGenerationFailed OnNarrativeGenerationFailed;

protected:
    void UpdateNarrativePhase();
    void ProcessCurrentPhase(float DeltaTime);

    void ProcessObservation(float DeltaTime);
    void ProcessDiaryWriting(float DeltaTime);
    void ProcessInsightGeneration(float DeltaTime);
    void ProcessBlogPublishing(float DeltaTime);

    /** Averages only the sources that were actually present in the observations. */
    static FNarrativeObservation AverageObservations(const TArray<FNarrativeObservation>& Observations);
    static FString DescribeObservation(const FNarrativeObservation& Average, int32 Count);
    FString BuildPersonaPrompt() const;

    /** Fills Config's prompt templates when they are empty (editor values are kept). */
    void InitializePromptTemplates();

    /** Pattern router: picks the insight prompt template from the entries' measured state. */
    const FNarrativeInsightPromptTemplate* SelectInsightTemplate(const TArray<const FNarrativeDiaryEntry*>& Entries,
        TMap<FString, FString>& OutSlots) const;

    void HandleDiaryReply(bool bSuccess, const FString& TextOrError, int32 ConsumedObservations, FNarrativeObservation Average);
    void HandleInsightReply(bool bSuccess, const FString& TextOrError, TArray<FGuid> SourceEntryIDs, FString Category);
    void HandleBlogReply(bool bSuccess, const FString& TextOrError, TArray<FNarrativeInsight> SourceInsights);
    void ReportFailure(ENarrativePhase Phase, const FString& Error);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
    FNarrativeLoopConfig Config;

    UPROPERTY(BlueprintReadOnly, Category = "Narrative")
    FNarrativeLoopState State;

private:
    UPROPERTY()
    UPersonalityTraitSystem* PersonalitySystem = nullptr;

    UPROPERTY()
    UNeurochemicalSimulationComponent* NeurochemicalSystem = nullptr;

    TArray<FNarrativeObservation> ObservationBuffer;
    TArray<FNarrativeDiaryEntry> DiaryEntries;
    TArray<FNarrativeInsight> Insights;
    TArray<FNarrativeBlogPost> BlogPosts;

    /** One outstanding model request per phase. */
    bool bDiaryRequestInFlight = false;
    bool bInsightRequestInFlight = false;
    bool bBlogRequestInFlight = false;
};
