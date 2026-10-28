// LanguageSystem.cpp
// Implementation of Natural Language Processing and Generation

#include "LanguageSystem.h"
#include "../Core/CognitiveCycleManager.h"
#include "../Memory/MemorySystems.h"
#include "../Emotion/EmotionalSystem.h"
#include "../Model/EchoModelClient.h"
#include "../Model/EchoCharacterReaction.h"
#include "Math/UnrealMathUtility.h"

ULanguageSystem::ULanguageSystem()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void ULanguageSystem::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
    InitializeLanguageSystem();
}

void ULanguageSystem::TickComponent(float DeltaTime, ELevelTick TickType, 
                                     FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    UpdateDialogueState(DeltaTime);
}

void ULanguageSystem::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        CycleManager = Owner->FindComponentByClass<UCognitiveCycleManager>();
        MemoryComponent = Owner->FindComponentByClass<UMemorySystems>();
        EmotionalComponent = Owner->FindComponentByClass<UEmotionalSystem>();
    }
}

void ULanguageSystem::InitializeLanguageSystem()
{
    DialogueContext.ConversationID = TEXT("");
    DialogueContext.State = EDialogueState::Idle;
    DialogueContext.TurnHistory.Empty();
}

// ========================================
// PARSING
// ========================================

FParsedUtterance ULanguageSystem::ParseUtterance(const FString& Text)
{
    FParsedUtterance Parsed;
    Parsed.UtteranceID = GenerateUtteranceID();
    Parsed.RawText = Text;
    Parsed.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    // Identify speech act
    Parsed.SpeechAct = IdentifySpeechAct(Text);

    // Extract entities
    Parsed.Entities = ExtractEntities(Text);

    // Extract keywords
    Parsed.Keywords = ExtractKeywords(Text);

    // Analyze sentiment
    Parsed.Sentiment = AnalyzeSentiment(Text);

    // Set confidence based on parsing success
    Parsed.Confidence = 0.7f;
    if (Parsed.Keywords.Num() > 0) Parsed.Confidence += 0.1f;
    if (Parsed.Entities.Num() > 0) Parsed.Confidence += 0.1f;
    if (Parsed.SpeechAct != ESpeechAct::Assertion) Parsed.Confidence += 0.1f;

    OnUtteranceParsed.Broadcast(Parsed);

    return Parsed;
}

ESpeechAct ULanguageSystem::IdentifySpeechAct(const FString& Text) const
{
    FString LowerText = Text.ToLower();

    if (IsGreeting(LowerText))
    {
        return ESpeechAct::Greeting;
    }

    if (LowerText.Contains(TEXT("goodbye")) || LowerText.Contains(TEXT("bye")) || 
        LowerText.Contains(TEXT("see you")))
    {
        return ESpeechAct::Farewell;
    }

    if (IsQuestion(Text))
    {
        return ESpeechAct::Question;
    }

    if (IsCommand(LowerText))
    {
        return ESpeechAct::Command;
    }

    if (LowerText.Contains(TEXT("please")) || LowerText.Contains(TEXT("could you")) ||
        LowerText.Contains(TEXT("would you")))
    {
        return ESpeechAct::Request;
    }

    if (LowerText.Contains(TEXT("i promise")) || LowerText.Contains(TEXT("i will")))
    {
        return ESpeechAct::Promise;
    }

    if (LowerText.Contains(TEXT("thank")) || LowerText.Contains(TEXT("okay")) ||
        LowerText.Contains(TEXT("yes")) || LowerText.Contains(TEXT("no")))
    {
        return ESpeechAct::Acknowledgment;
    }

    return ESpeechAct::Assertion;
}

TArray<FString> ULanguageSystem::ExtractEntities(const FString& Text) const
{
    TArray<FString> Entities;

    // Simple entity extraction - words starting with capital letters
    TArray<FString> Words;
    Text.ParseIntoArray(Words, TEXT(" "), true);

    for (const FString& Word : Words)
    {
        if (Word.Len() > 1)
        {
            FString CleanWord = Word;
            CleanWord.RemoveFromEnd(TEXT("."));
            CleanWord.RemoveFromEnd(TEXT(","));
            CleanWord.RemoveFromEnd(TEXT("!"));
            CleanWord.RemoveFromEnd(TEXT("?"));

            if (CleanWord.Len() > 0 && FChar::IsUpper(CleanWord[0]))
            {
                // Check if it's not a sentence start
                if (!Entities.Contains(CleanWord))
                {
                    Entities.Add(CleanWord);
                }
            }
        }
    }

    return Entities;
}

TArray<FString> ULanguageSystem::ExtractKeywords(const FString& Text) const
{
    TArray<FString> Keywords;

    // Common stop words to filter
    TArray<FString> StopWords = {
        TEXT("the"), TEXT("a"), TEXT("an"), TEXT("is"), TEXT("are"), TEXT("was"), TEXT("were"),
        TEXT("be"), TEXT("been"), TEXT("being"), TEXT("have"), TEXT("has"), TEXT("had"),
        TEXT("do"), TEXT("does"), TEXT("did"), TEXT("will"), TEXT("would"), TEXT("could"),
        TEXT("should"), TEXT("may"), TEXT("might"), TEXT("must"), TEXT("shall"),
        TEXT("i"), TEXT("you"), TEXT("he"), TEXT("she"), TEXT("it"), TEXT("we"), TEXT("they"),
        TEXT("this"), TEXT("that"), TEXT("these"), TEXT("those"),
        TEXT("and"), TEXT("or"), TEXT("but"), TEXT("if"), TEXT("then"), TEXT("else"),
        TEXT("to"), TEXT("of"), TEXT("in"), TEXT("on"), TEXT("at"), TEXT("by"), TEXT("for"),
        TEXT("with"), TEXT("about"), TEXT("into"), TEXT("through"), TEXT("during"),
        TEXT("what"), TEXT("which"), TEXT("who"), TEXT("whom"), TEXT("whose"),
        TEXT("where"), TEXT("when"), TEXT("why"), TEXT("how")
    };

    TArray<FString> Words;
    Text.ToLower().ParseIntoArray(Words, TEXT(" "), true);

    for (FString Word : Words)
    {
        Word.RemoveFromEnd(TEXT("."));
        Word.RemoveFromEnd(TEXT(","));
        Word.RemoveFromEnd(TEXT("!"));
        Word.RemoveFromEnd(TEXT("?"));

        if (Word.Len() > 2 && !StopWords.Contains(Word) && !Keywords.Contains(Word))
        {
            Keywords.Add(Word);
        }
    }

    return Keywords;
}

float ULanguageSystem::AnalyzeSentiment(const FString& Text) const
{
    FString LowerText = Text.ToLower();
    float Sentiment = 0.0f;

    // Positive words
    TArray<FString> PositiveWords = {
        TEXT("good"), TEXT("great"), TEXT("excellent"), TEXT("wonderful"), TEXT("amazing"),
        TEXT("happy"), TEXT("joy"), TEXT("love"), TEXT("like"), TEXT("best"),
        TEXT("beautiful"), TEXT("nice"), TEXT("awesome"), TEXT("fantastic"), TEXT("perfect"),
        TEXT("thank"), TEXT("thanks"), TEXT("please"), TEXT("appreciate"), TEXT("enjoy")
    };

    // Negative words
    TArray<FString> NegativeWords = {
        TEXT("bad"), TEXT("terrible"), TEXT("awful"), TEXT("horrible"), TEXT("worst"),
        TEXT("sad"), TEXT("angry"), TEXT("hate"), TEXT("dislike"), TEXT("ugly"),
        TEXT("stupid"), TEXT("dumb"), TEXT("wrong"), TEXT("fail"), TEXT("failed"),
        TEXT("never"), TEXT("nothing"), TEXT("nobody"), TEXT("nowhere"), TEXT("problem")
    };

    for (const FString& Word : PositiveWords)
    {
        if (LowerText.Contains(Word))
        {
            Sentiment += 0.2f;
        }
    }

    for (const FString& Word : NegativeWords)
    {
        if (LowerText.Contains(Word))
        {
            Sentiment -= 0.2f;
        }
    }

    // Check for negation
    if (LowerText.Contains(TEXT("not ")) || LowerText.Contains(TEXT("n't")))
    {
        Sentiment *= -0.5f;
    }

    return FMath::Clamp(Sentiment, -1.0f, 1.0f);
}

// ========================================
// DIALOGUE MANAGEMENT
// ========================================

void ULanguageSystem::StartConversation(const FString& InitialTopic)
{
    DialogueContext.ConversationID = GenerateConversationID();
    DialogueContext.TurnHistory.Empty();
    DialogueContext.CurrentTopic = InitialTopic;
    DialogueContext.MentionedEntities.Empty();
    DialogueContext.SharedKnowledge.Empty();
    DialogueContext.ConversationStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    SetDialogueState(EDialogueState::Listening);
}

FString ULanguageSystem::ProcessUtterance(const FString& Speaker, const FString& Text)
{
    SetDialogueState(EDialogueState::Processing);

    // Parse the utterance (heuristic metadata used only as model input)
    FParsedUtterance Parsed = ParseUtterance(Text);

    FDialogueTurn Turn;
    Turn.TurnID = GenerateTurnID();
    Turn.Speaker = Speaker;
    Turn.Utterance = Text;
    Turn.ParsedContent = Parsed;
    Turn.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    Turn.bIsAgentTurn = false;

    AddTurnToHistory(Turn);

    for (const FString& Entity : Parsed.Entities)
    {
        DialogueContext.MentionedEntities.AddUnique(Entity);
    }

    // The agent's turn is added to the history when (and only if) the model replies.
    return RequestResponse(Parsed);
}

FDialogueContext ULanguageSystem::GetDialogueContext() const
{
    return DialogueContext;
}

EDialogueState ULanguageSystem::GetDialogueState() const
{
    return DialogueContext.State;
}

void ULanguageSystem::SetDialogueState(EDialogueState NewState)
{
    if (DialogueContext.State != NewState)
    {
        EDialogueState OldState = DialogueContext.State;
        DialogueContext.State = NewState;
        OnDialogueStateChanged.Broadcast(OldState, NewState);
    }
}

FString ULanguageSystem::GetCurrentTopic() const
{
    return DialogueContext.CurrentTopic;
}

void ULanguageSystem::SetCurrentTopic(const FString& Topic)
{
    if (DialogueContext.CurrentTopic != Topic)
    {
        DialogueContext.CurrentTopic = Topic;
        OnTopicChanged.Broadcast(Topic);
    }
}

void ULanguageSystem::EndConversation()
{
    SetDialogueState(EDialogueState::Idle);
    DialogueContext.ConversationID = TEXT("");
}

TArray<FDialogueTurn> ULanguageSystem::GetTurnHistory() const
{
    return DialogueContext.TurnHistory;
}

// ========================================
// RESPONSE GENERATION (model-backed)
// ========================================

FString ULanguageSystem::RequestResponse(const FParsedUtterance& Input)
{
    return RequestEmotionalResponse(Input, GetCurrentEmotionalTone());
}

FString ULanguageSystem::RequestEmotionalResponse(const FParsedUtterance& Input, float EmotionalTone)
{
    const UEnum* ActEnum = StaticEnum<ESpeechAct>();
    const FString Instruction = FString::Printf(
        TEXT("Reply to the latest utterance: \"%s\".\n")
        TEXT("Heuristic parse of that utterance (may be wrong): speech act %s, sentiment %.2f (-1..1), keywords [%s], entities [%s]."),
        *Input.RawText,
        ActEnum ? *ActEnum->GetNameStringByValue(static_cast<int64>(Input.SpeechAct)) : TEXT("unknown"),
        Input.Sentiment,
        *FString::Join(Input.Keywords, TEXT(", ")),
        *FString::Join(Input.Entities, TEXT(", ")));
    return SendResponseRequest(Instruction, ESpeechAct::Assertion, EmotionalTone, EDialogueState::Waiting);
}

FString ULanguageSystem::RequestClarification(const FString& UnclearAspect)
{
    const FString Instruction = FString::Printf(
        TEXT("Something in the conversation is unclear to you: \"%s\". Ask the other person to clarify it."),
        *UnclearAspect);
    return SendResponseRequest(Instruction, ESpeechAct::Question, GetCurrentEmotionalTone(), EDialogueState::Clarifying);
}

FString ULanguageSystem::RequestAcknowledgment()
{
    return SendResponseRequest(TEXT("Briefly acknowledge what the other person just said."),
        ESpeechAct::Acknowledgment, GetCurrentEmotionalTone(), EDialogueState::Waiting);
}

FString ULanguageSystem::RequestGreeting(const FString& RecipientName)
{
    const FString Instruction = RecipientName.IsEmpty()
        ? FString(TEXT("Greet the person you are talking to."))
        : FString::Printf(TEXT("Greet %s."), *RecipientName);
    return SendResponseRequest(Instruction, ESpeechAct::Greeting, GetCurrentEmotionalTone(), EDialogueState::Waiting);
}

FString ULanguageSystem::RequestFarewell()
{
    return SendResponseRequest(TEXT("Say goodbye to the person you are talking to."),
        ESpeechAct::Farewell, GetCurrentEmotionalTone(), EDialogueState::Waiting);
}

// ========================================
// NARRATIVE GENERATION (model-backed)
// ========================================

FString ULanguageSystem::RequestNarrative(const FString& Subject, const TArray<FString>& Events)
{
    FString EventList;
    for (int32 i = 0; i < Events.Num(); ++i)
    {
        EventList += FString::Printf(TEXT("%d. %s\n"), i + 1, *Events[i]);
    }
    const FString Instruction = FString::Printf(
        TEXT("Narrate, in a short paragraph, what %s experienced. The events, in order:\n%s"),
        *Subject, *EventList);

    TArray<FString> Characters;
    Characters.Add(Subject);
    return SendNarrativeRequest(Instruction, TEXT("Description"), Characters);
}

FString ULanguageSystem::RequestSelfNarrative()
{
    TArray<FString> Characters;
    Characters.Add(AgentName);
    return SendNarrativeRequest(
        TEXT("Describe yourself and how you are feeling right now, in the first person, in a short paragraph."),
        TEXT("Self-Description"), Characters);
}

FString ULanguageSystem::RequestExperienceSummary(float TimeSpan)
{
    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    FString Turns;
    int32 TurnCount = 0;
    for (const FDialogueTurn& Turn : DialogueContext.TurnHistory)
    {
        if (Now - Turn.Timestamp <= TimeSpan)
        {
            Turns += FString::Printf(TEXT("%s: %s\n"), *Turn.Speaker, *Turn.Utterance);
            ++TurnCount;
        }
    }

    const FString Instruction = FString::Printf(
        TEXT("Summarize, in the first person and in a short paragraph, your experience over the last %.0f seconds. ")
        TEXT("Dialogue turns in that period (%d):\n%s\nEntities mentioned in this conversation: [%s]."),
        TimeSpan, TurnCount, TurnCount > 0 ? *Turns : TEXT("(none)\n"),
        *FString::Join(DialogueContext.MentionedEntities, TEXT(", ")));

    TArray<FString> Characters;
    Characters.Add(AgentName);
    return SendNarrativeRequest(Instruction, TEXT("Experience Summary"), Characters);
}

// ========================================
// MODEL PLUMBING
// ========================================

float ULanguageSystem::GetCurrentEmotionalTone() const
{
    return EmotionalComponent ? EmotionalComponent->GetValence() : 0.0f;
}

FString ULanguageSystem::BuildSystemPrompt() const
{
    // Persona and state are model INPUT only.
    FString Prompt = FString::Printf(TEXT("You are %s, an embodied cognitive agent."), *AgentName);
    if (!PersonaDescription.IsEmpty())
    {
        Prompt += TEXT(" ") + PersonaDescription;
    }
    if (EmotionalComponent)
    {
        Prompt += FString::Printf(TEXT("\nYour current emotional valence is %.2f on a -1..1 scale."),
            EmotionalComponent->GetValence());
    }
    if (!DialogueContext.CurrentTopic.IsEmpty())
    {
        Prompt += FString::Printf(TEXT("\nThe current conversation topic is: %s."), *DialogueContext.CurrentTopic);
    }
    Prompt += TEXT("\nAnswer with only the words you say, without quotation marks or stage directions.");
    return Prompt;
}

FString ULanguageSystem::BuildRecentTranscript(int32 MaxTurns) const
{
    FString Transcript;
    const int32 First = FMath::Max(0, DialogueContext.TurnHistory.Num() - MaxTurns);
    for (int32 i = First; i < DialogueContext.TurnHistory.Num(); ++i)
    {
        const FDialogueTurn& Turn = DialogueContext.TurnHistory[i];
        Transcript += FString::Printf(TEXT("%s: %s\n"), *Turn.Speaker, *Turn.Utterance);
    }
    return Transcript;
}

FString ULanguageSystem::SendResponseRequest(const FString& Instruction, ESpeechAct SpeechAct, float EmotionalTone,
                                             EDialogueState StateOnSuccess)
{
    const FString RequestID = GenerateResponseID();

    const FString Transcript = BuildRecentTranscript(MaxTurnHistory);
    TArray<FEchoModelMessage> Messages;
    Messages.Add(FEchoModelMessage(TEXT("system"), BuildSystemPrompt()));
    Messages.Add(FEchoModelMessage(TEXT("user"), FString::Printf(
        TEXT("Conversation so far:\n%s\nSpeak with an emotional tone of %.2f (-1 negative .. 1 positive).\n%s"),
        Transcript.IsEmpty() ? TEXT("(no turns yet)\n") : *Transcript, EmotionalTone, *Instruction)));

    SetDialogueState(EDialogueState::Processing);
    UEchoModelClient::Get()->Chat(Messages, FOnEchoModelReply::CreateUObject(
        this, &ULanguageSystem::HandleResponseReply, RequestID, SpeechAct, EmotionalTone, StateOnSuccess));
    return RequestID;
}

void ULanguageSystem::HandleResponseReply(bool bSuccess, const FString& TextOrError, FString RequestID,
                                          ESpeechAct SpeechAct, float EmotionalTone, EDialogueState StateOnSuccess)
{
    if (!bSuccess)
    {
        // No agent turn, no text: report the failure as a system event.
        UE_LOG(LogTemp, Error, TEXT("LanguageSystem: request %s produced no reply: %s"), *RequestID, *TextOrError);
        SetDialogueState(EDialogueState::Listening);
        OnGenerationFailed.Broadcast(RequestID, TextOrError);
        return;
    }

    FGeneratedResponse Response;
    Response.ResponseID = RequestID;
    Response.Text = TextOrError.TrimStartAndEnd();
    Response.SpeechAct = SpeechAct;
    Response.EmotionalTone = EmotionalTone;

    FDialogueTurn AgentTurn;
    AgentTurn.TurnID = GenerateTurnID();
    AgentTurn.Speaker = AgentName;
    AgentTurn.Utterance = Response.Text;
    AgentTurn.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    AgentTurn.bIsAgentTurn = true;
    AddTurnToHistory(AgentTurn);

    SetDialogueState(StateOnSuccess);

    // The agent speaks the model's words; no expression/gesture is chosen here.
    FEchoCharacterReaction Reaction;
    Reaction.CharacterName = AgentName;
    Reaction.Utterance = Response.Text;
    FEchoReactionBus::OnReaction().Broadcast(Reaction);

    OnResponseGenerated.Broadcast(Response);
}

FString ULanguageSystem::SendNarrativeRequest(const FString& Instruction, const FString& NarrativeType,
                                              const TArray<FString>& Characters)
{
    const FString RequestID = GenerateNarrativeID();

    TArray<FEchoModelMessage> Messages;
    Messages.Add(FEchoModelMessage(TEXT("system"), BuildSystemPrompt()));
    Messages.Add(FEchoModelMessage(TEXT("user"), Instruction));

    UEchoModelClient::Get()->Chat(Messages, FOnEchoModelReply::CreateUObject(
        this, &ULanguageSystem::HandleNarrativeReply, RequestID, NarrativeType, Characters, GetCurrentEmotionalTone()));
    return RequestID;
}

void ULanguageSystem::HandleNarrativeReply(bool bSuccess, const FString& TextOrError, FString RequestID,
                                           FString NarrativeType, TArray<FString> Characters, float EmotionalArc)
{
    if (!bSuccess)
    {
        UE_LOG(LogTemp, Error, TEXT("LanguageSystem: narrative request %s produced no text: %s"), *RequestID, *TextOrError);
        OnGenerationFailed.Broadcast(RequestID, TextOrError);
        return;
    }

    FNarrativeElement Narrative;
    Narrative.ElementID = RequestID;
    Narrative.NarrativeType = NarrativeType;
    Narrative.Characters = Characters;
    Narrative.Content = TextOrError.TrimStartAndEnd();
    Narrative.EmotionalArc = EmotionalArc;

    OnNarrativeGenerated.Broadcast(Narrative);
}

// ========================================
// INTERNAL METHODS
// ========================================

void ULanguageSystem::UpdateDialogueState(float DeltaTime)
{
    // Timeout handling
    if (DialogueContext.State == EDialogueState::Waiting)
    {
        float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        if (DialogueContext.TurnHistory.Num() > 0)
        {
            float LastTurnTime = DialogueContext.TurnHistory.Last().Timestamp;
            if (CurrentTime - LastTurnTime > ResponseTimeout)
            {
                SetDialogueState(EDialogueState::Listening);
            }
        }
    }
}

void ULanguageSystem::AddTurnToHistory(const FDialogueTurn& Turn)
{
    DialogueContext.TurnHistory.Add(Turn);

    // Limit history
    while (DialogueContext.TurnHistory.Num() > MaxTurnHistory)
    {
        DialogueContext.TurnHistory.RemoveAt(0);
    }
}

bool ULanguageSystem::IsQuestion(const FString& Text) const
{
    if (Text.EndsWith(TEXT("?")))
    {
        return true;
    }

    FString LowerText = Text.ToLower();
    return LowerText.StartsWith(TEXT("what")) || LowerText.StartsWith(TEXT("who")) ||
           LowerText.StartsWith(TEXT("where")) || LowerText.StartsWith(TEXT("when")) ||
           LowerText.StartsWith(TEXT("why")) || LowerText.StartsWith(TEXT("how")) ||
           LowerText.StartsWith(TEXT("is ")) || LowerText.StartsWith(TEXT("are ")) ||
           LowerText.StartsWith(TEXT("do ")) || LowerText.StartsWith(TEXT("does ")) ||
           LowerText.StartsWith(TEXT("can ")) || LowerText.StartsWith(TEXT("could "));
}

bool ULanguageSystem::IsCommand(const FString& Text) const
{
    FString LowerText = Text.ToLower();
    return LowerText.StartsWith(TEXT("do ")) || LowerText.StartsWith(TEXT("make ")) ||
           LowerText.StartsWith(TEXT("create ")) || LowerText.StartsWith(TEXT("show ")) ||
           LowerText.StartsWith(TEXT("tell ")) || LowerText.StartsWith(TEXT("give ")) ||
           LowerText.StartsWith(TEXT("find ")) || LowerText.StartsWith(TEXT("get ")) ||
           LowerText.StartsWith(TEXT("go ")) || LowerText.StartsWith(TEXT("stop "));
}

bool ULanguageSystem::IsGreeting(const FString& Text) const
{
    FString LowerText = Text.ToLower();
    return LowerText.Contains(TEXT("hello")) || LowerText.Contains(TEXT("hi ")) ||
           LowerText.StartsWith(TEXT("hi")) || LowerText.Contains(TEXT("hey")) ||
           LowerText.Contains(TEXT("greetings")) || LowerText.Contains(TEXT("good morning")) ||
           LowerText.Contains(TEXT("good afternoon")) || LowerText.Contains(TEXT("good evening"));
}

FString ULanguageSystem::GenerateUtteranceID()
{
    return FString::Printf(TEXT("UTT_%d_%d"), ++UtteranceIDCounter, FMath::RandRange(1000, 9999));
}

FString ULanguageSystem::GenerateTurnID()
{
    return FString::Printf(TEXT("TURN_%d_%d"), ++TurnIDCounter, FMath::RandRange(1000, 9999));
}

FString ULanguageSystem::GenerateResponseID()
{
    return FString::Printf(TEXT("RESP_%d_%d"), ++ResponseIDCounter, FMath::RandRange(1000, 9999));
}

FString ULanguageSystem::GenerateNarrativeID()
{
    return FString::Printf(TEXT("NAR_%d_%d"), ++NarrativeIDCounter, FMath::RandRange(1000, 9999));
}

FString ULanguageSystem::GenerateConversationID()
{
    return FString::Printf(TEXT("CONV_%d_%d"), ++ConversationIDCounter, FMath::RandRange(1000, 9999));
}
