#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AvatarCommunicationManager.generated.h"

// Forward declarations
class UPersonalityTraitSystem;
class UNeurochemicalSimulationComponent;
class UDiaryInsightBlogLoop;
class UAvatar9PServer;
class UAvatarAtomSpaceClient;

/**
 * Message Source Types
 */
UENUM(BlueprintType)
enum class EMessageSource : uint8
{
    DeltaChat UMETA(DisplayName = "DeltaChat"),
    Dovecot UMETA(DisplayName = "Dovecot/Email"),
    WebSocket UMETA(DisplayName = "WebSocket"),
    P9 UMETA(DisplayName = "9P Protocol"),
    Internal UMETA(DisplayName = "Internal System"),
    Agent UMETA(DisplayName = "AGI-OS Agent")
};

/**
 * Message Priority Levels
 */
UENUM(BlueprintType)
enum class EMessagePriority : uint8
{
    Low UMETA(DisplayName = "Low"),
    Normal UMETA(DisplayName = "Normal"),
    High UMETA(DisplayName = "High"),
    Urgent UMETA(DisplayName = "Urgent"),
    System UMETA(DisplayName = "System Critical")
};

/**
 * Conversation State
 */
UENUM(BlueprintType)
enum class EConversationState : uint8
{
    Idle UMETA(DisplayName = "Idle"),
    Active UMETA(DisplayName = "Active"),
    Waiting UMETA(DisplayName = "Waiting for Response"),
    Processing UMETA(DisplayName = "Processing"),
    Ended UMETA(DisplayName = "Ended")
};

/**
 * DeltaChat Message structure
 */
USTRUCT(BlueprintType)
struct FDeltaChatMessage
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString MessageId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ChatId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SenderId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SenderName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Content;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime Timestamp;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsGroupChat;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Attachments;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ReplyToMessageId;

    FDeltaChatMessage()
        : Timestamp(FDateTime::Now())
        , bIsGroupChat(false)
    {}
};

/**
 * Email Message structure (Dovecot)
 */
USTRUCT(BlueprintType)
struct FDovecotEmail
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString EmailId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString From;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> To;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Cc;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Subject;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString BodyText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString BodyHtml;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime ReceivedAt;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Attachments;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString InReplyTo;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ThreadId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EMessagePriority Priority;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsRead;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsStarred;

    FDovecotEmail()
        : ReceivedAt(FDateTime::Now())
        , Priority(EMessagePriority::Normal)
        , bIsRead(false)
        , bIsStarred(false)
    {}
};

/**
 * A reply composed by the character's model (UEchoModelClient).
 * Every text/label field here is parsed from the model's reply; nothing is filled from templates.
 */
USTRUCT(BlueprintType)
struct FCommunicationReply
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ResponseId;

    /** The model's reply text. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Content;

    /** Tone label chosen by the model (free text, may be empty). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Tone;

    /** Expression chosen by the model from the avatar's expression vocabulary (empty = none). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Expression;

    /** Gesture chosen by the model from the avatar's gesture vocabulary (empty = none). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Gesture;

    /** Model-chosen reaction intensity in [0, 1]. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Intensity;

    /** Model's reading of the incoming message's sentiment in [0, 1] (0 negative, 0.5 neutral, 1 positive). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float IncomingSentiment;

    /** Intent the keyword router detected, i.e. which intent prompt template was used (request metadata). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Intent;

    /** Avatar's neurochemical valence at reply time (measured state, not generated). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmotionalValence;

    /** Avatar's neurochemical arousal at reply time (measured state, not generated). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmotionalArousal;

    /** True only when a transport actually delivered the reply. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bDelivered;

    FCommunicationReply()
        : Intensity(0.0f)
        , IncomingSentiment(0.5f)
        , EmotionalValence(0.5f)
        , EmotionalArousal(0.5f)
        , bDelivered(false)
    {}
};

/**
 * Communication log record: a system record of one exchange (incoming message and the model's
 * reply, with its real delivery status). Kept in the manager's communication log, not in the
 * character's diary.
 */
USTRUCT(BlueprintType)
struct FCommunicationLogRecord
{
    GENERATED_BODY()

    /** "deltachat" or "email" */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Channel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Sender;

    /** Email subject (empty for chat) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Subject;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString IncomingContent;

    /** The model's reply, verbatim */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ReplyContent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bDelivered = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime Timestamp;
};

/**
 * Conversation Context
 */
USTRUCT(BlueprintType)
struct FConversationContext
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ConversationId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EMessageSource Source;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ParticipantId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ParticipantName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EConversationState State;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> MessageHistory;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime StartedAt;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime LastActivityAt;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Topic;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SentimentScore;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MessageCount;

    FConversationContext()
        : Source(EMessageSource::Internal)
        , State(EConversationState::Idle)
        , StartedAt(FDateTime::Now())
        , LastActivityAt(FDateTime::Now())
        , SentimentScore(0.5f)
        , MessageCount(0)
    {}
};

/** What a pending model request is replying to (plain C++; not exposed to reflection). */
struct FAvatarPendingReplyInfo
{
    EMessageSource Source = EMessageSource::Internal;
    FString Intent;          // routed intent (selects the intent prompt template)
    FString MessageId;
    FString ReplyTarget;     // chat id or email id
    FString ParticipantId;
    FString ParticipantName;
    FString Subject;
    FDeltaChatMessage ChatMessage;
    FDovecotEmail Email;
};

/**
 * Delegates for communication events
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeltaChatMessageReceived, const FDeltaChatMessage&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEmailReceived, const FDovecotEmail&, Email);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCommunicationReplyGenerated, const FString&, MessageId, const FCommunicationReply&, Reply);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCommunicationReplyFailed, const FString&, MessageId, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnConversationStateChanged, const FString&, ConversationId, EConversationState, NewState);

/**
 * Avatar Communication Manager Component
 * Handles messaging integration with DeltaChat and Dovecot for AGI-OS
 *
 * Message Flow:
 * DeltaChat/Dovecot -> 9P Interface -> Avatar Communication Manager
 *     |
 *     v
 * Intent router (DetectIntent keywords) picks an intent prompt template (greeting, farewell,
 * compliment, question, request, statement); the channel prompt template (chat or email) adds the
 * channel's form. Both are filled with the real message and sent to the echo model
 * (UEchoModelClient: configured API or verified local model) with the persona built from
 * personality/neurochemical state as INPUT; the reply text, tone, sentiment reading, expression
 * and gesture are the model's OUTPUT
 *     |
 *     v
 * FEchoReactionBus (expression/gesture/caption consumers), narrative diary (what was said)
 *     |
 *     v
 * DeltaChat/Dovecot transport (not implemented: replies are reported as not delivered)
 *
 * When the model is not configured or a request fails, no reply is produced, nothing is sent,
 * nothing is recorded as the character's words and OnReplyFailed carries the error.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALENGINE_API UAvatarCommunicationManager : public UActorComponent
{
    GENERATED_BODY()

public:
    UAvatarCommunicationManager();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ===== Connection Management =====

    /** No DeltaChat transport is implemented: always leaves the connection down and returns false. */
    UFUNCTION(BlueprintCallable, Category = "Communication|Connection")
    bool ConnectDeltaChat(const FString& AccountEmail, const FString& Password);

    /** No IMAP transport is implemented: always leaves the connection down and returns false. */
    UFUNCTION(BlueprintCallable, Category = "Communication|Connection")
    bool ConnectDovecot(const FString& ImapHost, int32 ImapPort, const FString& Username, const FString& Password);

    /** Last connection/transport error (empty when none). */
    UFUNCTION(BlueprintCallable, Category = "Communication|Connection")
    FString GetLastTransportError() const { return LastTransportError; }

    UFUNCTION(BlueprintCallable, Category = "Communication|Connection")
    void DisconnectAll();

    UFUNCTION(BlueprintCallable, Category = "Communication|Connection")
    bool IsDeltaChatConnected() const { return bDeltaChatConnected; }

    UFUNCTION(BlueprintCallable, Category = "Communication|Connection")
    bool IsDovecotConnected() const { return bDovecotConnected; }

    // ===== Message Reception =====

    UFUNCTION(BlueprintCallable, Category = "Communication|Messages")
    void OnDeltaChatMessageReceived(const FDeltaChatMessage& Message);

    UFUNCTION(BlueprintCallable, Category = "Communication|Messages")
    void OnEmailReceived(const FDovecotEmail& Email);

    UFUNCTION(BlueprintCallable, Category = "Communication|Messages")
    void ProcessPendingMessages();

    // ===== Response Generation (model-backed, asynchronous) =====

    /**
     * Asks the echo model for a reply to a DeltaChat message. Completes later via
     * OnReplyGenerated (model reply) or OnReplyFailed (error; no reply is produced).
     * Returns false if the request could not be started (another request in flight).
     */
    UFUNCTION(BlueprintCallable, Category = "Communication|Response")
    bool RequestDeltaChatResponse(const FDeltaChatMessage& Message);

    /** Same as RequestDeltaChatResponse, for an email. */
    UFUNCTION(BlueprintCallable, Category = "Communication|Response")
    bool RequestEmailResponse(const FDovecotEmail& Email);

    /** Blueprint-compatible name: starts the model request (see RequestDeltaChatResponse). */
    UFUNCTION(BlueprintCallable, Category = "Communication|Response")
    bool GenerateDeltaChatResponse(const FDeltaChatMessage& Message) { return RequestDeltaChatResponse(Message); }

    /** Blueprint-compatible name: starts the model request (see RequestEmailResponse). */
    UFUNCTION(BlueprintCallable, Category = "Communication|Response")
    bool GenerateEmailResponse(const FDovecotEmail& Email) { return RequestEmailResponse(Email); }

    /**
     * Keyword router: "greeting", "farewell", "compliment", "question", "request" or "statement".
     * It only selects which intent prompt template the model receives; it never produces words.
     */
    UFUNCTION(BlueprintPure, Category = "Communication|Analysis")
    FString DetectIntent(const FString& Text) const;

    /** Delivers a reply over DeltaChat. No transport is implemented: logs and returns false. */
    UFUNCTION(BlueprintCallable, Category = "Communication|Response")
    bool SendDeltaChatResponse(const FString& ChatId, const FCommunicationReply& Reply);

    /** Delivers a reply over SMTP. No transport is implemented: logs and returns false. */
    UFUNCTION(BlueprintCallable, Category = "Communication|Response")
    bool SendEmailResponse(const FString& EmailId, const FCommunicationReply& Reply);

    // ===== Conversation Management =====

    UFUNCTION(BlueprintCallable, Category = "Communication|Conversation")
    FConversationContext GetOrCreateConversation(const FString& ParticipantId, EMessageSource Source);

    UFUNCTION(BlueprintCallable, Category = "Communication|Conversation")
    void UpdateConversationState(const FString& ConversationId, EConversationState NewState);

    UFUNCTION(BlueprintCallable, Category = "Communication|Conversation")
    FConversationContext GetConversation(const FString& ConversationId);

    UFUNCTION(BlueprintCallable, Category = "Communication|Conversation")
    TArray<FConversationContext> GetActiveConversations();

    UFUNCTION(BlueprintCallable, Category = "Communication|Conversation")
    void EndConversation(const FString& ConversationId);

    // ===== Personality-Driven Pacing =====

    UFUNCTION(BlueprintCallable, Category = "Communication|Style")
    float CalculateResponseDelay(const FConversationContext& Context);

    // ===== Narrative Integration =====

    /**
     * Records a chat exchange whose reply came from the model (Reply.bDelivered says whether it was
     * sent) in the communication log. A system record, not written to the character's diary.
     */
    UFUNCTION(BlueprintCallable, Category = "Communication|Log")
    void RecordChatExchange(const FDeltaChatMessage& Message, const FCommunicationReply& Reply);

    /** Records an email exchange in the communication log (not the character's diary). */
    UFUNCTION(BlueprintCallable, Category = "Communication|Log")
    void RecordEmailExchange(const FDovecotEmail& Email, const FCommunicationReply& Reply);

    /** System record of exchanges (most recent last). */
    UFUNCTION(BlueprintPure, Category = "Communication|Log")
    TArray<FCommunicationLogRecord> GetCommunicationLog() const { return CommunicationLog; }

    /** Most records kept in the communication log. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Communication|Log", meta = (ClampMin = "1"))
    int32 MaxCommunicationLogRecords = 500;

    /** Asks the model to reflect on a conversation; the insight is stored only if the model answers. */
    UFUNCTION(BlueprintCallable, Category = "Communication|Narrative")
    void RequestConversationInsight(const FConversationContext& Context);

    // ===== Text Utilities =====

    UFUNCTION(BlueprintCallable, Category = "Communication|Analysis")
    TArray<FString> ExtractKeywords(const FString& Text);

    // ===== 9P Integration =====

    UFUNCTION(BlueprintCallable, Category = "Communication|9P")
    void Register9PNamespace();

    UFUNCTION(BlueprintCallable, Category = "Communication|9P")
    FString Handle9PRead(const FString& Path);

    UFUNCTION(BlueprintCallable, Category = "Communication|9P")
    bool Handle9PWrite(const FString& Path, const FString& Data);

    // ===== Events =====

    UPROPERTY(BlueprintAssignable, Category = "Communication|Events")
    FOnDeltaChatMessageReceived OnDeltaChatMessage;

    UPROPERTY(BlueprintAssignable, Category = "Communication|Events")
    FOnEmailReceived OnEmail;

    /** A reply arrived from the model (Content is the model's text). */
    UPROPERTY(BlueprintAssignable, Category = "Communication|Events")
    FOnCommunicationReplyGenerated OnReplyGenerated;

    /** The model produced no usable reply; Error is a system-level description (never character speech). */
    UPROPERTY(BlueprintAssignable, Category = "Communication|Events")
    FOnCommunicationReplyFailed OnReplyFailed;

    UPROPERTY(BlueprintAssignable, Category = "Communication|Events")
    FOnConversationStateChanged OnConversationStateChanged;

protected:
    // ===== Internal Methods =====
    void InitializePromptTemplates();
    FString BuildReplyPrompt(const FAvatarPendingReplyInfo& Info, const FString& IncomingText, const FString& Transcript);

    void ProcessDeltaChatQueue();
    void ProcessEmailQueue();
    void RequestModelReply(const FAvatarPendingReplyInfo& Info, const FString& IncomingText);
    void HandleModelReply(bool bSuccess, const FString& TextOrError, FAvatarPendingReplyInfo Info);
    void HandleInsightReply(bool bSuccess, const FString& TextOrError, FString ConversationId, FString ParticipantName, int32 MessageCount);
    FString BuildPersonaPrompt(const FAvatarPendingReplyInfo& Info) const;
    void ApplyModelSentiment(float IncomingSentiment);
    void AppendHistory(const FString& ParticipantId, const FString& Line);
    void AddCommunicationLogRecord(FCommunicationLogRecord&& Record);

    /** System record of exchanges; see RecordChatExchange */
    UPROPERTY()
    TArray<FCommunicationLogRecord> CommunicationLog;

    static FString ListEnumNames(const UEnum* Enum);
    static FString MatchVocabulary(const FString& Choice, const UEnum* Enum);

private:
    // ===== Component References =====

    UPROPERTY()
    UPersonalityTraitSystem* PersonalitySystem;

    UPROPERTY()
    UNeurochemicalSimulationComponent* NeurochemicalSystem;

    UPROPERTY()
    UDiaryInsightBlogLoop* NarrativeSystem;

    UPROPERTY()
    UAvatar9PServer* P9Server;

    UPROPERTY()
    UAvatarAtomSpaceClient* AtomSpaceClient;

    // ===== Connection State =====

    UPROPERTY()
    bool bDeltaChatConnected;

    UPROPERTY()
    bool bDovecotConnected;

    UPROPERTY()
    FString DeltaChatAccountEmail;

    UPROPERTY()
    FString DovecotImapHost;

    UPROPERTY()
    int32 DovecotImapPort;

    // ===== Message Queues =====

    UPROPERTY()
    TArray<FDeltaChatMessage> PendingDeltaChatMessages;

    UPROPERTY()
    TArray<FDovecotEmail> PendingEmails;

    bool bDeltaChatRequestInFlight;
    bool bEmailRequestInFlight;

    UPROPERTY()
    FString LastTransportError;

    // ===== Conversation State =====

    UPROPERTY()
    TMap<FString, FConversationContext> ActiveConversations;

    // ===== Configuration =====

    UPROPERTY(EditAnywhere, Category = "Communication|Configuration")
    float MessageProcessingInterval;

    UPROPERTY(EditAnywhere, Category = "Communication|Configuration")
    int32 MaxPendingMessages;

    UPROPERTY(EditAnywhere, Category = "Communication|Configuration")
    float MinResponseDelay;

    UPROPERTY(EditAnywhere, Category = "Communication|Configuration")
    float MaxResponseDelay;

    /** When true, queued messages are answered by the echo model (one request per queue at a time). */
    UPROPERTY(EditAnywhere, Category = "Communication|Configuration")
    bool bAutoRespond;

    /** Name the character goes by (model input). */
    UPROPERTY(EditAnywhere, Category = "Communication|Persona")
    FString CharacterName;

    /** Free-text persona description passed to the model as part of its system prompt (model input). */
    UPROPERTY(EditAnywhere, Category = "Communication|Persona", meta = (MultiLine = true))
    FString PersonaDescription;

    /** How many recent conversation lines are sent to the model as context. */
    UPROPERTY(EditAnywhere, Category = "Communication|Persona")
    int32 MaxHistoryLinesForModel;

    // ===== Prompt Templates (instructions to the model; never sent as the character's words) =====

    /**
     * Per-intent reply prompt templates, keyed by DetectIntent's result. Slots: {participant},
     * {message}, {keywords}, {topic}, {character}.
     */
    UPROPERTY(EditAnywhere, Category = "Communication|Prompts", meta = (MultiLine = true))
    TMap<FString, FString> IntentPromptTemplates;

    /** Channel form for DeltaChat replies. Slots: {participant}, {max_chars}, {character}. */
    UPROPERTY(EditAnywhere, Category = "Communication|Prompts", meta = (MultiLine = true))
    FString ChatChannelPromptTemplate;

    /** Channel form for email replies (includes the sign-off instruction). Slots: {participant}, {subject}, {character}. */
    UPROPERTY(EditAnywhere, Category = "Communication|Prompts", meta = (MultiLine = true))
    FString EmailChannelPromptTemplate;

    /** Maximum DeltaChat reply length asked of the model (characters). */
    UPROPERTY(EditAnywhere, Category = "Communication|Prompts")
    int32 MaxChatReplyChars;

    /**
     * Conversation-insight prompt templates keyed "positive" (sentiment > 0.7), "challenging"
     * (< 0.3) and "neutral". Slots: {participant}, {sentiment}, {message_count}, {transcript}.
     */
    UPROPERTY(EditAnywhere, Category = "Communication|Prompts", meta = (MultiLine = true))
    TMap<FString, FString> InsightPromptTemplates;

    UPROPERTY(EditAnywhere, Category = "Communication|Configuration")
    bool bVerboseLogging;

    // ===== Timers =====

    float ProcessingTimer;
};
