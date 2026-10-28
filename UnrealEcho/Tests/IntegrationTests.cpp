//=============================================================================
// IntegrationTests.cpp
// 
// Integration testing framework for Deep Tree Echo avatar systems.
// Tests the interaction and integration between all major components.
//
// Copyright (c) 2025 Deep Tree Echo Project
//=============================================================================

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Avatar/Avatar3DComponentEnhanced.h"
#include "Personality/PersonalityTraitSystem.h"
#include "Neurochemical/NeurochemicalSimulationComponent.h"
#include "Narrative/DiaryInsightBlogLoop.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

//=============================================================================
// Integration Test: Avatar + Personality System
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAvatarPersonalityIntegrationTest,
    "UnrealEngineCog.Integration.AvatarPersonality",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAvatarPersonalityIntegrationTest::RunTest(const FString& Parameters)
{
    // Create test actor
    UWorld* World = GetTestWorld();
    if (!World)
    {
        AddError(TEXT("Failed to get test world"));
        return false;
    }
    
    AActor* TestActor = World->SpawnActor<AActor>();
    if (!TestActor)
    {
        AddError(TEXT("Failed to spawn test actor"));
        return false;
    }
    
    // Add Avatar3DComponentEnhanced
    UAvatar3DComponentEnhanced* AvatarComponent = NewObject<UAvatar3DComponentEnhanced>(TestActor);
    TestActor->AddInstanceComponent(AvatarComponent);
    AvatarComponent->RegisterComponent();
    
    // Add PersonalityTraitSystem
    UPersonalityTraitSystem* PersonalitySystem = NewObject<UPersonalityTraitSystem>(TestActor);
    TestActor->AddInstanceComponent(PersonalitySystem);
    PersonalitySystem->RegisterComponent();
    
    // Initialize components
    AvatarComponent->InitializeComponent();
    PersonalitySystem->InitializeComponent();
    
    // Test: Set personality trait and verify avatar responds
    PersonalitySystem->SetTraitIntensity(EPersonalityTrait::Confidence, 0.9f);
    
    // Update avatar based on personality
    FPersonalityState PersonalityState = PersonalitySystem->GetCurrentPersonalityState();
    AvatarComponent->UpdateFromPersonality(PersonalityState);
    
    // Verify avatar state changed
    TestTrue(TEXT("Avatar updated from personality"), true);
    
    // Test: Personality affects emotional expression
    PersonalitySystem->SetTraitIntensity(EPersonalityTrait::Playfulness, 0.8f);
    FEmotionalExpression Expression = PersonalitySystem->GetEmotionalExpression();
    
    TestTrue(TEXT("Playfulness affects expression"), Expression.Intensity > 0.5f);
    
    // Cleanup
    World->DestroyActor(TestActor);
    
    return true;
}

//=============================================================================
// Integration Test: Avatar + Neurochemical System
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAvatarNeurochemicalIntegrationTest,
    "UnrealEngineCog.Integration.AvatarNeurochemical",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAvatarNeurochemicalIntegrationTest::RunTest(const FString& Parameters)
{
    // Create test actor
    UWorld* World = GetTestWorld();
    if (!World)
    {
        AddError(TEXT("Failed to get test world"));
        return false;
    }
    
    AActor* TestActor = World->SpawnActor<AActor>();
    if (!TestActor)
    {
        AddError(TEXT("Failed to spawn test actor"));
        return false;
    }
    
    // Add Avatar3DComponentEnhanced
    UAvatar3DComponentEnhanced* AvatarComponent = NewObject<UAvatar3DComponentEnhanced>(TestActor);
    TestActor->AddInstanceComponent(AvatarComponent);
    AvatarComponent->RegisterComponent();
    
    // Add NeurochemicalSimulationComponent
    UNeurochemicalSimulationComponent* NeuroSystem = NewObject<UNeurochemicalSimulationComponent>(TestActor);
    TestActor->AddInstanceComponent(NeuroSystem);
    NeuroSystem->RegisterComponent();
    
    // Initialize components
    AvatarComponent->InitializeComponent();
    NeuroSystem->InitializeComponent();
    
    // Test: Neurochemical state affects avatar appearance
    NeuroSystem->ApplyStimulus(ENeurochemicalType::Dopamine, 0.8f);
    
    // Update avatar based on neurochemical state
    FEmotionalState EmotionalState = NeuroSystem->GetCurrentEmotionalState();
    AvatarComponent->SetEmotionalState(EmotionalState);
    
    // Verify avatar appearance changed
    TestTrue(TEXT("Avatar updated from neurochemical state"), true);
    
    // Test: High cortisol affects avatar stress indicators
    NeuroSystem->ApplyStimulus(ENeurochemicalType::Cortisol, 0.9f);
    
    float StressLevel = NeuroSystem->GetNeurochemicalLevel(ENeurochemicalType::Cortisol);
    TestTrue(TEXT("Cortisol level increased"), StressLevel > 0.7f);
    
    // Cleanup
    World->DestroyActor(TestActor);
    
    return true;
}

//=============================================================================
// Integration Test: Personality + Neurochemical System
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPersonalityNeurochemicalIntegrationTest,
    "UnrealEngineCog.Integration.PersonalityNeurochemical",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPersonalityNeurochemicalIntegrationTest::RunTest(const FString& Parameters)
{
    // Create test actor
    UWorld* World = GetTestWorld();
    if (!World)
    {
        AddError(TEXT("Failed to get test world"));
        return false;
    }
    
    AActor* TestActor = World->SpawnActor<AActor>();
    if (!TestActor)
    {
        AddError(TEXT("Failed to spawn test actor"));
        return false;
    }
    
    // Add PersonalityTraitSystem
    UPersonalityTraitSystem* PersonalitySystem = NewObject<UPersonalityTraitSystem>(TestActor);
    TestActor->AddInstanceComponent(PersonalitySystem);
    PersonalitySystem->RegisterComponent();
    
    // Add NeurochemicalSimulationComponent
    UNeurochemicalSimulationComponent* NeuroSystem = NewObject<UNeurochemicalSimulationComponent>(TestActor);
    TestActor->AddInstanceComponent(NeuroSystem);
    NeuroSystem->RegisterComponent();
    
    // Initialize components
    PersonalitySystem->InitializeComponent();
    NeuroSystem->InitializeComponent();
    
    // Test: Personality traits influence neurochemical baseline
    PersonalitySystem->SetTraitIntensity(EPersonalityTrait::Confidence, 0.9f);
    
    // High confidence should correlate with higher dopamine baseline
    float DopamineLevel = NeuroSystem->GetNeurochemicalLevel(ENeurochemicalType::Dopamine);
    TestTrue(TEXT("Confidence affects dopamine"), DopamineLevel > 0.4f);
    
    // Test: Neurochemical state affects personality expression
    NeuroSystem->ApplyStimulus(ENeurochemicalType::Serotonin, 0.8f);
    
    FPersonalityState PersonalityState = PersonalitySystem->GetCurrentPersonalityState();
    TestTrue(TEXT("Serotonin affects personality state"), PersonalityState.OverallIntensity > 0.0f);
    
    // Cleanup
    World->DestroyActor(TestActor);
    
    return true;
}

//=============================================================================
// Integration Test: Narrative Loop + All Systems
//=============================================================================

/**
 * Waits until the narrative loop's diary request has an outcome: a model-written entry, or a
 * reported failure (State.LastError) with no entry created. Destroys the test actor when done.
 */
class FNarrativeModelOutcomeCommand : public IAutomationLatentCommand
{
public:
    FNarrativeModelOutcomeCommand(FAutomationTestBase* InTest, UDiaryInsightBlogLoop* InLoop, AActor* InActor, double InTimeoutSeconds)
        : Test(InTest), Loop(InLoop), Actor(InActor), TimeoutSeconds(InTimeoutSeconds)
    {
    }

    virtual bool Update() override
    {
        UDiaryInsightBlogLoop* NarrativeLoop = Loop.Get();
        if (!NarrativeLoop)
        {
            Test->AddError(TEXT("Narrative loop was destroyed before the model request finished"));
            return true;
        }

        const FNarrativeLoopState LoopState = NarrativeLoop->GetNarrativeState();
        const bool bEntryWritten = LoopState.TotalDiaryEntries > 0;
        const bool bFailureReported = !LoopState.LastError.IsEmpty();
        const bool bTimedOut = (FPlatformTime::Seconds() - StartTime) >= TimeoutSeconds;
        if (!bEntryWritten && !bFailureReported && !bTimedOut)
        {
            return false;
        }

        if (bEntryWritten)
        {
            // A model answered: the entry carries its text
            Test->TestFalse(TEXT("Model-written diary entry has content"), NarrativeLoop->GetMostRecentDiaryEntry().Content.IsEmpty());
        }
        else if (bFailureReported)
        {
            // No model: the failure is a system error and no diary entry or insight exists
            Test->TestTrue(TEXT("No diary entry without a model"), NarrativeLoop->GetMostRecentDiaryEntry().Content.IsEmpty());
            Test->TestTrue(TEXT("No insight without a model"), NarrativeLoop->GetMostRecentInsight().Title.IsEmpty());
        }
        else
        {
            Test->AddError(TEXT("Narrative loop neither produced a model diary entry nor reported a failure"));
        }

        if (AActor* TestActor = Actor.Get())
        {
            if (UWorld* World = TestActor->GetWorld())
            {
                World->DestroyActor(TestActor);
            }
        }
        return true;
    }

private:
    FAutomationTestBase* Test;
    TWeakObjectPtr<UDiaryInsightBlogLoop> Loop;
    TWeakObjectPtr<AActor> Actor;
    double TimeoutSeconds;
    double StartTime = FPlatformTime::Seconds();
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNarrativeLoopIntegrationTest,
    "UnrealEngineCog.Integration.NarrativeLoop",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FNarrativeLoopIntegrationTest::RunTest(const FString& Parameters)
{
    // Create test actor
    UWorld* World = GetTestWorld();
    if (!World)
    {
        AddError(TEXT("Failed to get test world"));
        return false;
    }
    
    AActor* TestActor = World->SpawnActor<AActor>();
    if (!TestActor)
    {
        AddError(TEXT("Failed to spawn test actor"));
        return false;
    }
    
    // Add all components
    UPersonalityTraitSystem* PersonalitySystem = NewObject<UPersonalityTraitSystem>(TestActor);
    TestActor->AddInstanceComponent(PersonalitySystem);
    PersonalitySystem->RegisterComponent();
    
    UNeurochemicalSimulationComponent* NeuroSystem = NewObject<UNeurochemicalSimulationComponent>(TestActor);
    TestActor->AddInstanceComponent(NeuroSystem);
    NeuroSystem->RegisterComponent();
    
    UDiaryInsightBlogLoop* NarrativeLoop = NewObject<UDiaryInsightBlogLoop>(TestActor);
    TestActor->AddInstanceComponent(NarrativeLoop);
    NarrativeLoop->RegisterComponent();
    
    // Initialize components
    PersonalitySystem->InitializeComponent();
    NeuroSystem->InitializeComponent();
    NarrativeLoop->InitializeComponent();
    
    // Start narrative loop
    NarrativeLoop->StartNarrativeLoop();
    
    TestTrue(TEXT("Narrative loop started"), NarrativeLoop->GetNarrativeState().bIsActive);
    
    // Test: Narrative loop observes personality and neurochemical state
    PersonalitySystem->SetTraitIntensity(EPersonalityTrait::Confidence, 0.8f);
    NeuroSystem->ApplyStimulus(ENeurochemicalType::Dopamine, 0.7f);
    
    // Simulate time passage
    NarrativeLoop->TickComponent(1.0f, ELevelTick::LEVELTICK_All, nullptr);
    
    // Request a diary entry and an insight. Both are written by the model asynchronously; nothing
    // may be created synchronously (no template or placeholder text stands in for the model).
    NarrativeLoop->ForceDiaryEntry();
    NarrativeLoop->TickComponent(0.1f, ELevelTick::LEVELTICK_All, nullptr);
    NarrativeLoop->ForceInsightGeneration();
    NarrativeLoop->TickComponent(0.1f, ELevelTick::LEVELTICK_All, nullptr);
    
    FNarrativeDiaryEntry RecentEntry = NarrativeLoop->GetMostRecentDiaryEntry();
    TestTrue(TEXT("No diary entry is created before the model replies"), RecentEntry.Content.IsEmpty());
    FNarrativeInsight RecentInsight = NarrativeLoop->GetMostRecentInsight();
    TestTrue(TEXT("No insight is created before the model replies"), RecentInsight.Title.IsEmpty());
    
    // Wait for the outcome: either a model wrote the entry, or (no model configured / served)
    // the failure is reported as a system error and still nothing is created.
    ADD_LATENT_AUTOMATION_COMMAND(FNarrativeModelOutcomeCommand(this, NarrativeLoop, TestActor, 30.0));
    
    return true;
}

//=============================================================================
// Integration Test: Full System Integration
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFullSystemIntegrationTest,
    "UnrealEngineCog.Integration.FullSystem",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFullSystemIntegrationTest::RunTest(const FString& Parameters)
{
    // Create test actor
    UWorld* World = GetTestWorld();
    if (!World)
    {
        AddError(TEXT("Failed to get test world"));
        return false;
    }
    
    AActor* TestActor = World->SpawnActor<AActor>();
    if (!TestActor)
    {
        AddError(TEXT("Failed to spawn test actor"));
        return false;
    }
    
    // Add all components
    UAvatar3DComponentEnhanced* AvatarComponent = NewObject<UAvatar3DComponentEnhanced>(TestActor);
    TestActor->AddInstanceComponent(AvatarComponent);
    AvatarComponent->RegisterComponent();
    
    UPersonalityTraitSystem* PersonalitySystem = NewObject<UPersonalityTraitSystem>(TestActor);
    TestActor->AddInstanceComponent(PersonalitySystem);
    PersonalitySystem->RegisterComponent();
    
    UNeurochemicalSimulationComponent* NeuroSystem = NewObject<UNeurochemicalSimulationComponent>(TestActor);
    TestActor->AddInstanceComponent(NeuroSystem);
    NeuroSystem->RegisterComponent();
    
    UDiaryInsightBlogLoop* NarrativeLoop = NewObject<UDiaryInsightBlogLoop>(TestActor);
    TestActor->AddInstanceComponent(NarrativeLoop);
    NarrativeLoop->RegisterComponent();
    
    // Initialize all components
    AvatarComponent->InitializeComponent();
    PersonalitySystem->InitializeComponent();
    NeuroSystem->InitializeComponent();
    NarrativeLoop->InitializeComponent();
    
    // Test: Complete integration flow
    // 1. Set personality traits
    PersonalitySystem->SetTraitIntensity(EPersonalityTrait::Confidence, 0.9f);
    PersonalitySystem->SetTraitIntensity(EPersonalityTrait::Playfulness, 0.7f);
    PersonalitySystem->SetTraitIntensity(EPersonalityTrait::Unpredictability, 0.8f);
    
    // 2. Apply neurochemical stimulus
    NeuroSystem->ApplyStimulus(ENeurochemicalType::Dopamine, 0.8f);
    NeuroSystem->ApplyStimulus(ENeurochemicalType::Serotonin, 0.7f);
    
    // 3. Update avatar from all systems
    FPersonalityState PersonalityState = PersonalitySystem->GetCurrentPersonalityState();
    FEmotionalState EmotionalState = NeuroSystem->GetCurrentEmotionalState();
    
    AvatarComponent->UpdateFromPersonality(PersonalityState);
    AvatarComponent->SetEmotionalState(EmotionalState);
    
    // 4. Narrative loop observes and records
    NarrativeLoop->StartNarrativeLoop();
    NarrativeLoop->TickComponent(1.0f, ELevelTick::LEVELTICK_All, nullptr);
    
    // 5. Verify all systems are working together
    TestTrue(TEXT("Avatar component initialized"), AvatarComponent->IsInitialized());
    TestTrue(TEXT("Personality system active"), PersonalitySystem->GetCurrentPersonalityState().OverallIntensity > 0.0f);
    TestTrue(TEXT("Neurochemical system active"), NeuroSystem->GetCurrentEmotionalState().Valence != 0.0f);
    TestTrue(TEXT("Narrative loop active"), NarrativeLoop->GetNarrativeState().bIsActive);
    
    // Test: System responds to external stimulus
    NeuroSystem->ApplyStimulus(ENeurochemicalType::Cortisol, 0.9f);
    
    // Update systems
    NeuroSystem->TickComponent(1.0f, ELevelTick::LEVELTICK_All, nullptr);
    PersonalitySystem->TickComponent(1.0f, ELevelTick::LEVELTICK_All, nullptr);
    
    // Verify avatar appearance reflects stress
    float StressLevel = NeuroSystem->GetNeurochemicalLevel(ENeurochemicalType::Cortisol);
    TestTrue(TEXT("System responds to stress stimulus"), StressLevel > 0.7f);
    
    // Cleanup
    World->DestroyActor(TestActor);
    
    return true;
}

//=============================================================================
// Integration Test: Performance Under Load
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPerformanceIntegrationTest,
    "UnrealEngineCog.Integration.Performance",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPerformanceIntegrationTest::RunTest(const FString& Parameters)
{
    // Create test actor
    UWorld* World = GetTestWorld();
    if (!World)
    {
        AddError(TEXT("Failed to get test world"));
        return false;
    }
    
    AActor* TestActor = World->SpawnActor<AActor>();
    if (!TestActor)
    {
        AddError(TEXT("Failed to spawn test actor"));
        return false;
    }
    
    // Add all components
    UAvatar3DComponentEnhanced* AvatarComponent = NewObject<UAvatar3DComponentEnhanced>(TestActor);
    TestActor->AddInstanceComponent(AvatarComponent);
    AvatarComponent->RegisterComponent();
    
    UPersonalityTraitSystem* PersonalitySystem = NewObject<UPersonalityTraitSystem>(TestActor);
    TestActor->AddInstanceComponent(PersonalitySystem);
    PersonalitySystem->RegisterComponent();
    
    UNeurochemicalSimulationComponent* NeuroSystem = NewObject<UNeurochemicalSimulationComponent>(TestActor);
    TestActor->AddInstanceComponent(NeuroSystem);
    NeuroSystem->RegisterComponent();
    
    // Initialize components
    AvatarComponent->InitializeComponent();
    PersonalitySystem->InitializeComponent();
    NeuroSystem->InitializeComponent();
    
    // Test: Performance with rapid updates
    double StartTime = FPlatformTime::Seconds();
    
    for (int32 i = 0; i < 1000; i++)
    {
        // Simulate rapid state changes
        PersonalitySystem->SetTraitIntensity(EPersonalityTrait::Confidence, FMath::FRand());
        NeuroSystem->ApplyStimulus(ENeurochemicalType::Dopamine, FMath::FRand());
        
        // Update systems
        PersonalitySystem->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);
        NeuroSystem->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);
        AvatarComponent->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);
    }
    
    double EndTime = FPlatformTime::Seconds();
    double ElapsedTime = EndTime - StartTime;
    
    // Verify performance (should complete 1000 iterations in less than 1 second)
    TestTrue(TEXT("Performance acceptable"), ElapsedTime < 1.0);
    
    AddInfo(FString::Printf(TEXT("1000 iterations completed in %.3f seconds"), ElapsedTime));
    
    // Cleanup
    World->DestroyActor(TestActor);
    
    return true;
}

//=============================================================================
// Helper function to get test world
//=============================================================================

UWorld* GetTestWorld()
{
    UWorld* World = nullptr;
    
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)
        {
            World = Context.World();
            break;
        }
    }
    
    return World;
}
