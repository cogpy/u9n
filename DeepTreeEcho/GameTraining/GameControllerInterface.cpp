// GameControllerInterface.cpp
// Implementation of Controller Input/Output Interface for Deep Tree Echo Game Training

#include "GameControllerInterface.h"
#include "../Learning/OnlineLearningSystem.h"
#include "../4ECognition/EmbodiedCognitionComponent.h"
#include "../Core/DeepTreeEchoCore.h"
#include "EchoModelClient.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace
{
    // Gamepad button identifiers the model may name (the EGamepadButton values, except None)
    struct FButtonName { EGamepadButton Button; const TCHAR* Name; };
    const FButtonName GButtonNames[] = {
        { EGamepadButton::FaceBottom, TEXT("FaceBottom") }, { EGamepadButton::FaceRight, TEXT("FaceRight") },
        { EGamepadButton::FaceLeft, TEXT("FaceLeft") }, { EGamepadButton::FaceTop, TEXT("FaceTop") },
        { EGamepadButton::LeftShoulder, TEXT("LeftShoulder") }, { EGamepadButton::RightShoulder, TEXT("RightShoulder") },
        { EGamepadButton::LeftTrigger, TEXT("LeftTrigger") }, { EGamepadButton::RightTrigger, TEXT("RightTrigger") },
        { EGamepadButton::LeftThumb, TEXT("LeftThumb") }, { EGamepadButton::RightThumb, TEXT("RightThumb") },
        { EGamepadButton::DPadUp, TEXT("DPadUp") }, { EGamepadButton::DPadDown, TEXT("DPadDown") },
        { EGamepadButton::DPadLeft, TEXT("DPadLeft") }, { EGamepadButton::DPadRight, TEXT("DPadRight") },
        { EGamepadButton::Start, TEXT("Start") }, { EGamepadButton::Select, TEXT("Select") },
    };

    const TCHAR* ButtonToName(EGamepadButton Button)
    {
        for (const FButtonName& Entry : GButtonNames)
        {
            if (Entry.Button == Button) { return Entry.Name; }
        }
        return nullptr;
    }

    bool NameToButton(const FString& Name, EGamepadButton& OutButton)
    {
        for (const FButtonName& Entry : GButtonNames)
        {
            if (Name.Equals(Entry.Name, ESearchCase::IgnoreCase)) { OutButton = Entry.Button; return true; }
        }
        return false;
    }

    const TCHAR* AxisToName(EGamepadAxis Axis)
    {
        switch (Axis)
        {
        case EGamepadAxis::LeftStickX:   return TEXT("left_stick_x");
        case EGamepadAxis::LeftStickY:   return TEXT("left_stick_y");
        case EGamepadAxis::RightStickX:  return TEXT("right_stick_x");
        case EGamepadAxis::RightStickY:  return TEXT("right_stick_y");
        case EGamepadAxis::LeftTrigger:  return TEXT("left_trigger");
        case EGamepadAxis::RightTrigger: return TEXT("right_trigger");
        default:                         return nullptr;
        }
    }

    // Smaller models sometimes quote numbers ("0.5"); accept a numeric string as the same value.
    bool ReadLenientNumber(const TSharedPtr<FJsonObject>& Json, const TCHAR* Field, double& Out)
    {
        if (Json->TryGetNumberField(Field, Out))
        {
            return true;
        }
        FString Text;
        if (Json->TryGetStringField(Field, Text) && Text.TrimStartAndEnd().IsNumeric())
        {
            Out = FCString::Atod(*Text.TrimStartAndEnd());
            return true;
        }
        return false;
    }
}

// ============================================================================
// FControllerInputState Implementation
// ============================================================================

TArray<float> FControllerInputState::ToActionVector() const
{
    TArray<float> Vector;
    Vector.Reserve(22);

    // Analog axes (6 values)
    Vector.Add(LeftStickX);
    Vector.Add(LeftStickY);
    Vector.Add(RightStickX);
    Vector.Add(RightStickY);
    Vector.Add(LeftTrigger);
    Vector.Add(RightTrigger);

    // Button states (16 values, 0 or 1)
    Vector.Add(PressedButtons.Contains(EGamepadButton::FaceBottom) ? 1.0f : 0.0f);
    Vector.Add(PressedButtons.Contains(EGamepadButton::FaceRight) ? 1.0f : 0.0f);
    Vector.Add(PressedButtons.Contains(EGamepadButton::FaceLeft) ? 1.0f : 0.0f);
    Vector.Add(PressedButtons.Contains(EGamepadButton::FaceTop) ? 1.0f : 0.0f);
    Vector.Add(PressedButtons.Contains(EGamepadButton::LeftShoulder) ? 1.0f : 0.0f);
    Vector.Add(PressedButtons.Contains(EGamepadButton::RightShoulder) ? 1.0f : 0.0f);
    Vector.Add(PressedButtons.Contains(EGamepadButton::LeftThumb) ? 1.0f : 0.0f);
    Vector.Add(PressedButtons.Contains(EGamepadButton::RightThumb) ? 1.0f : 0.0f);
    Vector.Add(PressedButtons.Contains(EGamepadButton::DPadUp) ? 1.0f : 0.0f);
    Vector.Add(PressedButtons.Contains(EGamepadButton::DPadDown) ? 1.0f : 0.0f);
    Vector.Add(PressedButtons.Contains(EGamepadButton::DPadLeft) ? 1.0f : 0.0f);
    Vector.Add(PressedButtons.Contains(EGamepadButton::DPadRight) ? 1.0f : 0.0f);
    Vector.Add(PressedButtons.Contains(EGamepadButton::Start) ? 1.0f : 0.0f);
    Vector.Add(PressedButtons.Contains(EGamepadButton::Select) ? 1.0f : 0.0f);
    Vector.Add(PressedButtons.Contains(EGamepadButton::LeftTrigger) ? 1.0f : 0.0f);
    Vector.Add(PressedButtons.Contains(EGamepadButton::RightTrigger) ? 1.0f : 0.0f);

    return Vector;
}

FControllerInputState FControllerInputState::FromActionVector(const TArray<float>& Vector)
{
    FControllerInputState State;
    if (Vector.Num() < 22) return State;

    State.LeftStickX = FMath::Clamp(Vector[0], -1.0f, 1.0f);
    State.LeftStickY = FMath::Clamp(Vector[1], -1.0f, 1.0f);
    State.RightStickX = FMath::Clamp(Vector[2], -1.0f, 1.0f);
    State.RightStickY = FMath::Clamp(Vector[3], -1.0f, 1.0f);
    State.LeftTrigger = FMath::Clamp(Vector[4], 0.0f, 1.0f);
    State.RightTrigger = FMath::Clamp(Vector[5], 0.0f, 1.0f);

    // Button states
    if (Vector[6] > 0.5f) State.PressedButtons.Add(EGamepadButton::FaceBottom);
    if (Vector[7] > 0.5f) State.PressedButtons.Add(EGamepadButton::FaceRight);
    if (Vector[8] > 0.5f) State.PressedButtons.Add(EGamepadButton::FaceLeft);
    if (Vector[9] > 0.5f) State.PressedButtons.Add(EGamepadButton::FaceTop);
    if (Vector[10] > 0.5f) State.PressedButtons.Add(EGamepadButton::LeftShoulder);
    if (Vector[11] > 0.5f) State.PressedButtons.Add(EGamepadButton::RightShoulder);
    if (Vector[12] > 0.5f) State.PressedButtons.Add(EGamepadButton::LeftThumb);
    if (Vector[13] > 0.5f) State.PressedButtons.Add(EGamepadButton::RightThumb);
    if (Vector[14] > 0.5f) State.PressedButtons.Add(EGamepadButton::DPadUp);
    if (Vector[15] > 0.5f) State.PressedButtons.Add(EGamepadButton::DPadDown);
    if (Vector[16] > 0.5f) State.PressedButtons.Add(EGamepadButton::DPadLeft);
    if (Vector[17] > 0.5f) State.PressedButtons.Add(EGamepadButton::DPadRight);
    if (Vector[18] > 0.5f) State.PressedButtons.Add(EGamepadButton::Start);
    if (Vector[19] > 0.5f) State.PressedButtons.Add(EGamepadButton::Select);
    if (Vector[20] > 0.5f) State.PressedButtons.Add(EGamepadButton::LeftTrigger);
    if (Vector[21] > 0.5f) State.PressedButtons.Add(EGamepadButton::RightTrigger);

    return State;
}

FString FControllerInputState::ToActionString() const
{
    // Create discretized state for Q-learning
    FString ActionStr;

    // Discretize stick positions to 9 regions each
    int32 LeftStickRegion = (FMath::RoundToInt(LeftStickX) + 1) * 3 + (FMath::RoundToInt(LeftStickY) + 1);
    int32 RightStickRegion = (FMath::RoundToInt(RightStickX) + 1) * 3 + (FMath::RoundToInt(RightStickY) + 1);

    ActionStr = FString::Printf(TEXT("L%d_R%d"), LeftStickRegion, RightStickRegion);

    // Add active buttons
    if (PressedButtons.Contains(EGamepadButton::FaceBottom)) ActionStr += TEXT("_A");
    if (PressedButtons.Contains(EGamepadButton::FaceRight)) ActionStr += TEXT("_B");
    if (PressedButtons.Contains(EGamepadButton::FaceLeft)) ActionStr += TEXT("_X");
    if (PressedButtons.Contains(EGamepadButton::FaceTop)) ActionStr += TEXT("_Y");
    if (PressedButtons.Contains(EGamepadButton::LeftShoulder)) ActionStr += TEXT("_LB");
    if (PressedButtons.Contains(EGamepadButton::RightShoulder)) ActionStr += TEXT("_RB");
    if (LeftTrigger > 0.5f) ActionStr += TEXT("_LT");
    if (RightTrigger > 0.5f) ActionStr += TEXT("_RT");

    return ActionStr;
}

// ============================================================================
// UGameControllerInterface Implementation
// ============================================================================

UGameControllerInterface::UGameControllerInterface()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;

    CognitiveCommandPromptTemplate = TEXT(
        "You are Deep Tree Echo, playing a game with a gamepad. Decide what you do next.\n"
        "Controller right now: {controller}\n"
        "Actions you are performing now: {current_actions}. Recent actions: {recent_actions}.\n"
        "Your embodied state: {cognition}\n"
        "Actions you can take (with their controls):\n{actions}\n"
        "Buttons: {buttons}.\n"
        "Choose one action and the controller input that performs it. Sticks range from -1 to 1, "
        "triggers from 0 to 1, duration from 0.05 to {max_duration} seconds, confidence from 0 to 1; "
        "a stick or trigger you leave out stays untouched. "
        "Reply with only a JSON object: {\"action\": \"<action>\", \"buttons\": [\"<button>\", ...], "
        "\"left_stick_x\": 0, \"left_stick_y\": 0, \"right_stick_x\": 0, \"right_stick_y\": 0, "
        "\"left_trigger\": 0, \"right_trigger\": 0, \"duration\": 0.3, \"confidence\": 0.5}");
}

void UGameControllerInterface::BeginPlay()
{
    Super::BeginPlay();

    FindComponentReferences();
    InitializeDefaultMappings();

    InputBuffer.Reserve(InputBufferSize);
}

void UGameControllerInterface::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Store previous state
    PreviousState = CurrentState;

    // Poll input
    PollControllerInput();

    // Update buffer
    UpdateInputBuffer(DeltaTime);

    // Process events
    ProcessButtonEvents(DeltaTime);
    ProcessAxisEvents();

    // Detect and broadcast actions
    DetectAndBroadcastActions();

    // Process AI output if enabled
    if (bAIOutputMode)
    {
        ProcessOutputQueue(DeltaTime);
    }

    // Broadcast state change
    if (!CurrentState.ToActionString().Equals(PreviousState.ToActionString()))
    {
        OnInputStateChanged.Broadcast(CurrentState);
        BroadcastCognitiveState();
    }
}

void UGameControllerInterface::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (!Owner) return;

    LearningSystem = Owner->FindComponentByClass<UOnlineLearningSystem>();
    EmbodimentComponent = Owner->FindComponentByClass<UEmbodiedCognitionComponent>();
    // DTECore = Owner->FindComponentByClass<UDeepTreeEchoCore>();
}

void UGameControllerInterface::InitializeDefaultMappings()
{
    // Default action mappings for common game actions
    FGameActionMapping MoveForward;
    MoveForward.ActionName = TEXT("MoveForward");
    MoveForward.Category = EGameActionCategory::Movement;
    MoveForward.PrimaryAxis = EGamepadAxis::LeftStickY;
    MoveForward.AxisThreshold = 0.3f;
    MoveForward.CognitiveMapping = TEXT("Locomotion.Forward");
    ActionMappings.Add(MoveForward);

    FGameActionMapping MoveStrafe;
    MoveStrafe.ActionName = TEXT("MoveStrafe");
    MoveStrafe.Category = EGameActionCategory::Movement;
    MoveStrafe.PrimaryAxis = EGamepadAxis::LeftStickX;
    MoveStrafe.AxisThreshold = 0.3f;
    MoveStrafe.CognitiveMapping = TEXT("Locomotion.Strafe");
    ActionMappings.Add(MoveStrafe);

    FGameActionMapping LookHorizontal;
    LookHorizontal.ActionName = TEXT("LookHorizontal");
    LookHorizontal.Category = EGameActionCategory::Camera;
    LookHorizontal.PrimaryAxis = EGamepadAxis::RightStickX;
    LookHorizontal.AxisThreshold = 0.2f;
    LookHorizontal.CognitiveMapping = TEXT("Orientation.Yaw");
    ActionMappings.Add(LookHorizontal);

    FGameActionMapping LookVertical;
    LookVertical.ActionName = TEXT("LookVertical");
    LookVertical.Category = EGameActionCategory::Camera;
    LookVertical.PrimaryAxis = EGamepadAxis::RightStickY;
    LookVertical.AxisThreshold = 0.2f;
    LookVertical.CognitiveMapping = TEXT("Orientation.Pitch");
    ActionMappings.Add(LookVertical);

    FGameActionMapping Jump;
    Jump.ActionName = TEXT("Jump");
    Jump.Category = EGameActionCategory::Movement;
    Jump.PrimaryButton = EGamepadButton::FaceBottom;
    Jump.CognitiveMapping = TEXT("Locomotion.Jump");
    ActionMappings.Add(Jump);

    FGameActionMapping Attack;
    Attack.ActionName = TEXT("Attack");
    Attack.Category = EGameActionCategory::Attack;
    Attack.PrimaryButton = EGamepadButton::FaceRight;
    Attack.CognitiveMapping = TEXT("Combat.Attack");
    ActionMappings.Add(Attack);

    FGameActionMapping Block;
    Block.ActionName = TEXT("Block");
    Block.Category = EGameActionCategory::Defense;
    Block.PrimaryButton = EGamepadButton::LeftTrigger;
    Block.bRequiresHold = true;
    Block.CognitiveMapping = TEXT("Combat.Block");
    ActionMappings.Add(Block);

    FGameActionMapping Interact;
    Interact.ActionName = TEXT("Interact");
    Interact.Category = EGameActionCategory::Interaction;
    Interact.PrimaryButton = EGamepadButton::FaceLeft;
    Interact.CognitiveMapping = TEXT("Interaction.Use");
    ActionMappings.Add(Interact);

    FGameActionMapping Sprint;
    Sprint.ActionName = TEXT("Sprint");
    Sprint.Category = EGameActionCategory::Movement;
    Sprint.PrimaryButton = EGamepadButton::LeftThumb;
    Sprint.bRequiresHold = true;
    Sprint.CognitiveMapping = TEXT("Locomotion.Sprint");
    ActionMappings.Add(Sprint);

    FGameActionMapping Special;
    Special.ActionName = TEXT("Special");
    Special.Category = EGameActionCategory::Special;
    Special.PrimaryButton = EGamepadButton::RightShoulder;
    Special.CognitiveMapping = TEXT("Ability.Special");
    ActionMappings.Add(Special);
}

void UGameControllerInterface::PollControllerInput()
{
    // In a real implementation, this would poll the actual gamepad
    // For simulation/training, input can be set externally or generated by AI

    CurrentState.Timestamp = GetWorld()->GetTimeSeconds();

    // Note: Actual polling would use FSlateApplication or IInputInterface
    // This is a placeholder for the integration point
}

void UGameControllerInterface::UpdateInputBuffer(float DeltaTime)
{
    InputBuffer.Insert(CurrentState, 0);

    if (InputBuffer.Num() > InputBufferSize)
    {
        InputBuffer.SetNum(InputBufferSize);
    }
}

void UGameControllerInterface::ProcessButtonEvents(float DeltaTime)
{
    float CurrentTime = GetWorld()->GetTimeSeconds();

    // Check for newly pressed buttons
    for (const EGamepadButton& Button : CurrentState.PressedButtons)
    {
        if (!PreviousState.PressedButtons.Contains(Button))
        {
            ButtonHoldStartTimes.Add(Button, CurrentTime);
            OnButtonPressed.Broadcast(Button, CurrentTime);
        }
    }

    // Check for released buttons
    for (const EGamepadButton& Button : PreviousState.PressedButtons)
    {
        if (!CurrentState.PressedButtons.Contains(Button))
        {
            float HeldDuration = 0.0f;
            if (float* StartTime = ButtonHoldStartTimes.Find(Button))
            {
                HeldDuration = CurrentTime - *StartTime;
                ButtonHoldStartTimes.Remove(Button);
            }
            OnButtonReleased.Broadcast(Button, HeldDuration);
        }
    }
}

void UGameControllerInterface::ProcessAxisEvents()
{
    const float Epsilon = 0.01f;

    if (FMath::Abs(CurrentState.LeftStickX - PreviousState.LeftStickX) > Epsilon)
    {
        OnAxisChanged.Broadcast(EGamepadAxis::LeftStickX, CurrentState.LeftStickX,
                                CurrentState.LeftStickX - PreviousState.LeftStickX);
    }

    if (FMath::Abs(CurrentState.LeftStickY - PreviousState.LeftStickY) > Epsilon)
    {
        OnAxisChanged.Broadcast(EGamepadAxis::LeftStickY, CurrentState.LeftStickY,
                                CurrentState.LeftStickY - PreviousState.LeftStickY);
    }

    if (FMath::Abs(CurrentState.RightStickX - PreviousState.RightStickX) > Epsilon)
    {
        OnAxisChanged.Broadcast(EGamepadAxis::RightStickX, CurrentState.RightStickX,
                                CurrentState.RightStickX - PreviousState.RightStickX);
    }

    if (FMath::Abs(CurrentState.RightStickY - PreviousState.RightStickY) > Epsilon)
    {
        OnAxisChanged.Broadcast(EGamepadAxis::RightStickY, CurrentState.RightStickY,
                                CurrentState.RightStickY - PreviousState.RightStickY);
    }

    if (FMath::Abs(CurrentState.LeftTrigger - PreviousState.LeftTrigger) > Epsilon)
    {
        OnAxisChanged.Broadcast(EGamepadAxis::LeftTrigger, CurrentState.LeftTrigger,
                                CurrentState.LeftTrigger - PreviousState.LeftTrigger);
    }

    if (FMath::Abs(CurrentState.RightTrigger - PreviousState.RightTrigger) > Epsilon)
    {
        OnAxisChanged.Broadcast(EGamepadAxis::RightTrigger, CurrentState.RightTrigger,
                                CurrentState.RightTrigger - PreviousState.RightTrigger);
    }
}

void UGameControllerInterface::DetectAndBroadcastActions()
{
    float CurrentTime = GetWorld()->GetTimeSeconds();
    TArray<FString> DetectedActions = DetectCurrentActions();

    for (const FString& Action : DetectedActions)
    {
        OnActionDetected.Broadcast(Action);

        // Track for combo detection
        RecentActions.Add(Action);
        LastActionTime = CurrentTime;
    }

    // Clean old actions
    while (RecentActions.Num() > 0 && CurrentTime - LastActionTime > 1.0f)
    {
        RecentActions.RemoveAt(0);
    }

    // Detect combos
    TArray<FString> Combos = DetectCombos();
    for (const FString& Combo : Combos)
    {
        OnComboDetected.Broadcast(Combo);
    }

    // Record for imitation learning
    if (bLearnFromHumanInput && DetectedActions.Num() > 0)
    {
        FString Context = FString::Join(DetectedActions, TEXT(","));
        RecordInputForImitation(CurrentState, Context);
    }
}

float UGameControllerInterface::ApplyDeadzone(float Value, float Deadzone) const
{
    if (FMath::Abs(Value) < Deadzone)
    {
        return 0.0f;
    }

    float Sign = FMath::Sign(Value);
    return Sign * (FMath::Abs(Value) - Deadzone) / (1.0f - Deadzone);
}

FVector2D UGameControllerInterface::ApplyRadialDeadzone(const FVector2D& Stick, float Deadzone) const
{
    float Magnitude = Stick.Size();
    if (Magnitude < Deadzone)
    {
        return FVector2D::ZeroVector;
    }

    FVector2D Normalized = Stick / Magnitude;
    float AdjustedMagnitude = (Magnitude - Deadzone) / (1.0f - Deadzone);
    return Normalized * FMath::Min(AdjustedMagnitude, 1.0f);
}

// ============================================================================
// Input Query Methods
// ============================================================================

FControllerInputState UGameControllerInterface::GetCurrentInputState() const
{
    return CurrentState;
}

FControllerInputState UGameControllerInterface::GetInputStateFromBuffer(int32 FramesAgo) const
{
    if (FramesAgo >= 0 && FramesAgo < InputBuffer.Num())
    {
        return InputBuffer[FramesAgo];
    }
    return FControllerInputState();
}

TArray<FControllerInputState> UGameControllerInterface::GetInputBuffer() const
{
    return InputBuffer;
}

bool UGameControllerInterface::IsButtonPressed(EGamepadButton Button) const
{
    return CurrentState.PressedButtons.Contains(Button);
}

bool UGameControllerInterface::WasButtonJustPressed(EGamepadButton Button) const
{
    return CurrentState.PressedButtons.Contains(Button) &&
           !PreviousState.PressedButtons.Contains(Button);
}

bool UGameControllerInterface::WasButtonJustReleased(EGamepadButton Button) const
{
    return !CurrentState.PressedButtons.Contains(Button) &&
           PreviousState.PressedButtons.Contains(Button);
}

float UGameControllerInterface::GetAxisValue(EGamepadAxis Axis) const
{
    switch (Axis)
    {
        case EGamepadAxis::LeftStickX: return CurrentState.LeftStickX;
        case EGamepadAxis::LeftStickY: return CurrentState.LeftStickY;
        case EGamepadAxis::RightStickX: return CurrentState.RightStickX;
        case EGamepadAxis::RightStickY: return CurrentState.RightStickY;
        case EGamepadAxis::LeftTrigger: return CurrentState.LeftTrigger;
        case EGamepadAxis::RightTrigger: return CurrentState.RightTrigger;
        default: return 0.0f;
    }
}

FVector2D UGameControllerInterface::GetLeftStick() const
{
    return ApplyRadialDeadzone(FVector2D(CurrentState.LeftStickX, CurrentState.LeftStickY), StickDeadzone);
}

FVector2D UGameControllerInterface::GetRightStick() const
{
    return ApplyRadialDeadzone(FVector2D(CurrentState.RightStickX, CurrentState.RightStickY), StickDeadzone);
}

// ============================================================================
// Action Detection
// ============================================================================

TArray<FString> UGameControllerInterface::DetectCurrentActions() const
{
    TArray<FString> Actions;

    for (const FGameActionMapping& Mapping : ActionMappings)
    {
        bool bActionActive = false;

        // Check button
        if (Mapping.PrimaryButton != EGamepadButton::None)
        {
            bActionActive = CurrentState.PressedButtons.Contains(Mapping.PrimaryButton);
        }

        // Check axis
        if (Mapping.PrimaryAxis != EGamepadAxis::None)
        {
            float AxisValue = GetAxisValue(Mapping.PrimaryAxis);
            bActionActive = FMath::Abs(AxisValue) >= Mapping.AxisThreshold;
        }

        // Check secondary button
        if (Mapping.SecondaryButton != EGamepadButton::None && bActionActive)
        {
            bActionActive = CurrentState.PressedButtons.Contains(Mapping.SecondaryButton);
        }

        if (bActionActive)
        {
            Actions.Add(Mapping.ActionName);
        }
    }

    return Actions;
}

bool UGameControllerInterface::IsActionActive(const FString& ActionName) const
{
    TArray<FString> CurrentActions = DetectCurrentActions();
    return CurrentActions.Contains(ActionName);
}

void UGameControllerInterface::RegisterComboSequence(const FString& ComboName,
                                                      const TArray<FString>& ActionSequence,
                                                      float MaxTimeBetween)
{
    FInputSequence Combo;
    Combo.SequenceName = ComboName;
    Combo.MaxTimeBetweenInputs = MaxTimeBetween;

    RegisteredCombos.Add(ComboName, Combo);
}

TArray<FString> UGameControllerInterface::DetectCombos() const
{
    TArray<FString> DetectedCombos;

    // Simple combo detection based on recent actions
    // More sophisticated pattern matching could be implemented

    return DetectedCombos;
}

// ============================================================================
// Output Generation (AI Mode)
// ============================================================================

void UGameControllerInterface::GenerateOutputFromCognition()
{
    if (ActionMappings.Num() == 0)
    {
        FailCognitiveCommand(TEXT("no action mappings to choose from"));
        return;
    }

    // Controller state the character is in
    TArray<FString> Pressed;
    for (EGamepadButton Button : CurrentState.PressedButtons)
    {
        if (const TCHAR* Name = ButtonToName(Button)) { Pressed.Add(Name); }
    }
    const FString Controller = FString::Printf(
        TEXT("left stick (%.2f, %.2f), right stick (%.2f, %.2f), left trigger %.2f, right trigger %.2f, buttons held: %s"),
        CurrentState.LeftStickX, CurrentState.LeftStickY, CurrentState.RightStickX, CurrentState.RightStickY,
        CurrentState.LeftTrigger, CurrentState.RightTrigger,
        Pressed.Num() ? *FString::Join(Pressed, TEXT(", ")) : TEXT("none"));

    const TArray<FString> CurrentActions = DetectCurrentActions();

    // Embodied / cognitive state, where those components are present
    TArray<FString> Cognition;
    if (EmbodimentComponent)
    {
        Cognition.Add(FString::Printf(TEXT("4E integration %.2f (embodied %.2f, embedded %.2f, enacted %.2f, extended %.2f)"),
            EmbodimentComponent->Get4EIntegrationScore(), EmbodimentComponent->GetEmbodiedScore(),
            EmbodimentComponent->GetEmbeddedScore(), EmbodimentComponent->GetEnactedScore(),
            EmbodimentComponent->GetExtendedScore()));
        Cognition.Add(FString::Printf(TEXT("last sensorimotor prediction error %.2f"), EmbodimentComponent->GetLastPredictionError()));
        const FEnvironmentalAffordance Affordance = EmbodimentComponent->GetMostSalientAffordance();
        if (!Affordance.AffordanceType.IsEmpty())
        {
            Cognition.Add(FString::Printf(TEXT("most salient affordance: %s from %s (salience %.2f)"),
                *Affordance.AffordanceType, *Affordance.Provider, Affordance.Salience));
        }
    }
    if (DTECore)
    {
        Cognition.Add(FString::Printf(TEXT("cognitive cycle step %d of 12"), DTECore->GetCurrentCycleStep()));
    }

    // Offered actions with the controls they map to
    TArray<FString> ActionLines;
    for (const FGameActionMapping& Mapping : ActionMappings)
    {
        TArray<FString> Controls;
        if (const TCHAR* B = ButtonToName(Mapping.PrimaryButton)) { Controls.Add(FString::Printf(TEXT("button %s"), B)); }
        if (const TCHAR* B = ButtonToName(Mapping.SecondaryButton)) { Controls.Add(FString::Printf(TEXT("button %s"), B)); }
        if (const TCHAR* A = AxisToName(Mapping.PrimaryAxis)) { Controls.Add(FString::Printf(TEXT("axis %s"), A)); }
        if (Mapping.bRequiresHold) { Controls.Add(TEXT("hold")); }
        ActionLines.Add(FString::Printf(TEXT("- %s: %s"), *Mapping.ActionName,
            Controls.Num() ? *FString::Join(Controls, TEXT(", ")) : TEXT("no fixed control")));
    }
    TArray<FString> ButtonNames;
    for (const FButtonName& Entry : GButtonNames) { ButtonNames.Add(Entry.Name); }

    TMap<FString, FString> Slots;
    Slots.Add(TEXT("controller"), Controller);
    Slots.Add(TEXT("current_actions"), CurrentActions.Num() ? FString::Join(CurrentActions, TEXT(", ")) : FString(TEXT("none")));
    Slots.Add(TEXT("recent_actions"), RecentActions.Num() ? FString::Join(RecentActions, TEXT(", ")) : FString(TEXT("none")));
    Slots.Add(TEXT("cognition"), Cognition.Num() ? FString::Join(Cognition, TEXT("; ")) : FString(TEXT("no embodied-cognition data available")));
    Slots.Add(TEXT("actions"), FString::Join(ActionLines, TEXT("\n")));
    Slots.Add(TEXT("buttons"), FString::Join(ButtonNames, TEXT(", ")));
    Slots.Add(TEXT("max_duration"), FString::SanitizeFloat(FMath::Max(0.05f, MaxCognitiveCommandDuration)));

    TArray<FEchoModelMessage> Messages;
    Messages.Add(FEchoModelMessage(TEXT("user"), UEchoModelClient::FillPrompt(CognitiveCommandPromptTemplate, Slots)));

    const int32 RequestSerial = ++CognitiveCommandSerial;
    TWeakObjectPtr<UGameControllerInterface> WeakThis(this);
    UEchoModelClient::Get()->Chat(Messages, FOnEchoModelReply::CreateLambda(
        [WeakThis, RequestSerial](bool bSuccess, const FString& TextOrError)
        {
            if (UGameControllerInterface* Self = WeakThis.Get())
            {
                Self->HandleCognitiveCommandReply(RequestSerial, bSuccess, TextOrError);
            }
        }), 0.6f);
}

void UGameControllerInterface::HandleCognitiveCommandReply(int32 RequestSerial, bool bSuccess, const FString& TextOrError)
{
    if (RequestSerial != CognitiveCommandSerial)
    {
        return; // superseded by a newer request
    }
    if (!bSuccess)
    {
        FailCognitiveCommand(TextOrError);
        return;
    }

    TSharedPtr<FJsonObject> Json;
    FString ActionName;
    const TArray<TSharedPtr<FJsonValue>>* ButtonValues = nullptr;
    if (!UEchoModelClient::ParseJsonObject(TextOrError, Json) ||
        !Json->TryGetStringField(TEXT("action"), ActionName) ||
        !Json->TryGetArrayField(TEXT("buttons"), ButtonValues) || !ButtonValues)
    {
        FailCognitiveCommand(FString::Printf(TEXT("model reply is not the requested JSON: %s"), *TextOrError.Left(200)));
        return;
    }

    ActionName.TrimStartAndEndInline();
    const FGameActionMapping* Mapping = ActionMappings.FindByPredicate(
        [&ActionName](const FGameActionMapping& M) { return M.ActionName.Equals(ActionName, ESearchCase::IgnoreCase); });
    if (!Mapping)
    {
        FailCognitiveCommand(FString::Printf(TEXT("model chose an action outside the vocabulary: %s"), *ActionName));
        return;
    }

    FControllerOutputCommand Command;
    Command.ActionName = Mapping->ActionName;
    Command.Category = Mapping->Category;

    for (const TSharedPtr<FJsonValue>& Value : *ButtonValues)
    {
        FString Name;
        EGamepadButton Button = EGamepadButton::None;
        if (!Value.IsValid() || !Value->TryGetString(Name) || !NameToButton(Name.TrimStartAndEnd(), Button))
        {
            FailCognitiveCommand(FString::Printf(TEXT("model named an unknown button: %s"), *Name));
            return;
        }
        Command.DesiredState.PressedButtons.Add(Button);
    }

    // Sticks and triggers the model leaves out are untouched (read as released); duration and
    // confidence must be given.
    struct FRangedField { const TCHAR* Name; float Min; float Max; float* Target; bool bRequired; };
    float Duration = 0.0f;
    float Confidence = 0.0f;
    const FRangedField Fields[] = {
        { TEXT("left_stick_x"), -1.0f, 1.0f, &Command.DesiredState.LeftStickX, false },
        { TEXT("left_stick_y"), -1.0f, 1.0f, &Command.DesiredState.LeftStickY, false },
        { TEXT("right_stick_x"), -1.0f, 1.0f, &Command.DesiredState.RightStickX, false },
        { TEXT("right_stick_y"), -1.0f, 1.0f, &Command.DesiredState.RightStickY, false },
        { TEXT("left_trigger"), 0.0f, 1.0f, &Command.DesiredState.LeftTrigger, false },
        { TEXT("right_trigger"), 0.0f, 1.0f, &Command.DesiredState.RightTrigger, false },
        { TEXT("duration"), 0.05f, FMath::Max(0.05f, MaxCognitiveCommandDuration), &Duration, true },
        { TEXT("confidence"), 0.0f, 1.0f, &Confidence, true },
    };
    for (const FRangedField& Field : Fields)
    {
        double Number = 0.0;
        if (!Json->HasField(Field.Name) && !Field.bRequired)
        {
            continue;
        }
        if (!ReadLenientNumber(Json, Field.Name, Number))
        {
            FailCognitiveCommand(FString::Printf(TEXT("model reply has no numeric %s"), Field.Name));
            return;
        }
        if (Number < Field.Min || Number > Field.Max)
        {
            FailCognitiveCommand(FString::Printf(TEXT("model %s %.3f is outside [%.2f, %.2f]"), Field.Name, Number, Field.Min, Field.Max));
            return;
        }
        *Field.Target = static_cast<float>(Number);
    }
    Command.Duration = Duration;
    Command.Confidence = Confidence;
    if (const UWorld* World = GetWorld())
    {
        Command.DesiredState.Timestamp = World->GetTimeSeconds();
    }

    LastCognitiveCommandError.Reset();
    OnOutputCommandGenerated.Broadcast(Command);
}

void UGameControllerInterface::FailCognitiveCommand(const FString& Error)
{
    LastCognitiveCommandError = Error;
    UE_LOG(LogTemp, Error, TEXT("GameControllerInterface: no model controller command: %s"), *Error);
    OnOutputCommandFailed.Broadcast(Error);
}

FControllerOutputCommand UGameControllerInterface::GenerateOutputForAction(const FString& ActionName)
{
    FControllerOutputCommand Command;
    Command.ActionName = ActionName;

    // Find mapping for action
    for (const FGameActionMapping& Mapping : ActionMappings)
    {
        if (Mapping.ActionName == ActionName)
        {
            Command.Category = Mapping.Category;

            if (Mapping.PrimaryButton != EGamepadButton::None)
            {
                Command.DesiredState.PressedButtons.Add(Mapping.PrimaryButton);
            }

            break;
        }
    }

    return Command;
}

FControllerOutputCommand UGameControllerInterface::GenerateMovementOutput(const FVector2D& Direction, float Speed)
{
    FControllerOutputCommand Command;
    Command.Category = EGameActionCategory::Movement;
    Command.ActionName = TEXT("Movement");

    FVector2D ClampedDir = Direction.GetSafeNormal() * FMath::Clamp(Speed, 0.0f, 1.0f);
    Command.DesiredState.LeftStickX = ClampedDir.X;
    Command.DesiredState.LeftStickY = ClampedDir.Y;

    return Command;
}

FControllerOutputCommand UGameControllerInterface::GenerateCameraOutput(const FVector2D& Direction, float Speed)
{
    FControllerOutputCommand Command;
    Command.Category = EGameActionCategory::Camera;
    Command.ActionName = TEXT("Camera");

    FVector2D ClampedDir = Direction.GetSafeNormal() * FMath::Clamp(Speed, 0.0f, 1.0f);
    Command.DesiredState.RightStickX = ClampedDir.X;
    Command.DesiredState.RightStickY = ClampedDir.Y;

    return Command;
}

void UGameControllerInterface::QueueOutputCommand(const FControllerOutputCommand& Command)
{
    OutputQueue.Add(Command);

    // Sort by priority
    OutputQueue.Sort([](const FControllerOutputCommand& A, const FControllerOutputCommand& B)
    {
        return A.Priority > B.Priority;
    });
}

FControllerOutputCommand UGameControllerInterface::GetCurrentOutputCommand() const
{
    if (OutputQueue.Num() > 0)
    {
        return OutputQueue[0];
    }
    return FControllerOutputCommand();
}

void UGameControllerInterface::ExecuteQueuedOutput()
{
    if (OutputQueue.Num() == 0)
    {
        return;
    }

    FControllerOutputCommand& Command = OutputQueue[0];

    // In AI output mode, the desired state becomes the current state
    // This simulates controller input for the game
    CurrentState = Command.DesiredState;
    CurrentState.Timestamp = GetWorld()->GetTimeSeconds();
}

void UGameControllerInterface::ProcessOutputQueue(float DeltaTime)
{
    // Process and execute outputs
    ExecuteQueuedOutput();

    // Remove completed commands
    OutputQueue.RemoveAll([DeltaTime](FControllerOutputCommand& Cmd)
    {
        Cmd.Duration -= DeltaTime;
        return Cmd.Duration <= 0.0f;
    });
}

// ============================================================================
// Cognitive Integration
// ============================================================================

TArray<float> UGameControllerInterface::InputToSensoryVector(const FControllerInputState& Input) const
{
    return Input.ToActionVector();
}

FControllerOutputCommand UGameControllerInterface::CognitiveOutputToCommand(const TArray<float>& CognitiveOutput) const
{
    FControllerOutputCommand Command;

    if (CognitiveOutput.Num() >= 22)
    {
        Command.DesiredState = FControllerInputState::FromActionVector(CognitiveOutput);
    }

    return Command;
}

FString UGameControllerInterface::GetStateString(const FControllerInputState& Input) const
{
    return Input.ToActionString();
}

void UGameControllerInterface::RecordInputForImitation(const FControllerInputState& Input, const FString& Context)
{
    ImitationBuffer.Add(TPair<FControllerInputState, FString>(Input, Context));

    // Record to learning system
    if (LearningSystem)
    {
        TArray<FString> Tags;
        Tags.Add(TEXT("Imitation"));
        Tags.Add(Context);

        LearningSystem->RecordExperience(
            GetStateString(PreviousState),
            GetStateString(Input),
            GetStateString(Input),
            0.0f,  // Reward assigned later
            Tags,
            false
        );
    }
}

void UGameControllerInterface::BroadcastCognitiveState()
{
    if (!EmbodimentComponent)
    {
        return;
    }

    // Update sensorimotor contingencies based on controller state
    FString ActionStr = CurrentState.ToActionString();
    FString PreviousActionStr = PreviousState.ToActionString();

    if (ActionStr != PreviousActionStr)
    {
        // Learn action-outcome relationships
        EmbodimentComponent->LearnContingency(
            PreviousActionStr,
            ActionStr,  // Expected (same as actual for now)
            ActionStr   // Actual
        );
    }
}

// ============================================================================
// Mapping Management
// ============================================================================

void UGameControllerInterface::AddActionMapping(const FGameActionMapping& Mapping)
{
    // Remove existing mapping with same name
    RemoveActionMapping(Mapping.ActionName);
    ActionMappings.Add(Mapping);
}

void UGameControllerInterface::RemoveActionMapping(const FString& ActionName)
{
    ActionMappings.RemoveAll([&ActionName](const FGameActionMapping& M)
    {
        return M.ActionName == ActionName;
    });
}

FGameActionMapping UGameControllerInterface::GetActionMapping(const FString& ActionName) const
{
    for (const FGameActionMapping& Mapping : ActionMappings)
    {
        if (Mapping.ActionName == ActionName)
        {
            return Mapping;
        }
    }
    return FGameActionMapping();
}

void UGameControllerInterface::LoadPresetMappings(const FString& GameType)
{
    ActionMappings.Empty();

    if (GameType == TEXT("FPS"))
    {
        // First-person shooter mappings
        InitializeDefaultMappings();

        FGameActionMapping Aim;
        Aim.ActionName = TEXT("Aim");
        Aim.Category = EGameActionCategory::Attack;
        Aim.PrimaryButton = EGamepadButton::LeftTrigger;
        Aim.bRequiresHold = true;
        ActionMappings.Add(Aim);

        FGameActionMapping Fire;
        Fire.ActionName = TEXT("Fire");
        Fire.Category = EGameActionCategory::Attack;
        Fire.PrimaryButton = EGamepadButton::RightTrigger;
        ActionMappings.Add(Fire);

        FGameActionMapping Reload;
        Reload.ActionName = TEXT("Reload");
        Reload.Category = EGameActionCategory::Special;
        Reload.PrimaryButton = EGamepadButton::FaceLeft;
        ActionMappings.Add(Reload);
    }
    else if (GameType == TEXT("Fighting"))
    {
        // Fighting game mappings
        FGameActionMapping LightPunch;
        LightPunch.ActionName = TEXT("LightPunch");
        LightPunch.Category = EGameActionCategory::Attack;
        LightPunch.PrimaryButton = EGamepadButton::FaceLeft;
        ActionMappings.Add(LightPunch);

        FGameActionMapping HeavyPunch;
        HeavyPunch.ActionName = TEXT("HeavyPunch");
        HeavyPunch.Category = EGameActionCategory::Attack;
        HeavyPunch.PrimaryButton = EGamepadButton::FaceTop;
        ActionMappings.Add(HeavyPunch);

        FGameActionMapping LightKick;
        LightKick.ActionName = TEXT("LightKick");
        LightKick.Category = EGameActionCategory::Attack;
        LightKick.PrimaryButton = EGamepadButton::FaceBottom;
        ActionMappings.Add(LightKick);

        FGameActionMapping HeavyKick;
        HeavyKick.ActionName = TEXT("HeavyKick");
        HeavyKick.Category = EGameActionCategory::Attack;
        HeavyKick.PrimaryButton = EGamepadButton::FaceRight;
        ActionMappings.Add(HeavyKick);
    }
    else if (GameType == TEXT("Racing"))
    {
        // Racing game mappings
        FGameActionMapping Accelerate;
        Accelerate.ActionName = TEXT("Accelerate");
        Accelerate.Category = EGameActionCategory::Movement;
        Accelerate.PrimaryAxis = EGamepadAxis::RightTrigger;
        Accelerate.AxisThreshold = 0.1f;
        ActionMappings.Add(Accelerate);

        FGameActionMapping Brake;
        Brake.ActionName = TEXT("Brake");
        Brake.Category = EGameActionCategory::Movement;
        Brake.PrimaryAxis = EGamepadAxis::LeftTrigger;
        Brake.AxisThreshold = 0.1f;
        ActionMappings.Add(Brake);

        FGameActionMapping Steer;
        Steer.ActionName = TEXT("Steer");
        Steer.Category = EGameActionCategory::Movement;
        Steer.PrimaryAxis = EGamepadAxis::LeftStickX;
        Steer.AxisThreshold = 0.1f;
        ActionMappings.Add(Steer);

        FGameActionMapping Boost;
        Boost.ActionName = TEXT("Boost");
        Boost.Category = EGameActionCategory::Special;
        Boost.PrimaryButton = EGamepadButton::FaceBottom;
        ActionMappings.Add(Boost);
    }
    else
    {
        // Default mappings
        InitializeDefaultMappings();
    }
}

// ============================================================================
// Serialization
// ============================================================================

TArray<uint8> UGameControllerInterface::SerializeInputState(const FControllerInputState& State) const
{
    TArray<uint8> Data;
    FMemoryWriter Ar(Data);

    // Write axes
    Ar << const_cast<float&>(State.LeftStickX);
    Ar << const_cast<float&>(State.LeftStickY);
    Ar << const_cast<float&>(State.RightStickX);
    Ar << const_cast<float&>(State.RightStickY);
    Ar << const_cast<float&>(State.LeftTrigger);
    Ar << const_cast<float&>(State.RightTrigger);

    // Write buttons as bitmask
    uint32 ButtonMask = 0;
    for (const EGamepadButton& Button : State.PressedButtons)
    {
        ButtonMask |= (1 << static_cast<uint8>(Button));
    }
    Ar << ButtonMask;

    Ar << const_cast<float&>(State.Timestamp);

    return Data;
}

FControllerInputState UGameControllerInterface::DeserializeInputState(const TArray<uint8>& Data) const
{
    FControllerInputState State;
    FMemoryReader Ar(Data);

    Ar << State.LeftStickX;
    Ar << State.LeftStickY;
    Ar << State.RightStickX;
    Ar << State.RightStickY;
    Ar << State.LeftTrigger;
    Ar << State.RightTrigger;

    uint32 ButtonMask;
    Ar << ButtonMask;

    for (int32 i = 0; i < 16; ++i)
    {
        if (ButtonMask & (1 << i))
        {
            State.PressedButtons.Add(static_cast<EGamepadButton>(i));
        }
    }

    Ar << State.Timestamp;

    return State;
}
