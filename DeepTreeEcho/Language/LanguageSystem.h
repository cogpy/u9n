// LanguageSystem.h
// Natural Language Processing and Generation for Deep Tree Echo
// Implements semantic parsing, dialogue management, and narrative generation

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LanguageSystem.generated.h"

// Forward declarations
class UCognitiveCycleManager;
class UMemorySystems;
class UEmotionalSystem;

/**
 * Speech act type
 */
UENUM(BlueprintType)
enum class ESpeechAct : uint8
{
    Assertion       UMETA(DisplayName = "Assertion"),
    Question        UMETA(DisplayName = "Question"),
    Command         UMETA(DisplayName = "Command"),
    Request         UMETA(DisplayName = "Request"),
    Promise         UMETA(DisplayName = "Promise"),
    Greeting        UMETA(DisplayName = "Greeting"),
    Farewell        UMETA(DisplayName = "Farewell"),
    Acknowledgment  UMETA(DisplayName = "Acknowledgment"),
    Expression      UMETA(DisplayName = "Expression"),
    Declaration     UMETA(DisplayName = "Declaration")
};

/**
 * Dialogue state
 */
UENUM(BlueprintType)
enum class EDialogueState : uint8
{
    Idle            UMETA(DisplayName = "Idle"),
    Listening       UMETA(DisplayName = "Listening"),
    Processing      UMETA(DisplayName = "Processing"),
    Responding      UMETA(DisplayName = "Responding"),
    Waiting         UMETA(DisplayName = "Waiting"),
    Clarifying      UMETA(DisplayName = "Clarifying")
};

/**
 * Semantic role
 */
UENUM(BlueprintType)
enum class ESemanticRole : uint8
{
    Agent           UMETA(DisplayName = "Agent"),
    Patient         UMETA(DisplayName = "Patient"),
    Theme           UMETA(DisplayName = "Theme"),
    Experiencer     UMETA(DisplayName = "Experiencer"),
    Beneficiary     UMETA(DisplayName = "Beneficiary"),
    Instrument      UMETA(DisplayName = "Instrument"),
    Location        UMETA(DisplayName = "Location"),
    Source          UMETA(DisplayName = "Source"),
    Goal            UMETA(DisplayName = "Goal"),
    Time            UMETA(DisplayName = "Time")
};

/**
 * Parsed utterance
 */
USTRUCT(BlueprintType)
struct FParsedUtterance
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString UtteranceID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString RawText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ESpeechAct SpeechAct = ESpeechAct::Assertion;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString MainVerb;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<ESemanticRole, FString> SemanticRoles;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Entities;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Keywords;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Sentiment = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Confidence = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Timestamp = 0.0f;
};

/**
 * Dialogue turn
 */
USTRUCT(BlueprintType)
struct FDialogueTurn
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TurnID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Speaker;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Utterance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FParsedUtterance ParsedContent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Timestamp = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsAgentTurn = false;
};

/**
 * Dialogue context
 */
USTRUCT(BlueprintType)
struct FDialogueContext
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ConversationID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FDialogueTurn> TurnHistory;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString CurrentTopic;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> MentionedEntities;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, FString> SharedKnowledge;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ConversationStartTime = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EDialogueState State = EDialogueState::Idle;
};

/**
 * Generated response. Text is always the echo model's reply; ResponseID is the request ID
 * returned by the Request* call that produced it.
 */
USTRUCT(BlueprintType)
struct FGeneratedResponse
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ResponseID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Text;

    /** The kind of utterance that was requested from the model (request metadata). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ESpeechAct SpeechAct = ESpeechAct::Assertion;

    /** Emotional tone passed to the model as input (-1..1). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmotionalTone = 0.0f;
};

/**
 * Narrative element
 */
USTRUCT(BlueprintType)
struct FNarrativeElement
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ElementID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Content;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString NarrativeType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Characters;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Setting;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmotionalArc = 0.0f;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUtteranceParsed, const FParsedUtterance&, Parsed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnResponseGenerated, const FGeneratedResponse&, Response);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDialogueStateChanged, EDialogueState, OldState, EDialogueState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTopicChanged, const FString&, NewTopic);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNarrativeGenerated, const FNarrativeElement&, Narrative);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLanguageGenerationFailed, const FString&, RequestID, const FString&, Error);

/**
 * Language System Component
 * Parses user utterances (keyword heuristics, used only as model INPUT metadata) and produces
 * every agent utterance and narrative through the echo model (UEchoModelClient).
 *
 * All generation is asynchronous: Request* functions return a request ID immediately; the
 * result arrives via OnResponseGenerated / OnNarrativeGenerated (model text) or
 * OnGenerationFailed (system error; no text is produced). There are no templates or fallbacks.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class UNREALECHO_API ULanguageSystem : public UActorComponent
{
    GENERATED_BODY()

public:
    ULanguageSystem();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, 
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 MaxTurnHistory = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float DefaultFormality = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float ResponseTimeout = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    FString AgentName = TEXT("Echo");

    /** Persona description given to the model as part of its system prompt (model input). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", meta = (MultiLine = true))
    FString PersonaDescription;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnUtteranceParsed OnUtteranceParsed;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnResponseGenerated OnResponseGenerated;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnDialogueStateChanged OnDialogueStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnTopicChanged OnTopicChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnNarrativeGenerated OnNarrativeGenerated;

    /** A Request* call produced no text (model unconfigured / request failed). */
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnLanguageGenerationFailed OnGenerationFailed;

    // ========================================
    // PARSING
    // ========================================

    /** Parse an utterance */
    UFUNCTION(BlueprintCallable, Category = "Parsing")
    FParsedUtterance ParseUtterance(const FString& Text);

    /** Identify speech act */
    UFUNCTION(BlueprintPure, Category = "Parsing")
    ESpeechAct IdentifySpeechAct(const FString& Text) const;

    /** Extract entities from text */
    UFUNCTION(BlueprintPure, Category = "Parsing")
    TArray<FString> ExtractEntities(const FString& Text) const;

    /** Extract keywords from text */
    UFUNCTION(BlueprintPure, Category = "Parsing")
    TArray<FString> ExtractKeywords(const FString& Text) const;

    /** Analyze sentiment */
    UFUNCTION(BlueprintPure, Category = "Parsing")
    float AnalyzeSentiment(const FString& Text) const;

    // ========================================
    // DIALOGUE MANAGEMENT
    // ========================================

    /** Start a new conversation */
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void StartConversation(const FString& InitialTopic);

    /**
     * Records an incoming utterance and asks the model for the agent's reply.
     * Returns the request ID; the reply arrives via OnResponseGenerated (and is then added to
     * the turn history) or OnGenerationFailed.
     */
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    FString ProcessUtterance(const FString& Speaker, const FString& Text);

    /** Get dialogue context */
    UFUNCTION(BlueprintPure, Category = "Dialogue")
    FDialogueContext GetDialogueContext() const;

    /** Get dialogue state */
    UFUNCTION(BlueprintPure, Category = "Dialogue")
    EDialogueState GetDialogueState() const;

    /** Set dialogue state */
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void SetDialogueState(EDialogueState NewState);

    /** Get current topic */
    UFUNCTION(BlueprintPure, Category = "Dialogue")
    FString GetCurrentTopic() const;

    /** Set current topic */
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void SetCurrentTopic(const FString& Topic);

    /** End conversation */
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void EndConversation();

    /** Get turn history */
    UFUNCTION(BlueprintPure, Category = "Dialogue")
    TArray<FDialogueTurn> GetTurnHistory() const;

    // ========================================
    // RESPONSE GENERATION
    // ========================================

    /** Request a model reply to a parsed utterance (tone from the emotional system). Returns the request ID. */
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FString RequestResponse(const FParsedUtterance& Input);

    /** Request a model reply with the given emotional tone (-1..1) as input. Returns the request ID. */
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FString RequestEmotionalResponse(const FParsedUtterance& Input, float EmotionalTone);

    /** Request a clarification question about UnclearAspect. Returns the request ID. */
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FString RequestClarification(const FString& UnclearAspect);

    /** Request an acknowledgment of the latest turn. Returns the request ID. */
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FString RequestAcknowledgment();

    /** Request a greeting (RecipientName may be empty). Returns the request ID. */
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FString RequestGreeting(const FString& RecipientName);

    /** Request a farewell. Returns the request ID. */
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FString RequestFarewell();

    // ========================================
    // NARRATIVE GENERATION (model-backed, asynchronous)
    // ========================================

    /** Request a narrative about Subject and Events. Result via OnNarrativeGenerated. Returns the request ID. */
    UFUNCTION(BlueprintCallable, Category = "Narrative")
    FString RequestNarrative(const FString& Subject, const TArray<FString>& Events);

    /** Request a first-person self-description. Returns the request ID. */
    UFUNCTION(BlueprintCallable, Category = "Narrative")
    FString RequestSelfNarrative();

    /** Request a summary of recent experience (dialogue turns within TimeSpan seconds). Returns the request ID. */
    UFUNCTION(BlueprintCallable, Category = "Narrative")
    FString RequestExperienceSummary(float TimeSpan);

protected:
    // Component references
    UPROPERTY()
    UCognitiveCycleManager* CycleManager;

    UPROPERTY()
    UMemorySystems* MemoryComponent;

    UPROPERTY()
    UEmotionalSystem* EmotionalComponent;

    // Internal state
    FDialogueContext DialogueContext;

    int32 UtteranceIDCounter = 0;
    int32 TurnIDCounter = 0;
    int32 ResponseIDCounter = 0;
    int32 NarrativeIDCounter = 0;
    int32 ConversationIDCounter = 0;

    // Internal methods
    void FindComponentReferences();
    void InitializeLanguageSystem();

    void UpdateDialogueState(float DeltaTime);
    void AddTurnToHistory(const FDialogueTurn& Turn);

    // Model plumbing
    FString BuildSystemPrompt() const;
    FString BuildRecentTranscript(int32 MaxTurns) const;
    float GetCurrentEmotionalTone() const;
    FString SendResponseRequest(const FString& Instruction, ESpeechAct SpeechAct, float EmotionalTone,
                                EDialogueState StateOnSuccess);
    FString SendNarrativeRequest(const FString& Instruction, const FString& NarrativeType,
                                 const TArray<FString>& Characters);
    void HandleResponseReply(bool bSuccess, const FString& TextOrError, FString RequestID,
                             ESpeechAct SpeechAct, float EmotionalTone, EDialogueState StateOnSuccess);
    void HandleNarrativeReply(bool bSuccess, const FString& TextOrError, FString RequestID,
                              FString NarrativeType, TArray<FString> Characters, float EmotionalArc);

    bool IsQuestion(const FString& Text) const;
    bool IsCommand(const FString& Text) const;
    bool IsGreeting(const FString& Text) const;

    FString GenerateUtteranceID();
    FString GenerateTurnID();
    FString GenerateResponseID();
    FString GenerateNarrativeID();
    FString GenerateConversationID();
};
