#include "AvatarCommunicationManager.h"
#include "../Personality/PersonalityTraitSystem.h"
#include "../Neurochemical/NeurochemicalSimulationComponent.h"
#include "../Narrative/DiaryInsightBlogLoop.h"
#include "../9P/Avatar9PServer.h"
#include "../AtomSpace/AvatarAtomSpaceClient.h"
#include "../Avatar/DeepTreeEchoExpressionSystem.h"
#include "../Avatar/GestureSystem.h"
#include "EchoModelClient.h"
#include "EchoCharacterReaction.h"
#include "Dom/JsonObject.h"

DEFINE_LOG_CATEGORY_STATIC(LogCommunication, Log, All);

UAvatarCommunicationManager::UAvatarCommunicationManager()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f; // 10 Hz

    bDeltaChatConnected = false;
    bDovecotConnected = false;
    DovecotImapPort = 993;
    MessageProcessingInterval = 0.5f;
    MaxPendingMessages = 100;
    MinResponseDelay = 0.5f;
    MaxResponseDelay = 3.0f;
    bAutoRespond = true;
    bVerboseLogging = false;
    ProcessingTimer = 0.0f;
    bDeltaChatRequestInFlight = false;
    bEmailRequestInFlight = false;
    CharacterName = TEXT("Deep Tree Echo");
    MaxHistoryLinesForModel = 12;
    MaxChatReplyChars = 500;

    InitializePromptTemplates();
}

// ===== Prompt Templates =====
//
// The intent and channel tables below are the structure the replies used to be built from
// (per-intent reply lines, chat vs email form, the email signature). Each entry is now an
// instruction to the model; the model writes the words.

void UAvatarCommunicationManager::InitializePromptTemplates()
{
    IntentPromptTemplates.Add(TEXT("greeting"),
        TEXT("{participant} greeted you: \"{message}\". Greet them back in your own voice and ask what is on their mind."));
    IntentPromptTemplates.Add(TEXT("farewell"),
        TEXT("{participant} is saying goodbye: \"{message}\". Say goodbye warmly; you may mention talking again soon."));
    IntentPromptTemplates.Add(TEXT("compliment"),
        TEXT("{participant} complimented you: \"{message}\". Accept the compliment graciously and in character."));
    IntentPromptTemplates.Add(TEXT("question"),
        TEXT("{participant} asked you: \"{message}\". Answer the question itself as well as you can (key terms: {keywords}); ")
        TEXT("if you do not know, say so honestly."));
    IntentPromptTemplates.Add(TEXT("request"),
        TEXT("{participant} asked you for help: \"{message}\". Respond to the request: say how you can help or what you need ")
        TEXT("to know. Do not claim to have done anything you have not done."));
    IntentPromptTemplates.Add(TEXT("statement"),
        TEXT("{participant} wrote: \"{message}\". Respond to what they shared and keep the conversation going."));

    ChatChannelPromptTemplate =
        TEXT("This is a DeltaChat chat with {participant}: write a short, casual chat message of at most {max_chars} characters.");
    EmailChannelPromptTemplate =
        TEXT("This is an email reply to {participant} (subject: \"{subject}\"): write the email body in a professional, ")
        TEXT("structured tone, and end it with a short sign-off followed by your name, {character}.");

    InsightPromptTemplates.Add(TEXT("positive"),
        TEXT("Your conversation with {participant} went well (their sentiment as you read it: {sentiment}; {message_count} messages). ")
        TEXT("Reflect privately: what made it a positive interaction, and what do you want to remember about {participant}?\n{transcript}"));
    InsightPromptTemplates.Add(TEXT("challenging"),
        TEXT("Your conversation with {participant} was challenging (their sentiment as you read it: {sentiment}; {message_count} messages). ")
        TEXT("Reflect privately: what was difficult, and how could you show more empathy next time?\n{transcript}"));
    InsightPromptTemplates.Add(TEXT("neutral"),
        TEXT("You exchanged {message_count} messages with {participant} (their sentiment as you read it: {sentiment}). ")
        TEXT("Reflect privately: what pattern do you notice in how you communicate with {participant}?\n{transcript}"));
}

FString UAvatarCommunicationManager::DetectIntent(const FString& Text) const
{
    const FString LowerText = Text.ToLower();

    if (LowerText.Contains(TEXT("?")) || LowerText.Contains(TEXT("how")) ||
        LowerText.Contains(TEXT("what")) || LowerText.Contains(TEXT("why")) ||
        LowerText.Contains(TEXT("when")) || LowerText.Contains(TEXT("where")))
    {
        return TEXT("question");
    }
    if (LowerText.Contains(TEXT("please")) || LowerText.Contains(TEXT("could you")) ||
        LowerText.Contains(TEXT("can you")) || LowerText.Contains(TEXT("help")))
    {
        return TEXT("request");
    }
    if (LowerText.Contains(TEXT("hello")) || LowerText.StartsWith(TEXT("hi")) || LowerText.Contains(TEXT(" hi ")) ||
        LowerText.Contains(TEXT("hey")) || LowerText.Contains(TEXT("good morning")))
    {
        return TEXT("greeting");
    }
    if (LowerText.Contains(TEXT("bye")) || LowerText.Contains(TEXT("goodbye")) ||
        LowerText.Contains(TEXT("see you")) || LowerText.Contains(TEXT("take care")))
    {
        return TEXT("farewell");
    }
    if (LowerText.Contains(TEXT("love")) || LowerText.Contains(TEXT("amazing")) ||
        LowerText.Contains(TEXT("beautiful")) || LowerText.Contains(TEXT("wonderful")))
    {
        return TEXT("compliment");
    }
    return TEXT("statement");
}

FString UAvatarCommunicationManager::BuildReplyPrompt(const FAvatarPendingReplyInfo& Info, const FString& IncomingText,
                                                     const FString& Transcript)
{
    TMap<FString, FString> Slots;
    Slots.Add(TEXT("participant"), Info.ParticipantName.IsEmpty() ? Info.ParticipantId : Info.ParticipantName);
    Slots.Add(TEXT("message"), IncomingText);
    Slots.Add(TEXT("subject"), Info.Subject);
    Slots.Add(TEXT("character"), CharacterName);
    Slots.Add(TEXT("max_chars"), FString::FromInt(MaxChatReplyChars));
    const TArray<FString> Keywords = ExtractKeywords(IncomingText);
    Slots.Add(TEXT("keywords"), Keywords.Num() ? FString::Join(Keywords, TEXT(", ")) : FString(TEXT("(none)")));
    const FConversationContext* Context = ActiveConversations.Find(Info.ParticipantId);
    Slots.Add(TEXT("topic"), Context && !Context->Topic.IsEmpty() ? Context->Topic : FString(TEXT("(none)")));

    const FString* IntentTemplate = IntentPromptTemplates.Find(Info.Intent);
    if (!IntentTemplate)
    {
        IntentTemplate = IntentPromptTemplates.Find(TEXT("statement"));
    }
    const FString& ChannelTemplate = Info.Source == EMessageSource::Dovecot ? EmailChannelPromptTemplate : ChatChannelPromptTemplate;

    FString Prompt = FString::Printf(TEXT("Recent conversation (oldest first):\n%s\n"), *Transcript);
    if (IntentTemplate)
    {
        Prompt += UEchoModelClient::FillPrompt(*IntentTemplate, Slots) + TEXT("\n");
    }
    Prompt += UEchoModelClient::FillPrompt(ChannelTemplate, Slots);
    return Prompt;
}

void UAvatarCommunicationManager::BeginPlay()
{
    Super::BeginPlay();

    // Find sibling components
    if (AActor* Owner = GetOwner())
    {
        PersonalitySystem = Owner->FindComponentByClass<UPersonalityTraitSystem>();
        NeurochemicalSystem = Owner->FindComponentByClass<UNeurochemicalSimulationComponent>();
        NarrativeSystem = Owner->FindComponentByClass<UDiaryInsightBlogLoop>();
        P9Server = Owner->FindComponentByClass<UAvatar9PServer>();
        AtomSpaceClient = Owner->FindComponentByClass<UAvatarAtomSpaceClient>();
    }

    // Register 9P namespace entries for communication
    Register9PNamespace();

    UE_LOG(LogCommunication, Log, TEXT("Avatar Communication Manager initialized"));
}

void UAvatarCommunicationManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    DisconnectAll();
    Super::EndPlay(EndPlayReason);
}

void UAvatarCommunicationManager::TickComponent(float DeltaTime, ELevelTick TickType,
                                                  FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    ProcessingTimer += DeltaTime;
    if (ProcessingTimer >= MessageProcessingInterval)
    {
        ProcessPendingMessages();
        ProcessingTimer = 0.0f;
    }
}

// ===== Connection Management =====

bool UAvatarCommunicationManager::ConnectDeltaChat(const FString& AccountEmail, const FString& Password)
{
    DeltaChatAccountEmail = AccountEmail;
    bDeltaChatConnected = false;
    LastTransportError = FString::Printf(
        TEXT("DeltaChat transport not implemented: cannot connect account %s"), *AccountEmail);
    UE_LOG(LogCommunication, Error, TEXT("%s"), *LastTransportError);
    return false;
}

bool UAvatarCommunicationManager::ConnectDovecot(const FString& ImapHost, int32 ImapPort,
                                                   const FString& Username, const FString& Password)
{
    DovecotImapHost = ImapHost;
    DovecotImapPort = ImapPort;
    bDovecotConnected = false;
    LastTransportError = FString::Printf(
        TEXT("IMAP/SMTP transport not implemented: cannot connect to %s:%d"), *ImapHost, ImapPort);
    UE_LOG(LogCommunication, Error, TEXT("%s"), *LastTransportError);
    return false;
}

void UAvatarCommunicationManager::DisconnectAll()
{
    bDeltaChatConnected = false;
    bDovecotConnected = false;
    PendingDeltaChatMessages.Empty();
    PendingEmails.Empty();

    UE_LOG(LogCommunication, Log, TEXT("Communication manager: connections closed, queues cleared"));
}

// ===== Message Reception =====

void UAvatarCommunicationManager::AppendHistory(const FString& ParticipantId, const FString& Line)
{
    if (FConversationContext* Context = ActiveConversations.Find(ParticipantId))
    {
        Context->MessageHistory.Add(Line);
        Context->LastActivityAt = FDateTime::Now();
    }
}

void UAvatarCommunicationManager::OnDeltaChatMessageReceived(const FDeltaChatMessage& Message)
{
    if (PendingDeltaChatMessages.Num() >= MaxPendingMessages)
    {
        UE_LOG(LogCommunication, Warning, TEXT("DeltaChat queue full (%d); message %s dropped"),
            MaxPendingMessages, *Message.MessageId);
        return;
    }

    PendingDeltaChatMessages.Add(Message);

    FConversationContext Context = GetOrCreateConversation(Message.SenderId, EMessageSource::DeltaChat);
    Context.ParticipantName = Message.SenderName;
    Context.MessageHistory.Add(FString::Printf(TEXT("%s: %s"), *Message.SenderName, *Message.Content));
    Context.LastActivityAt = FDateTime::Now();
    Context.MessageCount++;
    ActiveConversations.Add(Message.SenderId, Context);

    OnDeltaChatMessage.Broadcast(Message);

    if (bVerboseLogging)
    {
        UE_LOG(LogCommunication, Verbose, TEXT("Received DeltaChat message from %s: %s"),
            *Message.SenderName, *Message.Content);
    }
}

void UAvatarCommunicationManager::OnEmailReceived(const FDovecotEmail& Email)
{
    if (PendingEmails.Num() >= MaxPendingMessages)
    {
        UE_LOG(LogCommunication, Warning, TEXT("Email queue full (%d); email %s dropped"),
            MaxPendingMessages, *Email.EmailId);
        return;
    }

    PendingEmails.Add(Email);

    FConversationContext Context = GetOrCreateConversation(Email.From, EMessageSource::Dovecot);
    Context.ParticipantName = Email.From;
    Context.MessageHistory.Add(FString::Printf(TEXT("%s (subject: %s): %s"), *Email.From, *Email.Subject, *Email.BodyText));
    Context.LastActivityAt = FDateTime::Now();
    Context.MessageCount++;
    Context.Topic = Email.Subject;
    ActiveConversations.Add(Email.From, Context);

    OnEmail.Broadcast(Email);

    if (bVerboseLogging)
    {
        UE_LOG(LogCommunication, Verbose, TEXT("Received email from %s: %s"), *Email.From, *Email.Subject);
    }
}

void UAvatarCommunicationManager::ProcessPendingMessages()
{
    ProcessDeltaChatQueue();
    ProcessEmailQueue();
}

void UAvatarCommunicationManager::ProcessDeltaChatQueue()
{
    // One model request at a time per queue; completion arrives asynchronously.
    if (!bAutoRespond || bDeltaChatRequestInFlight || PendingDeltaChatMessages.Num() == 0)
    {
        return;
    }

    const FDeltaChatMessage Message = PendingDeltaChatMessages[0];
    PendingDeltaChatMessages.RemoveAt(0);
    RequestDeltaChatResponse(Message);
}

void UAvatarCommunicationManager::ProcessEmailQueue()
{
    if (!bAutoRespond || bEmailRequestInFlight || PendingEmails.Num() == 0)
    {
        return;
    }

    const FDovecotEmail Email = PendingEmails[0];
    PendingEmails.RemoveAt(0);
    RequestEmailResponse(Email);
}

// ===== Response Generation (model-backed) =====

bool UAvatarCommunicationManager::RequestDeltaChatResponse(const FDeltaChatMessage& Message)
{
    if (bDeltaChatRequestInFlight)
    {
        return false;
    }

    FAvatarPendingReplyInfo Info;
    Info.Source = EMessageSource::DeltaChat;
    Info.MessageId = Message.MessageId;
    Info.ReplyTarget = Message.ChatId;
    Info.ParticipantId = Message.SenderId;
    Info.ParticipantName = Message.SenderName;
    Info.ChatMessage = Message;

    // Make sure the conversation exists even when called directly (not via OnDeltaChatMessageReceived).
    if (!ActiveConversations.Contains(Message.SenderId))
    {
        FConversationContext Context = GetOrCreateConversation(Message.SenderId, EMessageSource::DeltaChat);
        Context.ParticipantName = Message.SenderName;
        Context.MessageHistory.Add(FString::Printf(TEXT("%s: %s"), *Message.SenderName, *Message.Content));
        Context.MessageCount++;
        ActiveConversations.Add(Message.SenderId, Context);
    }

    bDeltaChatRequestInFlight = true;
    RequestModelReply(Info, Message.Content);
    return true;
}

bool UAvatarCommunicationManager::RequestEmailResponse(const FDovecotEmail& Email)
{
    if (bEmailRequestInFlight)
    {
        return false;
    }

    FAvatarPendingReplyInfo Info;
    Info.Source = EMessageSource::Dovecot;
    Info.MessageId = Email.EmailId;
    Info.ReplyTarget = Email.EmailId;
    Info.ParticipantId = Email.From;
    Info.ParticipantName = Email.From;
    Info.Subject = Email.Subject;
    Info.Email = Email;

    if (!ActiveConversations.Contains(Email.From))
    {
        FConversationContext Context = GetOrCreateConversation(Email.From, EMessageSource::Dovecot);
        Context.ParticipantName = Email.From;
        Context.Topic = Email.Subject;
        Context.MessageHistory.Add(FString::Printf(TEXT("%s (subject: %s): %s"), *Email.From, *Email.Subject, *Email.BodyText));
        Context.MessageCount++;
        ActiveConversations.Add(Email.From, Context);
    }

    bEmailRequestInFlight = true;
    RequestModelReply(Info, Email.BodyText);
    return true;
}

FString UAvatarCommunicationManager::ListEnumNames(const UEnum* Enum)
{
    TArray<FString> Names;
    if (Enum)
    {
        // NumEnums() includes the implicit _MAX entry.
        for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)
        {
            Names.Add(Enum->GetNameStringByIndex(i));
        }
    }
    return FString::Join(Names, TEXT(", "));
}

FString UAvatarCommunicationManager::MatchVocabulary(const FString& Choice, const UEnum* Enum)
{
    const FString Trimmed = Choice.TrimStartAndEnd();
    if (Trimmed.IsEmpty() || !Enum)
    {
        return FString();
    }
    for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)
    {
        const FString Name = Enum->GetNameStringByIndex(i);
        if (Name.Equals(Trimmed, ESearchCase::IgnoreCase))
        {
            return Name;
        }
    }
    return FString();
}

FString UAvatarCommunicationManager::BuildPersonaPrompt(const FAvatarPendingReplyInfo& Info) const
{
    // Everything assembled here is model INPUT (persona and state), never shown as the character's words.
    FString Prompt = FString::Printf(TEXT("You are %s, an embodied AI avatar."), *CharacterName);
    if (!PersonaDescription.IsEmpty())
    {
        Prompt += TEXT(" ") + PersonaDescription;
    }

    if (PersonalitySystem)
    {
        const FSuperHotGirlTrait SHG = PersonalitySystem->GetSuperHotGirlTrait();
        const FHyperChaoticTrait HC = PersonalitySystem->GetHyperChaoticTrait();
        Prompt += FString::Printf(
            TEXT("\nYour current personality trait levels (0-1): confidence %.2f, charm %.2f, playfulness %.2f, ")
            TEXT("elegance %.2f, expressiveness %.2f, unpredictability %.2f, emotional volatility %.2f, impulsivity %.2f."),
            SHG.Confidence, SHG.Charm, SHG.Playfulness, SHG.Elegance, SHG.Expressiveness,
            HC.UnpredictabilityFactor, HC.EmotionalVolatility, HC.ImpulsivityLevel);
    }

    if (NeurochemicalSystem)
    {
        const FEmotionalChemistry E = NeurochemicalSystem->GetEmotionalChemistry();
        Prompt += FString::Printf(
            TEXT("\nYour current emotional chemistry (0-1): happiness %.2f, excitement %.2f, calmness %.2f, ")
            TEXT("anxiety %.2f, affection %.2f, focus %.2f."),
            E.Happiness, E.Excitement, E.Calmness, E.Anxiety, E.Affection, E.Focus);
    }

    Prompt += FString::Printf(
        TEXT("\nAnswer ONLY with a JSON object: {\"reply\": string, \"tone\": string, \"sentiment\": number, ")
        TEXT("\"expression\": string, \"gesture\": string, \"intensity\": number}.")
        TEXT("\n\"sentiment\" is your reading of the latest incoming message from 0 (negative) through 0.5 (neutral) to 1 (positive).")
        TEXT("\n\"expression\" must be one of [%s] or empty.")
        TEXT("\n\"gesture\" must be one of [%s] or empty.")
        TEXT("\n\"intensity\" is between 0 and 1."),
        *ListEnumNames(StaticEnum<EExpressionState>()),
        *ListEnumNames(StaticEnum<EGestureType>()));

    return Prompt;
}

void UAvatarCommunicationManager::RequestModelReply(const FAvatarPendingReplyInfo& Info, const FString& IncomingText)
{
    FString Transcript;
    if (const FConversationContext* Context = ActiveConversations.Find(Info.ParticipantId))
    {
        const int32 First = FMath::Max(0, Context->MessageHistory.Num() - MaxHistoryLinesForModel);
        for (int32 i = First; i < Context->MessageHistory.Num(); ++i)
        {
            Transcript += Context->MessageHistory[i] + TEXT("\n");
        }
    }
    if (Transcript.IsEmpty())
    {
        Transcript = FString::Printf(TEXT("%s: %s\n"), *Info.ParticipantName, *IncomingText);
    }

    // The keyword router picks the intent prompt template; the model writes the reply.
    FAvatarPendingReplyInfo Routed = Info;
    Routed.Intent = DetectIntent(IncomingText);

    TArray<FEchoModelMessage> Messages;
    Messages.Emplace(TEXT("system"), BuildPersonaPrompt(Routed));
    Messages.Emplace(TEXT("user"), BuildReplyPrompt(Routed, IncomingText, Transcript));

    UEchoModelClient::Get()->Chat(Messages,
        FOnEchoModelReply::CreateUObject(this, &UAvatarCommunicationManager::HandleModelReply, Routed));
}

void UAvatarCommunicationManager::HandleModelReply(bool bSuccess, const FString& TextOrError, FAvatarPendingReplyInfo Info)
{
    if (Info.Source == EMessageSource::Dovecot)
    {
        bEmailRequestInFlight = false;
    }
    else
    {
        bDeltaChatRequestInFlight = false;
    }

    TSharedPtr<FJsonObject> Json;
    FString ReplyText;
    FString Error;
    if (!bSuccess)
    {
        Error = TextOrError;
    }
    else if (!UEchoModelClient::ParseJsonObject(TextOrError, Json)
        || !Json->TryGetStringField(TEXT("reply"), ReplyText) || ReplyText.TrimStartAndEnd().IsEmpty())
    {
        Error = TEXT("Echo model reply was not usable: no JSON object with a non-empty \"reply\" field");
    }

    if (!Error.IsEmpty())
    {
        // No reply is sent, nothing is recorded as the character's words.
        UE_LOG(LogCommunication, Error, TEXT("No reply to message %s: %s"), *Info.MessageId, *Error);
        OnReplyFailed.Broadcast(Info.MessageId, Error);
        return;
    }

    FCommunicationReply Reply;
    Reply.ResponseId = FGuid::NewGuid().ToString();
    Reply.Content = ReplyText.TrimStartAndEnd();
    Reply.Intent = Info.Intent;
    Json->TryGetStringField(TEXT("tone"), Reply.Tone);

    double Number = 0.0;
    const bool bHasSentiment = Json->TryGetNumberField(TEXT("sentiment"), Number);
    if (bHasSentiment)
    {
        Reply.IncomingSentiment = FMath::Clamp(static_cast<float>(Number), 0.0f, 1.0f);
    }
    if (Json->TryGetNumberField(TEXT("intensity"), Number))
    {
        Reply.Intensity = FMath::Clamp(static_cast<float>(Number), 0.0f, 1.0f);
    }

    FString ExpressionChoice;
    FString GestureChoice;
    Json->TryGetStringField(TEXT("expression"), ExpressionChoice);
    Json->TryGetStringField(TEXT("gesture"), GestureChoice);
    Reply.Expression = MatchVocabulary(ExpressionChoice, StaticEnum<EExpressionState>());
    Reply.Gesture = MatchVocabulary(GestureChoice, StaticEnum<EGestureType>());
    if (!ExpressionChoice.TrimStartAndEnd().IsEmpty() && Reply.Expression.IsEmpty())
    {
        UE_LOG(LogCommunication, Warning, TEXT("Model chose unknown expression '%s'; ignored"), *ExpressionChoice);
    }
    if (!GestureChoice.TrimStartAndEnd().IsEmpty() && Reply.Gesture.IsEmpty())
    {
        UE_LOG(LogCommunication, Warning, TEXT("Model chose unknown gesture '%s'; ignored"), *GestureChoice);
    }

    // Measured (not generated) state at reply time.
    if (NeurochemicalSystem)
    {
        const FEmotionalChemistry Emotions = NeurochemicalSystem->GetEmotionalChemistry();
        Reply.EmotionalValence = (Emotions.Happiness - Emotions.Anxiety) * 0.5f + 0.5f;
        Reply.EmotionalArousal = Emotions.Excitement;
    }

    // Emotional impact only from the model's own sentiment reading (none if it gave none).
    if (bHasSentiment)
    {
        if (FConversationContext* Context = ActiveConversations.Find(Info.ParticipantId))
        {
            Context->SentimentScore = Reply.IncomingSentiment;
        }
        ApplyModelSentiment(Reply.IncomingSentiment);
    }

    // The avatar's visible reaction: only model-chosen content goes on the bus.
    FEchoCharacterReaction Reaction;
    Reaction.CharacterName = CharacterName;
    Reaction.Utterance = Reply.Content;
    Reaction.Expression = Reply.Expression;
    Reaction.Gesture = Reply.Gesture;
    Reaction.Intensity = Reply.Intensity;
    FEchoReactionBus::OnReaction().Broadcast(Reaction);

    if (Info.Source == EMessageSource::Dovecot)
    {
        Reply.bDelivered = SendEmailResponse(Info.ReplyTarget, Reply);
        AppendHistory(Info.ParticipantId, FString::Printf(TEXT("%s: %s"), *CharacterName, *Reply.Content));
        RecordEmailExchange(Info.Email, Reply);
    }
    else
    {
        Reply.bDelivered = SendDeltaChatResponse(Info.ReplyTarget, Reply);
        AppendHistory(Info.ParticipantId, FString::Printf(TEXT("%s: %s"), *CharacterName, *Reply.Content));
        RecordChatExchange(Info.ChatMessage, Reply);
    }

    OnReplyGenerated.Broadcast(Info.MessageId, Reply);
}

bool UAvatarCommunicationManager::SendDeltaChatResponse(const FString& ChatId, const FCommunicationReply& Reply)
{
    LastTransportError = FString::Printf(
        TEXT("DeltaChat transport not implemented: reply %s to chat %s was NOT sent"), *Reply.ResponseId, *ChatId);
    UE_LOG(LogCommunication, Error, TEXT("%s"), *LastTransportError);
    return false;
}

bool UAvatarCommunicationManager::SendEmailResponse(const FString& EmailId, const FCommunicationReply& Reply)
{
    LastTransportError = FString::Printf(
        TEXT("SMTP transport not implemented: reply %s to email %s was NOT sent"), *Reply.ResponseId, *EmailId);
    UE_LOG(LogCommunication, Error, TEXT("%s"), *LastTransportError);
    return false;
}

// ===== Conversation Management =====

FConversationContext UAvatarCommunicationManager::GetOrCreateConversation(const FString& ParticipantId, EMessageSource Source)
{
    if (FConversationContext* Existing = ActiveConversations.Find(ParticipantId))
    {
        return *Existing;
    }

    FConversationContext NewContext;
    NewContext.ConversationId = FGuid::NewGuid().ToString();
    NewContext.ParticipantId = ParticipantId;
    NewContext.Source = Source;
    NewContext.State = EConversationState::Active;
    NewContext.StartedAt = FDateTime::Now();
    NewContext.LastActivityAt = FDateTime::Now();

    ActiveConversations.Add(ParticipantId, NewContext);
    return NewContext;
}

void UAvatarCommunicationManager::UpdateConversationState(const FString& ConversationId, EConversationState NewState)
{
    for (auto& Pair : ActiveConversations)
    {
        if (Pair.Value.ConversationId == ConversationId)
        {
            Pair.Value.State = NewState;
            OnConversationStateChanged.Broadcast(ConversationId, NewState);
            return;
        }
    }
}

FConversationContext UAvatarCommunicationManager::GetConversation(const FString& ConversationId)
{
    for (auto& Pair : ActiveConversations)
    {
        if (Pair.Value.ConversationId == ConversationId)
        {
            return Pair.Value;
        }
    }
    return FConversationContext();
}

TArray<FConversationContext> UAvatarCommunicationManager::GetActiveConversations()
{
    TArray<FConversationContext> Contexts;
    for (auto& Pair : ActiveConversations)
    {
        if (Pair.Value.State == EConversationState::Active)
        {
            Contexts.Add(Pair.Value);
        }
    }
    return Contexts;
}

void UAvatarCommunicationManager::EndConversation(const FString& ConversationId)
{
    UpdateConversationState(ConversationId, EConversationState::Ended);

    // Extract insights from conversation before ending
    FConversationContext Context = GetConversation(ConversationId);
    if (Context.MessageCount > 3)
    {
        RequestConversationInsight(Context);
    }
}

// ===== Personality-Driven Pacing =====

float UAvatarCommunicationManager::CalculateResponseDelay(const FConversationContext& Context)
{
    if (!PersonalitySystem)
    {
        return (MinResponseDelay + MaxResponseDelay) / 2.0f;
    }

    FHyperChaoticTrait HCTrait = PersonalitySystem->GetHyperChaoticTrait();
    FSuperHotGirlTrait SHGTrait = PersonalitySystem->GetSuperHotGirlTrait();

    // Higher impulsivity = faster response
    float ImpulsivityFactor = 1.0f - HCTrait.ImpulsivityLevel;

    // Higher confidence = faster response
    float ConfidenceFactor = 1.0f - (SHGTrait.Confidence * 0.3f);

    // Long conversation = faster response (more engaged)
    float EngagementFactor = FMath::Clamp(1.0f - (Context.MessageCount * 0.05f), 0.3f, 1.0f);

    float Delay = MinResponseDelay + (MaxResponseDelay - MinResponseDelay) * ImpulsivityFactor * ConfidenceFactor * EngagementFactor;

    return FMath::Clamp(Delay, MinResponseDelay, MaxResponseDelay);
}

// ===== Communication Log =====
// Exchanges are system records, kept apart from the character's diary (which holds only what
// the model writes as the character).

void UAvatarCommunicationManager::AddCommunicationLogRecord(FCommunicationLogRecord&& Record)
{
    UE_LOG(LogTemp, Log, TEXT("AvatarCommunicationManager: %s exchange with %s, reply %s"),
        *Record.Channel, *Record.Sender, Record.bDelivered ? TEXT("delivered") : TEXT("NOT delivered (no transport)"));
    CommunicationLog.Add(MoveTemp(Record));
    const int32 Excess = CommunicationLog.Num() - FMath::Max(1, MaxCommunicationLogRecords);
    if (Excess > 0)
    {
        CommunicationLog.RemoveAt(0, Excess);
    }
}

void UAvatarCommunicationManager::RecordChatExchange(const FDeltaChatMessage& Message, const FCommunicationReply& Reply)
{
    FCommunicationLogRecord Record;
    Record.Channel = TEXT("deltachat");
    Record.Sender = Message.SenderName;
    Record.IncomingContent = Message.Content;
    Record.ReplyContent = Reply.Content;
    Record.bDelivered = Reply.bDelivered;
    Record.Timestamp = FDateTime::Now();
    AddCommunicationLogRecord(MoveTemp(Record));
}

void UAvatarCommunicationManager::RecordEmailExchange(const FDovecotEmail& Email, const FCommunicationReply& Reply)
{
    FCommunicationLogRecord Record;
    Record.Channel = TEXT("email");
    Record.Sender = Email.From;
    Record.Subject = Email.Subject;
    Record.IncomingContent = Email.BodyText;
    Record.ReplyContent = Reply.Content;
    Record.bDelivered = Reply.bDelivered;
    Record.Timestamp = FDateTime::Now();
    AddCommunicationLogRecord(MoveTemp(Record));
}

void UAvatarCommunicationManager::RequestConversationInsight(const FConversationContext& Context)
{
    if (!AtomSpaceClient || Context.MessageHistory.Num() == 0)
    {
        return;
    }

    FString Transcript;
    const int32 First = FMath::Max(0, Context.MessageHistory.Num() - MaxHistoryLinesForModel * 2);
    for (int32 i = First; i < Context.MessageHistory.Num(); ++i)
    {
        Transcript += Context.MessageHistory[i] + TEXT("\n");
    }

    // Sentiment band (the model's own readings) selects the insight prompt template.
    const TCHAR* Band = Context.SentimentScore > 0.7f ? TEXT("positive")
        : Context.SentimentScore < 0.3f ? TEXT("challenging") : TEXT("neutral");
    const FString* Template = InsightPromptTemplates.Find(Band);
    if (!Template)
    {
        UE_LOG(LogCommunication, Error, TEXT("No insight prompt template '%s'; no insight requested"), Band);
        return;
    }
    TMap<FString, FString> Slots;
    Slots.Add(TEXT("participant"), Context.ParticipantName.IsEmpty() ? Context.ParticipantId : Context.ParticipantName);
    Slots.Add(TEXT("sentiment"), FString::Printf(TEXT("%.2f on 0..1"), Context.SentimentScore));
    Slots.Add(TEXT("message_count"), FString::FromInt(Context.MessageCount));
    Slots.Add(TEXT("transcript"), Transcript);

    TArray<FEchoModelMessage> Messages;
    Messages.Emplace(TEXT("system"), FString::Printf(
        TEXT("You are %s, an embodied AI avatar. %s\n")
        TEXT("Answer ONLY with a JSON object: {\"insight\": string, \"confidence\": number between 0 and 1}."),
        *CharacterName, *PersonaDescription));
    Messages.Emplace(TEXT("user"), UEchoModelClient::FillPrompt(*Template, Slots));

    UEchoModelClient::Get()->Chat(Messages,
        FOnEchoModelReply::CreateUObject(this, &UAvatarCommunicationManager::HandleInsightReply,
            Context.ConversationId, Context.ParticipantName, Context.MessageCount));
}

void UAvatarCommunicationManager::HandleInsightReply(bool bSuccess, const FString& TextOrError,
                                                      FString ConversationId, FString ParticipantName, int32 MessageCount)
{
    TSharedPtr<FJsonObject> Json;
    FString InsightText;
    if (!bSuccess || !UEchoModelClient::ParseJsonObject(TextOrError, Json)
        || !Json->TryGetStringField(TEXT("insight"), InsightText) || InsightText.TrimStartAndEnd().IsEmpty())
    {
        UE_LOG(LogCommunication, Error, TEXT("No insight stored for conversation %s: %s"), *ConversationId,
            bSuccess ? TEXT("model reply had no \"insight\" field") : *TextOrError);
        return;
    }

    if (!AtomSpaceClient)
    {
        return;
    }

    FInsight Insight;
    Insight.InsightId = FGuid::NewGuid().ToString();
    Insight.Category = TEXT("communication_pattern");
    Insight.Content = InsightText.TrimStartAndEnd();
    double Confidence = 0.0;
    Insight.Confidence = Json->TryGetNumberField(TEXT("confidence"), Confidence)
        ? FMath::Clamp(static_cast<float>(Confidence), 0.0f, 1.0f) : 0.0f;
    AtomSpaceClient->StoreInsight(Insight);

    UE_LOG(LogCommunication, Log, TEXT("Stored model insight on conversation with %s (%d messages)"),
        *ParticipantName, MessageCount);
}

// ===== Text Utilities =====

TArray<FString> UAvatarCommunicationManager::ExtractKeywords(const FString& Text)
{
    TArray<FString> Keywords;

    // Simple word extraction (would use NLP in full implementation)
    TArray<FString> Words;
    Text.ParseIntoArray(Words, TEXT(" "));

    // Filter short words and common words
    TArray<FString> StopWords = {
        TEXT("the"), TEXT("a"), TEXT("an"), TEXT("is"), TEXT("are"), TEXT("was"),
        TEXT("were"), TEXT("be"), TEXT("been"), TEXT("being"), TEXT("have"), TEXT("has"),
        TEXT("had"), TEXT("do"), TEXT("does"), TEXT("did"), TEXT("will"), TEXT("would"),
        TEXT("could"), TEXT("should"), TEXT("may"), TEXT("might"), TEXT("must"),
        TEXT("i"), TEXT("you"), TEXT("he"), TEXT("she"), TEXT("it"), TEXT("we"), TEXT("they")
    };

    for (const FString& Word : Words)
    {
        FString CleanWord = Word.ToLower().TrimStartAndEnd();
        if (CleanWord.Len() > 3 && !StopWords.Contains(CleanWord))
        {
            Keywords.AddUnique(CleanWord);
        }
    }

    return Keywords;
}

// ===== 9P Integration =====

void UAvatarCommunicationManager::Register9PNamespace()
{
    if (!P9Server)
    {
        return;
    }

    // Register communication namespace entries
    FAvatarNamespaceEntry CommDir;
    CommDir.Name = TEXT("communication");
    CommDir.FullPath = TEXT("/mnt/avatar/communication");
    CommDir.bIsDirectory = true;
    CommDir.bReadable = true;
    CommDir.Description = TEXT("Communication state and message handling");
    P9Server->RegisterNamespaceEntry(CommDir);

    // Messages inbox
    FAvatarNamespaceEntry InboxEntry;
    InboxEntry.Name = TEXT("inbox");
    InboxEntry.FullPath = TEXT("/mnt/avatar/communication/inbox");
    InboxEntry.bIsDirectory = false;
    InboxEntry.bReadable = true;
    InboxEntry.Description = TEXT("Pending messages");
    P9Server->RegisterNamespaceEntry(InboxEntry);

    // Send message
    FAvatarNamespaceEntry SendEntry;
    SendEntry.Name = TEXT("send");
    SendEntry.FullPath = TEXT("/mnt/avatar/communication/send");
    SendEntry.bIsDirectory = false;
    SendEntry.bReadable = false;
    SendEntry.bWritable = true;
    SendEntry.Description = TEXT("Send message (write JSON payload)");
    P9Server->RegisterNamespaceEntry(SendEntry);

    // Conversation list
    FAvatarNamespaceEntry ConversationsEntry;
    ConversationsEntry.Name = TEXT("conversations");
    ConversationsEntry.FullPath = TEXT("/mnt/avatar/communication/conversations");
    ConversationsEntry.bIsDirectory = false;
    ConversationsEntry.bReadable = true;
    ConversationsEntry.Description = TEXT("Active conversations");
    P9Server->RegisterNamespaceEntry(ConversationsEntry);
}

FString UAvatarCommunicationManager::Handle9PRead(const FString& Path)
{
    if (Path.Contains(TEXT("inbox")))
    {
        // Return pending messages as JSON
        TArray<FString> MessageStrings;
        for (const FDeltaChatMessage& Msg : PendingDeltaChatMessages)
        {
            MessageStrings.Add(FString::Printf(TEXT("{\"id\":\"%s\",\"from\":\"%s\",\"content\":\"%s\"}"),
                *Msg.MessageId, *Msg.SenderName, *Msg.Content.Left(100)));
        }
        return FString::Printf(TEXT("[%s]"), *FString::Join(MessageStrings, TEXT(",")));
    }
    else if (Path.Contains(TEXT("conversations")))
    {
        // Return active conversations
        TArray<FString> ConvStrings;
        for (const auto& Pair : ActiveConversations)
        {
            const FConversationContext& Ctx = Pair.Value;
            ConvStrings.Add(FString::Printf(TEXT("{\"id\":\"%s\",\"participant\":\"%s\",\"messages\":%d}"),
                *Ctx.ConversationId, *Ctx.ParticipantName, Ctx.MessageCount));
        }
        return FString::Printf(TEXT("[%s]"), *FString::Join(ConvStrings, TEXT(",")));
    }

    return TEXT("{}");
}

bool UAvatarCommunicationManager::Handle9PWrite(const FString& Path, const FString& Data)
{
    if (Path.Contains(TEXT("send")))
    {
        // No outbound transport exists; report the write as failed instead of pretending to send.
        LastTransportError = TEXT("9P send: no DeltaChat/SMTP transport implemented; message NOT sent");
        UE_LOG(LogCommunication, Error, TEXT("%s (payload: %s)"), *LastTransportError, *Data.Left(100));
        return false;
    }

    return false;
}

// ===== Internal Methods =====

void UAvatarCommunicationManager::ApplyModelSentiment(float IncomingSentiment)
{
    if (!NeurochemicalSystem)
    {
        return;
    }

    // The sentiment value is the model's reading of the incoming message.
    if (IncomingSentiment > 0.6f)
    {
        NeurochemicalSystem->TriggerSocialBonding(0.3f);
        NeurochemicalSystem->TriggerRewardResponse(0.2f);
    }
    else if (IncomingSentiment < 0.4f)
    {
        NeurochemicalSystem->TriggerStressResponse(0.2f);
    }
}
