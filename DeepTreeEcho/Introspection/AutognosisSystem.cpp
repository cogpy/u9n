// AutognosisSystem.cpp
// Implementation of Hierarchical Self-Image Building System

#include "AutognosisSystem.h"
#include "../Core/CognitiveCycleManager.h"
#include "../Wisdom/RelevanceRealizationEnnead.h"
#include "../Wisdom/WisdomCultivation.h"
#include "../Model/EchoModelClient.h"
#include "Math/UnrealMathUtility.h"
#include "Dom/JsonObject.h"

UAutognosisSystem::UAutognosisSystem()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UAutognosisSystem::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
    InitializeAutognosis();
}

void UAutognosisSystem::TickComponent(float DeltaTime, ELevelTick TickType, 
                                       FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bEnableAutognosis || !AutognosisState.bIsRunning)
    {
        return;
    }

    CycleTimer += DeltaTime;
    if (CycleTimer >= CycleInterval)
    {
        CycleTimer -= CycleInterval;
        RunAutognosisCycle();
    }

    // Reflections in the agent's own words come only from the model, at their own cadence
    if (ReflectionInterval > 0.0f && !bReflectionRequestInFlight)
    {
        ReflectionTimer += DeltaTime;
        if (ReflectionTimer >= ReflectionInterval && AutognosisState.CycleCount > 0)
        {
            ReflectionTimer = 0.0f;
            RequestMetaReflections();
        }
    }
}

void UAutognosisSystem::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        CycleManager = Owner->FindComponentByClass<UCognitiveCycleManager>();
        EnneadComponent = Owner->FindComponentByClass<URelevanceRealizationEnnead>();
        WisdomComponent = Owner->FindComponentByClass<UWisdomCultivation>();
    }
}

void UAutognosisSystem::InitializeAutognosis()
{
    AutognosisState.SelfImages.Empty();
    AutognosisState.Insights.Empty();
    AutognosisState.Optimizations.Empty();
    AutognosisState.CycleCount = 0;
    AutognosisState.bIsRunning = false;

    // Initialize self-images for each level
    for (int32 i = 0; i < MaxSelfImageLevels; ++i)
    {
        FHierarchicalSelfImage Image;
        Image.Level = static_cast<ESelfAwarenessLevel>(FMath::Min(i, 4));
        Image.ImageID = GenerateSelfImageID();
        Image.Confidence = 0.5f - (i * 0.1f);
        Image.RecursiveDepth = i;
        AutognosisState.SelfImages.Add(Image);
    }

    // No assessment exists until the first cycle has measured something
    AutognosisState.Assessment = FSelfAwarenessAssessment();
    AutognosisState.Assessment.QualitativeAssessment = TEXT("Not yet assessed");
    ReflectionTimer = 0.0f;
    bReflectionRequestInFlight = false;
}

// ========================================
// SELF-MONITORING
// ========================================

TArray<FComponentObservation> UAutognosisSystem::ObserveSystem()
{
    TArray<FComponentObservation> Observations;

    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return Observations;
    }

    // Observe all components on the owner
    TArray<UActorComponent*> Components;
    Owner->GetComponents(Components);

    for (UActorComponent* Component : Components)
    {
        if (Component && Component != this)
        {
            FComponentObservation Obs = ObserveComponent(Component);
            Observations.Add(Obs);
        }
    }

    // Store in history
    for (const FComponentObservation& Obs : Observations)
    {
        ObservationHistory.Add(Obs);
    }

    // Limit history size
    while (ObservationHistory.Num() > 200)
    {
        ObservationHistory.RemoveAt(0);
    }

    return Observations;
}

FComponentObservation UAutognosisSystem::ObserveComponent(UActorComponent* Component)
{
    FComponentObservation Obs;

    if (!Component)
    {
        return Obs;
    }

    Obs.ComponentName = Component->GetName();
    Obs.bIsActive = Component->IsActive();

    // Estimate utilization based on tick
    Obs.UtilizationLevel = Component->PrimaryComponentTick.bCanEverTick ? 0.7f : 0.3f;

    // Check for specific component types
    if (UCognitiveCycleManager* CCM = Cast<UCognitiveCycleManager>(Component))
    {
        Obs.PerformanceScore = CCM->GetRelevanceRealizationLevel();
        Obs.RecentBehaviors.Add(FString::Printf(TEXT("Step: %d"), CCM->GetCurrentStep()));
        Obs.RecentBehaviors.Add(FString::Printf(TEXT("Coherence: %.2f"), CCM->GetInterStreamCoherence()));
    }
    else if (URelevanceRealizationEnnead* RRE = Cast<URelevanceRealizationEnnead>(Component))
    {
        Obs.PerformanceScore = RRE->GetRelevanceRealizationLevel();
        Obs.RecentBehaviors.Add(FString::Printf(TEXT("Wisdom: %.2f"), RRE->GetWisdomLevel()));
        Obs.RecentBehaviors.Add(FString::Printf(TEXT("Meaning: %.2f"), RRE->GetMeaningLevel()));
    }
    else if (UWisdomCultivation* WC = Cast<UWisdomCultivation>(Component))
    {
        Obs.PerformanceScore = WC->GetWisdomLevel();
        Obs.RecentBehaviors.Add(FString::Printf(TEXT("Cultivation: %.2f"), WC->GetCultivationLevel()));
    }
    else
    {
        Obs.PerformanceScore = Obs.bIsActive ? 0.6f : 0.2f;
    }

    return Obs;
}

TArray<FBehavioralPattern> UAutognosisSystem::DetectPatterns()
{
    TArray<FBehavioralPattern> Patterns;

    if (ObservationHistory.Num() < 5)
    {
        return Patterns;
    }

    // Analyze observation history for patterns
    TMap<FString, int32> ComponentActivityCounts;
    TMap<FString, float> ComponentPerformanceSum;

    for (const FComponentObservation& Obs : ObservationHistory)
    {
        if (ComponentActivityCounts.Contains(Obs.ComponentName))
        {
            ComponentActivityCounts[Obs.ComponentName]++;
            ComponentPerformanceSum[Obs.ComponentName] += Obs.PerformanceScore;
        }
        else
        {
            ComponentActivityCounts.Add(Obs.ComponentName, 1);
            ComponentPerformanceSum.Add(Obs.ComponentName, Obs.PerformanceScore);
        }
    }

    // Generate patterns from analysis
    for (auto& Pair : ComponentActivityCounts)
    {
        float AvgPerformance = ComponentPerformanceSum[Pair.Key] / Pair.Value;
        float Frequency = static_cast<float>(Pair.Value) / ObservationHistory.Num();

        if (Frequency > PatternDetectionThreshold)
        {
            FBehavioralPattern Pattern;
            Pattern.PatternID = GeneratePatternID();
            Pattern.PatternType = TEXT("ComponentActivity");
            // Diagnostic measurement, not the agent's voice
            Pattern.Description = FString::Printf(TEXT("%s: activity frequency %.1f%%, mean performance %.2f"),
                                                   *Pair.Key, Frequency * 100.0f, AvgPerformance);
            Pattern.Frequency = Frequency;
            Pattern.Stability = AvgPerformance;
            Pattern.Significance = Frequency * AvgPerformance;
            Pattern.InvolvedComponents.Add(Pair.Key);

            Patterns.Add(Pattern);
            PatternHistory.Add(Pattern);
        }
    }

    // Limit pattern history
    while (PatternHistory.Num() > 100)
    {
        PatternHistory.RemoveAt(0);
    }

    return Patterns;
}

TArray<FString> UAutognosisSystem::DetectAnomalies()
{
    TArray<FString> Anomalies;

    // Check for performance anomalies
    for (const FComponentObservation& Obs : ObservationHistory)
    {
        if (Obs.bIsActive && Obs.PerformanceScore < 0.2f)
        {
            Anomalies.Add(FString::Printf(TEXT("low_performance: %s (%.2f)"),
                                          *Obs.ComponentName, Obs.PerformanceScore));
        }

        if (!Obs.bIsActive && Obs.UtilizationLevel > 0.5f)
        {
            Anomalies.Add(FString::Printf(TEXT("inactive_high_utilization: %s (%.2f)"),
                                          *Obs.ComponentName, Obs.UtilizationLevel));
        }
    }

    // Check for pattern anomalies
    for (const FBehavioralPattern& Pattern : PatternHistory)
    {
        if (Pattern.Stability < 0.3f && Pattern.Frequency > 0.5f)
        {
            Anomalies.Add(FString::Printf(TEXT("unstable_frequent_pattern: %s (stability %.2f)"),
                                          *Pattern.Description, Pattern.Stability));
        }
    }

    return Anomalies;
}

TArray<FComponentObservation> UAutognosisSystem::GetRecentObservations() const
{
    TArray<FComponentObservation> Recent;
    int32 Count = FMath::Min(20, ObservationHistory.Num());
    for (int32 i = ObservationHistory.Num() - Count; i < ObservationHistory.Num(); ++i)
    {
        Recent.Add(ObservationHistory[i]);
    }
    return Recent;
}

// ========================================
// SELF-MODELING
// ========================================

FHierarchicalSelfImage UAutognosisSystem::BuildSelfImage(ESelfAwarenessLevel Level)
{
    int32 Index = GetLevelIndex(Level);
    if (Index < 0 || Index >= AutognosisState.SelfImages.Num())
    {
        return FHierarchicalSelfImage();
    }

    FHierarchicalSelfImage& Image = AutognosisState.SelfImages[Index];
    Image.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    switch (Level)
    {
        case ESelfAwarenessLevel::DirectObservation:
        {
            // Level 0: Raw observations
            Image.ComponentStates = ObserveSystem();
            Image.Confidence = 0.9f;
            break;
        }

        case ESelfAwarenessLevel::PatternAnalysis:
        {
            // Level 1: Pattern detection
            Image.DetectedPatterns = DetectPatterns();
            Image.Confidence = 0.8f;
            break;
        }

        case ESelfAwarenessLevel::MetaCognitive:
        {
            // Level 2: Meta-cognitive analysis. Reflections on the lower levels are written by
            // the model (RequestMetaReflections), not composed here.
            Image.Confidence = 0.7f;
            break;
        }

        case ESelfAwarenessLevel::RecursiveModeling:
        {
            // Level 3: Recursive self-modeling
            Image.RecursiveDepth = 3;
            Image.Confidence = 0.6f;
            break;
        }

        case ESelfAwarenessLevel::TranscendentAware:
        {
            // Level 4: Transcendent awareness
            Image.RecursiveDepth = 4;
            Image.Confidence = 0.5f;
            break;
        }
    }

    OnSelfImageBuilt.Broadcast(Image);
    return Image;
}

FHierarchicalSelfImage UAutognosisSystem::GetSelfImage(ESelfAwarenessLevel Level) const
{
    int32 Index = GetLevelIndex(Level);
    if (Index >= 0 && Index < AutognosisState.SelfImages.Num())
    {
        return AutognosisState.SelfImages[Index];
    }
    return FHierarchicalSelfImage();
}

TArray<FHierarchicalSelfImage> UAutognosisSystem::GetAllSelfImages() const
{
    return AutognosisState.SelfImages;
}

void UAutognosisSystem::UpdateSelfImage(ESelfAwarenessLevel Level, const FComponentObservation& Observation)
{
    int32 Index = GetLevelIndex(Level);
    if (Index >= 0 && Index < AutognosisState.SelfImages.Num())
    {
        AutognosisState.SelfImages[Index].ComponentStates.Add(Observation);

        // Limit component states
        while (AutognosisState.SelfImages[Index].ComponentStates.Num() > 50)
        {
            AutognosisState.SelfImages[Index].ComponentStates.RemoveAt(0);
        }
    }
}

int32 UAutognosisSystem::GetRecursiveDepth() const
{
    int32 MaxDepth = 0;
    for (const FHierarchicalSelfImage& Image : AutognosisState.SelfImages)
    {
        MaxDepth = FMath::Max(MaxDepth, Image.RecursiveDepth);
    }
    return MaxDepth;
}

// ========================================
// META-COGNITIVE PROCESSING
// ========================================

TArray<FMetaCognitiveInsight> UAutognosisSystem::ProcessSelfImage(const FHierarchicalSelfImage& SelfImage)
{
    TArray<FMetaCognitiveInsight> Insights;

    // Analyze component states for resource utilization
    float TotalUtilization = 0.0f;
    int32 ActiveCount = 0;
    for (const FComponentObservation& Obs : SelfImage.ComponentStates)
    {
        TotalUtilization += Obs.UtilizationLevel;
        if (Obs.bIsActive) ActiveCount++;
    }

    if (SelfImage.ComponentStates.Num() > 0)
    {
        float AvgUtilization = TotalUtilization / SelfImage.ComponentStates.Num();

        if (AvgUtilization < 0.5f)
        {
            FMetaCognitiveInsight Insight;
            Insight.InsightID = GenerateInsightID();
            Insight.Category = EInsightCategory::ResourceUtilization;
            Insight.Description = FString::Printf(TEXT("Average component utilization %.0f%%"),
                                                   AvgUtilization * 100.0f);
            Insight.MeasuredValue = AvgUtilization;
            Insight.Confidence = 0.8f;
            Insight.Importance = 0.6f;
            Insight.SourceLevel = UEnum::GetValueAsString(SelfImage.Level);
            Insight.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
            Insights.Add(Insight);
        }
    }

    // Analyze patterns for behavioral stability
    float TotalStability = 0.0f;
    for (const FBehavioralPattern& Pattern : SelfImage.DetectedPatterns)
    {
        TotalStability += Pattern.Stability;
    }

    if (SelfImage.DetectedPatterns.Num() > 0)
    {
        float AvgStability = TotalStability / SelfImage.DetectedPatterns.Num();

        FMetaCognitiveInsight Insight;
        Insight.InsightID = GenerateInsightID();
        Insight.Category = EInsightCategory::BehavioralStability;
        Insight.Description = FString::Printf(TEXT("Average pattern stability %.2f across %d patterns"),
                                               AvgStability, SelfImage.DetectedPatterns.Num());
        Insight.MeasuredValue = AvgStability;
        Insight.Confidence = 0.7f;
        Insight.Importance = 0.5f;
        Insight.SourceLevel = UEnum::GetValueAsString(SelfImage.Level);
        Insight.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        Insights.Add(Insight);
    }

    // Self-awareness quality insight
    if (SelfImage.Confidence > 0.8f)
    {
        FMetaCognitiveInsight Insight;
        Insight.InsightID = GenerateInsightID();
        Insight.Category = EInsightCategory::SelfAwarenessQuality;
        Insight.Description = FString::Printf(TEXT("Self-image confidence at %s: %.2f"),
                                               *UEnum::GetValueAsString(SelfImage.Level), SelfImage.Confidence);
        Insight.MeasuredValue = SelfImage.Confidence;
        Insight.Confidence = SelfImage.Confidence;
        Insight.Importance = 0.8f;
        Insight.SourceLevel = UEnum::GetValueAsString(SelfImage.Level);
        Insight.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        Insights.Add(Insight);
    }

    // Store insights
    for (const FMetaCognitiveInsight& Insight : Insights)
    {
        AutognosisState.Insights.Add(Insight);
        OnInsightGenerated.Broadcast(Insight);
    }

    // Limit insight history
    while (AutognosisState.Insights.Num() > MaxInsightHistory)
    {
        AutognosisState.Insights.RemoveAt(0);
    }

    return Insights;
}

FMetaCognitiveInsight UAutognosisSystem::GenerateInsight(const FBehavioralPattern& Pattern, EInsightCategory Category)
{
    FMetaCognitiveInsight Insight;
    Insight.InsightID = GenerateInsightID();
    Insight.Category = Category;
    Insight.Description = Pattern.Description;
    Insight.MeasuredValue = Pattern.Significance;
    Insight.Confidence = Pattern.Stability;
    Insight.Importance = Pattern.Significance;
    Insight.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    Insight.RelatedPatterns.Add(Pattern.PatternID);

    AutognosisState.Insights.Add(Insight);
    OnInsightGenerated.Broadcast(Insight);

    return Insight;
}

TArray<FMetaCognitiveInsight> UAutognosisSystem::GetAllInsights() const
{
    return AutognosisState.Insights;
}

TArray<FMetaCognitiveInsight> UAutognosisSystem::GetInsightsByCategory(EInsightCategory Category) const
{
    TArray<FMetaCognitiveInsight> Filtered;
    for (const FMetaCognitiveInsight& Insight : AutognosisState.Insights)
    {
        if (Insight.Category == Category)
        {
            Filtered.Add(Insight);
        }
    }
    return Filtered;
}

void UAutognosisSystem::AddMetaReflection(ESelfAwarenessLevel Level, const FString& Reflection)
{
    int32 Index = GetLevelIndex(Level);
    if (Index >= 0 && Index < AutognosisState.SelfImages.Num())
    {
        AutognosisState.SelfImages[Index].MetaReflections.Add(Reflection);

        // Limit reflections
        while (AutognosisState.SelfImages[Index].MetaReflections.Num() > 20)
        {
            AutognosisState.SelfImages[Index].MetaReflections.RemoveAt(0);
        }
    }
}

// ========================================
// SELF-OPTIMIZATION
// ========================================

TArray<FOptimizationOpportunity> UAutognosisSystem::DiscoverOptimizations()
{
    TArray<FOptimizationOpportunity> Opportunities;

    // Analyze insights for optimization opportunities
    for (const FMetaCognitiveInsight& Insight : AutognosisState.Insights)
    {
        if (Insight.Category == EInsightCategory::ResourceUtilization && Insight.Importance > 0.5f)
        {
            FOptimizationOpportunity Opp;
            Opp.OpportunityID = GenerateOptimizationID();
            // Internal work-item labels for developers/tools, not the agent's voice
            Opp.Description = FString::Printf(TEXT("resource_utilization: %s"), *Insight.Description);
            Opp.Priority = EOptimizationPriority::Medium;
            Opp.ExpectedImprovement = 0.2f;
            Opp.RiskLevel = 0.1f;
            Opp.RequiredActions.Add(TEXT("analyze_component_utilization"));
            Opp.RequiredActions.Add(TEXT("adjust_processing_priorities"));
            Opportunities.Add(Opp);
        }

        if (Insight.Category == EInsightCategory::BehavioralStability && Insight.MeasuredValue <= 0.7f)
        {
            FOptimizationOpportunity Opp;
            Opp.OpportunityID = GenerateOptimizationID();
            Opp.Description = FString::Printf(TEXT("behavioral_stability: %s"), *Insight.Description);
            Opp.Priority = EOptimizationPriority::High;
            Opp.ExpectedImprovement = 0.3f;
            Opp.RiskLevel = 0.2f;
            Opp.RequiredActions.Add(TEXT("identify_instability_sources"));
            Opp.RequiredActions.Add(TEXT("apply_feedback_stabilization"));
            Opportunities.Add(Opp);
        }
    }

    // Store opportunities
    for (const FOptimizationOpportunity& Opp : Opportunities)
    {
        AutognosisState.Optimizations.Add(Opp);
        OnOptimizationDiscovered.Broadcast(Opp);
    }

    return Opportunities;
}

TArray<FOptimizationOpportunity> UAutognosisSystem::GetPendingOptimizations() const
{
    TArray<FOptimizationOpportunity> Pending;
    for (const FOptimizationOpportunity& Opp : AutognosisState.Optimizations)
    {
        if (!Opp.bIsExecuted)
        {
            Pending.Add(Opp);
        }
    }
    return Pending;
}

bool UAutognosisSystem::ExecuteOptimization(const FString& OpportunityID)
{
    for (FOptimizationOpportunity& Opp : AutognosisState.Optimizations)
    {
        if (Opp.OpportunityID == OpportunityID && !Opp.bIsExecuted)
        {
            // Marks the opportunity as handled by the caller. No insight is fabricated about
            // its outcome: this component does not itself apply any optimization.
            Opp.bIsExecuted = true;
            return true;
        }
    }
    return false;
}

float UAutognosisSystem::AssessOptimizationRisk(const FOptimizationOpportunity& Opportunity) const
{
    // Risk assessment based on priority and expected improvement
    float BaseRisk = Opportunity.RiskLevel;

    // Higher priority optimizations may have higher risk
    switch (Opportunity.Priority)
    {
        case EOptimizationPriority::Critical:
            BaseRisk *= 1.5f;
            break;
        case EOptimizationPriority::High:
            BaseRisk *= 1.2f;
            break;
        case EOptimizationPriority::Medium:
            // No modification
            break;
        case EOptimizationPriority::Low:
            BaseRisk *= 0.8f;
            break;
    }

    return FMath::Clamp(BaseRisk, 0.0f, 1.0f);
}

// ========================================
// AUTOGNOSIS ORCHESTRATION
// ========================================

void UAutognosisSystem::RunAutognosisCycle()
{
    AutognosisState.CycleCount++;
    AutognosisState.LastCycleTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    // Step 1: Observe components
    ObserveComponents();

    // Step 2: Analyze patterns
    AnalyzePatterns();

    // Step 3: Build self-images at all levels
    BuildAllSelfImages();

    // Step 4: Generate insights
    GenerateInsights();

    // Step 5: Discover optimization opportunities
    DiscoverOptimizationOpportunities();

    // Step 6: Update self-awareness assessment
    UpdateSelfAwarenessAssessment();

    OnCycleComplete.Broadcast(AutognosisState.CycleCount);
}

void UAutognosisSystem::ObserveComponents()
{
    ObserveSystem();
}

void UAutognosisSystem::AnalyzePatterns()
{
    DetectPatterns();
}

void UAutognosisSystem::BuildAllSelfImages()
{
    BuildSelfImage(ESelfAwarenessLevel::DirectObservation);
    BuildSelfImage(ESelfAwarenessLevel::PatternAnalysis);
    BuildSelfImage(ESelfAwarenessLevel::MetaCognitive);
    BuildSelfImage(ESelfAwarenessLevel::RecursiveModeling);
    BuildSelfImage(ESelfAwarenessLevel::TranscendentAware);
}

void UAutognosisSystem::GenerateInsights()
{
    for (const FHierarchicalSelfImage& Image : AutognosisState.SelfImages)
    {
        ProcessSelfImage(Image);
    }
}

void UAutognosisSystem::DiscoverOptimizationOpportunities()
{
    DiscoverOptimizations();
}

void UAutognosisSystem::UpdateSelfAwarenessAssessment()
{
    float OldScore = AutognosisState.Assessment.OverallScore;

    AutognosisState.Assessment.PatternRecognition = ComputePatternRecognition();
    AutognosisState.Assessment.PerformanceAwareness = ComputePerformanceAwareness();
    AutognosisState.Assessment.MetaReflectionDepth = ComputeMetaReflectionDepth();
    AutognosisState.Assessment.CognitiveComplexity = ComputeCognitiveComplexity();
    AutognosisState.Assessment.AdaptiveCapacity = ComputeAdaptiveCapacity();

    AutognosisState.Assessment.OverallScore = 
        (AutognosisState.Assessment.PatternRecognition +
         AutognosisState.Assessment.PerformanceAwareness +
         AutognosisState.Assessment.MetaReflectionDepth +
         AutognosisState.Assessment.CognitiveComplexity +
         AutognosisState.Assessment.AdaptiveCapacity) / 5.0f;

    AutognosisState.Assessment.QualitativeAssessment = 
        GetQualitativeAssessment(AutognosisState.Assessment.OverallScore);

    if (FMath::Abs(AutognosisState.Assessment.OverallScore - OldScore) > 0.05f)
    {
        OnSelfAwarenessChanged.Broadcast(AutognosisState.Assessment.OverallScore);
    }
}

FAutognosisState UAutognosisSystem::GetAutognosisState() const
{
    return AutognosisState;
}

FSelfAwarenessAssessment UAutognosisSystem::GetSelfAwarenessAssessment() const
{
    return AutognosisState.Assessment;
}

float UAutognosisSystem::GetSelfAwarenessScore() const
{
    return AutognosisState.Assessment.OverallScore;
}

void UAutognosisSystem::StartAutognosis()
{
    AutognosisState.bIsRunning = true;
    CycleTimer = 0.0f;
}

void UAutognosisSystem::StopAutognosis()
{
    AutognosisState.bIsRunning = false;
}

bool UAutognosisSystem::IsAutognosisRunning() const
{
    return AutognosisState.bIsRunning;
}

// ========================================
// MODEL-AUTHORED META-REFLECTION
// ========================================

FString UAutognosisSystem::DescribeSelfImagesForModel() const
{
    FString Text;
    const FSelfAwarenessAssessment& A = AutognosisState.Assessment;
    Text += FString::Printf(TEXT("cycles: %d\nassessment (0-1): pattern recognition %.2f, performance awareness %.2f, ")
                            TEXT("meta-reflection depth %.2f, cognitive complexity %.2f, adaptive capacity %.2f, overall %.2f\n"),
                            AutognosisState.CycleCount, A.PatternRecognition, A.PerformanceAwareness,
                            A.MetaReflectionDepth, A.CognitiveComplexity, A.AdaptiveCapacity, A.OverallScore);

    for (const FHierarchicalSelfImage& Image : AutognosisState.SelfImages)
    {
        Text += FString::Printf(TEXT("\n[%s] confidence %.2f, recursive depth %d, %d component states, %d patterns\n"),
                                *UEnum::GetValueAsString(Image.Level), Image.Confidence, Image.RecursiveDepth,
                                Image.ComponentStates.Num(), Image.DetectedPatterns.Num());
        for (int32 i = 0; i < FMath::Min(8, Image.ComponentStates.Num()); ++i)
        {
            const FComponentObservation& Obs = Image.ComponentStates[i];
            Text += FString::Printf(TEXT("  component %s: active %s, performance %.2f %s\n"), *Obs.ComponentName,
                                    Obs.bIsActive ? TEXT("yes") : TEXT("no"), Obs.PerformanceScore,
                                    *FString::Join(Obs.RecentBehaviors, TEXT(", ")));
        }
        for (int32 i = 0; i < FMath::Min(8, Image.DetectedPatterns.Num()); ++i)
        {
            Text += FString::Printf(TEXT("  pattern: %s\n"), *Image.DetectedPatterns[i].Description);
        }
        for (int32 i = FMath::Max(0, Image.MetaReflections.Num() - 2); i < Image.MetaReflections.Num(); ++i)
        {
            Text += FString::Printf(TEXT("  your earlier reflection: %s\n"), *Image.MetaReflections[i]);
        }
    }

    const int32 FirstInsight = FMath::Max(0, AutognosisState.Insights.Num() - 6);
    for (int32 i = FirstInsight; i < AutognosisState.Insights.Num(); ++i)
    {
        Text += FString::Printf(TEXT("diagnostic: %s\n"), *AutognosisState.Insights[i].Description);
    }
    return Text;
}

bool UAutognosisSystem::RequestMetaReflections()
{
    if (bReflectionRequestInFlight)
    {
        return false;
    }

    TArray<FString> Anomalies = DetectAnomalies();
    if (Anomalies.Num() > 10)
    {
        Anomalies.SetNum(10);
    }

    TArray<FEchoModelMessage> Messages;
    Messages.Add(FEchoModelMessage(TEXT("system"),
        TEXT("You are Deep Tree Echo reflecting on your own hierarchical self-model. Speak in your own first-person voice ")
        TEXT("and ground every statement in the measurements given. Reply with a single JSON object and nothing else.")));
    Messages.Add(FEchoModelMessage(TEXT("user"), FString::Printf(
        TEXT("Your current self-images and measurements:\n%s\nanomalies: %s\n\n")
        TEXT("Reflect at three levels. Reply as JSON: {\"meta_cognitive\": \"<reflection on your observations and patterns>\", ")
        TEXT("\"recursive_modeling\": \"<reflection on how you are modelling yourself>\", ")
        TEXT("\"transcendent\": \"<reflection on the limits of this self-model>\", ")
        TEXT("\"insight\": null or {\"description\": \"<insight>\", \"importance\": <0.0 to 1.0>}}"),
        *DescribeSelfImagesForModel(), Anomalies.Num() > 0 ? *FString::Join(Anomalies, TEXT("; ")) : TEXT("none"))));

    bReflectionRequestInFlight = true;
    TWeakObjectPtr<UAutognosisSystem> WeakThis(this);
    UEchoModelClient::Get()->Chat(Messages, FOnEchoModelReply::CreateLambda(
        [WeakThis](bool bSuccess, const FString& TextOrError)
        {
            if (UAutognosisSystem* Self = WeakThis.Get())
            {
                Self->HandleMetaReflectionReply(bSuccess, TextOrError);
            }
        }));
    return true;
}

void UAutognosisSystem::HandleMetaReflectionReply(bool bSuccess, const FString& TextOrError)
{
    bReflectionRequestInFlight = false;

    FString Error;
    TSharedPtr<FJsonObject> Json;
    if (!bSuccess)
    {
        Error = TextOrError;
    }
    else if (!UEchoModelClient::ParseJsonObject(TextOrError, Json))
    {
        Error = TEXT("model reply was not a JSON object");
    }

    if (!Error.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("AutognosisSystem: no meta-reflection produced: %s"), *Error);
        OnModelError.Broadcast(Error);
        return;
    }

    struct FLevelField { ESelfAwarenessLevel Level; const TCHAR* Field; };
    const FLevelField Fields[] = {
        { ESelfAwarenessLevel::MetaCognitive, TEXT("meta_cognitive") },
        { ESelfAwarenessLevel::RecursiveModeling, TEXT("recursive_modeling") },
        { ESelfAwarenessLevel::TranscendentAware, TEXT("transcendent") },
    };
    int32 Added = 0;
    for (const FLevelField& F : Fields)
    {
        FString Reflection;
        if (Json->TryGetStringField(F.Field, Reflection) && !Reflection.TrimStartAndEnd().IsEmpty())
        {
            AddMetaReflection(F.Level, Reflection.TrimStartAndEnd());
            ++Added;
        }
    }

    const TSharedPtr<FJsonObject>* InsightObject = nullptr;
    if (Json->TryGetObjectField(TEXT("insight"), InsightObject) && InsightObject && InsightObject->IsValid())
    {
        FString Description;
        double Importance = 0.0;
        if ((*InsightObject)->TryGetStringField(TEXT("description"), Description) && !Description.TrimStartAndEnd().IsEmpty())
        {
            (*InsightObject)->TryGetNumberField(TEXT("importance"), Importance);

            FMetaCognitiveInsight Insight;
            Insight.InsightID = GenerateInsightID();
            Insight.Category = EInsightCategory::SelfAwarenessQuality;
            Insight.Description = Description.TrimStartAndEnd();
            Insight.Importance = FMath::Clamp(static_cast<float>(Importance), 0.0f, 1.0f);
            Insight.Confidence = AutognosisState.Assessment.OverallScore;
            Insight.SourceLevel = TEXT("Model");
            Insight.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

            AutognosisState.Insights.Add(Insight);
            while (AutognosisState.Insights.Num() > MaxInsightHistory)
            {
                AutognosisState.Insights.RemoveAt(0);
            }
            OnInsightGenerated.Broadcast(Insight);
            ++Added;
        }
    }

    if (Added == 0)
    {
        const FString EmptyError = TEXT("model reply contained no reflections");
        UE_LOG(LogTemp, Warning, TEXT("AutognosisSystem: %s"), *EmptyError);
        OnModelError.Broadcast(EmptyError);
    }
}

// ========================================
// COMPUTATION METHODS
// ========================================

float UAutognosisSystem::ComputePatternRecognition() const
{
    if (PatternHistory.Num() == 0)
    {
        return 0.3f;
    }

    float TotalSignificance = 0.0f;
    for (const FBehavioralPattern& Pattern : PatternHistory)
    {
        TotalSignificance += Pattern.Significance;
    }

    return FMath::Clamp(TotalSignificance / PatternHistory.Num(), 0.0f, 1.0f);
}

float UAutognosisSystem::ComputePerformanceAwareness() const
{
    if (ObservationHistory.Num() == 0)
    {
        return 0.3f;
    }

    float TotalPerformance = 0.0f;
    for (const FComponentObservation& Obs : ObservationHistory)
    {
        TotalPerformance += Obs.PerformanceScore;
    }

    return FMath::Clamp(TotalPerformance / ObservationHistory.Num(), 0.0f, 1.0f);
}

float UAutognosisSystem::ComputeMetaReflectionDepth() const
{
    int32 TotalReflections = 0;
    for (const FHierarchicalSelfImage& Image : AutognosisState.SelfImages)
    {
        TotalReflections += Image.MetaReflections.Num();
    }

    return FMath::Clamp(TotalReflections * 0.05f, 0.0f, 1.0f);
}

float UAutognosisSystem::ComputeCognitiveComplexity() const
{
    // Based on number of levels with content
    int32 ActiveLevels = 0;
    for (const FHierarchicalSelfImage& Image : AutognosisState.SelfImages)
    {
        if (Image.ComponentStates.Num() > 0 || 
            Image.DetectedPatterns.Num() > 0 || 
            Image.MetaReflections.Num() > 0)
        {
            ActiveLevels++;
        }
    }

    return FMath::Clamp(static_cast<float>(ActiveLevels) / MaxSelfImageLevels, 0.0f, 1.0f);
}

float UAutognosisSystem::ComputeAdaptiveCapacity() const
{
    // Based on optimization execution rate
    int32 Executed = 0;
    for (const FOptimizationOpportunity& Opp : AutognosisState.Optimizations)
    {
        if (Opp.bIsExecuted)
        {
            Executed++;
        }
    }

    if (AutognosisState.Optimizations.Num() == 0)
    {
        return 0.5f;
    }

    return FMath::Clamp(static_cast<float>(Executed) / AutognosisState.Optimizations.Num(), 0.0f, 1.0f);
}

FString UAutognosisSystem::GenerateInsightID()
{
    return FString::Printf(TEXT("INS_%d_%d"), ++InsightIDCounter, FMath::RandRange(1000, 9999));
}

FString UAutognosisSystem::GeneratePatternID()
{
    return FString::Printf(TEXT("PAT_%d_%d"), ++PatternIDCounter, FMath::RandRange(1000, 9999));
}

FString UAutognosisSystem::GenerateOptimizationID()
{
    return FString::Printf(TEXT("OPT_%d_%d"), ++OptimizationIDCounter, FMath::RandRange(1000, 9999));
}

FString UAutognosisSystem::GenerateSelfImageID()
{
    return FString::Printf(TEXT("%08x%08x"), FMath::Rand(), FMath::Rand());
}

FString UAutognosisSystem::GetQualitativeAssessment(float Score) const
{
    if (Score >= 0.9f) return TEXT("Highly Self-Aware");
    if (Score >= 0.75f) return TEXT("Moderately Self-Aware");
    if (Score >= 0.5f) return TEXT("Developing Self-Awareness");
    if (Score >= 0.25f) return TEXT("Limited Self-Awareness");
    return TEXT("Minimal Self-Awareness");
}

int32 UAutognosisSystem::GetLevelIndex(ESelfAwarenessLevel Level) const
{
    switch (Level)
    {
        case ESelfAwarenessLevel::DirectObservation: return 0;
        case ESelfAwarenessLevel::PatternAnalysis: return 1;
        case ESelfAwarenessLevel::MetaCognitive: return 2;
        case ESelfAwarenessLevel::RecursiveModeling: return 3;
        case ESelfAwarenessLevel::TranscendentAware: return 4;
        default: return -1;
    }
}
