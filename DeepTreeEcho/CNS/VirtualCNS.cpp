// VirtualCNS.cpp
// Virtual Central Nervous System — Implementation
// Wires console controller directly to the DTE cognitive architecture

#include "CNS/VirtualCNS.h"
#include "Core/DeepTreeEchoCore.h"
#include "4ECognition/EmbodiedCognitionComponent.h"
#include "GameTraining/ReinforcementLearningBridge.h"
#include "GameTraining/GameControllerInterface.h"
#include "Attention/AttentionSystem.h"
#include "Sensorimotor/SensorimotorIntegration.h"
#include "Avatar/MetaHumanCNSBinding.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Math/UnrealMathUtility.h"
#include "Model/EchoModelClient.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace
{
    /** Reads a number field; small models sometimes quote numbers, so a numeric string counts too. */
    bool ReadReflexNumber(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, double& Out)
    {
        if (Obj->TryGetNumberField(Key, Out))
        {
            return true;
        }
        FString Text;
        if (Obj->TryGetStringField(Key, Text))
        {
            Text.TrimStartAndEndInline();
            if (Text.IsNumeric())
            {
                Out = FCString::Atod(*Text);
                return true;
            }
        }
        return false;
    }

    /** Matches a model-given name against an enum's names (case-insensitive). */
    template <typename TEnum>
    bool ParseReflexEnum(const FString& Name, TEnum& Out)
    {
        const UEnum* Enum = StaticEnum<TEnum>();
        const FString Trimmed = Name.TrimStartAndEnd();
        if (!Enum || Trimmed.IsEmpty())
        {
            return false;
        }
        for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
        {
            if (Enum->GetNameStringByIndex(Index).Equals(Trimmed, ESearchCase::IgnoreCase))
            {
                Out = static_cast<TEnum>(Enum->GetValueByIndex(Index));
                return true;
            }
        }
        return false;
    }

    template <typename TEnum>
    FString ReflexEnumNames(bool bSkipFirst)
    {
        TArray<FString> Names;
        if (const UEnum* Enum = StaticEnum<TEnum>())
        {
            for (int32 Index = bSkipFirst ? 1 : 0; Index < Enum->NumEnums() - 1; ++Index)
            {
                Names.Add(Enum->GetNameStringByIndex(Index));
            }
        }
        return FString::Join(Names, TEXT(", "));
    }

    bool ContainsIgnoreCase(const TArray<FString>& List, const FString& Value, FString& OutCanonical)
    {
        for (const FString& Entry : List)
        {
            if (Entry.Equals(Value.TrimStartAndEnd(), ESearchCase::IgnoreCase))
            {
                OutCanonical = Entry;
                return true;
            }
        }
        return false;
    }
}

// -------------------------------------------------------
// FProprioceptiveSnapshot helpers
// -------------------------------------------------------

TArray<float> FProprioceptiveSnapshot::ToSensoryVector() const
{
    TArray<float> Vec;
    Vec.Reserve(18);

    // Position (normalised to ±1000 cm game units)
    Vec.Add(WorldLocation.X * 0.001f);
    Vec.Add(WorldLocation.Y * 0.001f);
    Vec.Add(WorldLocation.Z * 0.001f);

    // Rotation (normalised to ±1)
    Vec.Add(WorldRotation.Pitch / 180.0f);
    Vec.Add(WorldRotation.Yaw   / 180.0f);
    Vec.Add(WorldRotation.Roll  / 180.0f);

    // Velocity (normalised to max sprint ~600 cm/s)
    const float VelScale = 1.0f / 600.0f;
    Vec.Add(Velocity.X * VelScale);
    Vec.Add(Velocity.Y * VelScale);
    Vec.Add(Velocity.Z * VelScale);

    // Acceleration (normalised to ~980 cm/s²)
    const float AccScale = 1.0f / 980.0f;
    Vec.Add(Acceleration.X * AccScale);
    Vec.Add(Acceleration.Y * AccScale);
    Vec.Add(Acceleration.Z * AccScale);

    // Binary flags
    Vec.Add(bIsGrounded ? 1.0f : 0.0f);
    Vec.Add(bIsAirborne ? 1.0f : 0.0f);

    // Centre of mass offset (normalised)
    Vec.Add(CentreOfMass.X * 0.01f);
    Vec.Add(CentreOfMass.Y * 0.01f);
    Vec.Add(CentreOfMass.Z * 0.01f);

    // Timestamp (fractional seconds within the current second)
    Vec.Add(FMath::Fmod(Timestamp, 1.0f));

    return Vec;
}

// -------------------------------------------------------
// Constructor
// -------------------------------------------------------

UVirtualCNS::UVirtualCNS()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    // Tick in PrePhysics group so proprioception is up-to-date before physics
    PrimaryComponentTick.TickGroup = TG_PrePhysics;

    // Stimulus tags submitted by threat sources (AutonomousAgentController::SubmitThreatToCNS)
    ReflexStimulusCategories = { TEXT("ThreatLeft"), TEXT("ThreatRight"), TEXT("ThreatFront") };
    ReflexActionNames = { TEXT("DodgeLeft"), TEXT("DodgeRight"), TEXT("DodgeBack"),
                          TEXT("Block"), TEXT("Jump"), TEXT("Crouch"), TEXT("Counter") };

    ReflexAuthoringPromptTemplate = TEXT(
        "You are {character}, an embodied agent playing through a game controller. You are setting up your own "
        "spinal reflexes: fast automatic responses that fire without thinking when a threat of level {min_threat} "
        "to 1.0 arrives. Write one reflex for each of these stimuli: {stimuli}. For each reflex choose: "
        "the action name (exactly one of: {actions}); the threat level at or above which it fires "
        "(a number from {min_threat} to 1.0); how long to hold the input in seconds ({duration_min} to {duration_max}); "
        "the refractory period in seconds before it can fire again ({refractory_min} to {refractory_max}); "
        "the left stick x and y (numbers from -1 to 1, negative x = left, positive y = forward); the buttons "
        "to press (a list, each exactly one of: {buttons}; may be empty); and the category (exactly one of: {categories}). "
        "Reply with only a JSON object: {\"arcs\": [{\"stimulus\": \"<stimulus>\", \"action\": \"<action>\", "
        "\"threshold\": <number>, \"duration\": <number>, \"refractory\": <number>, \"left_stick_x\": <number>, "
        "\"left_stick_y\": <number>, \"buttons\": [<button names>], \"category\": \"<category>\"}]}");
}

// -------------------------------------------------------
// BeginPlay
// -------------------------------------------------------

void UVirtualCNS::BeginPlay()
{
    Super::BeginPlay();
    FindAndCacheComponents();
    // The character's model authors its reflex arcs; none fire until it has
    RequestReflexAuthoring();
    bInitialised = true;
}

// -------------------------------------------------------
// TickComponent — the main CNS heartbeat
// -------------------------------------------------------

void UVirtualCNS::TickComponent(float DeltaTime,
                                 ELevelTick TickType,
                                 FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bInitialised)
    {
        return;
    }

    // Advance tier elevation countdown
    if (TierElevationTimer > 0.0f)
    {
        TierElevationTimer -= DeltaTime;
        if (TierElevationTimer <= 0.0f)
        {
            TierElevationTimer = 0.0f;
            const ECNSPathwayTier NewTier = SelectPathwayTier();
            if (NewTier != ActivePathwayTier)
            {
                const ECNSPathwayTier Old = ActivePathwayTier;
                ActivePathwayTier = NewTier;
                OnCNSPathwayChanged.Broadcast(Old, ActivePathwayTier);
            }
        }
    }

    // Proprioception sampling
    TickProprioception(DeltaTime);

    // Threat evaluation
    if (bEnableThreatDetection)
    {
        EvaluateThreat(DeltaTime);
    }

    // Advance reflex refractory timers
    if (bEnableReflexArcs)
    {
        for (FReflexArc& Arc : ReflexArcs)
        {
            Arc.TimeSinceLastFire += DeltaTime;
        }
    }

    // Pull the latest controller input and route through CNS
    FControllerInputState InputState;
    if (ControllerInterface)
    {
        InputState = ControllerInterface->GetCurrentInputState();
    }

    int32 ReflexesFired = 0;

    // ── Layer 1: Spinal reflex (fastest path) ──────────────
    if (bEnableReflexArcs)
    {
        ProcessSpinalReflexes(InputState, DeltaTime);
    }

    // ── Layer 2-3: Brainstem / Subcortical ─────────────────
    if (static_cast<uint8>(ActivePathwayTier) >=
        static_cast<uint8>(ECNSPathwayTier::Brainstem))
    {
        ProcessSubcortical(InputState, DeltaTime);
    }

    // ── Layer 4-5: Cortical / Meta-cognitive ───────────────
    if (static_cast<uint8>(ActivePathwayTier) >=
        static_cast<uint8>(ECNSPathwayTier::Cortical))
    {
        ProcessCortical(InputState, DeltaTime);
    }

    UpdateTelemetry(DeltaTime, ReflexesFired);
}

// -------------------------------------------------------
// Afferent API
// -------------------------------------------------------

void UVirtualCNS::InjectSensoryInput(const TArray<float>& Data,
                                      const FString& Modality)
{
    if (DTECore)
    {
        DTECore->ProcessSensoryInput(Data, Modality);
    }
    if (SensorimotorSys)
    {
        // SensorimotorIntegration may have its own routing
        // We call the embodied path for tactile / proprioceptive modalities
    }
}

void UVirtualCNS::ProcessControllerInput(const FControllerInputState& InputState)
{
    if (!EmbodiedCognition)
    {
        return;
    }

    // Convert controller input to sensory vector
    TArray<float> SensoryVec;
    if (ControllerInterface)
    {
        SensoryVec = ControllerInterface->InputToSensoryVector(InputState);
    }
    else
    {
        // Fallback: pack sticks + triggers manually
        SensoryVec = {
            InputState.LeftStickX, InputState.LeftStickY,
            InputState.RightStickX, InputState.RightStickY,
            InputState.LeftTrigger, InputState.RightTrigger
        };
    }

    // Feed into the 4E embodied loop
    InjectSensoryInput(SensoryVec, TEXT("Controller"));

    // Also update enactive layer with sensorimotor contingencies
    TArray<FString> Contingencies;
    for (const EGamepadButton Btn : InputState.PressedButtons)
    {
        Contingencies.Add(FString::FromInt(static_cast<int32>(Btn)));
    }
    if (DTECore)
    {
        TArray<float> PredErrors;
        DTECore->UpdateEnactedState(Contingencies, PredErrors);
    }
}

void UVirtualCNS::UpdateProprioception(const FProprioceptiveSnapshot& Snapshot)
{
    CurrentProprioception = Snapshot;

    if (bEnableProprioception)
    {
        // Push to embodied cognition
        if (EmbodiedCognition && DTECore)
        {
            TArray<float> PropVec = Snapshot.ToSensoryVector();
            // First 6 elements are position/rotation for proprioception
            TArray<float> PropState(PropVec.GetData(), 6);
            TArray<float> InteroState; // velocity+acceleration as interoception
            InteroState.Reserve(6);
            for (int32 i = 6; i < 12; ++i) { InteroState.Add(PropVec[i]); }
            DTECore->UpdateEmbodiedState(PropState, InteroState);
        }

        OnProprioceptiveUpdate.Broadcast(Snapshot);
    }
}

void UVirtualCNS::SubmitThreatSignal(const FThreatSignal& Threat)
{
    if (Threat.ThreatLevel > CurrentThreat.ThreatLevel ||
        CurrentThreat.TimeToImpact > Threat.TimeToImpact)
    {
        CurrentThreat = Threat;
        OnThreatDetected.Broadcast(Threat);

        // Immediately elevate pathway tier based on urgency
        if (Threat.ThreatLevel >= ReflexThreatThreshold)
        {
            const float ElevationDur = FMath::Max(0.2f, Threat.TimeToImpact);
            ElevateTier(ECNSPathwayTier::Brainstem, ElevationDur);
        }
    }
}

// -------------------------------------------------------
// Efferent API
// -------------------------------------------------------

FControllerOutputCommand UVirtualCNS::PollMotorCommand()
{
    if (MotorCommandQueue.Num() == 0)
    {
        return FControllerOutputCommand{};
    }
    const FControllerOutputCommand Cmd = MotorCommandQueue[0];
    MotorCommandQueue.RemoveAt(0);
    return Cmd;
}

bool UVirtualCNS::HasPendingMotorCommand() const
{
    return MotorCommandQueue.Num() > 0;
}

void UVirtualCNS::OverrideMotorOutput(const FControllerOutputCommand& Command)
{
    // Human override: clear AI queue and push the human command
    MotorCommandQueue.Empty();
    MotorCommandQueue.Add(Command);
}

// -------------------------------------------------------
// Reflex Management
// -------------------------------------------------------

void UVirtualCNS::RegisterReflexArc(const FReflexArc& Arc)
{
    // Replace if ID already exists
    for (FReflexArc& Existing : ReflexArcs)
    {
        if (Existing.ReflexID == Arc.ReflexID)
        {
            Existing = Arc;
            return;
        }
    }
    ReflexArcs.Add(Arc);
}

bool UVirtualCNS::RemoveReflexArc(const FString& ReflexID)
{
    const int32 OldNum = ReflexArcs.Num();
    ReflexArcs.RemoveAll([&ReflexID](const FReflexArc& A)
    {
        return A.ReflexID == ReflexID;
    });
    return ReflexArcs.Num() < OldNum;
}

void UVirtualCNS::FireReflex(const FString& ReflexID)
{
    // Accepts an arc ID or a stimulus category (GamingMasterySystem routes stimuli by type)
    for (FReflexArc& Arc : ReflexArcs)
    {
        if (Arc.ReflexID == ReflexID || Arc.TriggerCondition == ReflexID)
        {
            if (!Arc.IsRefractory())
            {
                Arc.TimeSinceLastFire = 0.0f;
                EnqueueMotorCommand(Arc.Response, ECNSPathwayTier::SpinalReflex);
                OnReflexFired.Broadcast(Arc.ReflexID, Arc.Response);
            }
            return;
        }
    }
}

bool UVirtualCNS::HasReflexArc(const FString& ReflexID) const
{
    for (const FReflexArc& Arc : ReflexArcs)
    {
        if (Arc.ReflexID == ReflexID) { return true; }
    }
    return false;
}

// -------------------------------------------------------
// Pathway Control
// -------------------------------------------------------

void UVirtualCNS::SetPathwayTier(ECNSPathwayTier Tier)
{
    if (Tier == ActivePathwayTier) { return; }
    const ECNSPathwayTier Old = ActivePathwayTier;
    ActivePathwayTier = Tier;
    OnCNSPathwayChanged.Broadcast(Old, Tier);
}

void UVirtualCNS::ElevateTier(ECNSPathwayTier MinTier, float Duration)
{
    if (static_cast<uint8>(MinTier) <= static_cast<uint8>(ActivePathwayTier))
    {
        return; // Already at or above requested tier
    }
    TierElevationMin = MinTier;
    TierElevationTimer = Duration;
    SetPathwayTier(MinTier);
}

// -------------------------------------------------------
// Sensorimotor Coherence
// -------------------------------------------------------

float UVirtualCNS::GetBodySchemaCoherence() const
{
    return Telemetry.BodySchemaCoherence;
}

float UVirtualCNS::ComputePredictionError() const
{
    if (LastEfferenceCopy.Num() == 0) { return 0.0f; }

    const TArray<float> ActualSensory = CurrentProprioception.ToSensoryVector();
    const int32 N = FMath::Min(LastEfferenceCopy.Num(), ActualSensory.Num());
    float Error = 0.0f;
    for (int32 i = 0; i < N; ++i)
    {
        const float Diff = ActualSensory[i] - LastEfferenceCopy[i];
        Error += Diff * Diff;
    }
    return (N > 0) ? FMath::Sqrt(Error / N) : 0.0f;
}

// -------------------------------------------------------
// Telemetry
// -------------------------------------------------------

void UVirtualCNS::ResetTelemetry()
{
    Telemetry = FCNSTelemetry{};
}

// -------------------------------------------------------
// Component Wiring
// -------------------------------------------------------

void UVirtualCNS::SetDTECore(UDeepTreeEchoCore* Core)
{
    DTECore = Core;
}

void UVirtualCNS::SetControllerInterface(UGameControllerInterface* Controller)
{
    ControllerInterface = Controller;
}

void UVirtualCNS::SetMetaHumanBinding(UMetaHumanCNSBinding* Binding)
{
    MetaHumanBinding = Binding;
}

// -------------------------------------------------------
// PRIVATE — FindAndCacheComponents
// -------------------------------------------------------

void UVirtualCNS::FindAndCacheComponents()
{
    AActor* Owner = GetOwner();
    if (!Owner) { return; }

    if (!DTECore)
    {
        DTECore = Owner->FindComponentByClass<UDeepTreeEchoCore>();
    }
    if (!EmbodiedCognition)
    {
        EmbodiedCognition = Owner->FindComponentByClass<UEmbodiedCognitionComponent>();
    }
    if (!RLBridge)
    {
        RLBridge = Owner->FindComponentByClass<UReinforcementLearningBridge>();
    }
    if (!ControllerInterface)
    {
        ControllerInterface = Owner->FindComponentByClass<UGameControllerInterface>();
    }
    if (!AttentionSys)
    {
        AttentionSys = Owner->FindComponentByClass<UAttentionSystem>();
    }
    if (!SensorimotorSys)
    {
        SensorimotorSys = Owner->FindComponentByClass<USensorimotorLoopIntegration>();
    }
}

// -------------------------------------------------------
// Reflex authoring — the model writes the arcs once; reflexes then fire without a model call
// -------------------------------------------------------

void UVirtualCNS::RequestReflexAuthoring()
{
    if (ReflexStimulusCategories.Num() == 0 || ReflexActionNames.Num() == 0)
    {
        LastReflexAuthoringError = TEXT("no stimulus categories or action names to author reflexes for");
        UE_LOG(LogTemp, Error, TEXT("VirtualCNS: %s"), *LastReflexAuthoringError);
        return;
    }

    TMap<FString, FString> Slots;
    Slots.Add(TEXT("character"), !ReflexCharacterName.IsEmpty() ? ReflexCharacterName
        : (GetOwner() ? GetOwner()->GetName() : GetName()));
    Slots.Add(TEXT("stimuli"), FString::Join(ReflexStimulusCategories, TEXT(", ")));
    Slots.Add(TEXT("min_threat"), FString::Printf(TEXT("%.2f"), ReflexThreatThreshold));
    Slots.Add(TEXT("actions"), FString::Join(ReflexActionNames, TEXT(", ")));
    Slots.Add(TEXT("buttons"), ReflexEnumNames<EGamepadButton>(true));
    Slots.Add(TEXT("categories"), ReflexEnumNames<EGameActionCategory>(false));
    Slots.Add(TEXT("duration_min"), FString::Printf(TEXT("%.2f"), ReflexDurationRange.X));
    Slots.Add(TEXT("duration_max"), FString::Printf(TEXT("%.2f"), ReflexDurationRange.Y));
    Slots.Add(TEXT("refractory_min"), FString::Printf(TEXT("%.2f"), ReflexRefractoryRange.X));
    Slots.Add(TEXT("refractory_max"), FString::Printf(TEXT("%.2f"), ReflexRefractoryRange.Y));

    TArray<FEchoModelMessage> Messages;
    Messages.Add(FEchoModelMessage(TEXT("user"), UEchoModelClient::FillPrompt(ReflexAuthoringPromptTemplate, Slots)));

    const int32 RequestSerial = ++ReflexAuthoringSerial;
    bReflexAuthoringPending = true;
    TWeakObjectPtr<UVirtualCNS> WeakThis(this);
    UEchoModelClient::Get()->Chat(Messages, FOnEchoModelReply::CreateLambda(
        [WeakThis, RequestSerial](bool bSuccess, const FString& TextOrError)
        {
            if (UVirtualCNS* Self = WeakThis.Get())
            {
                Self->HandleReflexAuthoringReply(RequestSerial, bSuccess, TextOrError);
            }
        }), 0.4f);
}

void UVirtualCNS::HandleReflexAuthoringReply(int32 RequestSerial, bool bSuccess, const FString& TextOrError)
{
    if (RequestSerial != ReflexAuthoringSerial)
    {
        return; // superseded by a newer authoring request
    }
    bReflexAuthoringPending = false;

    if (!bSuccess)
    {
        LastReflexAuthoringError = TextOrError;
        UE_LOG(LogTemp, Error, TEXT("VirtualCNS: no model-authored reflex arcs: %s"), *TextOrError);
        return;
    }

    TSharedPtr<FJsonObject> Json;
    const TArray<TSharedPtr<FJsonValue>>* ArcValues = nullptr;
    if (!UEchoModelClient::ParseJsonObject(TextOrError, Json) || !Json->TryGetArrayField(TEXT("arcs"), ArcValues) || !ArcValues)
    {
        LastReflexAuthoringError = FString::Printf(TEXT("model reply is not the requested JSON: %s"), *TextOrError.Left(300));
        UE_LOG(LogTemp, Error, TEXT("VirtualCNS: %s"), *LastReflexAuthoringError);
        return;
    }

    TArray<FReflexArc> Authored;
    TArray<FString> Rejections;
    for (int32 Index = 0; Index < ArcValues->Num(); ++Index)
    {
        const TSharedPtr<FJsonValue>& Value = (*ArcValues)[Index];
        const TSharedPtr<FJsonObject>* ArcObjPtr = nullptr;
        if (!Value.IsValid() || !Value->TryGetObject(ArcObjPtr) || !ArcObjPtr || !ArcObjPtr->IsValid())
        {
            Rejections.Add(FString::Printf(TEXT("arc %d is not an object"), Index));
            continue;
        }
        const TSharedPtr<FJsonObject>& ArcObj = *ArcObjPtr;

        FString Stimulus, Action, CategoryName;
        double Threshold = 0.0, Duration = 0.0, Refractory = 0.0, StickX = 0.0, StickY = 0.0;
        if (!ArcObj->TryGetStringField(TEXT("stimulus"), Stimulus) ||
            !ArcObj->TryGetStringField(TEXT("action"), Action) ||
            !ArcObj->TryGetStringField(TEXT("category"), CategoryName) ||
            !ReadReflexNumber(ArcObj, TEXT("threshold"), Threshold) ||
            !ReadReflexNumber(ArcObj, TEXT("duration"), Duration) ||
            !ReadReflexNumber(ArcObj, TEXT("refractory"), Refractory))
        {
            Rejections.Add(FString::Printf(TEXT("arc %d is missing a required field"), Index));
            continue;
        }
        // Stick axes the model leaves out are simply not moved
        ReadReflexNumber(ArcObj, TEXT("left_stick_x"), StickX);
        ReadReflexNumber(ArcObj, TEXT("left_stick_y"), StickY);

        FString CanonicalStimulus, CanonicalAction;
        if (!ContainsIgnoreCase(ReflexStimulusCategories, Stimulus, CanonicalStimulus))
        {
            Rejections.Add(FString::Printf(TEXT("arc %d: stimulus '%s' is not one offered"), Index, *Stimulus));
            continue;
        }
        if (!ContainsIgnoreCase(ReflexActionNames, Action, CanonicalAction))
        {
            Rejections.Add(FString::Printf(TEXT("arc %d: action '%s' is not one offered"), Index, *Action));
            continue;
        }
        EGameActionCategory Category{};
        if (!ParseReflexEnum(CategoryName, Category))
        {
            Rejections.Add(FString::Printf(TEXT("arc %d: category '%s' is not one offered"), Index, *CategoryName));
            continue;
        }

        FControllerInputState Desired;
        Desired.LeftStickX = FMath::Clamp(static_cast<float>(StickX), -1.0f, 1.0f);
        Desired.LeftStickY = FMath::Clamp(static_cast<float>(StickY), -1.0f, 1.0f);

        bool bButtonsValid = true;
        TArray<FString> ButtonNames;
        const TArray<TSharedPtr<FJsonValue>>* ButtonValues = nullptr;
        FString SingleButton;
        if (ArcObj->TryGetArrayField(TEXT("buttons"), ButtonValues) && ButtonValues)
        {
            for (const TSharedPtr<FJsonValue>& B : *ButtonValues)
            {
                FString Name;
                if (B.IsValid() && B->TryGetString(Name)) { ButtonNames.Add(Name); }
                else { bButtonsValid = false; }
            }
        }
        else if (ArcObj->TryGetStringField(TEXT("buttons"), SingleButton) && !SingleButton.TrimStartAndEnd().IsEmpty())
        {
            ButtonNames.Add(SingleButton);
        }
        for (const FString& Name : ButtonNames)
        {
            EGamepadButton Button{};
            if (!ParseReflexEnum(Name, Button)) { bButtonsValid = false; break; }
            if (Button != EGamepadButton::None) { Desired.PressedButtons.Add(Button); }
        }
        if (!bButtonsValid)
        {
            Rejections.Add(FString::Printf(TEXT("arc %d: a button is not one offered"), Index));
            continue;
        }
        if (FMath::Abs(Desired.LeftStickX) < 0.01f && FMath::Abs(Desired.LeftStickY) < 0.01f &&
            Desired.PressedButtons.Num() == 0)
        {
            Rejections.Add(FString::Printf(TEXT("arc %d (%s) presses nothing"), Index, *CanonicalAction));
            continue;
        }

        FReflexArc Arc;
        Arc.ReflexID         = CanonicalStimulus + TEXT(":") + CanonicalAction;
        Arc.TriggerCondition = CanonicalStimulus;
        Arc.Threshold        = FMath::Clamp(static_cast<float>(Threshold), ReflexThreatThreshold, 1.0f);
        Arc.RefractoryPeriod = FMath::Clamp(static_cast<float>(Refractory), ReflexRefractoryRange.X, ReflexRefractoryRange.Y);
        Arc.Response.DesiredState = Desired;
        Arc.Response.Duration     = FMath::Clamp(static_cast<float>(Duration), ReflexDurationRange.X, ReflexDurationRange.Y);
        Arc.Response.Category     = Category;
        Arc.Response.ActionName   = CanonicalAction;
        Authored.RemoveAll([&Arc](const FReflexArc& A) { return A.ReflexID == Arc.ReflexID; });
        Authored.Add(Arc);
    }

    for (const FString& Rejection : Rejections)
    {
        UE_LOG(LogTemp, Error, TEXT("VirtualCNS: rejected model-authored reflex: %s"), *Rejection);
    }
    if (Authored.Num() == 0)
    {
        LastReflexAuthoringError = Rejections.Num() > 0
            ? FString::Printf(TEXT("no valid reflex arcs in model reply (%s)"), *FString::Join(Rejections, TEXT("; ")))
            : TEXT("model reply authored no reflex arcs");
        UE_LOG(LogTemp, Error, TEXT("VirtualCNS: %s"), *LastReflexAuthoringError);
        return;
    }

    ReflexArcs = MoveTemp(Authored);
    if (Rejections.Num() > 0)
    {
        LastReflexAuthoringError = FString::Join(Rejections, TEXT("; "));
    }
    else
    {
        LastReflexAuthoringError.Reset();
    }
    UE_LOG(LogTemp, Log, TEXT("VirtualCNS: installed %d model-authored reflex arcs"), ReflexArcs.Num());
}

// -------------------------------------------------------
// PRIVATE — ProcessSpinalReflexes
// -------------------------------------------------------

void UVirtualCNS::ProcessSpinalReflexes(const FControllerInputState& /*Input*/,
                                          float /*DeltaTime*/)
{
    if (CurrentThreat.ThreatLevel < ReflexThreatThreshold) { return; }

    for (FReflexArc& Arc : ReflexArcs)
    {
        if (Arc.IsRefractory()) { continue; }
        if (Arc.TriggerCondition != CurrentThreat.SuggestedReflex &&
            !CurrentThreat.ThreatType.Contains(Arc.TriggerCondition))
        {
            continue;
        }
        if (CurrentThreat.ThreatLevel < Arc.Threshold) { continue; }

        // Fire the reflex
        Arc.TimeSinceLastFire = 0.0f;
        EnqueueMotorCommand(Arc.Response, ECNSPathwayTier::SpinalReflex);
        OnReflexFired.Broadcast(Arc.ReflexID, Arc.Response);
        Telemetry.ReflexesFiredThisFrame++;
    }
}

// -------------------------------------------------------
// PRIVATE — ProcessSubcortical
// -------------------------------------------------------

void UVirtualCNS::ProcessSubcortical(const FControllerInputState& Input,
                                       float /*DeltaTime*/)
{
    // Update attention salience based on threat
    if (AttentionSys && CurrentThreat.ThreatLevel > 0.0f)
    {
        // Attention system would be updated here with threat direction
        // This is a thin integration shim; full logic lives in AttentionSystem
    }

    // Feed controller input to reinforcement learning bridge at brainstem speed
    if (RLBridge)
    {
        TArray<float> State;
        if (ControllerInterface)
        {
            State = ControllerInterface->InputToSensoryVector(Input);
        }
        // The RL bridge selects and emits the best-known action
        const FRLAction Action = RLBridge->SelectAction(State);
        const FControllerOutputCommand Cmd = RLBridge->ActionToControllerOutput(Action);
        if (Cmd.ActionName.Len() > 0)
        {
            EnqueueMotorCommand(Cmd, ECNSPathwayTier::Brainstem);
        }
    }
}

// -------------------------------------------------------
// PRIVATE — ProcessCortical
// -------------------------------------------------------

void UVirtualCNS::ProcessCortical(const FControllerInputState& Input,
                                    float DeltaTime)
{
    if (!DTECore) { return; }

    // Feed controller sensory data into the full DTE cognitive cycle
    TArray<float> SensoryVec;
    if (ControllerInterface)
    {
        SensoryVec = ControllerInterface->InputToSensoryVector(Input);
    }

    // Append proprioceptive state
    const TArray<float> PropVec = CurrentProprioception.ToSensoryVector();
    SensoryVec.Append(PropVec);

    DTECore->ProcessSensoryInput(SensoryVec, TEXT("CNS"));

    // Generate cognitive action output
    const TArray<float> CogOutput = DTECore->GenerateActionOutput();
    if (CogOutput.Num() > 0 && ControllerInterface)
    {
        const FControllerOutputCommand Cmd =
            ControllerInterface->CognitiveOutputToCommand(CogOutput);
        if (Cmd.ActionName.Len() > 0)
        {
            EnqueueMotorCommand(Cmd, ECNSPathwayTier::Cortical);
        }
    }

    Telemetry.CognitiveCyclesCompleted++;

    // Body schema coherence from prediction error
    if (bEnableEfferenceCopy)
    {
        const float PredErr = ComputePredictionError();
        Telemetry.BodySchemaCoherence = FMath::Clamp(1.0f - PredErr, 0.0f, 1.0f);
    }

    (void)DeltaTime; // reserved for future sub-delta processing
}

// -------------------------------------------------------
// PRIVATE — EvaluateThreat
// -------------------------------------------------------

void UVirtualCNS::EvaluateThreat(float DeltaTime)
{
    // Decay threat level over time when no new signal arrives
    CurrentThreat.ThreatLevel = FMath::Max(
        0.0f,
        CurrentThreat.ThreatLevel - DeltaTime * 2.0f);

    if (CurrentThreat.TimeToImpact < 9000.0f)
    {
        CurrentThreat.TimeToImpact -= DeltaTime;
        CurrentThreat.TimeToImpact = FMath::Max(0.0f, CurrentThreat.TimeToImpact);
    }
}

// -------------------------------------------------------
// PRIVATE — TickProprioception
// -------------------------------------------------------

void UVirtualCNS::TickProprioception(float DeltaTime)
{
    if (!bEnableProprioception || ProprioceptiveHz <= 0.0f) { return; }

    ProprioceptionTimer += DeltaTime;
    const float SampleInterval = 1.0f / ProprioceptiveHz;

    if (ProprioceptionTimer >= SampleInterval)
    {
        ProprioceptionTimer = 0.0f;

        // Populate current proprioception from the owning actor
        AActor* Owner = GetOwner();
        if (Owner)
        {
            FProprioceptiveSnapshot Snap;
            Snap.WorldLocation = Owner->GetActorLocation();
            Snap.WorldRotation = Owner->GetActorRotation();
            if (UPrimitiveComponent* Prim =
                    Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
            {
                Snap.Velocity     = Prim->GetPhysicsLinearVelocity();
            }
            Snap.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

            // Store efference copy before we overwrite
            if (bEnableEfferenceCopy && MotorCommandQueue.Num() > 0)
            {
                LastEfferenceCopy = Snap.ToSensoryVector();
                PropagateEfferenceCopy(MotorCommandQueue[0]);
            }

            UpdateProprioception(Snap);
        }
    }
}

// -------------------------------------------------------
// PRIVATE — EnqueueMotorCommand
// -------------------------------------------------------

void UVirtualCNS::EnqueueMotorCommand(const FControllerOutputCommand& Cmd,
                                       ECNSPathwayTier Tier)
{
    // Higher-tier (lower latency) commands are inserted at the front
    if (Tier <= ECNSPathwayTier::Brainstem)
    {
        MotorCommandQueue.Insert(Cmd, 0);
    }
    else
    {
        MotorCommandQueue.Add(Cmd);
    }

    // Cap queue size to avoid stale commands piling up
    while (MotorCommandQueue.Num() > 8)
    {
        MotorCommandQueue.RemoveAt(MotorCommandQueue.Num() - 1);
    }
}

// -------------------------------------------------------
// PRIVATE — PropagateEfferenceCopy
// -------------------------------------------------------

void UVirtualCNS::PropagateEfferenceCopy(const FControllerOutputCommand& Cmd)
{
    if (!DTECore) { return; }

    // Build prediction of where the body will be after this command
    TArray<FString> Contingencies;
    Contingencies.Add(Cmd.ActionName);

    // Prediction errors will be filled in on the next proprioceptive tick
    TArray<float> PredErrors;
    DTECore->UpdateEnactedState(Contingencies, PredErrors);
}

// -------------------------------------------------------
// PRIVATE — UpdateTelemetry
// -------------------------------------------------------

void UVirtualCNS::UpdateTelemetry(float DeltaTime, int32 ReflexesFired)
{
    Telemetry.TotalFrames++;
    Telemetry.ActiveTier = ActivePathwayTier;
    Telemetry.ReflexesFiredThisFrame = ReflexesFired;

    // Rolling average for latencies (milliseconds)
    const float Alpha = 0.05f; // EMA smoothing
    const float AfferentMs  = DeltaTime * 1000.0f * 0.5f; // approx half-frame
    const float EfferentMs  = DeltaTime * 1000.0f * 0.5f;
    Telemetry.AvgAfferentLatencyMs =
        Telemetry.AvgAfferentLatencyMs * (1.0f - Alpha) + AfferentMs * Alpha;
    Telemetry.AvgEfferentLatencyMs =
        Telemetry.AvgEfferentLatencyMs * (1.0f - Alpha) + EfferentMs * Alpha;
}

// -------------------------------------------------------
// PRIVATE — SelectPathwayTier
// -------------------------------------------------------

ECNSPathwayTier UVirtualCNS::SelectPathwayTier() const
{
    // High threat → drop to subcortical / brainstem
    if (CurrentThreat.ThreatLevel >= 0.85f)
    {
        return ECNSPathwayTier::SpinalReflex;
    }
    if (CurrentThreat.ThreatLevel >= 0.6f)
    {
        return ECNSPathwayTier::Brainstem;
    }
    if (CurrentThreat.ThreatLevel >= 0.3f)
    {
        return ECNSPathwayTier::Subcortical;
    }
    // Default: full cortical + meta-cognitive
    return ECNSPathwayTier::Cortical;
}
