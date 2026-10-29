#include "CognitiveCycleManager.h"

UCognitiveStreamCycleManager::UCognitiveStreamCycleManager()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f;
}

void UCognitiveStreamCycleManager::BeginPlay()
{
    Super::BeginPlay();
    InitializeCycle();
}

void UCognitiveStreamCycleManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bEnableAutoCycle)
    {
        AccumulatedTime += DeltaTime;
        if (AccumulatedTime >= StepDuration)
        {
            AccumulatedTime -= StepDuration;
            AdvanceStep();
        }
    }
}

// ========================================
// INITIALIZATION
// ========================================

void UCognitiveStreamCycleManager::InitializeCycle()
{
    // Initialize cycle state
    CycleState.GlobalStep = 1;
    CycleState.CyclesCompleted = 0;
    CycleState.CurrentMode = ECognitiveStreamMode::Expressive;
    CycleState.CurrentStepType = ECognitiveStreamStepType::RelevanceRealization;
    CycleState.CurrentTriad = ETriadGroup::Triad1;
    CycleState.ExpressiveStepsRemaining = 7;
    CycleState.ReflectiveStepsRemaining = 5;

    InitializeStreams();
    InitializeNestedShells();
    InitializeTriads();
}

void UCognitiveStreamCycleManager::ResetCycle()
{
    InitializeCycle();
}

void UCognitiveStreamCycleManager::InitializeStreams()
{
    StreamStates.Empty();

    // Stream 1: Perception (phase offset 0)
    FConsciousnessStreamState PerceptionStream;
    PerceptionStream.StreamType = EConsciousnessStream::Perception;
    PerceptionStream.CurrentStep = 1;
    PerceptionStream.PhaseOffset = 0;
    PerceptionStream.Mode = ECognitiveStreamMode::Expressive;
    PerceptionStream.StepType = ECognitiveStreamStepType::RelevanceRealization;
    PerceptionStream.ActivationLevel = 1.0f;
    PerceptionStream.InterStreamCoherence = 1.0f;
    PerceptionStream.SalienceValue = 0.5f;
    PerceptionStream.AffordanceValue = 0.5f;
    StreamStates.Add(PerceptionStream);

    // Stream 2: Action (phase offset 4 = 120°)
    FConsciousnessStreamState ActionStream;
    ActionStream.StreamType = EConsciousnessStream::Action;
    ActionStream.CurrentStep = 5; // 4 steps ahead
    ActionStream.PhaseOffset = 4;
    ActionStream.Mode = ECognitiveStreamMode::Expressive;
    ActionStream.StepType = ECognitiveStreamStepType::AffordanceInteraction;
    ActionStream.ActivationLevel = 1.0f;
    ActionStream.InterStreamCoherence = 1.0f;
    ActionStream.SalienceValue = 0.5f;
    ActionStream.AffordanceValue = 0.5f;
    StreamStates.Add(ActionStream);

    // Stream 3: Simulation (phase offset 8 = 240°)
    FConsciousnessStreamState SimulationStream;
    SimulationStream.StreamType = EConsciousnessStream::Simulation;
    SimulationStream.CurrentStep = 9; // 8 steps ahead
    SimulationStream.PhaseOffset = 8;
    SimulationStream.Mode = ECognitiveStreamMode::Reflective;
    SimulationStream.StepType = ECognitiveStreamStepType::SalienceSimulation;
    SimulationStream.ActivationLevel = 1.0f;
    SimulationStream.InterStreamCoherence = 1.0f;
    SimulationStream.SalienceValue = 0.5f;
    SimulationStream.AffordanceValue = 0.5f;
    StreamStates.Add(SimulationStream);
}

void UCognitiveStreamCycleManager::InitializeNestedShells()
{
    NestedShells.Empty();

    // OEIS A000081 structure:
    // Level 1: 1 term, 1 step apart
    // Level 2: 2 terms, 2 steps apart
    // Level 3: 4 terms, 3 steps apart
    // Level 4: 9 terms, 4 steps apart

    const int32 TermCounts[] = {1, 2, 4, 9};
    const int32 StepsApart[] = {1, 2, 3, 4};

    for (int32 i = 0; i < 4; ++i)
    {
        FCognitiveStreamShellState Shell;
        Shell.NestingLevel = i + 1;
        Shell.TermCount = TermCounts[i];
        Shell.StepsApart = StepsApart[i];
        Shell.Coherence = 1.0f;

        // Initialize term values
        Shell.TermValues.SetNum(Shell.TermCount);
        for (int32 j = 0; j < Shell.TermCount; ++j)
        {
            Shell.TermValues[j] = 0.5f;
        }

        NestedShells.Add(Shell);
    }
}

void UCognitiveStreamCycleManager::InitializeTriads()
{
    TriadStates.Empty();

    // Triad 1: {1, 5, 9}
    FTriadState Triad1;
    Triad1.Group = ETriadGroup::Triad1;
    Triad1.Steps = {1, 5, 9};
    Triad1.Activation = 0.0f;
    Triad1.Coherence = 1.0f;
    TriadStates.Add(Triad1);

    // Triad 2: {2, 6, 10}
    FTriadState Triad2;
    Triad2.Group = ETriadGroup::Triad2;
    Triad2.Steps = {2, 6, 10};
    Triad2.Activation = 0.0f;
    Triad2.Coherence = 1.0f;
    TriadStates.Add(Triad2);

    // Triad 3: {3, 7, 11}
    FTriadState Triad3;
    Triad3.Group = ETriadGroup::Triad3;
    Triad3.Steps = {3, 7, 11};
    Triad3.Activation = 0.0f;
    Triad3.Coherence = 1.0f;
    TriadStates.Add(Triad3);

    // Triad 4: {4, 8, 12}
    FTriadState Triad4;
    Triad4.Group = ETriadGroup::Triad4;
    Triad4.Steps = {4, 8, 12};
    Triad4.Activation = 0.0f;
    Triad4.Coherence = 1.0f;
    TriadStates.Add(Triad4);
}

// ========================================
// CYCLE CONTROL
// ========================================

void UCognitiveStreamCycleManager::AdvanceStep()
{
    // Process current step
    ProcessCurrentStep();

    // Advance global step
    CycleState.GlobalStep++;
    if (CycleState.GlobalStep > 12)
    {
        CycleState.GlobalStep = 1;
        CycleState.CyclesCompleted++;
        CycleState.ExpressiveStepsRemaining = 7;
        CycleState.ReflectiveStepsRemaining = 5;
    }

    // Update cycle state
    CycleState.CurrentMode = CalculateStepMode(CycleState.GlobalStep);
    CycleState.CurrentStepType = CalculateStepType(CycleState.GlobalStep);
    CycleState.CurrentTriad = CalculateTriadGroup(CycleState.GlobalStep);

    // Update mode counters
    if (CycleState.CurrentMode == ECognitiveStreamMode::Expressive)
    {
        CycleState.ExpressiveStepsRemaining--;
    }
    else
    {
        CycleState.ReflectiveStepsRemaining--;
    }

    // Update all streams
    for (FConsciousnessStreamState& Stream : StreamStates)
    {
        UpdateStream(Stream);
    }

    // Update nested shells
    UpdateNestedShells();

    // Update triad activations
    UpdateTriadActivations();

    // Apply inter-stream feedback if enabled
    if (bEnableInterStreamAwareness)
    {
        ApplyInterStreamFeedback();
    }
}

void UCognitiveStreamCycleManager::AdvanceSteps(int32 Steps)
{
    for (int32 i = 0; i < Steps; ++i)
    {
        AdvanceStep();
    }
}

void UCognitiveStreamCycleManager::ProcessCurrentStep()
{
    // Process based on current step type
    switch (CycleState.CurrentStepType)
    {
        case ECognitiveStreamStepType::RelevanceRealization:
            // Pivotal step: orient present commitment
            for (FConsciousnessStreamState& Stream : StreamStates)
            {
                // Relevance realization affects all streams equally
                Stream.SalienceValue = FMath::Lerp(Stream.SalienceValue, 0.5f, 0.3f);
                Stream.AffordanceValue = FMath::Lerp(Stream.AffordanceValue, 0.5f, 0.3f);
            }
            break;

        case ECognitiveStreamStepType::AffordanceInteraction:
            // Actual affordance: condition past performance
            for (FConsciousnessStreamState& Stream : StreamStates)
            {
                if (Stream.StreamType == EConsciousnessStream::Action)
                {
                    // Action stream is primary during affordance interaction
                    Stream.ActivationLevel = FMath::Min(1.0f, Stream.ActivationLevel + 0.1f);
                }
            }
            break;

        case ECognitiveStreamStepType::SalienceSimulation:
            // Virtual salience: anticipate future potential
            for (FConsciousnessStreamState& Stream : StreamStates)
            {
                if (Stream.StreamType == EConsciousnessStream::Simulation)
                {
                    // Simulation stream is primary during salience simulation
                    Stream.ActivationLevel = FMath::Min(1.0f, Stream.ActivationLevel + 0.1f);
                }
            }
            break;
    }
}

// ========================================
// STREAM ACCESS
// ========================================

FConsciousnessStreamState UCognitiveStreamCycleManager::GetStreamState(EConsciousnessStream StreamType) const
{
    for (const FConsciousnessStreamState& Stream : StreamStates)
    {
        if (Stream.StreamType == StreamType)
        {
            return Stream;
        }
    }
    return FConsciousnessStreamState();
}

int32 UCognitiveStreamCycleManager::GetStreamStep(EConsciousnessStream StreamType) const
{
    for (const FConsciousnessStreamState& Stream : StreamStates)
    {
        if (Stream.StreamType == StreamType)
        {
            return Stream.CurrentStep;
        }
    }
    return 1;
}

float UCognitiveStreamCycleManager::GetStreamActivation(EConsciousnessStream StreamType) const
{
    for (const FConsciousnessStreamState& Stream : StreamStates)
    {
        if (Stream.StreamType == StreamType)
        {
            return Stream.ActivationLevel;
        }
    }
    return 0.0f;
}

void UCognitiveStreamCycleManager::SetStreamSalience(EConsciousnessStream StreamType, float Salience)
{
    for (FConsciousnessStreamState& Stream : StreamStates)
    {
        if (Stream.StreamType == StreamType)
        {
            Stream.SalienceValue = FMath::Clamp(Salience, 0.0f, 1.0f);
            return;
        }
    }
}

void UCognitiveStreamCycleManager::SetStreamAffordance(EConsciousnessStream StreamType, float Affordance)
{
    for (FConsciousnessStreamState& Stream : StreamStates)
    {
        if (Stream.StreamType == StreamType)
        {
            Stream.AffordanceValue = FMath::Clamp(Affordance, 0.0f, 1.0f);
            return;
        }
    }
}

// ========================================
// TRIAD ACCESS
// ========================================

ETriadGroup UCognitiveStreamCycleManager::GetCurrentTriad() const
{
    return CycleState.CurrentTriad;
}

FTriadState UCognitiveStreamCycleManager::GetTriadState(ETriadGroup Group) const
{
    int32 Index = static_cast<int32>(Group);
    if (TriadStates.IsValidIndex(Index))
    {
        return TriadStates[Index];
    }
    return FTriadState();
}

bool UCognitiveStreamCycleManager::IsStepInTriad(int32 Step, ETriadGroup Group) const
{
    int32 Index = static_cast<int32>(Group);
    if (TriadStates.IsValidIndex(Index))
    {
        return TriadStates[Index].Steps.Contains(Step);
    }
    return false;
}

// ========================================
// NESTED SHELLS
// ========================================

FCognitiveStreamShellState UCognitiveStreamCycleManager::GetNestedShellState(int32 Level) const
{
    int32 Index = Level - 1;
    if (NestedShells.IsValidIndex(Index))
    {
        return NestedShells[Index];
    }
    return FCognitiveStreamShellState();
}

int32 UCognitiveStreamCycleManager::GetTotalNestedTerms() const
{
    // OEIS A000081: 1 + 2 + 4 + 9 = 16 total terms
    int32 Total = 0;
    for (const FCognitiveStreamShellState& Shell : NestedShells)
    {
        Total += Shell.TermCount;
    }
    return Total;
}

// ========================================
// INTER-STREAM AWARENESS
// ========================================

float UCognitiveStreamCycleManager::GetPerceptionOfAction() const
{
    // Stream 1 (Perception) perceives Stream 2's (Action) state
    float PerceptionActivation = 0.0f;
    float ActionAffordance = 0.0f;

    for (const FConsciousnessStreamState& Stream : StreamStates)
    {
        if (Stream.StreamType == EConsciousnessStream::Perception)
        {
            PerceptionActivation = Stream.ActivationLevel;
        }
        else if (Stream.StreamType == EConsciousnessStream::Action)
        {
            ActionAffordance = Stream.AffordanceValue;
        }
    }

    return PerceptionActivation * ActionAffordance;
}

float UCognitiveStreamCycleManager::GetSimulationOfAction() const
{
    // Stream 3 (Simulation) reflects on Stream 2's (Action) state
    float SimulationActivation = 0.0f;
    float ActionAffordance = 0.0f;

    for (const FConsciousnessStreamState& Stream : StreamStates)
    {
        if (Stream.StreamType == EConsciousnessStream::Simulation)
        {
            SimulationActivation = Stream.ActivationLevel;
        }
        else if (Stream.StreamType == EConsciousnessStream::Action)
        {
            ActionAffordance = Stream.AffordanceValue;
        }
    }

    return SimulationActivation * ActionAffordance;
}

float UCognitiveStreamCycleManager::CalculateInterStreamCoherence() const
{
    if (StreamStates.Num() < 3)
    {
        return 0.0f;
    }

    // Calculate coherence based on how well streams are synchronized
    float TotalCoherence = 0.0f;
    for (const FConsciousnessStreamState& Stream : StreamStates)
    {
        TotalCoherence += Stream.InterStreamCoherence;
    }

    return TotalCoherence / StreamStates.Num();
}

// ========================================
// STEP TYPE QUERIES
// ========================================

ECognitiveStreamStepType UCognitiveStreamCycleManager::GetStepType(int32 Step) const
{
    return CalculateStepType(Step);
}

ECognitiveStreamMode UCognitiveStreamCycleManager::GetStepMode(int32 Step) const
{
    return CalculateStepMode(Step);
}

bool UCognitiveStreamCycleManager::IsRelevanceRealizationStep(int32 Step) const
{
    // Steps 1 and 7 are pivotal relevance realization steps
    return (Step == 1 || Step == 7);
}

// ========================================
// INTERNAL METHODS
// ========================================

void UCognitiveStreamCycleManager::UpdateStream(FConsciousnessStreamState& Stream)
{
    // Calculate stream's current step based on global step and phase offset
    int32 StreamStep = ((CycleState.GlobalStep - 1 + Stream.PhaseOffset) % 12) + 1;
    Stream.CurrentStep = StreamStep;

    // Update mode and step type
    Stream.Mode = CalculateStepMode(StreamStep);
    Stream.StepType = CalculateStepType(StreamStep);

    // Decay activation slightly each step
    Stream.ActivationLevel = FMath::Max(0.5f, Stream.ActivationLevel - 0.02f);
}

void UCognitiveStreamCycleManager::UpdateNestedShells()
{
    for (FCognitiveStreamShellState& Shell : NestedShells)
    {
        // Update terms based on current step
        int32 ActiveTermIndex = (CycleState.GlobalStep - 1) % Shell.TermCount;
        
        for (int32 i = 0; i < Shell.TermValues.Num(); ++i)
        {
            if (i == ActiveTermIndex)
            {
                // Active term increases
                Shell.TermValues[i] = FMath::Min(1.0f, Shell.TermValues[i] + 0.1f);
            }
            else
            {
                // Inactive terms decay
                Shell.TermValues[i] = FMath::Max(0.0f, Shell.TermValues[i] - 0.05f);
            }
        }

        // Calculate shell coherence
        float Sum = 0.0f;
        for (float Value : Shell.TermValues)
        {
            Sum += Value;
        }
        Shell.Coherence = Sum / FMath::Max(1, Shell.TermCount);
    }
}

void UCognitiveStreamCycleManager::UpdateTriadActivations()
{
    for (FTriadState& Triad : TriadStates)
    {
        // Check if current step is in this triad
        bool bIsActive = Triad.Steps.Contains(CycleState.GlobalStep);
        
        if (bIsActive)
        {
            Triad.Activation = FMath::Min(1.0f, Triad.Activation + 0.3f);
        }
        else
        {
            Triad.Activation = FMath::Max(0.0f, Triad.Activation - 0.1f);
        }
    }
}

ECognitiveStreamStepType UCognitiveStreamCycleManager::CalculateStepType(int32 Step) const
{
    // Step composition:
    // 1: Pivotal relevance realization (orienting present commitment)
    // 2-6: Actual affordance interaction (conditioning past performance)
    // 7: Pivotal relevance realization (orienting present commitment)
    // 8-12: Virtual salience simulation (anticipating future potential)

    if (Step == 1 || Step == 7)
    {
        return ECognitiveStreamStepType::RelevanceRealization;
    }
    else if (Step >= 2 && Step <= 6)
    {
        return ECognitiveStreamStepType::AffordanceInteraction;
    }
    else // Steps 8-12
    {
        return ECognitiveStreamStepType::SalienceSimulation;
    }
}

ECognitiveStreamMode UCognitiveStreamCycleManager::CalculateStepMode(int32 Step) const
{
    // 7 expressive steps: 1, 2, 3, 4, 5, 6, 7
    // 5 reflective steps: 8, 9, 10, 11, 12

    if (Step <= 7)
    {
        return ECognitiveStreamMode::Expressive;
    }
    else
    {
        return ECognitiveStreamMode::Reflective;
    }
}

ETriadGroup UCognitiveStreamCycleManager::CalculateTriadGroup(int32 Step) const
{
    // Triads: {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12}
    // Pattern: (Step - 1) % 4

    int32 TriadIndex = (Step - 1) % 4;
    return static_cast<ETriadGroup>(TriadIndex);
}

void UCognitiveStreamCycleManager::ApplyInterStreamFeedback()
{
    // Inter-stream awareness: each stream is aware of others' states
    // Stream 1 perceives Stream 2's action
    // Stream 3 reflects on a simulation of the action

    float PerceptionOfAction = GetPerceptionOfAction();
    float SimulationOfAction = GetSimulationOfAction();

    for (FConsciousnessStreamState& Stream : StreamStates)
    {
        switch (Stream.StreamType)
        {
            case EConsciousnessStream::Perception:
                // Perception is influenced by action feedback
                Stream.InterStreamCoherence = FMath::Lerp(Stream.InterStreamCoherence, PerceptionOfAction, 0.1f);
                break;

            case EConsciousnessStream::Action:
                // Action is influenced by both perception and simulation
                Stream.InterStreamCoherence = FMath::Lerp(Stream.InterStreamCoherence, 
                    (PerceptionOfAction + SimulationOfAction) * 0.5f, 0.1f);
                break;

            case EConsciousnessStream::Simulation:
                // Simulation is influenced by action
                Stream.InterStreamCoherence = FMath::Lerp(Stream.InterStreamCoherence, SimulationOfAction, 0.1f);
                break;
        }
    }
}
