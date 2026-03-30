// GamingMasterySystem.h
// Deep Tree Echo — AI Gaming Mastery System
//
// Provides state-of-the-art tools for achieving gaming mastery:
//   • Lightning Reflex Engine   — sub-16ms input pipeline with predictor
//   • Combo Mastery Engine      — learns and executes optimal combo sequences
//   • Opponent Modelling        — predicts opponent moves with ESN patterns
//   • Meta-Strategy Planner     — dynamic difficulty adaptation
//   • Situational Micro-Tactics — frame-perfect timing windows
//
// Design Philosophy:
//   "Mastery is systematic improvement in relevance realization."
//   — Vervaeke
//
//   The system continuously narrows the gap between what IS relevant
//   in the current game state and what motor command best exploits it.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameTraining/GameControllerInterface.h"
#include "GameTraining/GameSkillTrainingSystem.h"
#include "GamingMasterySystem.generated.h"

// Forward declarations
class UVirtualCNS;
class UReinforcementLearningBridge;
class UGameTrainingEnvironment;
class UDeepTreeEchoCore;
class UAttentionSystem;

// ============================================================
// Reflex Speed Tier
// ============================================================

UENUM(BlueprintType)
enum class EReflexSpeedTier : uint8
{
    /** Trained reflex: < 4 ms — pre-compiled motor program */
    Instantaneous   UMETA(DisplayName = "Instantaneous (<4ms)"),

    /** Fast: 4–16 ms — one-frame response with prediction */
    Fast            UMETA(DisplayName = "Fast (4-16ms)"),

    /** Normal: 16–50 ms — deliberate but rapid action */
    Normal          UMETA(DisplayName = "Normal (16-50ms)"),

    /** Considered: > 50 ms — planned tactical action */
    Considered      UMETA(DisplayName = "Considered (>50ms)")
};

// ============================================================
// Timing Window
// ============================================================

/** Frame-perfect timing window for executing an action */
USTRUCT(BlueprintType)
struct FTimingWindow
{
    GENERATED_BODY()

    /** Action to execute in this window */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ActionName;

    /** Earliest execution time (seconds from now) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EarliestTime = 0.0f;

    /** Optimal execution time */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OptimalTime = 0.0f;

    /** Latest execution time */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LatestTime = 0.0f;

    /** Reward multiplier if executed in the perfect window */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PerfectMultiplier = 2.0f;

    /** Current elapsed time */
    UPROPERTY(BlueprintReadOnly)
    float ElapsedTime = 0.0f;

    /** Is window still open? */
    bool IsOpen() const { return ElapsedTime <= LatestTime; }

    /** Is this the perfect frame? */
    bool IsPerfect() const
    {
        return FMath::Abs(ElapsedTime - OptimalTime) < 0.016f; // ±1 frame
    }
};

// ============================================================
// Opponent Model
// ============================================================

USTRUCT(BlueprintType)
struct FOpponentModel
{
    GENERATED_BODY()

    /** Opponent identifier */
    UPROPERTY(BlueprintReadWrite)
    FString OpponentID;

    /** Predicted next action */
    UPROPERTY(BlueprintReadOnly)
    FString PredictedAction;

    /** Prediction confidence [0,1] */
    UPROPERTY(BlueprintReadOnly)
    float PredictionConfidence = 0.0f;

    /** Time until predicted action (seconds) */
    UPROPERTY(BlueprintReadOnly)
    float TimeToAction = 0.0f;

    /** Historical action counts (action → count) */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, int32> ActionHistory;

    /** Exploitable patterns detected */
    UPROPERTY(BlueprintReadOnly)
    TArray<FString> ExploitablePatterns;

    /** Observed moves */
    UPROPERTY(BlueprintReadWrite)
    int32 ObservedMoves = 0;

    /** Successful counter-moves against this opponent */
    UPROPERTY(BlueprintReadWrite)
    int32 SuccessfulCounters = 0;

    /** Counter-success rate */
    UPROPERTY(BlueprintReadOnly)
    float CounterSuccessRate = 0.0f;
};

// ============================================================
// Mastery Metrics
// ============================================================

USTRUCT(BlueprintType)
struct FMasteryMetrics
{
    GENERATED_BODY()

    /** APM — actions per minute */
    UPROPERTY(BlueprintReadOnly)
    float APM = 0.0f;

    /** Average reflex speed (ms) */
    UPROPERTY(BlueprintReadOnly)
    float AvgReflexMs = 0.0f;

    /** Perfect-frame timing rate [0,1] */
    UPROPERTY(BlueprintReadOnly)
    float PerfectTimingRate = 0.0f;

    /** Combo execution success rate [0,1] */
    UPROPERTY(BlueprintReadOnly)
    float ComboSuccessRate = 0.0f;

    /** Opponent prediction accuracy [0,1] */
    UPROPERTY(BlueprintReadOnly)
    float PredictionAccuracy = 0.0f;

    /** Current skill tier (0 = beginner, 10 = grandmaster) */
    UPROPERTY(BlueprintReadOnly)
    int32 SkillTier = 0;

    /** Total actions executed */
    UPROPERTY(BlueprintReadOnly)
    int64 TotalActions = 0;

    /** Actions in last 60 seconds (for APM calculation) */
    TArray<float> RecentActionTimestamps;
};

// ============================================================
// Combo Definition
// ============================================================

USTRUCT(BlueprintType)
struct FComboDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ComboID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ComboName;

    /** Ordered sequence of action names */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> ActionSequence;

    /** Maximum time between actions */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxTimeBetweenActions = 0.3f;

    /** Difficulty [0,1] */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Difficulty = 0.5f;

    /** Expected damage or reward */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExpectedReward = 1.0f;

    /** Proficiency learned [0,1] */
    UPROPERTY(BlueprintReadWrite)
    float Proficiency = 0.0f;

    /** Execution count */
    UPROPERTY(BlueprintReadWrite)
    int32 ExecutionCount = 0;

    /** Success count */
    UPROPERTY(BlueprintReadWrite)
    int32 SuccessCount = 0;
};

// ============================================================
// Delegates
// ============================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnComboExecuted,
    const FString&, ComboID, bool, bPerfect);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOpponentActionPredicted,
    const FString&, OpponentID, const FString&, PredictedAction);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTimingWindowOpened,
    const FTimingWindow&, Window);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSkillTierAdvanced,
    int32, OldTier, int32, NewTier);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMasteryMetricsUpdated,
    const FMasteryMetrics&, Metrics);

// ============================================================
// UGamingMasterySystem
// ============================================================

/**
 * UGamingMasterySystem
 *
 * State-of-the-art tools for AI Gaming Mastery.
 *
 * Key capabilities:
 *
 *  1. Lightning Reflex Engine:
 *     Maintains a ring buffer of game-state snapshots. When a 
 *     relevant stimulus is detected, it fires a pre-compiled motor
 *     program (reflex arc) within a single frame (<16 ms) or 
 *     sub-frame (<4 ms) using speculative execution.
 *
 *  2. Combo Mastery Engine:
 *     Registers combos as finite state machines. Monitors input
 *     stream and executes partial inputs autonomously when the FSM
 *     advances to a committed state.
 *
 *  3. Opponent Modelling:
 *     Accumulates opponent action history in a frequency table.
 *     Uses Echo State Network temporal patterns to predict the
 *     next move with confidence scores.
 *
 *  4. Meta-Strategy Planner:
 *     Selects and switches macro-strategies (aggressive, defensive,
 *     turtle, rush) based on score delta, health, and opponent model.
 *
 *  5. Frame-Perfect Timing Windows:
 *     Exposes timing windows (e.g. parry window, punish window) and
 *     tracks frame-perfect execution rates.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent),
       DisplayName="Gaming Mastery System")
class DEEPTREEECHO_API UGamingMasterySystem : public UActorComponent
{
    GENERATED_BODY()

public:
    UGamingMasterySystem();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ====================================================
    // CONFIGURATION
    // ====================================================

    /** Enable speculative pre-execution of likely next action */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mastery|Config")
    bool bSpeculativeExecution = true;

    /** Maximum speculative look-ahead (frames) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mastery|Config",
              meta=(ClampMin="1", ClampMax="8"))
    int32 SpeculativeLookahead = 3;

    /** Enable opponent modelling */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mastery|Config")
    bool bEnableOpponentModelling = true;

    /** Enable combo learning */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mastery|Config")
    bool bLearnNewCombos = true;

    /** APM target for the agent */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mastery|Config",
              meta=(ClampMin="60", ClampMax="1500"))
    float TargetAPM = 400.0f;

    /** Minimum confidence to fire a predicted reflex */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mastery|Config",
              meta=(ClampMin="0.0", ClampMax="1.0"))
    float MinPredictionConfidence = 0.7f;

    /** Pre-registered combo definitions */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mastery|Config")
    TArray<FComboDefinition> ComboCatalog;

    // ====================================================
    // EVENTS
    // ====================================================

    UPROPERTY(BlueprintAssignable, Category="Mastery|Events")
    FOnComboExecuted OnComboExecuted;

    UPROPERTY(BlueprintAssignable, Category="Mastery|Events")
    FOnOpponentActionPredicted OnOpponentActionPredicted;

    UPROPERTY(BlueprintAssignable, Category="Mastery|Events")
    FOnTimingWindowOpened OnTimingWindowOpened;

    UPROPERTY(BlueprintAssignable, Category="Mastery|Events")
    FOnSkillTierAdvanced OnSkillTierAdvanced;

    UPROPERTY(BlueprintAssignable, Category="Mastery|Events")
    FOnMasteryMetricsUpdated OnMasteryMetricsUpdated;

    // ====================================================
    // LIGHTNING REFLEX ENGINE
    // ====================================================

    /** Notify the system of a stimulus that may trigger a reflex */
    UFUNCTION(BlueprintCallable, Category="Mastery|Reflex")
    void OnStimulusDetected(const FString& StimulusType, float Urgency);

    /** Speculatively pre-execute the most likely next action */
    UFUNCTION(BlueprintCallable, Category="Mastery|Reflex")
    FControllerOutputCommand SpeculativelyPreExecute(const TArray<float>& GameState);

    /** Record the measured latency for a reflex (ms) */
    UFUNCTION(BlueprintCallable, Category="Mastery|Reflex")
    void RecordReflexLatency(float LatencyMs);

    /** Get the fastest reflex tier currently capable */
    UFUNCTION(BlueprintPure, Category="Mastery|Reflex")
    EReflexSpeedTier GetCurrentReflexTier() const;

    // ====================================================
    // COMBO MASTERY ENGINE
    // ====================================================

    /** Register a new combo */
    UFUNCTION(BlueprintCallable, Category="Mastery|Combos")
    void RegisterCombo(const FComboDefinition& Combo);

    /** Advance the combo state machine with a new action */
    UFUNCTION(BlueprintCallable, Category="Mastery|Combos")
    void AdvanceComboState(const FString& ActionName);

    /** Check if a combo is in progress and return next required action */
    UFUNCTION(BlueprintPure, Category="Mastery|Combos")
    FString GetNextComboAction() const;

    /** Get the commands to auto-complete the current combo */
    UFUNCTION(BlueprintCallable, Category="Mastery|Combos")
    TArray<FControllerOutputCommand> GetComboCompletionSequence() const;

    /** Record a combo outcome */
    UFUNCTION(BlueprintCallable, Category="Mastery|Combos")
    void RecordComboOutcome(const FString& ComboID, bool bSuccess);

    /** Get all known combos sorted by proficiency */
    UFUNCTION(BlueprintPure, Category="Mastery|Combos")
    TArray<FComboDefinition> GetComboCatalogSortedByProficiency() const;

    // ====================================================
    // OPPONENT MODELLING
    // ====================================================

    /** Record an observed opponent action */
    UFUNCTION(BlueprintCallable, Category="Mastery|Opponent")
    void ObserveOpponentAction(const FString& OpponentID, const FString& ActionName);

    /** Predict the opponent's next action */
    UFUNCTION(BlueprintCallable, Category="Mastery|Opponent")
    FString PredictOpponentAction(const FString& OpponentID) const;

    /** Get the full opponent model */
    UFUNCTION(BlueprintPure, Category="Mastery|Opponent")
    FOpponentModel GetOpponentModel(const FString& OpponentID) const;

    /** Get the best counter to the predicted opponent action */
    UFUNCTION(BlueprintCallable, Category="Mastery|Opponent")
    FString GetBestCounter(const FString& OpponentID) const;

    /** Record whether a counter was successful */
    UFUNCTION(BlueprintCallable, Category="Mastery|Opponent")
    void RecordCounterOutcome(const FString& OpponentID, bool bSuccess);

    // ====================================================
    // TIMING WINDOWS
    // ====================================================

    /** Open a timing window */
    UFUNCTION(BlueprintCallable, Category="Mastery|Timing")
    void OpenTimingWindow(const FTimingWindow& Window);

    /** Check if an action is in its perfect window right now */
    UFUNCTION(BlueprintPure, Category="Mastery|Timing")
    bool IsInPerfectWindow(const FString& ActionName) const;

    /** Get remaining time in the current window for an action */
    UFUNCTION(BlueprintPure, Category="Mastery|Timing")
    float GetWindowRemainingTime(const FString& ActionName) const;

    /** Close a timing window manually */
    UFUNCTION(BlueprintCallable, Category="Mastery|Timing")
    void CloseTimingWindow(const FString& ActionName);

    // ====================================================
    // MASTERY METRICS
    // ====================================================

    /** Get current mastery metrics */
    UFUNCTION(BlueprintPure, Category="Mastery|Metrics")
    FMasteryMetrics GetMasteryMetrics() const { return Metrics; }

    /** Get current skill tier (0-10) */
    UFUNCTION(BlueprintPure, Category="Mastery|Metrics")
    int32 GetSkillTier() const { return Metrics.SkillTier; }

    /** Get APM */
    UFUNCTION(BlueprintPure, Category="Mastery|Metrics")
    float GetCurrentAPM() const { return Metrics.APM; }

    // ====================================================
    // COMPONENT WIRING
    // ====================================================

    /** Set Virtual CNS reference */
    UFUNCTION(BlueprintCallable, Category="Mastery|Wiring")
    void SetVirtualCNS(UVirtualCNS* CNS);

    /** Set DTE Core reference */
    UFUNCTION(BlueprintCallable, Category="Mastery|Wiring")
    void SetDTECore(UDeepTreeEchoCore* Core);

protected:
    // ====================================================
    // CACHED REFERENCES
    // ====================================================

    UPROPERTY()
    UVirtualCNS* VirtualCNS = nullptr;

    UPROPERTY()
    UDeepTreeEchoCore* DTECore = nullptr;

    UPROPERTY()
    UReinforcementLearningBridge* RLBridge = nullptr;

    UPROPERTY()
    UAttentionSystem* AttentionSys = nullptr;

    // ====================================================
    // INTERNAL STATE
    // ====================================================

    /** Opponent models keyed by ID */
    TMap<FString, FOpponentModel> OpponentModels;

    /** Active timing windows */
    TArray<FTimingWindow> ActiveTimingWindows;

    /** Combo state machine: current combo in progress */
    FString ActiveComboID;
    int32 ComboProgress = 0;
    float ComboTimer = 0.0f;

    /** Mastery metrics */
    FMasteryMetrics Metrics;

    /** Metrics update timer */
    float MetricsTimer = 0.0f;

    /** Counter-action lookup table (predicted action → best counter) */
    TMap<FString, FString> CounterTable;

    /** System initialised */
    bool bInitialised = false;

    // ====================================================
    // INTERNAL METHODS
    // ====================================================

    void FindAndCacheComponents();
    void InitialiseDefaultCombos();
    void InitialiseCounterTable();

    void UpdateTimingWindows(float DeltaTime);
    void UpdateComboStateMachine(float DeltaTime);
    void UpdateMetrics(float DeltaTime);
    void UpdateAPM(float CurrentTime);
    void AdvanceSkillTier();

    FComboDefinition* FindCombo(const FString& ComboID);
    const FComboDefinition* FindComboConst(const FString& ComboID) const;

    FString SelectActionByFrequency(const TMap<FString, int32>& ActionHistory) const;
};
