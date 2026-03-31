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
}

// -------------------------------------------------------
// BeginPlay
// -------------------------------------------------------

void UVirtualCNS::BeginPlay()
{
    Super::BeginPlay();
    FindAndCacheComponents();
    InitialiseDefaultReflexArcs();
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
    for (FReflexArc& Arc : ReflexArcs)
    {
        if (Arc.ReflexID == ReflexID)
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
        SensorimotorSys = Owner->FindComponentByClass<USensorimotorIntegration>();
    }
}

// -------------------------------------------------------
// PRIVATE — InitialiseDefaultReflexArcs
// -------------------------------------------------------

void UVirtualCNS::InitialiseDefaultReflexArcs()
{
    if (!ReflexArcs.IsEmpty()) { return; } // already set in editor / blueprint

    // ── Dodge Left ────────────────────────────────────────
    {
        FReflexArc Arc;
        Arc.ReflexID          = TEXT("DodgeLeft");
        Arc.TriggerCondition  = TEXT("ThreatRight");
        Arc.Threshold         = 0.7f;
        Arc.RefractoryPeriod  = 0.4f;
        FControllerInputState Dodge;
        Dodge.LeftStickX = -1.0f;
        Arc.Response.DesiredState = Dodge;
        Arc.Response.Duration     = 0.25f;
        Arc.Response.Priority     = 10.0f;
        Arc.Response.Category     = EGameActionCategory::Defense;
        Arc.Response.ActionName   = TEXT("DodgeLeft");
        Arc.Response.Confidence   = 0.95f;
        ReflexArcs.Add(Arc);
    }

    // ── Dodge Right ───────────────────────────────────────
    {
        FReflexArc Arc;
        Arc.ReflexID          = TEXT("DodgeRight");
        Arc.TriggerCondition  = TEXT("ThreatLeft");
        Arc.Threshold         = 0.7f;
        Arc.RefractoryPeriod  = 0.4f;
        FControllerInputState Dodge;
        Dodge.LeftStickX = 1.0f;
        Arc.Response.DesiredState = Dodge;
        Arc.Response.Duration     = 0.25f;
        Arc.Response.Priority     = 10.0f;
        Arc.Response.Category     = EGameActionCategory::Defense;
        Arc.Response.ActionName   = TEXT("DodgeRight");
        Arc.Response.Confidence   = 0.95f;
        ReflexArcs.Add(Arc);
    }

    // ── Block High ────────────────────────────────────────
    {
        FReflexArc Arc;
        Arc.ReflexID          = TEXT("BlockHigh");
        Arc.TriggerCondition  = TEXT("ThreatProjectileHigh");
        Arc.Threshold         = 0.65f;
        Arc.RefractoryPeriod  = 0.3f;
        FControllerInputState Block;
        Block.PressedButtons.Add(EGamepadButton::LeftShoulder);
        Arc.Response.DesiredState = Block;
        Arc.Response.Duration     = 0.3f;
        Arc.Response.Priority     = 9.0f;
        Arc.Response.Category     = EGameActionCategory::Defense;
        Arc.Response.ActionName   = TEXT("BlockHigh");
        Arc.Response.Confidence   = 0.9f;
        ReflexArcs.Add(Arc);
    }

    // ── Jump Dodge ────────────────────────────────────────
    {
        FReflexArc Arc;
        Arc.ReflexID          = TEXT("JumpDodge");
        Arc.TriggerCondition  = TEXT("ThreatSweep");
        Arc.Threshold         = 0.8f;
        Arc.RefractoryPeriod  = 0.6f;
        FControllerInputState Jump;
        Jump.PressedButtons.Add(EGamepadButton::FaceBottom);
        Arc.Response.DesiredState = Jump;
        Arc.Response.Duration     = 0.15f;
        Arc.Response.Priority     = 10.0f;
        Arc.Response.Category     = EGameActionCategory::Defense;
        Arc.Response.ActionName   = TEXT("JumpDodge");
        Arc.Response.Confidence   = 0.9f;
        ReflexArcs.Add(Arc);
    }
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
