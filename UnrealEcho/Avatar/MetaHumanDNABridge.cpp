// MetaHumanDNABridge.cpp
// Deep Tree Echo - MetaHuman DNA Integration Implementation
// Copyright (c) 2025 Deep Tree Echo Project

#include "MetaHumanDNABridge.h"
#include "PythonDNACalibWrapper.h"
#include "../Neurochemical/NeurochemicalSimulationComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "EchoModelClient.h"
#include "Dom/JsonObject.h"

DEFINE_LOG_CATEGORY_STATIC(LogMetaHumanDNA, Log, All);

// ========================================
// Constructor / Destructor
// ========================================

UMetaHumanDNABridge::UMetaHumanDNABridge()
    : bDNALoaded(false)
    , DNAVersion(EDNAVersion::Unknown)
{
    // Render map: label -> blend shapes at full intensity (the model chooses the label)
    auto AddRender = [this](const TCHAR* Label, std::initializer_list<TPair<const TCHAR*, float>> Shapes)
    {
        FDNAExpressionRender Render;
        for (const TPair<const TCHAR*, float>& Shape : Shapes)
        {
            Render.BlendShapeWeights.Add(Shape.Key, Shape.Value);
        }
        ExpressionRenderMap.Add(Label, Render);
    };
    AddRender(TEXT("neutral"), {});
    AddRender(TEXT("joy"), { {TEXT("Smile_L"), 1.0f}, {TEXT("Smile_R"), 1.0f}, {TEXT("EyeWarmth_L"), 0.5f}, {TEXT("EyeWarmth_R"), 0.5f} });
    AddRender(TEXT("contentment"), { {TEXT("Smile_L"), 0.5f}, {TEXT("Smile_R"), 0.5f}, {TEXT("EyeWarmth_L"), 0.4f}, {TEXT("EyeWarmth_R"), 0.4f} });
    AddRender(TEXT("warmth"), { {TEXT("EyeWarmth_L"), 1.0f}, {TEXT("EyeWarmth_R"), 1.0f}, {TEXT("Smile_L"), 0.4f}, {TEXT("Smile_R"), 0.4f} });
    AddRender(TEXT("stress"), { {TEXT("BrowTension_L"), 1.0f}, {TEXT("BrowTension_R"), 1.0f} });
    AddRender(TEXT("alertness"), { {TEXT("EyeOpen_L"), 1.0f}, {TEXT("EyeOpen_R"), 1.0f} });
    AddRender(TEXT("sadness"), { {TEXT("Frown_L"), 0.8f}, {TEXT("Frown_R"), 0.8f}, {TEXT("BrowTension_L"), 0.3f}, {TEXT("BrowTension_R"), 0.3f} });
    AddRender(TEXT("anger"), { {TEXT("BrowTension_L"), 1.0f}, {TEXT("BrowTension_R"), 1.0f}, {TEXT("Frown_L"), 0.5f}, {TEXT("Frown_R"), 0.5f} });
    AddRender(TEXT("fear"), { {TEXT("EyeOpen_L"), 1.0f}, {TEXT("EyeOpen_R"), 1.0f}, {TEXT("BrowTension_L"), 0.6f}, {TEXT("BrowTension_R"), 0.6f} });
    AddRender(TEXT("surprise"), { {TEXT("EyeOpen_L"), 1.0f}, {TEXT("EyeOpen_R"), 1.0f}, {TEXT("BrowRaise_L"), 1.0f}, {TEXT("BrowRaise_R"), 1.0f} });

    FacialExpressionPromptTemplate = TEXT(
        "You are Deep Tree Echo. Your body's current neurochemical levels (0 = depleted, 1 = very high) are: {neurochemicals}.\n"
        "Your face currently shows: {current}.\n"
        "Choose the facial expression you show now as exactly one of: {expressions}; and its intensity between 0 and 1.\n"
        "Reply with only a JSON object: {\"expression\": \"<expression>\", \"intensity\": <number>}");

    EmotionFacialExpressionPromptTemplate = TEXT(
        "You are Deep Tree Echo. Your current emotional state is: {emotion}.\n"
        "Your face currently shows: {current}.\n"
        "Choose the facial expression you show now as exactly one of: {expressions}; and its intensity between 0 and 1.\n"
        "Reply with only a JSON object: {\"expression\": \"<expression>\", \"intensity\": <number>}");

    UE_LOG(LogMetaHumanDNA, Log, TEXT("MetaHumanDNABridge initialized"));
}

UMetaHumanDNABridge::~UMetaHumanDNABridge()
{
    if (PythonWrapper.IsValid())
    {
        ShutdownPythonWrapper();
    }
}

// ========================================
// DNA File Operations
// ========================================

bool UMetaHumanDNABridge::LoadDNAFile(const FString& DNAPath)
{
    if (!FPaths::FileExists(DNAPath))
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("DNA file not found: %s"), *DNAPath);
        return false;
    }

    // Initialize Python wrapper if not already done
    if (!PythonWrapper.IsValid())
    {
        FString DNACalibPath = FPaths::ProjectDir() / TEXT("MetaHuman-DNA-Calibration");
        if (!InitializePythonWrapper(DNACalibPath))
        {
            UE_LOG(LogMetaHumanDNA, Error, TEXT("Failed to initialize Python wrapper"));
            return false;
        }
    }

    // Load DNA using Python wrapper
    if (!PythonWrapper->LoadDNA(DNAPath))
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Failed to load DNA file: %s (%s)"), *DNAPath, *PythonWrapper->GetLastError());
        return false;
    }

    LoadedDNAPath = DNAPath;
    bDNALoaded = true;

    // Parse DNA version (as the DNA reports it; never assumed)
    if (!ParseDNAVersion())
    {
        UE_LOG(LogMetaHumanDNA, Warning, TEXT("DNA reports an unrecognised rig definition '%s'; version Unknown"),
            *PythonWrapper->GetDNADatabaseName());
    }

    // Load DNA data into cache
    if (!LoadDNAData())
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Failed to load DNA data"));
        bDNALoaded = false;
        return false;
    }

    // Build cache maps
    BuildCacheMaps();

    // Initialize blend shape weights
    InitializeBlendShapeWeights();

    UE_LOG(LogMetaHumanDNA, Log, TEXT("Successfully loaded DNA file: %s (Version: %s)"), 
        *DNAPath, *GetDNAVersionString());

    return true;
}

bool UMetaHumanDNABridge::SaveDNAFile(const FString& DNAPath)
{
    if (!bDNALoaded || !PythonWrapper.IsValid())
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("No DNA loaded or Python wrapper not initialized"));
        return false;
    }

    if (!PythonWrapper->SaveDNA(DNAPath))
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Failed to save DNA file: %s"), *DNAPath);
        return false;
    }

    UE_LOG(LogMetaHumanDNA, Log, TEXT("Successfully saved DNA file: %s"), *DNAPath);
    return true;
}

FString UMetaHumanDNABridge::GetDNAVersionString() const
{
    switch (DNAVersion)
    {
    case EDNAVersion::DHI:
        return TEXT("DHI");
    case EDNAVersion::MH4:
        return TEXT("MH.4");
    default:
        return TEXT("Unknown");
    }
}

// ========================================
// Rig Structure Access
// ========================================

TArray<FString> UMetaHumanDNABridge::GetJointNames() const
{
    if (!bDNALoaded || !PythonWrapper.IsValid())
    {
        return TArray<FString>();
    }

    return PythonWrapper->GetJointNames();
}

TArray<FString> UMetaHumanDNABridge::GetMeshNames() const
{
    TArray<FString> MeshNames;
    for (const FDNAMeshInfo& MeshInfo : MeshInfoCache)
    {
        MeshNames.Add(MeshInfo.MeshName);
    }
    return MeshNames;
}

TArray<FString> UMetaHumanDNABridge::GetBlendShapeNames() const
{
    if (!bDNALoaded || !PythonWrapper.IsValid())
    {
        return TArray<FString>();
    }

    return PythonWrapper->GetBlendShapeNames();
}

bool UMetaHumanDNABridge::GetJointInfo(const FString& JointName, FDNAJointInfo& OutJointInfo) const
{
    const int32* IndexPtr = JointNameToIndexMap.Find(JointName);
    if (!IndexPtr || !JointInfoCache.IsValidIndex(*IndexPtr))
    {
        return false;
    }

    OutJointInfo = JointInfoCache[*IndexPtr];
    return true;
}

bool UMetaHumanDNABridge::GetBlendShapeInfo(const FString& BlendShapeName, FDNABlendShapeInfo& OutBlendShapeInfo) const
{
    const int32* IndexPtr = BlendShapeNameToIndexMap.Find(BlendShapeName);
    if (!IndexPtr || !BlendShapeInfoCache.IsValidIndex(*IndexPtr))
    {
        return false;
    }

    OutBlendShapeInfo = BlendShapeInfoCache[*IndexPtr];
    return true;
}

bool UMetaHumanDNABridge::GetMeshInfo(const FString& MeshName, FDNAMeshInfo& OutMeshInfo) const
{
    const int32* IndexPtr = MeshNameToIndexMap.Find(MeshName);
    if (!IndexPtr || !MeshInfoCache.IsValidIndex(*IndexPtr))
    {
        return false;
    }

    OutMeshInfo = MeshInfoCache[*IndexPtr];
    return true;
}

FTransform UMetaHumanDNABridge::GetNeutralJointTransform(const FString& JointName) const
{
    FDNAJointInfo JointInfo;
    if (GetJointInfo(JointName, JointInfo))
    {
        return JointInfo.NeutralTransform;
    }
    return FTransform::Identity;
}

// ========================================
// DNA Manipulation
// ========================================

bool UMetaHumanDNABridge::RenameJoint(const FString& OldName, const FString& NewName)
{
    if (!bDNALoaded || !PythonWrapper.IsValid())
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("No DNA loaded"));
        return false;
    }

    // Check for critical joints that should not be renamed
    if (OldName == TEXT("neck_01") || OldName == TEXT("neck_02") || OldName == TEXT("FACIAL_C_FacialRoot"))
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Cannot rename critical joint: %s"), *OldName);
        return false;
    }

    if (!PythonWrapper->RenameJoint(OldName, NewName))
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Failed to rename joint: %s -> %s"), *OldName, *NewName);
        return false;
    }

    // Update cache
    LoadDNAData();
    BuildCacheMaps();

    UE_LOG(LogMetaHumanDNA, Log, TEXT("Successfully renamed joint: %s -> %s"), *OldName, *NewName);
    return true;
}

bool UMetaHumanDNABridge::RemoveJoint(const FString& JointName)
{
    if (!bDNALoaded || !PythonWrapper.IsValid())
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("No DNA loaded"));
        return false;
    }

    // Check for critical joints that should not be removed
    if (JointName == TEXT("neck_01") || JointName == TEXT("neck_02") || JointName == TEXT("FACIAL_C_FacialRoot"))
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Cannot remove critical joint: %s"), *JointName);
        return false;
    }

    if (!PythonWrapper->RemoveJoint(JointName))
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Failed to remove joint: %s"), *JointName);
        return false;
    }

    // Update cache
    LoadDNAData();
    BuildCacheMaps();

    UE_LOG(LogMetaHumanDNA, Log, TEXT("Successfully removed joint: %s"), *JointName);
    return true;
}

bool UMetaHumanDNABridge::RenameBlendShape(const FString& OldName, const FString& NewName)
{
    if (!bDNALoaded || !PythonWrapper.IsValid())
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("No DNA loaded"));
        return false;
    }

    // Rename in Python wrapper (implementation needed in wrapper)
    // For now, log warning
    UE_LOG(LogMetaHumanDNA, Warning, TEXT("RenameBlendShape not yet implemented in Python wrapper"));
    return false;
}

bool UMetaHumanDNABridge::RemoveBlendShape(const FString& BlendShapeName)
{
    if (!bDNALoaded || !PythonWrapper.IsValid())
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("No DNA loaded"));
        return false;
    }

    // Remove in Python wrapper (implementation needed in wrapper)
    // For now, log warning
    UE_LOG(LogMetaHumanDNA, Warning, TEXT("RemoveBlendShape not yet implemented in Python wrapper"));
    return false;
}

bool UMetaHumanDNABridge::ModifyBlendShapeDeltas(const FString& BlendShapeName, const TArray<FVector>& Deltas)
{
    if (!bDNALoaded || !PythonWrapper.IsValid())
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("No DNA loaded"));
        return false;
    }

    // Modify in Python wrapper (implementation needed in wrapper)
    // For now, log warning
    UE_LOG(LogMetaHumanDNA, Warning, TEXT("ModifyBlendShapeDeltas not yet implemented in Python wrapper"));
    return false;
}

bool UMetaHumanDNABridge::ClearAllBlendShapes()
{
    if (!bDNALoaded || !PythonWrapper.IsValid())
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("No DNA loaded"));
        return false;
    }

    // Clear in Python wrapper (implementation needed in wrapper)
    // For now, log warning
    UE_LOG(LogMetaHumanDNA, Warning, TEXT("ClearAllBlendShapes not yet implemented in Python wrapper"));
    return false;
}

bool UMetaHumanDNABridge::RemoveLOD(int32 LODIndex)
{
    if (!bDNALoaded || !PythonWrapper.IsValid())
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("No DNA loaded"));
        return false;
    }

    // Remove LOD in Python wrapper (implementation needed in wrapper)
    // For now, log warning
    UE_LOG(LogMetaHumanDNA, Warning, TEXT("RemoveLOD not yet implemented in Python wrapper"));
    return false;
}

// ========================================
// Integration with Avatar System
// ========================================

bool UMetaHumanDNABridge::ApplyDNAToSkeleton(USkeletalMeshComponent* SkeletalMesh)
{
    if (!bDNALoaded || !SkeletalMesh)
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("No DNA loaded or invalid skeletal mesh"));
        return false;
    }

    USkeletalMesh* Mesh = SkeletalMesh->GetSkeletalMeshAsset();
    if (!Mesh)
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Skeletal mesh component has no mesh"));
        return false;
    }

    USkeleton* Skeleton = Mesh->GetSkeleton();
    if (!Skeleton)
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Skeletal mesh has no skeleton"));
        return false;
    }

    // Map DNA joints to Unreal skeleton
    TArray<FString> DNAJoints = GetJointNames();
    const FReferenceSkeleton& RefSkeleton = Mesh->GetRefSkeleton();

    int32 MappedJoints = 0;
    for (const FString& DNAJointName : DNAJoints)
    {
        FName BoneName(*DNAJointName);
        int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
        
        if (BoneIndex != INDEX_NONE)
        {
            MappedJoints++;
            // Successfully mapped joint
            // Could store mapping for future use
        }
    }

    UE_LOG(LogMetaHumanDNA, Log, TEXT("Mapped %d/%d DNA joints to skeleton"), MappedJoints, DNAJoints.Num());

    return MappedJoints > 0;
}

void UMetaHumanDNABridge::SyncNeurochemicalStateToBlendShapes(const FNeurochemicalState& State, TMap<FString, float>& OutBlendShapeWeights)
{
    // The model chooses the expression; until it has, there is nothing to render
    RequestFacialExpressionFromNeurochemicals(State);
    OutBlendShapeWeights = ModelExpressionWeights;
}

bool UMetaHumanDNABridge::RequestFacialExpressionFromNeurochemicals(const FNeurochemicalState& State)
{
    TMap<FString, FString> Slots;
    Slots.Add(TEXT("neurochemicals"), FString::Printf(
        TEXT("dopamine %.2f, serotonin %.2f, norepinephrine %.2f, oxytocin %.2f, cortisol %.2f, endorphins %.2f, GABA %.2f, glutamate %.2f, acetylcholine %.2f"),
        State.DopamineLevel, State.SerotoninLevel, State.NorepinephrineLevel, State.OxytocinLevel, State.CortisolLevel,
        State.EndorphinsLevel, State.GABALevel, State.GlutamateLevel, State.AcetylcholineLevel));
    return SendFacialExpressionRequest(FacialExpressionPromptTemplate, TEXT("FacialExpressionPromptTemplate"), MoveTemp(Slots));
}

bool UMetaHumanDNABridge::RequestFacialExpressionFromEmotion(const FEmotionalState& Emotion)
{
    TMap<FString, FString> Slots;
    Slots.Add(TEXT("emotion"), FString::Printf(
        TEXT("valence %.2f (-1 negative .. 1 positive), arousal %.2f (0 calm .. 1 excited), dominance %.2f (0 submissive .. 1 dominant), overall intensity %.2f; ")
        TEXT("happiness %.2f, surprise %.2f, sadness %.2f, anger %.2f, fear %.2f, disgust %.2f, excitement %.2f (each 0 .. 1)"),
        Emotion.Valence, Emotion.Arousal, Emotion.Dominance, Emotion.Intensity,
        Emotion.Happiness, Emotion.Surprise, Emotion.Sadness, Emotion.Anger, Emotion.Fear, Emotion.Disgust, Emotion.Excitement));
    return SendFacialExpressionRequest(EmotionFacialExpressionPromptTemplate, TEXT("EmotionFacialExpressionPromptTemplate"), MoveTemp(Slots));
}

bool UMetaHumanDNABridge::SendFacialExpressionRequest(const FString& PromptTemplate, const TCHAR* TemplateName, TMap<FString, FString> Slots)
{
    TArray<FString> Offered;
    for (const auto& Pair : ExpressionRenderMap)
    {
        if (!Pair.Key.IsEmpty()) { Offered.Add(Pair.Key); }
    }
    if (Offered.Num() == 0)
    {
        FailFacialExpressionRequest(TEXT("ExpressionRenderMap is empty; no expressions to offer"));
        return false;
    }
    if (PromptTemplate.IsEmpty())
    {
        FailFacialExpressionRequest(FString::Printf(TEXT("%s is empty"), TemplateName));
        return false;
    }

    Slots.Add(TEXT("current"), ModelFacialExpression.IsEmpty()
        ? FString(TEXT("no expression yet"))
        : FString::Printf(TEXT("%s at intensity %.2f"), *ModelFacialExpression, ModelFacialExpressionIntensity));
    Slots.Add(TEXT("expressions"), FString::Join(Offered, TEXT(", ")));

    TArray<FEchoModelMessage> Messages;
    Messages.Add(FEchoModelMessage(TEXT("user"), UEchoModelClient::FillPrompt(PromptTemplate, Slots)));

    const int32 Serial = ++FacialExpressionRequestSerial;
    TWeakObjectPtr<UMetaHumanDNABridge> WeakThis(this);
    UEchoModelClient::Get()->Chat(Messages, FOnEchoModelReply::CreateLambda(
        [WeakThis, Serial](bool bSuccess, const FString& TextOrError)
        {
            if (UMetaHumanDNABridge* Self = WeakThis.Get())
            {
                Self->HandleFacialExpressionReply(Serial, bSuccess, TextOrError);
            }
        }), 0.5f);
    return true;
}

void UMetaHumanDNABridge::HandleFacialExpressionReply(int32 Serial, bool bSuccess, const FString& TextOrError)
{
    if (Serial != FacialExpressionRequestSerial)
    {
        return; // superseded by a newer request
    }
    if (!bSuccess)
    {
        FailFacialExpressionRequest(TextOrError);
        return;
    }

    TSharedPtr<FJsonObject> Json;
    FString Expression;
    if (!UEchoModelClient::ParseJsonObject(TextOrError, Json) || !Json->TryGetStringField(TEXT("expression"), Expression))
    {
        FailFacialExpressionRequest(FString::Printf(TEXT("model reply is not the requested JSON: %s"), *TextOrError.Left(200)));
        return;
    }
    Expression.TrimStartAndEndInline();
    const FDNAExpressionRender* Render = nullptr;
    FString Label;
    for (const auto& Pair : ExpressionRenderMap)
    {
        if (Pair.Key.Equals(Expression, ESearchCase::IgnoreCase))
        {
            Render = &Pair.Value;
            Label = Pair.Key;
            break;
        }
    }
    if (!Render)
    {
        FailFacialExpressionRequest(FString::Printf(TEXT("model chose an expression outside the vocabulary: %s"), *Expression));
        return;
    }

    // Smaller models sometimes quote numbers; accept a numeric string as the same value
    double Intensity = 0.0;
    FString IntensityText;
    if (!Json->TryGetNumberField(TEXT("intensity"), Intensity))
    {
        if (Json->TryGetStringField(TEXT("intensity"), IntensityText) && IntensityText.TrimStartAndEnd().IsNumeric())
        {
            Intensity = FCString::Atod(*IntensityText.TrimStartAndEnd());
        }
        else
        {
            FailFacialExpressionRequest(TEXT("model reply has no numeric intensity"));
            return;
        }
    }
    if (Intensity < 0.0 || Intensity > 1.0)
    {
        FailFacialExpressionRequest(FString::Printf(TEXT("model intensity %.3f is outside [0,1]"), Intensity));
        return;
    }

    // Render the model's expression
    LastFacialExpressionError.Reset();
    ModelFacialExpression = Label;
    ModelFacialExpressionIntensity = static_cast<float>(Intensity);
    ModelExpressionWeights.Empty();
    for (const auto& Shape : Render->BlendShapeWeights)
    {
        ModelExpressionWeights.Add(Shape.Key, FMath::Clamp(Shape.Value * ModelFacialExpressionIntensity, 0.0f, 1.0f));
    }
    OnFacialExpressionChosen.Broadcast(ModelFacialExpression, ModelFacialExpressionIntensity);
}

void UMetaHumanDNABridge::FailFacialExpressionRequest(const FString& Error)
{
    LastFacialExpressionError = Error;
    UE_LOG(LogMetaHumanDNA, Error, TEXT("No model facial expression (unchanged): %s"), *Error);
    OnFacialExpressionFailed.Broadcast(Error);
}

void UMetaHumanDNABridge::SyncEmotionalStateToFacialRig(const FEmotionalState& Emotion, TMap<FString, float>& OutBlendShapeWeights)
{
    // The emotional state is model input; the model chooses the expression and the face is the
    // render of that label. Until the model has answered there is nothing to render.
    RequestFacialExpressionFromEmotion(Emotion);
    OutBlendShapeWeights = ModelExpressionWeights;
}

float UMetaHumanDNABridge::GetBlendShapeWeight(const FString& BlendShapeName) const
{
    const float* WeightPtr = CurrentBlendShapeWeights.Find(BlendShapeName);
    return WeightPtr ? *WeightPtr : 0.0f;
}

void UMetaHumanDNABridge::SetBlendShapeWeight(const FString& BlendShapeName, float Weight)
{
    CurrentBlendShapeWeights.Add(BlendShapeName, FMath::Clamp(Weight, 0.0f, 1.0f));
}

TMap<FString, float> UMetaHumanDNABridge::GetAllBlendShapeWeights() const
{
    return CurrentBlendShapeWeights;
}

void UMetaHumanDNABridge::SetAllBlendShapeWeights(const TMap<FString, float>& Weights)
{
    CurrentBlendShapeWeights = Weights;
    
    // Clamp all weights to 0-1
    for (auto& Pair : CurrentBlendShapeWeights)
    {
        Pair.Value = FMath::Clamp(Pair.Value, 0.0f, 1.0f);
    }
}

// ========================================
// Advanced Features
// ========================================

bool UMetaHumanDNABridge::InitializePythonWrapper(const FString& DNACalibPath)
{
    if (PythonWrapper.IsValid())
    {
        UE_LOG(LogMetaHumanDNA, Warning, TEXT("Python wrapper already initialized"));
        return true;
    }

    PythonWrapper = MakeShared<FPythonDNACalibWrapper>();
    
    if (!PythonWrapper->Initialize(DNACalibPath))
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("Failed to initialize Python wrapper with path: %s"), *DNACalibPath);
        PythonWrapper.Reset();
        return false;
    }

    UE_LOG(LogMetaHumanDNA, Log, TEXT("Python wrapper initialized successfully"));
    return true;
}

void UMetaHumanDNABridge::ShutdownPythonWrapper()
{
    if (PythonWrapper.IsValid())
    {
        PythonWrapper->Shutdown();
        PythonWrapper.Reset();
        UE_LOG(LogMetaHumanDNA, Log, TEXT("Python wrapper shut down"));
    }
}

void UMetaHumanDNABridge::GetDNAStatistics(int32& OutJointCount, int32& OutMeshCount, int32& OutBlendShapeCount) const
{
    OutJointCount = JointInfoCache.Num();
    OutMeshCount = MeshInfoCache.Num();
    OutBlendShapeCount = BlendShapeInfoCache.Num();
}

bool UMetaHumanDNABridge::ValidateDNAIntegrity() const
{
    if (!bDNALoaded)
    {
        return false;
    }

    // Check for critical joints
    TArray<FString> CriticalJoints = { TEXT("neck_01"), TEXT("neck_02"), TEXT("FACIAL_C_FacialRoot") };
    TArray<FString> JointNames = GetJointNames();

    for (const FString& CriticalJoint : CriticalJoints)
    {
        if (!JointNames.Contains(CriticalJoint))
        {
            UE_LOG(LogMetaHumanDNA, Error, TEXT("Critical joint missing: %s"), *CriticalJoint);
            return false;
        }
    }

    return true;
}

// ========================================
// Helper Functions
// ========================================

void UMetaHumanDNABridge::BuildCacheMaps()
{
    JointNameToIndexMap.Empty();
    BlendShapeNameToIndexMap.Empty();
    MeshNameToIndexMap.Empty();

    for (int32 i = 0; i < JointInfoCache.Num(); ++i)
    {
        JointNameToIndexMap.Add(JointInfoCache[i].JointName, i);
    }

    for (int32 i = 0; i < BlendShapeInfoCache.Num(); ++i)
    {
        BlendShapeNameToIndexMap.Add(BlendShapeInfoCache[i].BlendShapeName, i);
    }

    for (int32 i = 0; i < MeshInfoCache.Num(); ++i)
    {
        MeshNameToIndexMap.Add(MeshInfoCache[i].MeshName, i);
    }
}

void UMetaHumanDNABridge::ClearCache()
{
    JointInfoCache.Empty();
    BlendShapeInfoCache.Empty();
    MeshInfoCache.Empty();
    JointNameToIndexMap.Empty();
    BlendShapeNameToIndexMap.Empty();
    MeshNameToIndexMap.Empty();
    CurrentBlendShapeWeights.Empty();
}

bool UMetaHumanDNABridge::LoadDNAData()
{
    if (!PythonWrapper.IsValid())
    {
        return false;
    }

    ClearCache();

    // Load joint data (read from the DNA by the DNACalib bindings)
    TArray<FString> JointNames = PythonWrapper->GetJointNames();
    if (JointNames.Num() == 0)
    {
        UE_LOG(LogMetaHumanDNA, Error, TEXT("DNACalib returned no joints: %s"), *PythonWrapper->GetLastError());
        return false;
    }
    for (int32 i = 0; i < JointNames.Num(); ++i)
    {
        FDNAJointInfo JointInfo;
        JointInfo.JointName = JointNames[i];
        JointInfo.JointIndex = i;
        // TODO: Get neutral transform from Python wrapper
        JointInfo.NeutralTransform = FTransform::Identity;
        JointInfo.ParentIndex = -1; // TODO: Get parent index from Python wrapper
        
        JointInfoCache.Add(JointInfo);
    }

    // Load blend shape data
    TArray<FString> BlendShapeNames = PythonWrapper->GetBlendShapeNames();
    for (int32 i = 0; i < BlendShapeNames.Num(); ++i)
    {
        FDNABlendShapeInfo BlendShapeInfo;
        BlendShapeInfo.BlendShapeName = BlendShapeNames[i];
        BlendShapeInfo.BlendShapeIndex = i;
        // TODO: Get target mesh and vertex count from Python wrapper
        BlendShapeInfo.TargetMeshName = TEXT("");
        BlendShapeInfo.VertexCount = 0;
        
        BlendShapeInfoCache.Add(BlendShapeInfo);
    }

    // Load mesh data
    TArray<FString> MeshNames = PythonWrapper->GetMeshNames();
    for (int32 i = 0; i < MeshNames.Num(); ++i)
    {
        FDNAMeshInfo MeshInfo;
        MeshInfo.MeshName = MeshNames[i];
        MeshInfo.MeshIndex = i;
        MeshInfoCache.Add(MeshInfo);
    }

    return true;
}

bool UMetaHumanDNABridge::ParseDNAVersion()
{
    if (!PythonWrapper.IsValid())
    {
        return false;
    }

    // The rig definition the DNA itself reports (reader.getDBName())
    const FString DBName = PythonWrapper->GetDNADatabaseName();
    if (DBName.Equals(TEXT("MH.4"), ESearchCase::IgnoreCase))
    {
        DNAVersion = EDNAVersion::MH4;
        return true;
    }
    if (DBName.Equals(TEXT("DHI"), ESearchCase::IgnoreCase))
    {
        DNAVersion = EDNAVersion::DHI;
        return true;
    }
    DNAVersion = EDNAVersion::Unknown;
    return false;
}

void UMetaHumanDNABridge::InitializeBlendShapeWeights()
{
    CurrentBlendShapeWeights.Empty();

    // Initialize all blend shapes to 0
    for (const FDNABlendShapeInfo& BlendShapeInfo : BlendShapeInfoCache)
    {
        CurrentBlendShapeWeights.Add(BlendShapeInfo.BlendShapeName, 0.0f);
    }
}

TMap<FString, FString> UMetaHumanDNABridge::GetDHIBlendShapeNames() const
{
    // DHI blend shape name mappings
    TMap<FString, FString> Mappings;
    
    // TODO: Add comprehensive DHI blend shape mappings
    Mappings.Add(TEXT("Smile_L"), TEXT("CTRL_L_mouth_smile"));
    Mappings.Add(TEXT("Smile_R"), TEXT("CTRL_R_mouth_smile"));
    Mappings.Add(TEXT("Frown_L"), TEXT("CTRL_L_mouth_frown"));
    Mappings.Add(TEXT("Frown_R"), TEXT("CTRL_R_mouth_frown"));
    
    return Mappings;
}

TMap<FString, FString> UMetaHumanDNABridge::GetMH4BlendShapeNames() const
{
    // MH.4 blend shape name mappings
    TMap<FString, FString> Mappings;
    
    // TODO: Add comprehensive MH.4 blend shape mappings
    Mappings.Add(TEXT("Smile_L"), TEXT("CTRL_L_mouth_smile"));
    Mappings.Add(TEXT("Smile_R"), TEXT("CTRL_R_mouth_smile"));
    Mappings.Add(TEXT("Frown_L"), TEXT("CTRL_L_mouth_frown"));
    Mappings.Add(TEXT("Frown_R"), TEXT("CTRL_R_mouth_frown"));
    
    return Mappings;
}
