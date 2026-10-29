#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepTreeEchoExpressionSystem.generated.h"

struct FEchoCharacterReaction;

/**
 * Deep-Tree-Echo Expression System
 * 
 * Priority Implementation for Unreal Engine Avatar
 * Based on video reference analysis - focusing on photorealistic cyberpunk/biopunk avatar
 * 
 * Top 5 Priority Expressions:
 * 1. Neutral + Breathing (Foundation)
 * 2. Gentle Smile (JOY_03) - Core positive valence
 * 3. Open Vowel (SPEAK_01) - Essential communication
 * 4. Contemplative (WONDER_02/03) - Represents "thinking"
 * 5. Broad Smile/Laugh (JOY_01/02) - Peak positive emotion
 * 
 * Procedural physiology (not reactions): blinking, breathing, saccadic eye movement.
 *
 * Which expression the face shows is chosen by the character model only. The component
 * subscribes to FEchoReactionBus and renders each model reaction (expression preset at the
 * model's intensity, micro-expressions named by the model, and visemes for the model's
 * utterance). Cognitive state changes are recorded and, when enabled, ask the model how the
 * character reacts (ReactionPromptTemplate); the reply is validated against the expression
 * vocabulary and broadcast on the bus. No table maps cognition or clock steps to a face.
 */

// Forward declarations
class USkeletalMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Cognitive state enum - maps to Deep-Tree-Echo cognitive architecture
 */
UENUM(BlueprintType)
enum class ECognitiveState : uint8
{
    Idle            UMETA(DisplayName = "Idle"),
    Processing      UMETA(DisplayName = "Processing"),
    Reasoning       UMETA(DisplayName = "Reasoning"),
    Insight         UMETA(DisplayName = "Insight"),
    Communicating   UMETA(DisplayName = "Communicating"),
    Listening       UMETA(DisplayName = "Listening"),
    Reflecting      UMETA(DisplayName = "Reflecting"),
    Engaging        UMETA(DisplayName = "Engaging")
};

/**
 * Expression state enum - priority expressions from video analysis
 */
UENUM(BlueprintType)
enum class EExpressionState : uint8
{
    // Foundation (Priority 1)
    Neutral         UMETA(DisplayName = "Neutral"),
    
    // Joy Spectrum (Priority 2 & 5)
    GentleSmile     UMETA(DisplayName = "Gentle Smile (JOY_03)"),
    BroadSmile      UMETA(DisplayName = "Broad Smile (JOY_01)"),
    Laughing        UMETA(DisplayName = "Laughing (JOY_02)"),
    Blissful        UMETA(DisplayName = "Blissful (JOY_05)"),
    PlayfulGrin     UMETA(DisplayName = "Playful Grin (JOY_06)"),
    
    // Wonder Spectrum (Priority 4)
    Contemplative   UMETA(DisplayName = "Contemplative (WONDER_03)"),
    CuriousGaze     UMETA(DisplayName = "Curious Gaze (WONDER_02)"),
    Awe             UMETA(DisplayName = "Awe (WONDER_01)"),
    
    // Speaking (Priority 3)
    SpeakingVowel   UMETA(DisplayName = "Speaking Vowel (SPEAK_01)"),
    SpeakingConsonant UMETA(DisplayName = "Speaking Consonant (SPEAK_02)"),
    
    // Attention
    AlertGaze       UMETA(DisplayName = "Alert Gaze (FOCUS_01)"),
    UpwardGaze      UMETA(DisplayName = "Upward Gaze (FOCUS_02)")
};

/**
 * Microexpression pattern enum
 */
UENUM(BlueprintType)
enum class EMicroExpression : uint8
{
    None            UMETA(DisplayName = "None"),
    Blink           UMETA(DisplayName = "Blink"),
    UnilateralBrowRaise UMETA(DisplayName = "Unilateral Brow Raise"),
    LipCornerTwitch UMETA(DisplayName = "Lip Corner Twitch"),
    NoseWrinkle     UMETA(DisplayName = "Nose Wrinkle"),
    EyelidFlutter   UMETA(DisplayName = "Eyelid Flutter"),
    LipPurse        UMETA(DisplayName = "Lip Purse"),
    JawSlack        UMETA(DisplayName = "Jaw Slack")
};

/**
 * Complete morph target data structure
 * Based on expression catalog analysis
 */
USTRUCT(BlueprintType)
struct FDeepTreeEchoMorphTargets
{
    GENERATED_BODY()

    // Brow controls
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Brow")
    float BrowRaiseL = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Brow")
    float BrowRaiseR = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Brow")
    float BrowFurrow = 0.0f;

    // Eye controls
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye")
    float EyeWideL = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye")
    float EyeWideR = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye")
    float EyeSquintL = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye")
    float EyeSquintR = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye")
    float EyeCloseL = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye")
    float EyeCloseR = 0.0f;

    // Nose controls
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nose")
    float NoseScrunch = 0.0f;

    // Cheek controls
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cheek")
    float CheekRaiseL = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cheek")
    float CheekRaiseR = 0.0f;

    // Mouth controls
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouth")
    float MouthSmileL = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouth")
    float MouthSmileR = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouth")
    float MouthOpen = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouth")
    float MouthWide = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouth")
    float LipPucker = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouth")
    float LipPart = 0.0f;

    // Visemes for speech
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viseme")
    float VisemeAA = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viseme")
    float VisemeEE = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viseme")
    float VisemeOH = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viseme")
    float VisemeOO = 0.0f;

    // Interpolation helper
    static FDeepTreeEchoMorphTargets Lerp(const FDeepTreeEchoMorphTargets& A, const FDeepTreeEchoMorphTargets& B, float Alpha);
    
    // Additive blend helper
    static FDeepTreeEchoMorphTargets Add(const FDeepTreeEchoMorphTargets& Base, const FDeepTreeEchoMorphTargets& Additive, float Weight = 1.0f);
};

/**
 * Expression preset data
 */
USTRUCT(BlueprintType)
struct FExpressionPreset
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Expression")
    EExpressionState ExpressionState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Expression")
    FDeepTreeEchoMorphTargets MorphTargets;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Expression")
    float EmissiveIntensity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Expression")
    FVector2D HeadTilt = FVector2D::ZeroVector;
};

/**
 * Procedural animation settings
 */
USTRUCT(BlueprintType)
struct FProceduralAnimSettings
{
    GENERATED_BODY()

    // Breathing
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breathing")
    float BreathingRate = 0.2f;  // Breaths per second
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breathing")
    float BreathingAmplitude = 0.02f;

    // Blinking
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blinking")
    float MinBlinkInterval = 2.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blinking")
    float MaxBlinkInterval = 6.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blinking")
    float BlinkDuration = 0.15f;

    // Saccades (eye movement)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saccades")
    float SaccadeFrequency = 0.5f;  // Per second
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saccades")
    float SaccadeAmplitude = 5.0f;  // Degrees
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saccades")
    float SaccadeSpeed = 500.0f;  // Degrees per second

    // Head micro-movement
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeadMovement")
    float HeadSwayAmplitude = 1.0f;  // Degrees
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeadMovement")
    float HeadSwayFrequency = 0.1f;
};

/**
 * Echobeats cognitive loop step record (cadence only; it never selects an expression)
 */
USTRUCT(BlueprintType)
struct FEchobeatsStep
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echobeats")
    int32 StepNumber = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echobeats")
    FString CognitivePhase;  // Pivotal / Affordance / Salience / Integration
};

/**
 * Main Deep-Tree-Echo Expression System Component
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREECHO_API UDeepTreeEchoExpressionSystem : public UActorComponent
{
    GENERATED_BODY()

public:
    UDeepTreeEchoExpressionSystem();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========== Core Expression Control ==========

    /** Renderer: transition to an expression preset at full strength. Reactions arrive via the bus. */
    UFUNCTION(BlueprintCallable, Category = "Expression")
    void SetExpressionState(EExpressionState NewState, float TransitionTime = 0.3f);

    /** Get current expression state */
    UFUNCTION(BlueprintPure, Category = "Expression")
    EExpressionState GetCurrentExpressionState() const { return CurrentExpressionState; }

    /**
     * Records the cognitive state. It does not set an expression. When bRequestReactionOnCognitiveStateChange
     * is set, a change of state asks the model (throttled) how the character reacts; the face changes
     * only if the model's validated reply arrives on the reaction bus.
     */
    UFUNCTION(BlueprintCallable, Category = "Cognitive")
    void SetCognitiveState(ECognitiveState NewState);

    /** Intensity [0,1] of the model reaction currently shown (0 before any reaction). */
    UFUNCTION(BlueprintPure, Category = "Expression")
    float GetReactionIntensity() const { return ReactionIntensity; }

    /** Asks the model for a reaction to the current cognitive state. False if a request is already pending. */
    UFUNCTION(BlueprintCallable, Category = "Reaction")
    bool RequestModelReaction();

    /** Last model/validation error for reaction requests (system status, never shown as the character). */
    UFUNCTION(BlueprintPure, Category = "Reaction")
    FString GetLastReactionError() const { return LastReactionError; }

    /** Names the model may choose from: expression presets and micro-expressions. */
    UFUNCTION(BlueprintPure, Category = "Reaction")
    static TArray<FString> GetReactionVocabulary();

    /** Get current cognitive state */
    UFUNCTION(BlueprintPure, Category = "Cognitive")
    ECognitiveState GetCurrentCognitiveState() const { return CurrentCognitiveState; }

    // ========== Microexpression Control ==========

    /** Trigger a microexpression */
    UFUNCTION(BlueprintCallable, Category = "Microexpression")
    void TriggerMicroExpression(EMicroExpression MicroExp, float Duration = 0.2f);

    // ========== Procedural Animation Control ==========

    /** Enable/disable procedural blinking */
    UFUNCTION(BlueprintCallable, Category = "Procedural")
    void SetBlinkingEnabled(bool bEnabled);

    /** Enable/disable procedural breathing */
    UFUNCTION(BlueprintCallable, Category = "Procedural")
    void SetBreathingEnabled(bool bEnabled);

    /** Enable/disable saccadic eye movement */
    UFUNCTION(BlueprintCallable, Category = "Procedural")
    void SetSaccadesEnabled(bool bEnabled);

    /** Trigger manual blink */
    UFUNCTION(BlueprintCallable, Category = "Procedural")
    void TriggerBlink();

    // ========== Gaze Control ==========

    /** Set gaze target in world space */
    UFUNCTION(BlueprintCallable, Category = "Gaze")
    void SetGazeTarget(const FVector& WorldTarget);

    /** Clear gaze target (return to forward) */
    UFUNCTION(BlueprintCallable, Category = "Gaze")
    void ClearGazeTarget();

    // ========== Speech/Lip Sync ==========

    /** Drive visemes from text being spoken (the model's utterance) for its playback duration. */
    UFUNCTION(BlueprintCallable, Category = "Speech")
    void StartTextLipSync(const FString& Text, float CharactersPerSecond = 14.0f);

    /** Stop text-driven visemes and relax the mouth. */
    UFUNCTION(BlueprintCallable, Category = "Speech")
    void StopLipSync();

    UFUNCTION(BlueprintPure, Category = "Speech")
    bool IsLipSyncing() const { return bTextLipSyncActive; }

    /** Set speaking state with audio amplitude */
    UFUNCTION(BlueprintCallable, Category = "Speech")
    void SetSpeakingAmplitude(float Amplitude);

    /** Set specific viseme */
    UFUNCTION(BlueprintCallable, Category = "Speech")
    void SetViseme(const FName& VisemeName, float Weight);

    // ========== Emissive/Tech Element Control ==========

    /** Set bioluminescent marking intensity */
    UFUNCTION(BlueprintCallable, Category = "Emissive")
    void SetEmissiveIntensity(float Intensity);

    /** Pulse emissives (for insight/engagement moments) */
    UFUNCTION(BlueprintCallable, Category = "Emissive")
    void PulseEmissives(float Duration = 0.5f, float PeakIntensity = 2.0f);

    // ========== Echobeats Integration ==========

    /** Records the echobeats step (cadence only; does not change the expression) */
    UFUNCTION(BlueprintCallable, Category = "Echobeats")
    void OnEchobeatsStep(int32 StepNumber);

    UFUNCTION(BlueprintPure, Category = "Echobeats")
    FEchobeatsStep GetCurrentEchobeatsStep() const { return CurrentEchobeatsStep; }

    // ========== Model Reaction Configuration ==========

    /** Only reactions for this character are rendered; empty = the owning actor's name, and all reactions are accepted. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction")
    FString ReactionCharacterName;

    /** Ask the model for a reaction when the recorded cognitive state changes. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction")
    bool bRequestReactionOnCognitiveStateChange = true;

    /** Minimum seconds between reaction requests. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction", meta = (ClampMin = "0.0"))
    float MinSecondsBetweenReactionRequests = 4.0f;

    /** Drive visemes from the utterance of each model reaction. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction")
    bool bDriveVisemesFromUtterance = true;

    /**
     * Prompt template (model input). Slots: {character}, {previous_state}, {state}, {echobeats_step},
     * {echobeats_phase}, {speaking}, {current_expression}, {expressions}. The model's JSON reply is validated
     * against {expressions} and broadcast on the reaction bus.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction", meta = (MultiLine = "true"))
    FString ReactionPromptTemplate;

    // ========== Output ==========

    /** Get current morph targets for application to skeletal mesh */
    UFUNCTION(BlueprintPure, Category = "Output")
    FDeepTreeEchoMorphTargets GetCurrentMorphTargets() const { return CurrentMorphTargets; }

    /** Apply morph targets to skeletal mesh component */
    UFUNCTION(BlueprintCallable, Category = "Output")
    void ApplyToSkeletalMesh(USkeletalMeshComponent* TargetMesh);

    /** Apply emissive settings to material */
    UFUNCTION(BlueprintCallable, Category = "Output")
    void ApplyToMaterial(UMaterialInstanceDynamic* TargetMaterial);

protected:
    // ========== Internal Update Functions ==========

    void InitializeExpressionPresets();

    void UpdateExpressionTransition(float DeltaTime);
    void UpdateProceduralBlinking(float DeltaTime);
    void UpdateProceduralBreathing(float DeltaTime);
    void UpdateProceduralSaccades(float DeltaTime);
    void UpdateMicroExpression(float DeltaTime);
    void UpdateEmissivePulse(float DeltaTime);
    void UpdateTextLipSync(float DeltaTime);

    // ========== Model Reactions ==========

    FString GetReactionCharacterNameResolved() const;
    FString BuildReactionPrompt() const;
    void HandleModelReactionReply(bool bSuccess, const FString& TextOrError);
    void HandleCharacterReaction(const FEchoCharacterReaction& Reaction);
    void ApplyReactionExpression(EExpressionState NewState, float Intensity, float TransitionTime);
    static FString GetEchobeatsPhaseName(int32 StepNumber);

    // ========== State Variables ==========

    UPROPERTY()
    EExpressionState CurrentExpressionState;

    UPROPERTY()
    EExpressionState TargetExpressionState;

    UPROPERTY()
    ECognitiveState CurrentCognitiveState;

    UPROPERTY()
    ECognitiveState PreviousCognitiveState;

    UPROPERTY()
    FDeepTreeEchoMorphTargets CurrentMorphTargets;

    UPROPERTY()
    FDeepTreeEchoMorphTargets TargetMorphTargets;

    /** Physiological layer (blink, breathing, micro-expressions); rebuilt each tick. */
    UPROPERTY()
    FDeepTreeEchoMorphTargets ProceduralMorphTargets;

    /** Expression layer (model-chosen preset, interpolated). */
    UPROPERTY()
    FDeepTreeEchoMorphTargets ExpressionMorphTargets;

    /** Expression layer at the start of the current transition. */
    UPROPERTY()
    FDeepTreeEchoMorphTargets TransitionStartMorphTargets;

    /** Speech layer (audio amplitude / visemes / text lip sync). */
    UPROPERTY()
    FDeepTreeEchoMorphTargets SpeechMorphTargets;

    // Expression presets
    UPROPERTY()
    TMap<EExpressionState, FExpressionPreset> ExpressionPresets;

    // Last recorded echobeats step (cadence only)
    UPROPERTY()
    FEchobeatsStep CurrentEchobeatsStep;

    // ========== Transition State ==========

    float ExpressionTransitionProgress;
    float ExpressionTransitionDuration;
    float TransitionStartEmissive;

    // ========== Procedural Animation State ==========

    UPROPERTY(EditAnywhere, Category = "Procedural Settings")
    FProceduralAnimSettings ProceduralSettings;

    bool bBlinkingEnabled;
    bool bBreathingEnabled;
    bool bSaccadesEnabled;

    float BlinkTimer;
    float NextBlinkTime;
    bool bIsBlinking;
    float BlinkProgress;

    float BreathingPhase;

    FVector2D CurrentSaccadeOffset;
    FVector2D TargetSaccadeOffset;
    float SaccadeTimer;

    // ========== Gaze State ==========

    bool bHasGazeTarget;
    FVector GazeTarget;
    FRotator CurrentEyeRotation;

    // ========== Microexpression State ==========

    bool bMicroExpressionActive;
    EMicroExpression ActiveMicroExpression;
    float MicroExpressionTimer;
    float MicroExpressionDuration;

    // ========== Emissive State ==========

    float CurrentEmissiveIntensity;
    float TargetEmissiveIntensity;
    bool bEmissivePulseActive;
    float EmissivePulseTimer;
    float EmissivePulseDuration;
    float EmissivePulsePeak;

    // ========== Speech State ==========

    float CurrentSpeakingAmplitude;

    bool bTextLipSyncActive = false;
    FString LipSyncText;
    float LipSyncTime = 0.0f;
    float LipSyncCharsPerSecond = 14.0f;

    // ========== Model Reaction State ==========

    float ReactionIntensity = 0.0f;
    bool bReactionRequestPending = false;
    double LastReactionRequestTime = -1.0e9;
    FString LastReactionError;
    FDelegateHandle ReactionHandle;
};
