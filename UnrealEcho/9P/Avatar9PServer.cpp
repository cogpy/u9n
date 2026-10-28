#include "Avatar9PServer.h"
#include "../Personality/PersonalityTraitSystem.h"
#include "../Neurochemical/NeurochemicalSimulationComponent.h"
#include "../Narrative/DiaryInsightBlogLoop.h"
#include "../Avatar/Avatar3DComponent.h"
#include "../AtomSpace/AvatarAtomSpaceClient.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "Misc/DefaultValueHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(Log9PServer, Log, All);

namespace
{
    FString JsonToString(const TSharedRef<FJsonObject>& Object)
    {
        FString Out;
        TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
            TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
        FJsonSerializer::Serialize(Object, Writer);
        return Out;
    }
}

FString UAvatar9PServer::FailRead(const FString& Error)
{
    LastPathError = Error;
    return FString();
}

UAvatar9PServer::UAvatar9PServer()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.016f; // ~60 Hz

    bServerRunning = false;
    ServerPort = 564; // Standard 9P port
    NextFid = 1;
    bEnableCoalescing = true;
    CoalescingInterval = 0.01f; // 10ms coalescing window
    CoalescingTimer = 0.0f;
    NamespaceRoot = TEXT("/mnt/avatar");
    MaxMessageSize = 8192;
    MaxPendingMessages = 100;
    bVerboseLogging = false;
}

void UAvatar9PServer::BeginPlay()
{
    Super::BeginPlay();
    Initialize();
}

void UAvatar9PServer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopServer();
    Super::EndPlay(EndPlayReason);
}

void UAvatar9PServer::TickComponent(float DeltaTime, ELevelTick TickType,
                                     FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bServerRunning && bEnableCoalescing)
    {
        CoalescingTimer += DeltaTime;
        if (CoalescingTimer >= CoalescingInterval)
        {
            ProcessPendingMessages();
            CoalescingTimer = 0.0f;
        }
    }
}

void UAvatar9PServer::Initialize()
{
    // Find sibling components
    if (AActor* Owner = GetOwner())
    {
        PersonalitySystem = Owner->FindComponentByClass<UPersonalityTraitSystem>();
        NeurochemicalSystem = Owner->FindComponentByClass<UNeurochemicalSimulationComponent>();
        NarrativeSystem = Owner->FindComponentByClass<UDiaryInsightBlogLoop>();
        AvatarComponent = Owner->FindComponentByClass<UAvatar3DComponent>();
        AtomSpaceClient = Owner->FindComponentByClass<UAvatarAtomSpaceClient>();
    }

    // Build the namespace
    BuildNamespace();

    UE_LOG(Log9PServer, Log, TEXT("Avatar 9P Server initialized. Namespace root: %s"), *NamespaceRoot);
}

void UAvatar9PServer::StartServer(int32 Port)
{
    ServerPort = Port;
    bServerRunning = true;

    UE_LOG(Log9PServer, Log, TEXT("Avatar 9P Server started on port %d"), ServerPort);
}

void UAvatar9PServer::StopServer()
{
    bServerRunning = false;
    FileHandles.Empty();
    PendingRequests.Empty();

    UE_LOG(Log9PServer, Log, TEXT("Avatar 9P Server stopped"));
}

void UAvatar9PServer::BuildNamespace()
{
    NamespaceEntries.Empty();

    // Root directory
    FAvatarNamespaceEntry RootEntry;
    RootEntry.Name = TEXT("avatar");
    RootEntry.FullPath = NamespaceRoot;
    RootEntry.bIsDirectory = true;
    RootEntry.bReadable = true;
    RootEntry.Description = TEXT("Deep Tree Echo Avatar namespace root");
    NamespaceEntries.Add(RootEntry);

    // Personality namespace
    FAvatarNamespaceEntry PersonalityDir;
    PersonalityDir.Name = TEXT("personality");
    PersonalityDir.FullPath = NamespaceRoot + TEXT("/personality");
    PersonalityDir.bIsDirectory = true;
    PersonalityDir.bReadable = true;
    PersonalityDir.Description = TEXT("Personality traits and state");
    NamespaceEntries.Add(PersonalityDir);

    // Personality traits
    TArray<FString> PersonalityTraits = {
        TEXT("confidence"), TEXT("charm"), TEXT("playfulness"), TEXT("elegance"),
        TEXT("expressiveness"), TEXT("unpredictability"), TEXT("volatility"),
        TEXT("impulsivity"), TEXT("chaos_factor")
    };

    for (const FString& Trait : PersonalityTraits)
    {
        FAvatarNamespaceEntry TraitEntry;
        TraitEntry.Name = Trait;
        TraitEntry.FullPath = NamespaceRoot + TEXT("/personality/traits/") + Trait;
        TraitEntry.bIsDirectory = false;
        TraitEntry.bReadable = true;
        TraitEntry.bWritable = true;
        TraitEntry.Description = FString::Printf(TEXT("Personality trait: %s"), *Trait);
        NamespaceEntries.Add(TraitEntry);
    }

    // Neurochemical namespace
    FAvatarNamespaceEntry NeurochemicalDir;
    NeurochemicalDir.Name = TEXT("neurochemical");
    NeurochemicalDir.FullPath = NamespaceRoot + TEXT("/neurochemical");
    NeurochemicalDir.bIsDirectory = true;
    NeurochemicalDir.bReadable = true;
    NeurochemicalDir.Description = TEXT("Neurochemical levels and emotional chemistry");
    NamespaceEntries.Add(NeurochemicalDir);

    // Neurochemical levels
    TArray<FString> Neurochemicals = {
        TEXT("dopamine"), TEXT("serotonin"), TEXT("norepinephrine"), TEXT("oxytocin"),
        TEXT("cortisol"), TEXT("endorphins"), TEXT("gaba"), TEXT("glutamate"),
        TEXT("acetylcholine")
    };

    for (const FString& Chemical : Neurochemicals)
    {
        FAvatarNamespaceEntry ChemicalEntry;
        ChemicalEntry.Name = Chemical;
        ChemicalEntry.FullPath = NamespaceRoot + TEXT("/neurochemical/levels/") + Chemical;
        ChemicalEntry.bIsDirectory = false;
        ChemicalEntry.bReadable = true;
        ChemicalEntry.bWritable = true;
        ChemicalEntry.Description = FString::Printf(TEXT("Neurochemical level: %s"), *Chemical);
        NamespaceEntries.Add(ChemicalEntry);
    }

    // Emotional state
    FAvatarNamespaceEntry EmotionalStateEntry;
    EmotionalStateEntry.Name = TEXT("emotional_state");
    EmotionalStateEntry.FullPath = NamespaceRoot + TEXT("/neurochemical/emotional_state");
    EmotionalStateEntry.bIsDirectory = false;
    EmotionalStateEntry.bReadable = true;
    EmotionalStateEntry.Description = TEXT("Current emotional state derived from neurochemistry");
    NamespaceEntries.Add(EmotionalStateEntry);

    // Narrative namespace
    FAvatarNamespaceEntry NarrativeDir;
    NarrativeDir.Name = TEXT("narrative");
    NarrativeDir.FullPath = NamespaceRoot + TEXT("/narrative");
    NarrativeDir.bIsDirectory = true;
    NarrativeDir.bReadable = true;
    NarrativeDir.Description = TEXT("Diary entries, insights, and blog posts");
    NamespaceEntries.Add(NarrativeDir);

    // Visual namespace
    FAvatarNamespaceEntry VisualDir;
    VisualDir.Name = TEXT("visual");
    VisualDir.FullPath = NamespaceRoot + TEXT("/visual");
    VisualDir.bIsDirectory = true;
    VisualDir.bReadable = true;
    VisualDir.Description = TEXT("Visual appearance and animation state");
    NamespaceEntries.Add(VisualDir);

    // Cognitive namespace
    FAvatarNamespaceEntry CognitiveDir;
    CognitiveDir.Name = TEXT("cognitive");
    CognitiveDir.FullPath = NamespaceRoot + TEXT("/cognitive");
    CognitiveDir.bIsDirectory = true;
    CognitiveDir.bReadable = true;
    CognitiveDir.Description = TEXT("Cognitive state (not available: no cognitive system is wired to this server)");
    NamespaceEntries.Add(CognitiveDir);

    // Performance namespace
    FAvatarNamespaceEntry PerformanceDir;
    PerformanceDir.Name = TEXT("performance");
    PerformanceDir.FullPath = NamespaceRoot + TEXT("/performance");
    PerformanceDir.bIsDirectory = true;
    PerformanceDir.bReadable = true;
    PerformanceDir.Description = TEXT("Performance metrics and budgets");
    NamespaceEntries.Add(PerformanceDir);

    // Batch namespace
    FAvatarNamespaceEntry BatchDir;
    BatchDir.Name = TEXT("batch");
    BatchDir.FullPath = NamespaceRoot + TEXT("/batch");
    BatchDir.bIsDirectory = true;
    BatchDir.bReadable = true;
    BatchDir.bWritable = true;
    BatchDir.Description = TEXT("Batch query and write operations");
    NamespaceEntries.Add(BatchDir);

    FAvatarNamespaceEntry BatchQueryEntry;
    BatchQueryEntry.Name = TEXT("query");
    BatchQueryEntry.FullPath = NamespaceRoot + TEXT("/batch/query");
    BatchQueryEntry.bIsDirectory = false;
    BatchQueryEntry.bReadable = true;
    BatchQueryEntry.bWritable = true;
    BatchQueryEntry.Description = TEXT("Batch query input");
    NamespaceEntries.Add(BatchQueryEntry);

    FAvatarNamespaceEntry BatchResultsEntry;
    BatchResultsEntry.Name = TEXT("results");
    BatchResultsEntry.FullPath = NamespaceRoot + TEXT("/batch/results");
    BatchResultsEntry.bIsDirectory = false;
    BatchResultsEntry.bReadable = true;
    BatchResultsEntry.Description = TEXT("Batch query results");
    NamespaceEntries.Add(BatchResultsEntry);

    UE_LOG(Log9PServer, Log, TEXT("Namespace built with %d entries"), NamespaceEntries.Num());
}

void UAvatar9PServer::ExportNamespace()
{
    // Rebuild namespace with current state
    BuildNamespace();
}

void UAvatar9PServer::RegisterNamespaceEntry(const FAvatarNamespaceEntry& Entry)
{
    // Check if entry already exists
    for (int32 i = 0; i < NamespaceEntries.Num(); i++)
    {
        if (NamespaceEntries[i].FullPath == Entry.FullPath)
        {
            NamespaceEntries[i] = Entry;
            return;
        }
    }

    NamespaceEntries.Add(Entry);
}

F9PResponse UAvatar9PServer::HandleRequest(const F9PRequest& Request)
{
    F9PResponse Response;
    Response.Tag = Request.Tag;

    switch (Request.MessageType)
    {
    case E9PMessageType::Tread:
        return HandleRead(Request.Path, Request.Offset, Request.Count);

    case E9PMessageType::Twrite:
        return HandleWrite(Request.Path, Request.Data);

    case E9PMessageType::Tstat:
        return HandleStat(Request.Path);

    case E9PMessageType::Twalk:
        // Walk to path
        Response.MessageType = E9PMessageType::Rwalk;
        Response.bSuccess = true;
        break;

    case E9PMessageType::Topen:
        // Open file
        Response.MessageType = E9PMessageType::Ropen;
        Response.bSuccess = true;
        break;

    case E9PMessageType::Tclunk:
        // Close fid
        Response.MessageType = E9PMessageType::Rclunk;
        FileHandles.Remove(Request.Fid);
        Response.bSuccess = true;
        break;

    default:
        Response.MessageType = E9PMessageType::Rerror;
        Response.bSuccess = false;
        Response.ErrorMessage = TEXT("Unsupported message type");
        break;
    }

    return Response;
}

F9PResponse UAvatar9PServer::HandleRead(const FString& Path, uint64 Offset, uint32 Count)
{
    F9PResponse Response;
    Response.MessageType = E9PMessageType::Rread;

    LastPathError.Reset();
    FString Content = ReadFromPath(Path);

    if (Content.IsEmpty())
    {
        Response.MessageType = E9PMessageType::Rerror;
        Response.bSuccess = false;
        Response.ErrorMessage = LastPathError.IsEmpty()
            ? FString::Printf(TEXT("Path not found: %s"), *Path)
            : FString::Printf(TEXT("%s: %s"), *Path, *LastPathError);
        return Response;
    }

    // Apply offset and count
    if (Offset >= (uint64)Content.Len())
    {
        Response.StringData = TEXT("");
    }
    else
    {
        int32 StartPos = (int32)Offset;
        int32 ReadCount = FMath::Min((int32)Count, Content.Len() - StartPos);
        Response.StringData = Content.Mid(StartPos, ReadCount);
    }

    Response.bSuccess = true;

    if (bVerboseLogging)
    {
        UE_LOG(Log9PServer, Verbose, TEXT("Read %s: %s"), *Path, *Response.StringData);
    }

    return Response;
}

F9PResponse UAvatar9PServer::HandleWrite(const FString& Path, const TArray<uint8>& Data)
{
    F9PResponse Response;
    Response.MessageType = E9PMessageType::Rwrite;

    // Convert data to string
    FString Value = FString(ANSI_TO_TCHAR((const char*)Data.GetData()));

    LastPathError.Reset();
    bool bSuccess = WriteToPath(Path, Value);

    Response.bSuccess = bSuccess;
    if (!bSuccess)
    {
        Response.MessageType = E9PMessageType::Rerror;
        Response.ErrorMessage = LastPathError.IsEmpty()
            ? FString::Printf(TEXT("Failed to write to path: %s"), *Path)
            : FString::Printf(TEXT("%s: %s"), *Path, *LastPathError);
    }
    else
    {
        // Broadcast state change event
        On9PStateChanged.Broadcast(Path, Value);

        if (bVerboseLogging)
        {
            UE_LOG(Log9PServer, Verbose, TEXT("Write %s: %s"), *Path, *Value);
        }
    }

    return Response;
}

F9PResponse UAvatar9PServer::HandleStat(const FString& Path)
{
    F9PResponse Response;
    Response.MessageType = E9PMessageType::Rstat;

    // Find the namespace entry
    for (const FAvatarNamespaceEntry& Entry : NamespaceEntries)
    {
        if (Entry.FullPath == Path)
        {
            Response.bSuccess = true;
            // Return stat info as JSON
            Response.StringData = FString::Printf(
                TEXT("{\"name\":\"%s\",\"path\":\"%s\",\"isDir\":%s,\"readable\":%s,\"writable\":%s,\"desc\":\"%s\"}"),
                *Entry.Name,
                *Entry.FullPath,
                Entry.bIsDirectory ? TEXT("true") : TEXT("false"),
                Entry.bReadable ? TEXT("true") : TEXT("false"),
                Entry.bWritable ? TEXT("true") : TEXT("false"),
                *Entry.Description
            );
            return Response;
        }
    }

    Response.bSuccess = false;
    Response.ErrorMessage = FString::Printf(TEXT("Path not found: %s"), *Path);
    return Response;
}

FString UAvatar9PServer::ReadFromPath(const FString& Path)
{
    // Parse the path and route to appropriate handler
    TArray<FString> PathParts = ParsePath(Path);

    if (PathParts.Num() < 2) return TEXT("");

    FString Namespace = PathParts[1]; // First part after /mnt

    if (Namespace == TEXT("avatar"))
    {
        if (PathParts.Num() < 3) return TEXT("");
        FString SubNamespace = PathParts[2];

        if (SubNamespace == TEXT("personality"))
        {
            if (PathParts.Num() >= 5 && PathParts[3] == TEXT("traits"))
            {
                return ReadPersonalityTrait(PathParts[4]);
            }
            else if (PathParts.Num() >= 4 && PathParts[3] == TEXT("state"))
            {
                return ReadPersonalityState();
            }
        }
        else if (SubNamespace == TEXT("neurochemical"))
        {
            if (PathParts.Num() >= 5 && PathParts[3] == TEXT("levels"))
            {
                return ReadNeurochemicalLevel(PathParts[4]);
            }
            else if (PathParts.Num() >= 4 && PathParts[3] == TEXT("emotional_state"))
            {
                return ReadEmotionalState();
            }
        }
        else if (SubNamespace == TEXT("cognitive"))
        {
            if (PathParts.Num() >= 4)
            {
                return ReadCognitiveState(PathParts[3]);
            }
        }
        else if (SubNamespace == TEXT("performance"))
        {
            if (PathParts.Num() >= 4)
            {
                if (PathParts[3] == TEXT("metrics"))
                {
                    return ReadPerformanceMetrics();
                }
                else if (PathParts[3] == TEXT("budgets"))
                {
                    return ReadPerformanceBudgets();
                }
            }
        }
        else if (SubNamespace == TEXT("visual"))
        {
            if (PathParts.Num() >= 4)
            {
                if (PathParts[3] == TEXT("animation"))
                {
                    return ReadAnimationState();
                }
                else if (PathParts[3] == TEXT("appearance") && PathParts.Num() >= 5)
                {
                    return ReadAppearance(PathParts[4]);
                }
            }
        }
        else if (SubNamespace == TEXT("batch"))
        {
            if (PathParts.Num() >= 4 && PathParts[3] == TEXT("results"))
            {
                return LastBatchResults.IsEmpty()
                    ? FailRead(TEXT("no batch query has been run"))
                    : LastBatchResults;
            }
        }
        else if (SubNamespace == TEXT("narrative"))
        {
            if (PathParts.Num() == 4 && PathParts[3] == TEXT("diary"))
            {
                if (!AtomSpaceClient)
                {
                    return FailRead(TEXT("not available: no AtomSpace client holds diary entries"));
                }
                const TArray<FString> Ids = ListDiaryEntries();
                // An empty directory is a valid read; return a newline so it is not treated as missing.
                return Ids.Num() > 0 ? FString::Join(Ids, TEXT("\n")) : FString(TEXT("\n"));
            }
            if (PathParts.Num() >= 5)
            {
                if (PathParts[3] == TEXT("diary"))
                {
                    return ReadDiaryEntry(PathParts[4]);
                }
                else if (PathParts[3] == TEXT("insights"))
                {
                    return ReadInsight(PathParts[4]);
                }
            }
        }
    }

    return TEXT("");
}

bool UAvatar9PServer::WriteToPath(const FString& Path, const FString& Value)
{
    TArray<FString> PathParts = ParsePath(Path);

    if (PathParts.Num() < 2) return false;

    FString Namespace = PathParts[1];

    if (Namespace == TEXT("avatar"))
    {
        if (PathParts.Num() < 3) return false;
        FString SubNamespace = PathParts[2];

        if (SubNamespace == TEXT("personality"))
        {
            if (PathParts.Num() >= 5 && PathParts[3] == TEXT("traits"))
            {
                float FloatValue = FCString::Atof(*Value);
                return WritePersonalityTrait(PathParts[4], FloatValue);
            }
        }
        else if (SubNamespace == TEXT("neurochemical"))
        {
            if (PathParts.Num() >= 5 && PathParts[3] == TEXT("levels"))
            {
                float FloatValue = FCString::Atof(*Value);
                return WriteNeurochemicalLevel(PathParts[4], FloatValue);
            }
        }
        else if (SubNamespace == TEXT("visual"))
        {
            if (PathParts.Num() >= 5 && PathParts[3] == TEXT("appearance"))
            {
                return WriteAppearance(PathParts[4], Value);
            }
        }
        else if (SubNamespace == TEXT("batch"))
        {
            if (PathParts.Num() >= 4 && PathParts[3] == TEXT("query"))
            {
                // Process batch query - results are served from batch/results
                LastBatchResults = ProcessBatchQuery(Value);
                return true;
            }
        }
    }

    return false;
}

// ===== Personality Namespace Handlers =====

FString UAvatar9PServer::ReadPersonalityTrait(const FString& TraitName)
{
    if (!PersonalitySystem)
    {
        return FailRead(TEXT("not available: no personality system"));
    }

    FSuperHotGirlTrait SHGTrait = PersonalitySystem->GetSuperHotGirlTrait();
    FHyperChaoticTrait HCTrait = PersonalitySystem->GetHyperChaoticTrait();

    if (TraitName == TEXT("confidence"))
    {
        return FString::Printf(TEXT("%.4f"), SHGTrait.Confidence);
    }
    else if (TraitName == TEXT("charm"))
    {
        return FString::Printf(TEXT("%.4f"), SHGTrait.Charm);
    }
    else if (TraitName == TEXT("playfulness"))
    {
        return FString::Printf(TEXT("%.4f"), SHGTrait.Playfulness);
    }
    else if (TraitName == TEXT("elegance"))
    {
        return FString::Printf(TEXT("%.4f"), SHGTrait.Elegance);
    }
    else if (TraitName == TEXT("expressiveness"))
    {
        return FString::Printf(TEXT("%.4f"), SHGTrait.Expressiveness);
    }
    else if (TraitName == TEXT("unpredictability"))
    {
        return FString::Printf(TEXT("%.4f"), HCTrait.UnpredictabilityFactor);
    }
    else if (TraitName == TEXT("volatility"))
    {
        return FString::Printf(TEXT("%.4f"), HCTrait.EmotionalVolatility);
    }
    else if (TraitName == TEXT("impulsivity"))
    {
        return FString::Printf(TEXT("%.4f"), HCTrait.ImpulsivityLevel);
    }
    else if (TraitName == TEXT("chaos_factor"))
    {
        return FString::Printf(TEXT("%.4f"), HCTrait.GlitchEffectProbability);
    }

    return FailRead(FString::Printf(TEXT("unknown personality trait '%s'"), *TraitName));
}

bool UAvatar9PServer::WritePersonalityTrait(const FString& TraitName, float Value)
{
    if (!PersonalitySystem)
    {
        return false;
    }

    Value = FMath::Clamp(Value, 0.0f, 1.0f);

    if (TraitName == TEXT("confidence") || TraitName == TEXT("charm") || TraitName == TEXT("playfulness"))
    {
        FSuperHotGirlTrait CurrentTrait = PersonalitySystem->GetSuperHotGirlTrait();
        float Confidence = TraitName == TEXT("confidence") ? Value : CurrentTrait.Confidence;
        float Charm = TraitName == TEXT("charm") ? Value : CurrentTrait.Charm;
        float Playfulness = TraitName == TEXT("playfulness") ? Value : CurrentTrait.Playfulness;
        PersonalitySystem->SetSuperHotGirlParameters(Confidence, Charm, Playfulness);
        return true;
    }
    else if (TraitName == TEXT("unpredictability") || TraitName == TEXT("volatility") || TraitName == TEXT("impulsivity"))
    {
        FHyperChaoticTrait CurrentTrait = PersonalitySystem->GetHyperChaoticTrait();
        float Unpredictability = TraitName == TEXT("unpredictability") ? Value : CurrentTrait.UnpredictabilityFactor;
        float Volatility = TraitName == TEXT("volatility") ? Value : CurrentTrait.EmotionalVolatility;
        float Impulsivity = TraitName == TEXT("impulsivity") ? Value : CurrentTrait.ImpulsivityLevel;
        PersonalitySystem->SetHyperChaoticParameters(Unpredictability, Volatility, Impulsivity);
        return true;
    }

    return false;
}

FString UAvatar9PServer::ReadPersonalityState()
{
    if (!PersonalitySystem)
    {
        return FailRead(TEXT("not available: no personality system"));
    }

    FSuperHotGirlTrait SHGTrait = PersonalitySystem->GetSuperHotGirlTrait();
    FHyperChaoticTrait HCTrait = PersonalitySystem->GetHyperChaoticTrait();
    FVector2D EmotionalTendency = PersonalitySystem->GetEmotionalTendency();

    return FString::Printf(
        TEXT("{\"confidence\":%.4f,\"charm\":%.4f,\"playfulness\":%.4f,")
        TEXT("\"elegance\":%.4f,\"expressiveness\":%.4f,")
        TEXT("\"unpredictability\":%.4f,\"volatility\":%.4f,")
        TEXT("\"valence\":%.4f,\"arousal\":%.4f,")
        TEXT("\"predictability\":%.4f,\"social_engagement\":%.4f}"),
        SHGTrait.Confidence, SHGTrait.Charm, SHGTrait.Playfulness,
        SHGTrait.Elegance, SHGTrait.Expressiveness,
        HCTrait.UnpredictabilityFactor, HCTrait.EmotionalVolatility,
        EmotionalTendency.X, EmotionalTendency.Y,
        PersonalitySystem->GetBehavioralPredictability(),
        PersonalitySystem->GetSocialEngagementLevel()
    );
}

// ===== Neurochemical Namespace Handlers =====

FString UAvatar9PServer::ReadNeurochemicalLevel(const FString& ChemicalName)
{
    if (!NeurochemicalSystem)
    {
        return FailRead(TEXT("not available: no neurochemical system"));
    }

    FNeurochemicalState State = NeurochemicalSystem->GetCurrentState();

    if (ChemicalName == TEXT("dopamine"))
    {
        return FString::Printf(TEXT("%.4f"), State.DopamineLevel);
    }
    else if (ChemicalName == TEXT("serotonin"))
    {
        return FString::Printf(TEXT("%.4f"), State.SerotoninLevel);
    }
    else if (ChemicalName == TEXT("norepinephrine"))
    {
        return FString::Printf(TEXT("%.4f"), State.NorepinephrineLevel);
    }
    else if (ChemicalName == TEXT("oxytocin"))
    {
        return FString::Printf(TEXT("%.4f"), State.OxytocinLevel);
    }
    else if (ChemicalName == TEXT("cortisol"))
    {
        return FString::Printf(TEXT("%.4f"), State.CortisolLevel);
    }
    else if (ChemicalName == TEXT("endorphins"))
    {
        return FString::Printf(TEXT("%.4f"), State.EndorphinsLevel);
    }
    else if (ChemicalName == TEXT("gaba"))
    {
        return FString::Printf(TEXT("%.4f"), State.GABALevel);
    }
    else if (ChemicalName == TEXT("glutamate"))
    {
        return FString::Printf(TEXT("%.4f"), State.GlutamateLevel);
    }
    else if (ChemicalName == TEXT("acetylcholine"))
    {
        return FString::Printf(TEXT("%.4f"), State.AcetylcholineLevel);
    }

    return FailRead(FString::Printf(TEXT("unknown neurochemical '%s'"), *ChemicalName));
}

bool UAvatar9PServer::WriteNeurochemicalLevel(const FString& ChemicalName, float Value)
{
    if (!NeurochemicalSystem)
    {
        return false;
    }

    Value = FMath::Clamp(Value, 0.0f, 1.0f);

    ENeurochemicalType Type;
    if (ChemicalName == TEXT("dopamine"))
    {
        Type = ENeurochemicalType::Dopamine;
    }
    else if (ChemicalName == TEXT("serotonin"))
    {
        Type = ENeurochemicalType::Serotonin;
    }
    else if (ChemicalName == TEXT("norepinephrine"))
    {
        Type = ENeurochemicalType::Norepinephrine;
    }
    else if (ChemicalName == TEXT("oxytocin"))
    {
        Type = ENeurochemicalType::Oxytocin;
    }
    else if (ChemicalName == TEXT("cortisol"))
    {
        Type = ENeurochemicalType::Cortisol;
    }
    else if (ChemicalName == TEXT("endorphins"))
    {
        Type = ENeurochemicalType::Endorphins;
    }
    else if (ChemicalName == TEXT("gaba"))
    {
        Type = ENeurochemicalType::GABA;
    }
    else if (ChemicalName == TEXT("glutamate"))
    {
        Type = ENeurochemicalType::Glutamate;
    }
    else if (ChemicalName == TEXT("acetylcholine"))
    {
        Type = ENeurochemicalType::Acetylcholine;
    }
    else
    {
        return false;
    }

    NeurochemicalSystem->SetNeurochemicalLevel(Type, Value);
    return true;
}

FString UAvatar9PServer::ReadEmotionalState()
{
    if (!NeurochemicalSystem)
    {
        return FailRead(TEXT("not available: no neurochemical system"));
    }

    FEmotionalChemistry Emotions = NeurochemicalSystem->GetEmotionalChemistry();

    return FString::Printf(
        TEXT("{\"happiness\":%.4f,\"excitement\":%.4f,\"calmness\":%.4f,")
        TEXT("\"anxiety\":%.4f,\"affection\":%.4f,\"focus\":%.4f,")
        TEXT("\"motivation\":%.4f,\"energy\":%.4f,\"social_desire\":%.4f,")
        TEXT("\"clarity\":%.4f,\"stability\":%.4f}"),
        Emotions.Happiness, Emotions.Excitement, Emotions.Calmness,
        Emotions.Anxiety, Emotions.Affection, Emotions.Focus,
        NeurochemicalSystem->GetMotivationLevel(),
        NeurochemicalSystem->GetEnergyLevel(),
        NeurochemicalSystem->GetSocialDesire(),
        NeurochemicalSystem->GetCognitiveClarity(),
        NeurochemicalSystem->GetEmotionalStability()
    );
}

// ===== Narrative Namespace Handlers =====

FString UAvatar9PServer::ReadDiaryEntry(const FString& EntryId)
{
    if (!AtomSpaceClient)
    {
        return FailRead(TEXT("not available: no AtomSpace client holds diary entries"));
    }

    FDiaryEntry Entry;
    if (!AtomSpaceClient->TryGetDiaryEntry(EntryId, Entry))
    {
        return FailRead(FString::Printf(TEXT("diary entry '%s' not found"), *EntryId));
    }

    TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("id"), Entry.EntryId);
    Json->SetStringField(TEXT("content"), Entry.Content);
    Json->SetStringField(TEXT("timestamp"), Entry.Timestamp.ToIso8601());
    Json->SetStringField(TEXT("emotional_context"), Entry.EmotionalContext);
    Json->SetNumberField(TEXT("valence"), Entry.EmotionalValence);
    Json->SetNumberField(TEXT("arousal"), Entry.EmotionalArousal);
    TArray<TSharedPtr<FJsonValue>> Tags;
    for (const FString& Tag : Entry.Tags)
    {
        Tags.Add(MakeShared<FJsonValueString>(Tag));
    }
    Json->SetArrayField(TEXT("tags"), Tags);
    return JsonToString(Json);
}

TArray<FString> UAvatar9PServer::ListDiaryEntries()
{
    return AtomSpaceClient ? AtomSpaceClient->ListDiaryEntryIds() : TArray<FString>();
}

FString UAvatar9PServer::ReadInsight(const FString& InsightId)
{
    if (!AtomSpaceClient)
    {
        return FailRead(TEXT("not available: no AtomSpace client holds insights"));
    }

    FInsight Insight;
    if (!AtomSpaceClient->TryGetInsight(InsightId, Insight))
    {
        return FailRead(FString::Printf(TEXT("insight '%s' not found"), *InsightId));
    }

    TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("id"), Insight.InsightId);
    Json->SetStringField(TEXT("insight"), Insight.Content);
    Json->SetStringField(TEXT("category"), Insight.Category);
    Json->SetNumberField(TEXT("confidence"), Insight.Confidence);
    Json->SetStringField(TEXT("discovered_at"), Insight.DiscoveredAt.ToIso8601());
    Json->SetBoolField(TEXT("validated"), Insight.bIsValidated);
    return JsonToString(Json);
}

// ===== Visual Namespace Handlers =====

FString UAvatar9PServer::ReadAppearance(const FString& Parameter)
{
    if (!AvatarComponent)
    {
        return FailRead(TEXT("not available: no avatar mesh component"));
    }

    const FHashedMaterialParameterInfo ParamInfo{FName(*Parameter)};
    for (int32 i = 0; i < AvatarComponent->GetNumMaterials(); ++i)
    {
        UMaterialInterface* Material = AvatarComponent->GetMaterial(i);
        float Value = 0.0f;
        if (Material && Material->GetScalarParameterValue(ParamInfo, Value))
        {
            return FString::Printf(TEXT("%.4f"), Value);
        }
    }

    return FailRead(FString::Printf(TEXT("appearance parameter '%s' not found on avatar materials"), *Parameter));
}

bool UAvatar9PServer::WriteAppearance(const FString& Parameter, const FString& Value)
{
    if (!AvatarComponent)
    {
        LastPathError = TEXT("not available: no avatar mesh component");
        return false;
    }

    float FloatValue = 0.0f;
    if (!FDefaultValueHelper::ParseFloat(Value.TrimStartAndEnd(), FloatValue))
    {
        LastPathError = FString::Printf(TEXT("appearance value '%s' is not a number"), *Value);
        return false;
    }

    // Only parameters that exist on the avatar's materials can be applied.
    const FHashedMaterialParameterInfo ParamInfo{FName(*Parameter)};
    bool bParameterExists = false;
    for (int32 i = 0; i < AvatarComponent->GetNumMaterials() && !bParameterExists; ++i)
    {
        UMaterialInterface* Material = AvatarComponent->GetMaterial(i);
        float Existing = 0.0f;
        bParameterExists = Material && Material->GetScalarParameterValue(ParamInfo, Existing);
    }
    if (!bParameterExists)
    {
        LastPathError = FString::Printf(TEXT("appearance parameter '%s' not found on avatar materials"), *Parameter);
        return false;
    }

    AvatarComponent->SetScalarParameterValueOnMaterials(FName(*Parameter), FloatValue);
    return true;
}

FString UAvatar9PServer::ReadAnimationState()
{
    if (!AvatarComponent)
    {
        return FailRead(TEXT("not available: no avatar mesh component"));
    }

    UAnimInstance* AnimInstance = AvatarComponent->GetAnimInstance();
    if (!AnimInstance)
    {
        return FailRead(TEXT("not available: avatar mesh has no anim instance"));
    }

    UAnimMontage* Montage = AnimInstance->GetCurrentActiveMontage();

    TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("anim_instance"), AnimInstance->GetClass()->GetName());
    Json->SetBoolField(TEXT("playing_montage"), AnimInstance->IsAnyMontagePlaying());
    Json->SetStringField(TEXT("montage"), Montage ? Montage->GetName() : FString());
    Json->SetNumberField(TEXT("montage_position"), Montage ? AnimInstance->Montage_GetPosition(Montage) : 0.0f);
    return JsonToString(Json);
}

// ===== Cognitive Namespace Handlers =====

FString UAvatar9PServer::ReadCognitiveState(const FString& Parameter)
{
    return FailRead(FString::Printf(
        TEXT("cognitive parameter '%s' not available: no cognitive system is wired to the 9P server"), *Parameter));
}

// ===== Performance Namespace Handlers =====

FString UAvatar9PServer::ReadPerformanceMetrics()
{
    // Only measured values: the engine's current frame delta and this server's queue depth.
    const double DeltaSeconds = FApp::GetDeltaTime();
    TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("frame_time_ms"), DeltaSeconds * 1000.0);
    Json->SetNumberField(TEXT("fps"), DeltaSeconds > 0.0 ? 1.0 / DeltaSeconds : 0.0);
    Json->SetNumberField(TEXT("pending_requests"), PendingRequests.Num());
    return JsonToString(Json);
}

FString UAvatar9PServer::ReadPerformanceBudgets()
{
    return TEXT("{\"cpu_budget_ms\":10.0,\"gpu_budget_ms\":6.0,\"memory_mb\":512,\"network_kbps\":1000}");
}

// ===== Batch Operations =====

FString UAvatar9PServer::ProcessBatchQuery(const FString& QueryList)
{
    // Parse newline-separated list of paths
    TArray<FString> Paths;
    QueryList.ParseIntoArray(Paths, TEXT("\n"));

    TArray<FString> Results;
    for (const FString& Path : Paths)
    {
        FString TrimmedPath = Path.TrimStartAndEnd();
        if (!TrimmedPath.IsEmpty())
        {
            LastPathError.Reset();
            FString Value = ReadFromPath(TrimmedPath);
            if (Value.IsEmpty())
            {
                Results.Add(FString::Printf(TEXT("%s!error:%s"), *TrimmedPath,
                    LastPathError.IsEmpty() ? TEXT("path not found") : *LastPathError));
            }
            else
            {
                Results.Add(FString::Printf(TEXT("%s=%s"), *TrimmedPath, *Value));
            }
        }
    }

    return FString::Join(Results, TEXT("\n"));
}

FString UAvatar9PServer::ProcessBatchWrite(const FString& WriteList)
{
    // Parse lines of "path=value"
    TArray<FString> Lines;
    WriteList.ParseIntoArray(Lines, TEXT("\n"));

    TArray<FString> Results;
    for (const FString& Line : Lines)
    {
        FString TrimmedLine = Line.TrimStartAndEnd();
        int32 EqualsPos;
        if (TrimmedLine.FindChar('=', EqualsPos))
        {
            FString Path = TrimmedLine.Left(EqualsPos);
            FString Value = TrimmedLine.RightChop(EqualsPos + 1);
            bool bSuccess = WriteToPath(Path, Value);
            Results.Add(FString::Printf(TEXT("%s:%s"), *Path, bSuccess ? TEXT("ok") : TEXT("error")));
        }
    }

    return FString::Join(Results, TEXT("\n"));
}

// ===== Message Coalescing =====

void UAvatar9PServer::EnableMessageCoalescing(bool bEnable)
{
    bEnableCoalescing = bEnable;
}

void UAvatar9PServer::SetCoalescingInterval(float IntervalSeconds)
{
    CoalescingInterval = FMath::Max(0.001f, IntervalSeconds);
}

void UAvatar9PServer::ProcessPendingMessages()
{
    if (PendingRequests.Num() == 0)
    {
        return;
    }

    // Process all pending requests
    for (const F9PRequest& Request : PendingRequests)
    {
        HandleRequest(Request);
    }

    PendingRequests.Empty();
}

// ===== Path Parsing Helpers =====

TArray<FString> UAvatar9PServer::ParsePath(const FString& Path)
{
    TArray<FString> Parts;
    Path.ParseIntoArray(Parts, TEXT("/"));
    return Parts;
}

FString UAvatar9PServer::GetNamespaceRoot(const FString& Path)
{
    TArray<FString> Parts = ParsePath(Path);
    if (Parts.Num() >= 2)
    {
        return Parts[1];
    }
    return TEXT("");
}

FString UAvatar9PServer::GetRelativePath(const FString& Path)
{
    if (Path.StartsWith(NamespaceRoot))
    {
        return Path.RightChop(NamespaceRoot.Len());
    }
    return Path;
}

FString UAvatar9PServer::SerializeToJson(const FString& Path)
{
    return ReadFromPath(Path);
}

bool UAvatar9PServer::DeserializeFromJson(const FString& Path, const FString& JsonValue)
{
    return WriteToPath(Path, JsonValue);
}
