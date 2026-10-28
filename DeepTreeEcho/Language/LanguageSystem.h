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
 * Response prompt template. Each entry is an instruction to the model for one kind of agent
 * utterance (greeting, acknowledgment, answer, clarification, farewell, ...), with "{slot}"
 * placeholders filled from the real dialogue (see ULanguageSystem::InitializeResponseTemplates
 * for the slots). The model's reply is the utterance; template text is never spoken.
 */
USTRUCT(BlueprintType)
struct FResponseTemplate
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TemplateID;

    /** Kind of utterance this template asks the model for. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ESpeechAct ResponseType = ESpeechAct::Assertion;

    /** Incoming speech acts routed to this template (empty = only used when requested by ID). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<ESpeechAct> InputActs;

    /** Incoming sentiment range routed to this template. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinInputSentiment = -1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxInputSentiment = 1.0f;

    /** The prompt template: an instruction to the model with {slot} placeholders. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (MultiLine = true))
    FString PromptTemplate;

    /** Slots this template needs; a request missing one is reported as a failure. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> RequiredSlots;

    /** Tone this template suits (-1..1); among matching templates the nearest to the agent's tone is used. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmotionalTone = 0.0f;

    /** Formality (0 casual .. 1 formal) asked of the model. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Formality = 0.5f;

    /** Optional style example shown to the model, clearly marked as an example not to copy. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString StyleExample;
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

    /** The prompt template the reply was generated from. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TemplateID;
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
 * Parses user utterances (keyword heuristics) and produces every agent utterance and narrative
 * through the echo model (UEchoModelClient: a configured API or a verified local model).
 *
 * Response and narrative templates are PROMPT templates: the parse routes an utterance to a
 * template (by speech act, sentiment and tone), the template's {slots} are filled with the real
 * dialogue, and the model writes the words.
 *
 * All generation is asynchronous: Request* / Generate* functions return a request ID
 * immediately; the result arrives via OnResponseGenerated / OnNarrativeGenerated (model text)
 * or OnGenerationFailed (system error; no text is produced, nothing is spoken).
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
    // PROMPT TEMPLATES
    // ========================================

    /** Response prompt templates (filled by InitializeResponseTemplates when empty). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Templates")
    TArray<FResponseTemplate> ResponseTemplates;

    /** Style instruction appended to every response prompt. Slots: {tone}, {tone_value}, {formality}. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Templates", meta = (MultiLine = true))
    FString ResponseStylePromptTemplate;

    /** Narrative prompt templates keyed by narrative type ("Description", "Self-Description", "Experience Summary"). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Templates")
    TMap<FString, FString> NarrativePromptTemplates;

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

    /**
     * Request an utterance from the response prompt template TemplateID, with Slots filling its
     * placeholders (dialogue slots such as {speaker}/{utterance} are filled automatically when
     * not given). Returns the request ID.
     */
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FString RequestFromTemplate(const FString& TemplateID, const TMap<FString, FString>& Slots);

    /** Template the parse would route Input to (by speech act, sentiment and tone). */
    UFUNCTION(BlueprintPure, Category = "Generation")
    FResponseTemplate SelectResponseTemplate(const FParsedUtterance& Input, float EmotionalTone) const;

    // Blueprint-compatible names: each starts the model request and returns its request ID; the
    // text arrives via OnResponseGenerated / OnNarrativeGenerated.
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FString GenerateResponse(const FParsedUtterance& Input) { return RequestResponse(Input); }
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FString GenerateEmotionalResponse(const FParsedUtterance& Input, float EmotionalTone) { return RequestEmotionalResponse(Input, EmotionalTone); }
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FString GenerateClarification(const FString& UnclearAspect) { return RequestClarification(UnclearAspect); }
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FString GenerateAcknowledgment() { return RequestAcknowledgment(); }
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FString GenerateGreeting(const FString& RecipientName) { return RequestGreeting(RecipientName); }
    UFUNCTION(BlueprintCallable, Category = "Generation")
    FString GenerateFarewell() { return RequestFarewell(); }

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

    UFUNCTION(BlueprintCallable, Category = "Narrative")
    FString GenerateNarrative(const FString& Subject, const TArray<FString>& Events) { return RequestNarrative(Subject, Events); }
    UFUNCTION(BlueprintCallable, Category = "Narrative")
    FString GenerateSelfNarrative() { return RequestSelfNarrative(); }
    UFUNCTION(BlueprintCallable, Category = "Narrative")
    FString GenerateExperienceSummary(float TimeSpan) { return RequestExperienceSummary(TimeSpan); }

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

    // Prompt templates
    void InitializeResponseTemplates();
    void InitializeNarrativeTemplates();
    const FResponseTemplate* FindResponseTemplate(const FString& TemplateID) const;
    void AddDialogueSlots(TMap<FString, FString>& Slots) const;
    static FString DescribeTone(float EmotionalTone);
    static FString DescribeFormality(float Formality);

    // Model plumbing
    FString BuildSystemPrompt() const;
    FString BuildRecentTranscript(int32 MaxTurns) const;
    float GetCurrentEmotionalTone() const;
    FString SendTemplateRequest(const FResponseTemplate& Template, TMap<FString, FString> Slots, float EmotionalTone,
                                EDialogueState StateOnSuccess);
    FString SendNarrativeRequest(const FString& NarrativeType, const TMap<FString, FString>& Slots,
                                 const TArray<FString>& Characters);
    void HandleResponseReply(bool bSuccess, const FString& TextOrError, FString RequestID,
                             ESpeechAct SpeechAct, float EmotionalTone, EDialogueState StateOnSuccess, FString TemplateID);
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
