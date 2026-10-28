#include "AvatarAtomSpaceClient.h"
#include "../9P/Avatar9PServer.h"
#include "../Narrative/DiaryInsightBlogLoop.h"

DEFINE_LOG_CATEGORY_STATIC(LogAtomSpace, Log, All);

UAvatarAtomSpaceClient::UAvatarAtomSpaceClient()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f; // 10 Hz for sync

    bConnected = false;
    ServerHost = TEXT("localhost");
    ServerPort = 17001;
    bUse9PConnection = true;
    NextHandle = 1;
    bAutoSyncEnabled = true;
    SyncInterval = 5.0f;
    SyncTimer = 0.0f;
    AvatarName = TEXT("deep_tree_echo");
    MaxCacheSize = 10000;
    bVerboseLogging = false;
}

void UAvatarAtomSpaceClient::BeginPlay()
{
    Super::BeginPlay();

    // Find 9P server component if available
    if (AActor* Owner = GetOwner())
    {
        P9Server = Owner->FindComponentByClass<UAvatar9PServer>();
    }

    // Initialize the avatar concept in AtomSpace
    InitializeAvatarConcept();

    UE_LOG(LogAtomSpace, Log, TEXT("AtomSpace client initialized for avatar: %s"), *AvatarName);
}

void UAvatarAtomSpaceClient::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (PendingChanges.Num() > 0 && bConnected)
    {
        SyncWithServer();
    }

    Disconnect();
    Super::EndPlay(EndPlayReason);
}

void UAvatarAtomSpaceClient::TickComponent(float DeltaTime, ELevelTick TickType,
                                            FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bAutoSyncEnabled && bConnected)
    {
        SyncTimer += DeltaTime;
        if (SyncTimer >= SyncInterval)
        {
            ProcessPendingChanges();
            SyncTimer = 0.0f;
        }
    }
}

void UAvatarAtomSpaceClient::InitializeAvatarConcept()
{
    // Create the main avatar concept node
    FTruthValue TV(1.0f, 1.0f, 1);
    AvatarConceptHandle = CreateNode(EAtomType::ConceptNode, AvatarName, TV);

    // Create personality anchor
    FAtomHandle PersonalityAnchor = CreateNode(EAtomType::AnchorNode, AvatarName + TEXT("_personality"), TV);

    // Create memory anchor
    FAtomHandle MemoryAnchor = CreateNode(EAtomType::AnchorNode, AvatarName + TEXT("_memory"), TV);

    // Link personality anchor to avatar
    TArray<FAtomHandle> PersonalityOutgoing;
    PersonalityOutgoing.Add(AvatarConceptHandle);
    PersonalityOutgoing.Add(PersonalityAnchor);
    CreateLink(EAtomType::MemberLink, PersonalityOutgoing, TV);

    // Link memory anchor to avatar
    TArray<FAtomHandle> MemoryOutgoing;
    MemoryOutgoing.Add(AvatarConceptHandle);
    MemoryOutgoing.Add(MemoryAnchor);
    CreateLink(EAtomType::MemberLink, MemoryOutgoing, TV);

    UE_LOG(LogAtomSpace, Log, TEXT("Avatar concept initialized with handle: %llu"), AvatarConceptHandle.Handle);
}

// ===== Connection Management =====

bool UAvatarAtomSpaceClient::Connect(const FString& Host, int32 Port)
{
    ServerHost = Host;
    ServerPort = Port;

    // There is no AtomSpace network transport in this client; do not claim a connection.
    bConnected = false;
    LastConnectionError = FString::Printf(
        TEXT("AtomSpace transport not implemented: cannot connect to %s:%d; atoms stay in the local cache only"),
        *ServerHost, ServerPort);
    UE_LOG(LogAtomSpace, Error, TEXT("%s"), *LastConnectionError);
    OnConnectionStateChanged.Broadcast(false);
    return false;
}

void UAvatarAtomSpaceClient::Disconnect()
{
    bConnected = false;
    OnConnectionStateChanged.Broadcast(false);
    UE_LOG(LogAtomSpace, Log, TEXT("Disconnected from AtomSpace"));
}

void UAvatarAtomSpaceClient::SetConnectionVia9P(bool bUse9P)
{
    bUse9PConnection = bUse9P;
}

// ===== Atom Operations =====

FAtomHandle UAvatarAtomSpaceClient::CreateNode(EAtomType Type, const FString& Name, const FTruthValue& TV)
{
    FAtom NewAtom;
    NewAtom.Handle = FAtomHandle(NextHandle++);
    NewAtom.Type = Type;
    NewAtom.Name = Name;
    NewAtom.TruthValue = TV;
    NewAtom.CreatedAt = FDateTime::Now();
    NewAtom.ModifiedAt = FDateTime::Now();

    AtomCache.Add(NewAtom.Handle.Handle, NewAtom);
    PendingChanges.Add(NewAtom);

    OnAtomCreated.Broadcast(NewAtom.Handle, NewAtom);

    if (bVerboseLogging)
    {
        UE_LOG(LogAtomSpace, Verbose, TEXT("Created node: %s (type: %d, handle: %llu)"),
            *Name, (int32)Type, NewAtom.Handle.Handle);
    }

    return NewAtom.Handle;
}

FAtomHandle UAvatarAtomSpaceClient::CreateLink(EAtomType Type, const TArray<FAtomHandle>& Outgoing, const FTruthValue& TV)
{
    FAtom NewAtom;
    NewAtom.Handle = FAtomHandle(NextHandle++);
    NewAtom.Type = Type;
    NewAtom.Outgoing = Outgoing;
    NewAtom.TruthValue = TV;
    NewAtom.CreatedAt = FDateTime::Now();
    NewAtom.ModifiedAt = FDateTime::Now();

    AtomCache.Add(NewAtom.Handle.Handle, NewAtom);
    PendingChanges.Add(NewAtom);

    OnAtomCreated.Broadcast(NewAtom.Handle, NewAtom);

    if (bVerboseLogging)
    {
        UE_LOG(LogAtomSpace, Verbose, TEXT("Created link: type %d, outgoing count: %d, handle: %llu"),
            (int32)Type, Outgoing.Num(), NewAtom.Handle.Handle);
    }

    return NewAtom.Handle;
}

FAtom UAvatarAtomSpaceClient::GetAtom(const FAtomHandle& Handle)
{
    if (FAtom* Found = AtomCache.Find(Handle.Handle))
    {
        return *Found;
    }

    return FAtom();
}

bool UAvatarAtomSpaceClient::DeleteAtom(const FAtomHandle& Handle)
{
    if (AtomCache.Remove(Handle.Handle) > 0)
    {
        OnAtomDeleted.Broadcast(Handle);
        return true;
    }
    return false;
}

void UAvatarAtomSpaceClient::SetTruthValue(const FAtomHandle& Handle, const FTruthValue& TV)
{
    if (FAtom* Found = AtomCache.Find(Handle.Handle))
    {
        Found->TruthValue = TV;
        Found->ModifiedAt = FDateTime::Now();
        PendingChanges.Add(*Found);
        OnAtomModified.Broadcast(Handle, *Found);
    }
}

void UAvatarAtomSpaceClient::SetAttentionValue(const FAtomHandle& Handle, const FAttentionValue& AV)
{
    if (FAtom* Found = AtomCache.Find(Handle.Handle))
    {
        Found->AttentionValue = AV;
        Found->ModifiedAt = FDateTime::Now();
        PendingChanges.Add(*Found);
        OnAtomModified.Broadcast(Handle, *Found);
    }
}

// ===== Diary Storage =====

void UAvatarAtomSpaceClient::StoreDiaryEntry(const FDiaryEntry& Entry)
{
    FTruthValue TV(1.0f, 0.9f, 1);

    // Create diary entry node
    FAtomHandle EntryNode = CreateNode(EAtomType::DiaryEntryNode, Entry.EntryId, TV);

    // Create emotional state node
    FString EmotionalStateId = FString::Printf(TEXT("emotional_state_%s"), *Entry.EntryId);
    FAtomHandle EmotionalNode = CreateNode(EAtomType::EmotionalStateNode, EmotionalStateId, TV);

    // Create timestamp node
    FAtomHandle TimestampNode = CreateNode(EAtomType::NumberNode,
        FString::Printf(TEXT("%lld"), Entry.Timestamp.ToUnixTimestamp()), TV);

    // Create EvaluationLink: (diary_entry avatar entry_id emotional_state)
    FAtomHandle PredicateNode = CreateNode(EAtomType::PredicateNode, TEXT("diary_entry"), TV);

    TArray<FAtomHandle> ListOutgoing;
    ListOutgoing.Add(AvatarConceptHandle);
    ListOutgoing.Add(EntryNode);
    ListOutgoing.Add(EmotionalNode);
    FAtomHandle ListLink = CreateLink(EAtomType::ListLink, ListOutgoing, TV);

    TArray<FAtomHandle> EvalOutgoing;
    EvalOutgoing.Add(PredicateNode);
    EvalOutgoing.Add(ListLink);
    CreateLink(EAtomType::EvaluationLink, EvalOutgoing, TV);

    // Keep the full entry (content included) so it can be served back verbatim.
    DiaryStore.Add(Entry.EntryId, Entry);

    UE_LOG(LogAtomSpace, Log, TEXT("Stored diary entry: %s"), *Entry.EntryId);
}

FDiaryEntry UAvatarAtomSpaceClient::GetDiaryEntry(const FString& EntryId)
{
    FDiaryEntry Entry;
    if (!TryGetDiaryEntry(EntryId, Entry))
    {
        Entry = FDiaryEntry();
        Entry.EntryId = EntryId;
    }
    return Entry;
}

bool UAvatarAtomSpaceClient::TryGetDiaryEntry(const FString& EntryId, FDiaryEntry& OutEntry) const
{
    if (const FDiaryEntry* Found = DiaryStore.Find(EntryId))
    {
        OutEntry = *Found;
        return true;
    }
    return false;
}

TArray<FString> UAvatarAtomSpaceClient::ListDiaryEntryIds() const
{
    TArray<FString> Ids;
    DiaryStore.GetKeys(Ids);
    return Ids;
}

TArray<FDiaryEntry> UAvatarAtomSpaceClient::GetRecentDiaryEntries(int32 Count)
{
    TArray<FDiaryEntry> Entries;
    DiaryStore.GenerateValueArray(Entries);

    // Most recent first
    Entries.Sort([](const FDiaryEntry& A, const FDiaryEntry& B) {
        return A.Timestamp > B.Timestamp;
    });

    if (Entries.Num() > Count)
    {
        Entries.SetNum(FMath::Max(0, Count));
    }
    return Entries;
}

TArray<FDiaryEntry> UAvatarAtomSpaceClient::SearchDiaryEntries(const FString& Query, int32 MaxResults)
{
    TArray<FDiaryEntry> Results;

    for (const auto& Pair : DiaryStore)
    {
        const FDiaryEntry& Entry = Pair.Value;
        if (Entry.EntryId.Contains(Query) || Entry.Content.Contains(Query) || Entry.Tags.Contains(Query))
        {
            Results.Add(Entry);
            if (Results.Num() >= MaxResults)
            {
                break;
            }
        }
    }

    return Results;
}

// ===== Insight Storage =====

void UAvatarAtomSpaceClient::StoreInsight(const FInsight& Insight)
{
    FTruthValue TV(Insight.Confidence, 0.8f, 1);

    // Create insight node
    FAtomHandle InsightNode = CreateNode(EAtomType::InsightNode, Insight.InsightId, TV);

    // Create category node
    FAtomHandle CategoryNode = CreateNode(EAtomType::ConceptNode, Insight.Category, TV);

    // Create InheritanceLink: insight -> category
    TArray<FAtomHandle> InheritOutgoing;
    InheritOutgoing.Add(InsightNode);
    InheritOutgoing.Add(CategoryNode);
    CreateLink(EAtomType::InheritanceLink, InheritOutgoing, TV);

    // Link source diary entries
    for (const FString& SourceId : Insight.SourceDiaryEntryIds)
    {
        TArray<FAtomHandle> SourceHandles = FindByName(SourceId);
        if (SourceHandles.Num() > 0)
        {
            TArray<FAtomHandle> ContextOutgoing;
            ContextOutgoing.Add(SourceHandles[0]);
            ContextOutgoing.Add(InsightNode);
            CreateLink(EAtomType::ContextLink, ContextOutgoing, TV);
        }
    }

    // Keep the full insight (content included) so it can be served back verbatim.
    InsightStore.Add(Insight.InsightId, Insight);

    UE_LOG(LogAtomSpace, Log, TEXT("Stored insight: %s (category: %s, confidence: %.2f)"),
        *Insight.InsightId, *Insight.Category, Insight.Confidence);
}

FInsight UAvatarAtomSpaceClient::GetInsight(const FString& InsightId)
{
    FInsight Insight;
    if (!TryGetInsight(InsightId, Insight))
    {
        Insight = FInsight();
        Insight.InsightId = InsightId;
    }
    return Insight;
}

bool UAvatarAtomSpaceClient::TryGetInsight(const FString& InsightId, FInsight& OutInsight) const
{
    if (const FInsight* Found = InsightStore.Find(InsightId))
    {
        OutInsight = *Found;
        return true;
    }
    return false;
}

TArray<FString> UAvatarAtomSpaceClient::ListInsightIds() const
{
    TArray<FString> Ids;
    InsightStore.GetKeys(Ids);
    return Ids;
}

TArray<FInsight> UAvatarAtomSpaceClient::GetRelatedInsights(const FString& Context, int32 MaxResults)
{
    // Stored insights, those mentioning the context (category or content) first.
    TArray<FInsight> Matching;
    TArray<FInsight> Others;
    for (const auto& Pair : InsightStore)
    {
        const FInsight& Insight = Pair.Value;
        if (!Context.IsEmpty() && (Insight.Category.Contains(Context) || Insight.Content.Contains(Context)))
        {
            Matching.Add(Insight);
        }
        else
        {
            Others.Add(Insight);
        }
    }

    Matching.Append(Others);
    if (Matching.Num() > MaxResults)
    {
        Matching.SetNum(FMath::Max(0, MaxResults));
    }
    return Matching;
}

void UAvatarAtomSpaceClient::ValidateInsight(const FString& InsightId, bool bValid)
{
    TArray<FAtomHandle> Handles = FindByName(InsightId);
    if (Handles.Num() > 0)
    {
        FTruthValue TV = GetAtom(Handles[0]).TruthValue;
        TV.Strength = bValid ? FMath::Min(1.0f, TV.Strength + 0.1f) : FMath::Max(0.0f, TV.Strength - 0.1f);
        TV.Confidence = FMath::Min(1.0f, TV.Confidence + 0.05f);
        TV.Count++;
        SetTruthValue(Handles[0], TV);
    }

    if (FInsight* Stored = InsightStore.Find(InsightId))
    {
        Stored->bIsValidated = bValid;
    }
}

// ===== Personality Trait Storage =====

void UAvatarAtomSpaceClient::StorePersonalityTrait(const FString& TraitName, float Value, float Confidence)
{
    FTruthValue TV(Value, Confidence, 1);

    // Create trait node if it doesn't exist
    FAtomHandle TraitNode = CreateNode(EAtomType::PersonalityTraitNode, TraitName, TV);

    // Create value node
    FAtomHandle ValueNode = CreateNode(EAtomType::NumberNode, FString::Printf(TEXT("%.4f"), Value), TV);

    // Create has_trait predicate
    FAtomHandle PredicateNode = CreateNode(EAtomType::PredicateNode, TEXT("has_trait"), TV);

    // Create EvaluationLink: (has_trait (avatar trait value))
    TArray<FAtomHandle> ListOutgoing;
    ListOutgoing.Add(AvatarConceptHandle);
    ListOutgoing.Add(TraitNode);
    ListOutgoing.Add(ValueNode);
    FAtomHandle ListLink = CreateLink(EAtomType::ListLink, ListOutgoing, TV);

    TArray<FAtomHandle> EvalOutgoing;
    EvalOutgoing.Add(PredicateNode);
    EvalOutgoing.Add(ListLink);
    CreateLink(EAtomType::EvaluationLink, EvalOutgoing, TV);
}

float UAvatarAtomSpaceClient::GetPersonalityTrait(const FString& TraitName)
{
    TArray<FAtomHandle> Handles = FindByName(TraitName);
    for (const FAtomHandle& Handle : Handles)
    {
        FAtom Atom = GetAtom(Handle);
        if (Atom.Type == EAtomType::PersonalityTraitNode)
        {
            return Atom.TruthValue.Strength;
        }
    }
    return 0.5f; // Default value
}

TMap<FString, float> UAvatarAtomSpaceClient::GetAllPersonalityTraits()
{
    TMap<FString, float> Traits;

    TArray<FAtomHandle> TraitHandles = FindByType(EAtomType::PersonalityTraitNode);
    for (const FAtomHandle& Handle : TraitHandles)
    {
        FAtom Atom = GetAtom(Handle);
        Traits.Add(Atom.Name, Atom.TruthValue.Strength);
    }

    return Traits;
}

// ===== Memory Queries =====

TArray<FMemory> UAvatarAtomSpaceClient::QueryRelatedMemories(const FString& Context, int32 MaxResults)
{
    TArray<FMemory> Results;

    // Find memories related to context
    TArray<FAtomHandle> MemoryHandles = FindByType(EAtomType::MemoryNode);

    // Calculate relevance based on echo resonance
    TArray<TPair<FAtomHandle, float>> ScoredMemories;
    for (const FAtomHandle& Handle : MemoryHandles)
    {
        FAtom Atom = GetAtom(Handle);
        float Resonance = GetEchoResonance(Context, Atom.Name);
        ScoredMemories.Add(TPair<FAtomHandle, float>(Handle, Resonance));
    }

    // Sort by resonance
    ScoredMemories.Sort([](const TPair<FAtomHandle, float>& A, const TPair<FAtomHandle, float>& B) {
        return A.Value > B.Value;
    });

    // Return top results
    int32 Count = FMath::Min(MaxResults, ScoredMemories.Num());
    for (int32 i = 0; i < Count; i++)
    {
        FAtom Atom = GetAtom(ScoredMemories[i].Key);
        FMemory Memory;
        Memory.MemoryId = Atom.Name;
        Memory.Timestamp = Atom.CreatedAt;
        Memory.Relevance = ScoredMemories[i].Value;
        Memory.Strength = Atom.TruthValue.Strength;
        Results.Add(Memory);
    }

    return Results;
}

TArray<FAtom> UAvatarAtomSpaceClient::QueryAtoms(const FMemoryQuery& Query)
{
    TArray<FAtom> Results;

    for (auto& Pair : AtomCache)
    {
        FAtom& Atom = Pair.Value;

        // Check type filter
        if (Query.AtomTypes.Num() > 0 && !Query.AtomTypes.Contains(Atom.Type))
        {
            continue;
        }

        // Check name pattern
        if (!Query.NamePattern.IsEmpty() && !Atom.Name.Contains(Query.NamePattern))
        {
            continue;
        }

        // Check truth value filters
        if (Atom.TruthValue.Strength < Query.MinStrength)
        {
            continue;
        }
        if (Atom.TruthValue.Confidence < Query.MinConfidence)
        {
            continue;
        }

        // Check attention value filter
        if (Atom.AttentionValue.STI < Query.MinSTI)
        {
            continue;
        }

        // Check time range
        if (Atom.CreatedAt < Query.FromTime || Atom.CreatedAt > Query.ToTime)
        {
            continue;
        }

        Results.Add(Atom);

        if (Results.Num() >= Query.MaxResults)
        {
            break;
        }
    }

    return Results;
}

TArray<FAtomHandle> UAvatarAtomSpaceClient::FindByName(const FString& NamePattern)
{
    TArray<FAtomHandle> Results;

    for (auto& Pair : AtomCache)
    {
        if (Pair.Value.Name.Contains(NamePattern) || Pair.Value.Name.MatchesWildcard(NamePattern))
        {
            Results.Add(Pair.Value.Handle);
        }
    }

    return Results;
}

TArray<FAtomHandle> UAvatarAtomSpaceClient::FindByType(EAtomType Type)
{
    TArray<FAtomHandle> Results;

    for (auto& Pair : AtomCache)
    {
        if (Pair.Value.Type == Type)
        {
            Results.Add(Pair.Value.Handle);
        }
    }

    return Results;
}

// ===== Pattern Mining =====

TMap<FString, int32> UAvatarAtomSpaceClient::CountFrequentConcepts(int32 MinSupport)
{
    TMap<FString, int32> ConceptFrequency;
    for (const auto& Pair : AtomCache)
    {
        if (Pair.Value.Type == EAtomType::ConceptNode)
        {
            ConceptFrequency.FindOrAdd(Pair.Value.Name)++;
        }
    }

    TMap<FString, int32> Frequent;
    for (const auto& FreqPair : ConceptFrequency)
    {
        if (FreqPair.Value >= MinSupport)
        {
            Frequent.Add(FreqPair.Key, FreqPair.Value);
        }
    }
    return Frequent;
}

float UAvatarAtomSpaceClient::ComputeCooccurrence(const FString& ConceptA, const FString& ConceptB)
{
    TArray<FAtomHandle> HandlesA = FindByName(ConceptA);
    TArray<FAtomHandle> HandlesB = FindByName(ConceptB);
    if (HandlesA.Num() == 0 || HandlesB.Num() == 0)
    {
        return -1.0f;
    }

    TSet<uint64> SetA;
    TSet<uint64> SetB;
    for (const FAtomHandle& H : HandlesA) { SetA.Add(H.Handle); }
    for (const FAtomHandle& H : HandlesB) { SetB.Add(H.Handle); }

    // Count links whose outgoing set touches A, B, or both.
    int32 Both = 0;
    int32 Either = 0;
    for (const auto& Pair : AtomCache)
    {
        const FAtom& Link = Pair.Value;
        if (Link.Outgoing.Num() == 0)
        {
            continue;
        }
        bool bHasA = false;
        bool bHasB = false;
        for (const FAtomHandle& Out : Link.Outgoing)
        {
            bHasA |= SetA.Contains(Out.Handle);
            bHasB |= SetB.Contains(Out.Handle);
        }
        if (bHasA || bHasB)
        {
            ++Either;
            if (bHasA && bHasB)
            {
                ++Both;
            }
        }
    }

    const float Cooccurrence = Either > 0 ? static_cast<float>(Both) / static_cast<float>(Either) : 0.0f;

    // Record the measurement: (EvaluationLink (PredicateNode "cooccurs_with") (ListLink A B)).
    // Confidence grows with the amount of evidence (n / (n + k), k = 10).
    const FTruthValue TV(Cooccurrence, static_cast<float>(Either) / (static_cast<float>(Either) + 10.0f), Either);
    FAtomHandle Predicate = CreateNode(EAtomType::PredicateNode, TEXT("cooccurs_with"), TV);
    TArray<FAtomHandle> ListOutgoing;
    ListOutgoing.Add(HandlesA[0]);
    ListOutgoing.Add(HandlesB[0]);
    FAtomHandle ListLink = CreateLink(EAtomType::ListLink, ListOutgoing, TV);
    TArray<FAtomHandle> EvalOutgoing;
    EvalOutgoing.Add(Predicate);
    EvalOutgoing.Add(ListLink);
    CreateLink(EAtomType::EvaluationLink, EvalOutgoing, TV);

    return Cooccurrence;
}

// ===== Echo Resonance Integration =====

void UAvatarAtomSpaceClient::StoreEchoResonance(const FString& SourceId, const FString& TargetId, float Resonance)
{
    FTruthValue TV(Resonance, 0.9f, 1);

    TArray<FAtomHandle> SourceHandles = FindByName(SourceId);
    TArray<FAtomHandle> TargetHandles = FindByName(TargetId);

    if (SourceHandles.Num() > 0 && TargetHandles.Num() > 0)
    {
        TArray<FAtomHandle> Outgoing;
        Outgoing.Add(SourceHandles[0]);
        Outgoing.Add(TargetHandles[0]);
        CreateLink(EAtomType::EchoResonanceLink, Outgoing, TV);
    }
}

float UAvatarAtomSpaceClient::GetEchoResonance(const FString& SourceId, const FString& TargetId)
{
    TArray<FAtomHandle> ResonanceLinks = FindByType(EAtomType::EchoResonanceLink);

    for (const FAtomHandle& Handle : ResonanceLinks)
    {
        FAtom Atom = GetAtom(Handle);
        if (Atom.Outgoing.Num() >= 2)
        {
            FAtom Source = GetAtom(Atom.Outgoing[0]);
            FAtom Target = GetAtom(Atom.Outgoing[1]);
            if (Source.Name == SourceId && Target.Name == TargetId)
            {
                return Atom.TruthValue.Strength;
            }
        }
    }

    return 0.0f;
}

TArray<FString> UAvatarAtomSpaceClient::GetResonantMemories(const FString& CurrentContext, float MinResonance)
{
    TArray<FString> ResonantIds;

    TArray<FAtomHandle> ResonanceLinks = FindByType(EAtomType::EchoResonanceLink);

    for (const FAtomHandle& Handle : ResonanceLinks)
    {
        FAtom Atom = GetAtom(Handle);
        if (Atom.TruthValue.Strength >= MinResonance && Atom.Outgoing.Num() >= 2)
        {
            FAtom Source = GetAtom(Atom.Outgoing[0]);
            if (Source.Name.Contains(CurrentContext))
            {
                FAtom Target = GetAtom(Atom.Outgoing[1]);
                ResonantIds.Add(Target.Name);
            }
        }
    }

    return ResonantIds;
}

// ===== Synchronization =====

bool UAvatarAtomSpaceClient::SyncWithServer()
{
    return ProcessPendingChanges();
}

void UAvatarAtomSpaceClient::EnableAutoSync(bool bEnable, float IntervalSeconds)
{
    bAutoSyncEnabled = bEnable;
    SyncInterval = FMath::Max(0.5f, IntervalSeconds);
}

bool UAvatarAtomSpaceClient::ProcessPendingChanges()
{
    if (PendingChanges.Num() == 0)
    {
        return true;
    }

    // There is no server transport; never report changes as synced.
    UE_LOG(LogAtomSpace, Warning,
        TEXT("%d pending AtomSpace changes NOT synced: no AtomSpace server transport is implemented"),
        PendingChanges.Num());
    return false;
}

// ===== Helper Methods =====

FString UAvatarAtomSpaceClient::GenerateUniqueId()
{
    return FGuid::NewGuid().ToString();
}

FString UAvatarAtomSpaceClient::AtomToScheme(const FAtom& Atom)
{
    // Convert atom to Scheme representation (OpenCog format)
    FString TypeName;
    switch (Atom.Type)
    {
    case EAtomType::ConceptNode:
        TypeName = TEXT("ConceptNode");
        break;
    case EAtomType::PredicateNode:
        TypeName = TEXT("PredicateNode");
        break;
    case EAtomType::NumberNode:
        TypeName = TEXT("NumberNode");
        break;
    case EAtomType::InheritanceLink:
        TypeName = TEXT("InheritanceLink");
        break;
    case EAtomType::EvaluationLink:
        TypeName = TEXT("EvaluationLink");
        break;
    case EAtomType::ListLink:
        TypeName = TEXT("ListLink");
        break;
    default:
        TypeName = TEXT("Atom");
        break;
    }

    if (Atom.Outgoing.Num() == 0)
    {
        // Node
        return FString::Printf(TEXT("(%s \"%s\")"), *TypeName, *Atom.Name);
    }
    else
    {
        // Link
        FString OutgoingStr;
        for (const FAtomHandle& Handle : Atom.Outgoing)
        {
            FAtom OutAtom = GetAtom(Handle);
            OutgoingStr += TEXT("\n    ") + AtomToScheme(OutAtom);
        }
        return FString::Printf(TEXT("(%s%s)"), *TypeName, *OutgoingStr);
    }
}

FAtom UAvatarAtomSpaceClient::SchemeToAtom(const FString& Scheme)
{
    // Parse Scheme representation (simplified)
    FAtom Atom;
    // In full implementation, would parse the Scheme string
    return Atom;
}

FString UAvatarAtomSpaceClient::Read9PPath(const FString& Path)
{
    if (P9Server)
    {
        F9PResponse Response = P9Server->HandleRead(Path, 0, 8192);
        return Response.StringData;
    }
    return TEXT("");
}

bool UAvatarAtomSpaceClient::Write9PPath(const FString& Path, const FString& Data)
{
    if (P9Server)
    {
        TArray<uint8> ByteData;
        ByteData.SetNumUninitialized(Data.Len());
        FMemory::Memcpy(ByteData.GetData(), TCHAR_TO_ANSI(*Data), Data.Len());

        F9PResponse Response = P9Server->HandleWrite(Path, ByteData);
        return Response.bSuccess;
    }
    return false;
}
