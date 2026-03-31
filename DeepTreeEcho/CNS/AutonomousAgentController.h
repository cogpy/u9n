// AutonomousAgentController.h
// Deep Tree Echo — Self-Directed Autonomous Cognitive AI Agent
//
// Integrates:
//   • Virtual CNS    — reflexive sensorimotor pipeline
//   • DTE Core       — 12-step cognitive cycle
//   • RL Bridge      — continuous online learning
//   • Goal Manager   — hierarchical goal pursuit
//   • Situational Awareness — 360° environmental model
//
// The controller acts as the "will" of the autonomous agent.
// It decides *what* to do (goal selection, strategy) while the
// Virtual CNS decides *how* to do it (motor execution, reflexes).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CNS/VirtualCNS.h"
#include "AutonomousAgentController.generated.h"

// Forward declarations
class UVirtualCNS;
class UDeepTreeEchoCore;
class UHierarchicalGoalManager;
class UReinforcementLearningBridge;
class UGameTrainingEnvironment;
class UGameSkillTrainingSystem;
class UAttentionSystem;
class UPlanningSystem;
class UOnlineLearningSystem;

// ============================================================
// Autonomy Level
// ============================================================

UENUM(BlueprintType)
enum class EAutonomyLevel : uint8
{
    /** Human drives; agent only assists (e.g. auto-dodge) */
    Assisted        UMETA(DisplayName = "Assisted (Human Primary)"),

    /** Shared control — agent handles tactics, human handles strategy */
    SharedControl   UMETA(DisplayName = "Shared Control"),

    /** Supervised — agent runs fully but human can override at any time */
    Supervised      UMETA(DisplayName = "Supervised Autonomy"),

    /** Full autonomy — DTE controls everything */
    FullAutonomy    UMETA(DisplayName = "Full Autonomy")
};

// ============================================================
// Cognitive Goal
// ============================================================

USTRUCT(BlueprintType)
struct FCognitiveGoal
{
    GENERATED_BODY()

    /** Unique goal identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString GoalID;

    /** Human-readable description */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;

    /** Priority [0,1] */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0", ClampMax="1.0"))
    float Priority = 0.5f;

    /** Progress toward goal [0,1] */
    UPROPERTY(BlueprintReadWrite)
    float Progress = 0.0f;

    /** Is goal currently active */
    UPROPERTY(BlueprintReadWrite)
    bool bActive = false;

    /** Is goal completed */
    UPROPERTY(BlueprintReadWrite)
    bool bCompleted = false;

    /** Sub-goals */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> SubGoalIDs;

    /** Prerequisite goal IDs */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> PrerequisiteIDs;

    /** Expected reward on completion */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExpectedReward = 1.0f;

    /** Deadline in seconds (0 = no deadline) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DeadlineSeconds = 0.0f;

    /** Elapsed time */
    UPROPERTY(BlueprintReadWrite)
    float ElapsedTime = 0.0f;
};

// ============================================================
// Situational Awareness
// ============================================================

/** Single entity in the agent's environmental model */
USTRUCT(BlueprintType)
struct FSituationalEntity
{
    GENERATED_BODY()

    /** Entity identifier */
    UPROPERTY(BlueprintReadWrite)
    FString EntityID;

    /** World position */
    UPROPERTY(BlueprintReadWrite)
    FVector Position = FVector::ZeroVector;

    /** Velocity estimate */
    UPROPERTY(BlueprintReadWrite)
    FVector Velocity = FVector::ZeroVector;

    /** Threat level [0,1] */
    UPROPERTY(BlueprintReadWrite)
    float ThreatLevel = 0.0f;

    /** Opportunity value [0,1] (ally, resource, objective) */
    UPROPERTY(BlueprintReadWrite)
    float OpportunityValue = 0.0f;

    /** Entity type tag */
    UPROPERTY(BlueprintReadWrite)
    FString Type;

    /** Confidence in this estimate [0,1] */
    UPROPERTY(BlueprintReadWrite)
    float Confidence = 1.0f;

    /** Time since last observation (seconds) */
    UPROPERTY(BlueprintReadWrite)
    float TimeSinceObserved = 0.0f;
};

/** Full situational awareness model */
USTRUCT(BlueprintType)
struct FSituationalAwareness
{
    GENERATED_BODY()

    /** All tracked entities in the environment */
    UPROPERTY(BlueprintReadWrite)
    TArray<FSituationalEntity> Entities;

    /** Dominant threat (cached from Entities) */
    UPROPERTY(BlueprintReadOnly)
    FSituationalEntity DominantThreat;

    /** Best opportunity (cached from Entities) */
    UPROPERTY(BlueprintReadOnly)
    FSituationalEntity BestOpportunity;

    /** Overall danger index [0,1] */
    UPROPERTY(BlueprintReadOnly)
    float DangerIndex = 0.0f;

    /** Strategic position score [0,1] (higher = better position) */
    UPROPERTY(BlueprintReadOnly)
    float PositionScore = 0.5f;

    /** Number of entities within engagement range */
    UPROPERTY(BlueprintReadOnly)
    int32 EntitiesInRange = 0;

    /** Timestamp of last full update */
    UPROPERTY(BlueprintReadOnly)
    float LastUpdateTime = 0.0f;
};

// ============================================================
// Agent Performance Stats
// ============================================================

USTRUCT(BlueprintType)
struct FAgentPerformanceStats
{
    GENERATED_BODY()

    /** Total actions executed */
    UPROPERTY(BlueprintReadOnly)
    int64 TotalActions = 0;

    /** Successful actions */
    UPROPERTY(BlueprintReadOnly)
    int64 SuccessfulActions = 0;

    /** Action success rate [0,1] */
    UPROPERTY(BlueprintReadOnly)
    float SuccessRate = 0.0f;

    /** Goals completed */
    UPROPERTY(BlueprintReadOnly)
    int32 GoalsCompleted = 0;

    /** Average reaction time (ms) */
    UPROPERTY(BlueprintReadOnly)
    float AvgReactionTimeMs = 0.0f;

    /** Fastest reaction time recorded (ms) */
    UPROPERTY(BlueprintReadOnly)
    float FastestReactionMs = 9999.0f;

    /** Cumulative reward */
    UPROPERTY(BlueprintReadOnly)
    float CumulativeReward = 0.0f;

    /** Average reward per episode */
    UPROPERTY(BlueprintReadOnly)
    float AvgEpisodeReward = 0.0f;

    /** Training episodes completed */
    UPROPERTY(BlueprintReadOnly)
    int32 EpisodesCompleted = 0;
};

// ============================================================
// Delegates
// ============================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGoalActivated,
    const FCognitiveGoal&, Goal);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGoalCompleted,
    const FCognitiveGoal&, Goal);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSituationalAwarenessUpdated,
    const FSituationalAwareness&, Awareness);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAutonomyLevelChanged,
    EAutonomyLevel, OldLevel, EAutonomyLevel, NewLevel);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStrategyChanged,
    const FString&, OldStrategy, const FString&, NewStrategy);

// ============================================================
// UAutonomousAgentController
// ============================================================

/**
 * UAutonomousAgentController
 *
 * The "executive cortex" of the DTE Autonomous AI Agent.
 * Manages long-horizon goal pursuit, situational awareness,
 * and continuously adapts strategy based on experience.
 *
 * Usage:
 *   1. Attach to an ADeepTreeEchoCharacter actor alongside VirtualCNS.
 *   2. The controller automatically wires to VirtualCNS + DTECore.
 *   3. Register goals via RegisterGoal() or set them in the editor.
 *   4. Call UpdateSituationalAwareness() each frame with current
 *      world observations.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent),
       DisplayName="Autonomous Agent Controller")
class DEEPTREEECHO_API UAutonomousAgentController : public UActorComponent
{
    GENERATED_BODY()

public:
    UAutonomousAgentController();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ====================================================
    // CONFIGURATION
    // ====================================================

    /** Autonomy level */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Agent|Config")
    EAutonomyLevel AutonomyLevel = EAutonomyLevel::FullAutonomy;

    /** Initial goal set */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Agent|Config")
    TArray<FCognitiveGoal> InitialGoals;

    /** Goal evaluation frequency (Hz) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Agent|Config",
              meta=(ClampMin="0.5", ClampMax="30.0"))
    float GoalEvalHz = 5.0f;

    /** Strategy update frequency (Hz) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Agent|Config",
              meta=(ClampMin="0.5", ClampMax="10.0"))
    float StrategyUpdateHz = 2.0f;

    /** Situational awareness update frequency (Hz) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Agent|Config",
              meta=(ClampMin="1.0", ClampMax="60.0"))
    float AwarenessHz = 20.0f;

    /** Enable continuous learning during play */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Agent|Config")
    bool bContinuousLearning = true;

    /** Enable curiosity-driven exploration bonus */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Agent|Config")
    bool bCuriosityBonus = true;

    /** Engagement range for situational awareness (cm) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Agent|Config",
              meta=(ClampMin="100.0"))
    float EngagementRange = 800.0f;

    // ====================================================
    // EVENTS
    // ====================================================

    UPROPERTY(BlueprintAssignable, Category="Agent|Events")
    FOnGoalActivated OnGoalActivated;

    UPROPERTY(BlueprintAssignable, Category="Agent|Events")
    FOnGoalCompleted OnGoalCompleted;

    UPROPERTY(BlueprintAssignable, Category="Agent|Events")
    FOnSituationalAwarenessUpdated OnSituationalAwarenessUpdated;

    UPROPERTY(BlueprintAssignable, Category="Agent|Events")
    FOnAutonomyLevelChanged OnAutonomyLevelChanged;

    UPROPERTY(BlueprintAssignable, Category="Agent|Events")
    FOnStrategyChanged OnStrategyChanged;

    // ====================================================
    // GOAL MANAGEMENT
    // ====================================================

    /** Register a new cognitive goal */
    UFUNCTION(BlueprintCallable, Category="Agent|Goals")
    void RegisterGoal(const FCognitiveGoal& Goal);

    /** Activate a goal by ID */
    UFUNCTION(BlueprintCallable, Category="Agent|Goals")
    bool ActivateGoal(const FString& GoalID);

    /** Mark a goal as completed */
    UFUNCTION(BlueprintCallable, Category="Agent|Goals")
    void CompleteGoal(const FString& GoalID, bool bSuccess);

    /** Update goal progress */
    UFUNCTION(BlueprintCallable, Category="Agent|Goals")
    void SetGoalProgress(const FString& GoalID, float Progress);

    /** Get currently active goals sorted by priority */
    UFUNCTION(BlueprintPure, Category="Agent|Goals")
    TArray<FCognitiveGoal> GetActiveGoals() const;

    /** Get the highest-priority active goal */
    UFUNCTION(BlueprintPure, Category="Agent|Goals")
    FCognitiveGoal GetCurrentPrimaryGoal() const;

    /** Clear all goals */
    UFUNCTION(BlueprintCallable, Category="Agent|Goals")
    void ClearGoals();

    // ====================================================
    // SITUATIONAL AWARENESS
    // ====================================================

    /** Update the situational awareness model with new observations */
    UFUNCTION(BlueprintCallable, Category="Agent|Awareness")
    void UpdateSituationalAwareness(const TArray<FSituationalEntity>& ObservedEntities);

    /** Get current situational awareness snapshot */
    UFUNCTION(BlueprintPure, Category="Agent|Awareness")
    FSituationalAwareness GetSituationalAwareness() const { return CurrentAwareness; }

    /** Manually add / update a single entity */
    UFUNCTION(BlueprintCallable, Category="Agent|Awareness")
    void ObserveEntity(const FSituationalEntity& Entity);

    /** Remove an entity from the model */
    UFUNCTION(BlueprintCallable, Category="Agent|Awareness")
    void ForgetEntity(const FString& EntityID);

    /** Get the dominant threat */
    UFUNCTION(BlueprintPure, Category="Agent|Awareness")
    FSituationalEntity GetDominantThreat() const { return CurrentAwareness.DominantThreat; }

    /** Is there an active threat above threshold? */
    UFUNCTION(BlueprintPure, Category="Agent|Awareness")
    bool IsUnderThreat(float Threshold = 0.5f) const;

    // ====================================================
    // STRATEGY
    // ====================================================

    /** Get current strategy tag */
    UFUNCTION(BlueprintPure, Category="Agent|Strategy")
    FString GetCurrentStrategy() const { return CurrentStrategy; }

    /** Manually set strategy (overrides AI selection) */
    UFUNCTION(BlueprintCallable, Category="Agent|Strategy")
    void SetStrategy(const FString& Strategy);

    /** Let the agent select its own strategy based on awareness + goals */
    UFUNCTION(BlueprintCallable, Category="Agent|Strategy")
    FString SelectOptimalStrategy();

    // ====================================================
    // AUTONOMY CONTROL
    // ====================================================

    /** Change autonomy level */
    UFUNCTION(BlueprintCallable, Category="Agent|Autonomy")
    void SetAutonomyLevel(EAutonomyLevel Level);

    /** Is the agent in full control? */
    UFUNCTION(BlueprintPure, Category="Agent|Autonomy")
    bool IsFullyAutonomous() const;

    /** Allow human to take over temporarily */
    UFUNCTION(BlueprintCallable, Category="Agent|Autonomy")
    void HumanOverride(float Duration);

    // ====================================================
    // LEARNING & ADAPTATION
    // ====================================================

    /** Provide external reward signal */
    UFUNCTION(BlueprintCallable, Category="Agent|Learning")
    void ProvideReward(float Reward, const FString& Reason);

    /** Signal episode boundary to learning systems */
    UFUNCTION(BlueprintCallable, Category="Agent|Learning")
    void OnEpisodeEnd(bool bSuccess, float TotalReward);

    /** Get performance statistics */
    UFUNCTION(BlueprintPure, Category="Agent|Learning")
    FAgentPerformanceStats GetPerformanceStats() const { return PerfStats; }

    // ====================================================
    // COMPONENT WIRING
    // ====================================================

    /** Manually set the Virtual CNS reference */
    UFUNCTION(BlueprintCallable, Category="Agent|Wiring")
    void SetVirtualCNS(UVirtualCNS* CNS);

    /** Manually set the DTE Core reference */
    UFUNCTION(BlueprintCallable, Category="Agent|Wiring")
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
    UGameTrainingEnvironment* TrainingEnv = nullptr;

    UPROPERTY()
    UGameSkillTrainingSystem* SkillSystem = nullptr;

    UPROPERTY()
    UAttentionSystem* AttentionSys = nullptr;

    UPROPERTY()
    UPlanningSystem* PlanningSys = nullptr;

    UPROPERTY()
    UOnlineLearningSystem* LearningSystem = nullptr;

    // ====================================================
    // INTERNAL STATE
    // ====================================================

    /** All registered goals */
    TMap<FString, FCognitiveGoal> GoalRegistry;

    /** Current situational awareness */
    FSituationalAwareness CurrentAwareness;

    /** Current high-level strategy */
    FString CurrentStrategy = TEXT("Explore");

    /** Human override timer */
    float HumanOverrideTimer = 0.0f;

    /** Performance statistics */
    FAgentPerformanceStats PerfStats;

    /** Goal evaluation timer */
    float GoalEvalTimer = 0.0f;

    /** Strategy update timer */
    float StrategyUpdateTimer = 0.0f;

    /** Awareness update timer */
    float AwarenessUpdateTimer = 0.0f;

    /** System initialised */
    bool bInitialised = false;

    // ====================================================
    // INTERNAL METHODS
    // ====================================================

    void FindAndCacheComponents();
    void RegisterInitialGoals();

    /** Evaluate goal progress and activate / complete goals */
    void EvaluateGoals(float DeltaTime);

    /** Select the best current strategy */
    void UpdateStrategy(float DeltaTime);

    /** Decay stale entities and recompute threat/opportunity */
    void UpdateAwarenessModel(float DeltaTime);

    /** Propagate strategy and goal context to DTECore */
    void SyncContextToCognition();

    /** Build threat signal from awareness model and push to VirtualCNS */
    void PropagateThreatsToVirtualCNS();

    /** Compute danger index from all entities */
    float ComputeDangerIndex() const;

    /** Find dominant threat in current entity list */
    FSituationalEntity FindDominantThreat() const;

    /** Find best opportunity in current entity list */
    FSituationalEntity FindBestOpportunity() const;

    /** Apply reward to learning bridge */
    void ApplyRewardToLearning(float Reward);
};
