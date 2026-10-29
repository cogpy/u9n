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

    if (ResponseTemplates.Num() == 0)
    {
        InitializeResponseTemplates();
    }
    if (NarrativePromptTemplates.Num() == 0)
    {
        InitializeNarrativeTemplates();
    }
    if (ResponseStylePromptTemplate.IsEmpty())
    {
        ResponseStylePromptTemplate =
            TEXT("Speak in a {tone} tone (emotional tone {tone_value} on a -1..1 scale), with {formality} formality.");
    }
}

// ========================================
// PROMPT TEMPLATES
// ========================================
//
// Each template is an instruction to the model. Slots filled for every response template:
//   {agent} agent name, {speaker} the other person, {utterance} their latest words,
//   {topic} current topic, {keywords} / {entities} from the parse of the latest utterance.
// Template-specific slots: {recipient} (greeting/farewell), {question} (answer),
//   {unclear} (clarification).

void ULanguageSystem::InitializeResponseTemplates()
{
    auto Add = [this](const TCHAR* ID, ESpeechAct ResponseType, TArray<ESpeechAct> InputActs,
                      float MinSentiment, float MaxSentiment, float Tone, float Formality,
                      const TCHAR* Prompt, TArray<FString> Required, const TCHAR* StyleExample)
    {
        FResponseTemplate T;
        T.TemplateID = ID;
        T.ResponseType = ResponseType;
        T.InputActs = MoveTemp(InputActs);
        T.MinInputSentiment = MinSentiment;
        T.MaxInputSentiment = MaxSentiment;
        T.EmotionalTone = Tone;
        T.Formality = Formality;
        T.PromptTemplate = Prompt;
        T.RequiredSlots = MoveTemp(Required);
        T.StyleExample = StyleExample;
        ResponseTemplates.Add(T);
    };

    // Greeting
    Add(TEXT("GREET"), ESpeechAct::Greeting, {ESpeechAct::Greeting}, -1.f, 1.f, 0.3f, 0.5f,
        TEXT("Greet {recipient}. Open the conversation warmly and invite them to say what is on their mind. One or two short sentences."),
        {TEXT("recipient")}, TEXT("Hello! How can I help you today?"));

    // Acknowledgment
    Add(TEXT("ACK"), ESpeechAct::Acknowledgment, {ESpeechAct::Acknowledgment}, -1.f, 1.f, 0.1f, 0.5f,
        TEXT("Briefly acknowledge what {speaker} just said (\"{utterance}\") so they know you understood. One short sentence."),
        {}, TEXT("I understand."));

    // Answer to a question (the old "{answer}" slot: the model now writes the answer itself)
    Add(TEXT("ANSWER"), ESpeechAct::Assertion, {ESpeechAct::Question}, -1.f, 1.f, 0.0f, 0.6f,
        TEXT("{speaker} asked: \"{question}\". Answer it from your own understanding (key terms: {keywords}). ")
        TEXT("If you do not know, say so honestly and say what you would need to find out."),
        {TEXT("question")}, TEXT("Based on my understanding, ..."));

    // Command / request
    Add(TEXT("HELP"), ESpeechAct::Acknowledgment, {ESpeechAct::Command, ESpeechAct::Request}, -1.f, 1.f, 0.2f, 0.5f,
        TEXT("{speaker} asked you to do something: \"{utterance}\". Respond to the request: say how you will help, ")
        TEXT("or what you need to know first. Do not claim to have already done anything."),
        {}, TEXT(""));

    // Statements, routed by sentiment
    const TArray<ESpeechAct> Statements = {ESpeechAct::Assertion, ESpeechAct::Promise, ESpeechAct::Expression, ESpeechAct::Declaration};
    Add(TEXT("SHARE_POSITIVE"), ESpeechAct::Assertion, Statements, 0.3f, 1.f, 0.5f, 0.4f,
        TEXT("{speaker} shared something positive: \"{utterance}\". Respond with genuine appreciation for what they shared."),
        {}, TEXT(""));
    Add(TEXT("SHARE_DIFFICULT"), ESpeechAct::Assertion, Statements, -1.f, -0.3f, -0.3f, 0.5f,
        TEXT("{speaker} shared something difficult: \"{utterance}\". Respond with empathy and ask how you can help."),
        {}, TEXT(""));
    Add(TEXT("SHARE_NEUTRAL"), ESpeechAct::Assertion, Statements, -0.3f, 0.3f, 0.0f, 0.5f,
        TEXT("{speaker} said: \"{utterance}\". Respond to it and invite them to tell you more."),
        {}, TEXT(""));

    // Clarification (requested explicitly)
    Add(TEXT("CLARIFY"), ESpeechAct::Question, {}, -1.f, 1.f, 0.0f, 0.6f,
        TEXT("Something in the conversation is unclear to you: \"{unclear}\". Ask {speaker} to clarify what they mean by it. One question."),
        {TEXT("unclear")}, TEXT("Could you please clarify what you mean by ...?"));

    // Farewell
    Add(TEXT("FAREWELL"), ESpeechAct::Farewell, {ESpeechAct::Farewell}, -1.f, 1.f, 0.2f, 0.5f,
        TEXT("Say goodbye to {recipient}, closing the conversation warmly (you may refer to what you talked about). One or two short sentences."),
        {TEXT("recipient")}, TEXT("Goodbye! It was nice talking with you."));

    // Anything else
    Add(TEXT("GENERAL"), ESpeechAct::Assertion, {}, -1.f, 1.f, 0.0f, 0.5f,
        TEXT("Reply to the latest thing {speaker} said: \"{utterance}\"."),
        {}, TEXT(""));
}

void ULanguageSystem::InitializeNarrativeTemplates()
{
    NarrativePromptTemplates.Add(TEXT("Description"),
        TEXT("Narrate, in a short paragraph in the third person, what {subject} experienced. ")
        TEXT("Tell these events, in this order, as one connected story:\n{events}"));
    NarrativePromptTemplates.Add(TEXT("Self-Description"),
        TEXT("Describe yourself in the first person, in a short paragraph: who you are ({agent}, an embodied cognitive agent), ")
        TEXT("how you are feeling right now ({emotional_state}; valence {valence} on -1..1), and what you hope for in your interactions."));
    NarrativePromptTemplates.Add(TEXT("Experience Summary"),
        TEXT("Summarize, in the first person and in a short paragraph, your experience over the last {timespan} seconds. ")
        TEXT("Dialogue turns in that period ({turn_count}):\n{turns}\nEntities mentioned in this conversation: {entities}. ")
        TEXT("If there were no turns, describe what you were observing and processing instead."));
}

const FResponseTemplate* ULanguageSystem::FindResponseTemplate(const FString& TemplateID) const
{
    return ResponseTemplates.FindByPredicate([&TemplateID](const FResponseTemplate& T) { return T.TemplateID == TemplateID; });
}

FResponseTemplate ULanguageSystem::SelectResponseTemplate(const FParsedUtterance& Input, float EmotionalTone) const
{
    const FResponseTemplate* Best = nullptr;
    for (const FResponseTemplate& T : ResponseTemplates)
    {
        if (T.InputActs.Contains(Input.SpeechAct)
            && Input.Sentiment >= T.MinInputSentiment && Input.Sentiment <= T.MaxInputSentiment
            && (!Best || FMath::Abs(T.EmotionalTone - EmotionalTone) < FMath::Abs(Best->EmotionalTone - EmotionalTone)))
        {
            Best = &T;
        }
    }
    if (!Best)
    {
        Best = FindResponseTemplate(TEXT("GENERAL"));
    }
    return Best ? *Best : FResponseTemplate();
}

void ULanguageSystem::AddDialogueSlots(TMap<FString, FString>& Slots) const
{
    const FDialogueTurn* LastOther = nullptr;
    for (int32 i = DialogueContext.TurnHistory.Num() - 1; i >= 0; --i)
    {
        if (!DialogueContext.TurnHistory[i].bIsAgentTurn)
        {
            LastOther = &DialogueContext.TurnHistory[i];
            break;
        }
    }
    const FString Speaker = LastOther && !LastOther->Speaker.IsEmpty() ? LastOther->Speaker : FString(TEXT("the person you are talking to"));
    Slots.FindOrAdd(TEXT("agent"), AgentName);
    Slots.FindOrAdd(TEXT("speaker"), Speaker);
    Slots.FindOrAdd(TEXT("recipient"), Speaker);
    Slots.FindOrAdd(TEXT("utterance"), LastOther ? LastOther->Utterance : FString(TEXT("(nothing yet)")));
    Slots.FindOrAdd(TEXT("topic"), DialogueContext.CurrentTopic.IsEmpty() ? FString(TEXT("(none)")) : DialogueContext.CurrentTopic);
    if (LastOther)
    {
        Slots.FindOrAdd(TEXT("keywords"), LastOther->ParsedContent.Keywords.Num() ? FString::Join(LastOther->ParsedContent.Keywords, TEXT(", ")) : FString(TEXT("(none)")));
        Slots.FindOrAdd(TEXT("entities"), LastOther->ParsedContent.Entities.Num() ? FString::Join(LastOther->ParsedContent.Entities, TEXT(", ")) : FString(TEXT("(none)")));
    }
    else
    {
        Slots.FindOrAdd(TEXT("keywords"), TEXT("(none)"));
        Slots.FindOrAdd(TEXT("entities"), TEXT("(none)"));
    }
}

FString ULanguageSystem::DescribeTone(float EmotionalTone)
{
    if (EmotionalTone > 0.5f) return TEXT("enthusiastic, warm");
    if (EmotionalTone > 0.15f) return TEXT("friendly");
    if (EmotionalTone < -0.5f) return TEXT("gentle, empathetic");
    if (EmotionalTone < -0.15f) return TEXT("subdued, careful");
    return TEXT("calm, attentive");
}

FString ULanguageSystem::DescribeFormality(float Formality)
{
    if (Formality >= 0.7f) return TEXT("high");
    if (Formality <= 0.3f) return TEXT("low (casual)");
    return TEXT("moderate");
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

    // Parse the utterance: the parse routes it to a prompt template and fills its slots
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
// RESPONSE GENERATION (prompt templates -> model)
// ========================================

FString ULanguageSystem::RequestResponse(const FParsedUtterance& Input)
{
    return RequestEmotionalResponse(Input, GetCurrentEmotionalTone());
}

FString ULanguageSystem::RequestEmotionalResponse(const FParsedUtterance& Input, float EmotionalTone)
{
    // The parse routes the utterance to a prompt template; the model writes the reply.
    const FResponseTemplate Template = SelectResponseTemplate(Input, EmotionalTone);
    TMap<FString, FString> Slots;
    Slots.Add(TEXT("utterance"), Input.RawText);
    Slots.Add(TEXT("question"), Input.RawText);
    Slots.Add(TEXT("keywords"), Input.Keywords.Num() ? FString::Join(Input.Keywords, TEXT(", ")) : FString(TEXT("(none)")));
    Slots.Add(TEXT("entities"), Input.Entities.Num() ? FString::Join(Input.Entities, TEXT(", ")) : FString(TEXT("(none)")));
    return SendTemplateRequest(Template, MoveTemp(Slots), EmotionalTone, EDialogueState::Waiting);
}

FString ULanguageSystem::RequestClarification(const FString& UnclearAspect)
{
    TMap<FString, FString> Slots;
    Slots.Add(TEXT("unclear"), UnclearAspect);
    return RequestFromTemplate(TEXT("CLARIFY"), Slots);
}

FString ULanguageSystem::RequestAcknowledgment()
{
    return RequestFromTemplate(TEXT("ACK"), TMap<FString, FString>());
}

FString ULanguageSystem::RequestGreeting(const FString& RecipientName)
{
    TMap<FString, FString> Slots;
    if (!RecipientName.IsEmpty())
    {
        Slots.Add(TEXT("recipient"), RecipientName);
    }
    return RequestFromTemplate(TEXT("GREET"), Slots);
}

FString ULanguageSystem::RequestFarewell()
{
    return RequestFromTemplate(TEXT("FAREWELL"), TMap<FString, FString>());
}

FString ULanguageSystem::RequestFromTemplate(const FString& TemplateID, const TMap<FString, FString>& Slots)
{
    const FResponseTemplate* Template = FindResponseTemplate(TemplateID);
    if (!Template)
    {
        const FString RequestID = GenerateResponseID();
        const FString Error = FString::Printf(TEXT("No response prompt template '%s'"), *TemplateID);
        UE_LOG(LogTemp, Error, TEXT("LanguageSystem: %s"), *Error);
        OnGenerationFailed.Broadcast(RequestID, Error);
        return RequestID;
    }
    const EDialogueState StateOnSuccess = Template->ResponseType == ESpeechAct::Question
        ? EDialogueState::Clarifying : EDialogueState::Waiting;
    return SendTemplateRequest(*Template, Slots, GetCurrentEmotionalTone(), StateOnSuccess);
}

// ========================================
// NARRATIVE GENERATION (prompt templates -> model)
// ========================================

FString ULanguageSystem::RequestNarrative(const FString& Subject, const TArray<FString>& Events)
{
    FString EventList;
    for (int32 i = 0; i < Events.Num(); ++i)
    {
        EventList += FString::Printf(TEXT("%d. %s\n"), i + 1, *Events[i]);
    }
    TMap<FString, FString> Slots;
    Slots.Add(TEXT("subject"), Subject);
    Slots.Add(TEXT("events"), Events.Num() ? EventList : FString(TEXT("(no events recorded)\n")));

    TArray<FString> Characters;
    Characters.Add(Subject);
    return SendNarrativeRequest(TEXT("Description"), Slots, Characters);
}

FString ULanguageSystem::RequestSelfNarrative()
{
    TMap<FString, FString> Slots;
    Slots.Add(TEXT("agent"), AgentName);
    if (EmotionalComponent)
    {
        const float Valence = EmotionalComponent->GetValence();
        Slots.Add(TEXT("valence"), FString::Printf(TEXT("%.2f"), Valence));
        Slots.Add(TEXT("emotional_state"), Valence > 0.3f ? TEXT("positive and engaged")
            : Valence < -0.3f ? TEXT("working through something challenging") : TEXT("calm and receptive"));
    }
    else
    {
        Slots.Add(TEXT("valence"), TEXT("unknown"));
        Slots.Add(TEXT("emotional_state"), TEXT("no emotional reading available"));
    }

    TArray<FString> Characters;
    Characters.Add(AgentName);
    return SendNarrativeRequest(TEXT("Self-Description"), Slots, Characters);
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

    TMap<FString, FString> Slots;
    Slots.Add(TEXT("timespan"), FString::Printf(TEXT("%.0f"), TimeSpan));
    Slots.Add(TEXT("turn_count"), FString::FromInt(TurnCount));
    Slots.Add(TEXT("turns"), TurnCount > 0 ? Turns : FString(TEXT("(none)\n")));
    Slots.Add(TEXT("entities"), DialogueContext.MentionedEntities.Num()
        ? FString::Join(DialogueContext.MentionedEntities, TEXT(", ")) : FString(TEXT("(none)")));

    TArray<FString> Characters;
    Characters.Add(AgentName);
    return SendNarrativeRequest(TEXT("Experience Summary"), Slots, Characters);
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

FString ULanguageSystem::SendTemplateRequest(const FResponseTemplate& Template, TMap<FString, FString> Slots,
                                             float EmotionalTone, EDialogueState StateOnSuccess)
{
    const FString RequestID = GenerateResponseID();

    if (Template.PromptTemplate.TrimStartAndEnd().IsEmpty())
    {
        const FString Error = FString::Printf(TEXT("Response prompt template '%s' is empty"), *Template.TemplateID);
        UE_LOG(LogTemp, Error, TEXT("LanguageSystem: %s"), *Error);
        OnGenerationFailed.Broadcast(RequestID, Error);
        return RequestID;
    }

    AddDialogueSlots(Slots);
    for (const FString& Required : Template.RequiredSlots)
    {
        const FString* Value = Slots.Find(Required);
        if (!Value || Value->TrimStartAndEnd().IsEmpty())
        {
            const FString Error = FString::Printf(TEXT("Prompt template '%s' needs slot {%s}"), *Template.TemplateID, *Required);
            UE_LOG(LogTemp, Error, TEXT("LanguageSystem: %s"), *Error);
            OnGenerationFailed.Broadcast(RequestID, Error);
            return RequestID;
        }
    }
    Slots.Add(TEXT("tone"), DescribeTone(EmotionalTone));
    Slots.Add(TEXT("tone_value"), FString::Printf(TEXT("%.2f"), EmotionalTone));
    Slots.Add(TEXT("formality"), DescribeFormality(FMath::Lerp(DefaultFormality, Template.Formality, 0.5f)));

    FString Prompt = UEchoModelClient::FillPrompt(Template.PromptTemplate, Slots);
    Prompt += TEXT("\n") + UEchoModelClient::FillPrompt(ResponseStylePromptTemplate, Slots);
    if (!Template.StyleExample.IsEmpty())
    {
        Prompt += FString::Printf(TEXT("\n(Style example only, do not copy it: \"%s\")"), *Template.StyleExample);
    }

    const FString Transcript = BuildRecentTranscript(MaxTurnHistory);
    TArray<FEchoModelMessage> Messages;
    Messages.Add(FEchoModelMessage(TEXT("system"), BuildSystemPrompt()));
    Messages.Add(FEchoModelMessage(TEXT("user"), FString::Printf(TEXT("Conversation so far:\n%s\n%s"),
        Transcript.IsEmpty() ? TEXT("(no turns yet)\n") : *Transcript, *Prompt)));

    SetDialogueState(EDialogueState::Processing);
    UEchoModelClient::Get()->Chat(Messages, FOnEchoModelReply::CreateUObject(
        this, &ULanguageSystem::HandleResponseReply, RequestID, Template.ResponseType, EmotionalTone, StateOnSuccess,
        Template.TemplateID, Template.StyleExample));
    return RequestID;
}

void ULanguageSystem::HandleResponseReply(bool bSuccess, const FString& TextOrError, FString RequestID,
                                          ESpeechAct SpeechAct, float EmotionalTone, EDialogueState StateOnSuccess,
                                          FString TemplateID, FString StyleExample)
{
    auto Fail = [this, &RequestID](const FString& Error)
    {
        // No agent turn, no text: report the failure as a system event.
        UE_LOG(LogTemp, Error, TEXT("LanguageSystem: request %s produced no reply: %s"), *RequestID, *Error);
        SetDialogueState(EDialogueState::Listening);
        OnGenerationFailed.Broadcast(RequestID, Error);
    };

    if (!bSuccess)
    {
        Fail(TextOrError);
        return;
    }

    const FString Text = TextOrError.TrimStartAndEnd();
    if (Text.IsEmpty())
    {
        Fail(TEXT("model reply is empty"));
        return;
    }

    // A reply that is the style example itself is the designer's line, not the model's
    auto Normalize = [](FString In)
    {
        In.TrimStartAndEndInline();
        while (In.Len() > 0 && (In[0] == TEXT('"') || In[0] == TEXT('\'')))
        {
            In.RightChopInline(1);
        }
        while (In.Len() > 0 && (In[In.Len() - 1] == TEXT('"') || In[In.Len() - 1] == TEXT('\'')))
        {
            In.LeftChopInline(1);
        }
        In.TrimStartAndEndInline();
        return In;
    };
    const FString Example = Normalize(StyleExample);
    if (!Example.IsEmpty() && Normalize(Text).Equals(Example, ESearchCase::IgnoreCase))
    {
        Fail(FString::Printf(TEXT("model copied the '%s' style example verbatim"), *TemplateID));
        return;
    }

    FGeneratedResponse Response;
    Response.ResponseID = RequestID;
    Response.Text = Text;
    Response.SpeechAct = SpeechAct;
    Response.EmotionalTone = EmotionalTone;
    Response.TemplateID = TemplateID;

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

FString ULanguageSystem::SendNarrativeRequest(const FString& NarrativeType, const TMap<FString, FString>& Slots,
                                              const TArray<FString>& Characters)
{
    const FString RequestID = GenerateNarrativeID();

    const FString* Template = NarrativePromptTemplates.Find(NarrativeType);
    if (!Template || Template->IsEmpty())
    {
        const FString Error = FString::Printf(TEXT("No narrative prompt template '%s'"), *NarrativeType);
        UE_LOG(LogTemp, Error, TEXT("LanguageSystem: %s"), *Error);
        OnGenerationFailed.Broadcast(RequestID, Error);
        return RequestID;
    }

    TMap<FString, FString> AllSlots = Slots;
    AddDialogueSlots(AllSlots);

    TArray<FEchoModelMessage> Messages;
    Messages.Add(FEchoModelMessage(TEXT("system"), BuildSystemPrompt()));
    Messages.Add(FEchoModelMessage(TEXT("user"), UEchoModelClient::FillPrompt(*Template, AllSlots)));

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
