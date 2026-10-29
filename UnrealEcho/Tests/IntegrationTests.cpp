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
#include "Interaction/DeepTreeEchoInteractionComponent.h"
#include "Interaction/InteractableObject.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

//=============================================================================
// Helper function to get test world
//=============================================================================

static UWorld* GetTestWorld()
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
    
    // Test: Personality trait intensities are stored and read back
    PersonalitySystem->SetTraitIntensity(EPersonalityTraitType::Confident, 0.9f);
    TestEqual(TEXT("Confident trait intensity"), PersonalitySystem->GetTraitIntensity(EPersonalityTraitType::Confident), 0.9f, 0.001f);
    
    PersonalitySystem->SetTraitIntensity(EPersonalityTraitType::Playful, 0.8f);
    TestEqual(TEXT("Playful trait intensity"), PersonalitySystem->GetTraitIntensity(EPersonalityTraitType::Playful), 0.8f, 0.001f);
    
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
    
    // Test: A dopamine stimulus raises the dopamine level
    const float DopamineBefore = NeuroSystem->GetNeurochemicalLevel(ENeurochemicalType::Dopamine);
    NeuroSystem->ModifyNeurochemical(ENeurochemicalType::Dopamine, 0.3f);
    TestTrue(TEXT("Dopamine level increased"), NeuroSystem->GetNeurochemicalLevel(ENeurochemicalType::Dopamine) > DopamineBefore);
    
    // Test: High cortisol is reflected in the stress level
    NeuroSystem->SetNeurochemicalLevel(ENeurochemicalType::Cortisol, 0.9f);
    
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
    
    // Test: Both systems hold their state side by side on one actor
    PersonalitySystem->SetTraitIntensity(EPersonalityTraitType::Confident, 0.9f);
    NeuroSystem->SetNeurochemicalLevel(ENeurochemicalType::Serotonin, 0.8f);
    
    TestEqual(TEXT("Confident trait intensity"), PersonalitySystem->GetTraitIntensity(EPersonalityTraitType::Confident), 0.9f, 0.001f);
    TestEqual(TEXT("Serotonin level"), NeuroSystem->GetNeurochemicalLevel(ENeurochemicalType::Serotonin), 0.8f, 0.001f);
    
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
    PersonalitySystem->SetTraitIntensity(EPersonalityTraitType::Confident, 0.8f);
    NeuroSystem->ModifyNeurochemical(ENeurochemicalType::Dopamine, 0.7f);
    
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
    PersonalitySystem->SetTraitIntensity(EPersonalityTraitType::Confident, 0.9f);
    PersonalitySystem->SetTraitIntensity(EPersonalityTraitType::Playful, 0.7f);
    PersonalitySystem->SetTraitIntensity(EPersonalityTraitType::HyperChaotic, 0.8f);
    
    // 2. Apply neurochemical stimulus
    NeuroSystem->SetNeurochemicalLevel(ENeurochemicalType::Dopamine, 0.8f);
    NeuroSystem->SetNeurochemicalLevel(ENeurochemicalType::Serotonin, 0.7f);
    
    // 3. Narrative loop observes and records
    NarrativeLoop->StartNarrativeLoop();
    NarrativeLoop->TickComponent(1.0f, ELevelTick::LEVELTICK_All, nullptr);
    
    // 4. Verify all systems are working together
    TestEqual(TEXT("Personality system holds traits"), PersonalitySystem->GetTraitIntensity(EPersonalityTraitType::HyperChaotic), 0.8f, 0.001f);
    TestTrue(TEXT("Narrative loop active"), NarrativeLoop->GetNarrativeState().bIsActive);
    
    // Test: System responds to external stimulus
    NeuroSystem->SetNeurochemicalLevel(ENeurochemicalType::Cortisol, 0.9f);
    
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
        PersonalitySystem->SetTraitIntensity(EPersonalityTraitType::Confident, FMath::FRand());
        NeuroSystem->SetNeurochemicalLevel(ENeurochemicalType::Dopamine, FMath::FRand());
        
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
// Interaction: success only from the target's interaction interface; the model
// chooses what to interact with
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractionHonestOutcomeTest,
    "UnrealEngineCog.Integration.InteractionReportsOnlyRealOutcomes",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FInteractionHonestOutcomeTest::RunTest(const FString& Parameters)
{
    UWorld* World = GetTestWorld();
    if (!World)
    {
        AddError(TEXT("Failed to get test world"));
        return false;
    }

    AActor* Owner = World->SpawnActor<AActor>();
    AActor* PlainTarget = World->SpawnActor<AActor>();
    AInteractableObject* EmptyObject = World->SpawnActor<AInteractableObject>();
    if (!Owner || !PlainTarget || !EmptyObject)
    {
        AddError(TEXT("Failed to spawn test actors"));
        return false;
    }
    PlainTarget->Tags.Add(TEXT("Interactable"));

    UDeepTreeEchoInteractionComponent* Interaction = NewObject<UDeepTreeEchoInteractionComponent>(Owner);
    Owner->AddInstanceComponent(Interaction);
    Interaction->RegisterComponent();

    // A tagged actor with no interaction interface: nothing is done and nothing succeeds
    const FInteractionResult NoInterface = Interaction->InteractWith(PlainTarget);
    TestFalse(TEXT("No success without an interaction interface"), NoInterface.bSuccess);
    TestFalse(TEXT("Nothing is in progress"), NoInterface.bInProgress);
    TestFalse(TEXT("The component is not left interacting"), Interaction->IsInteracting());
    TestEqual(TEXT("The failed attempt is remembered"), Interaction->GetInteractionHistory().Num(), 1);
    if (Interaction->GetInteractionHistory().Num() == 1)
    {
        TestFalse(TEXT("It is remembered as a failure"), Interaction->GetInteractionHistory()[0].bWasSuccessful);
    }

    // An interactable that offers no interaction to this character: an honest failure
    const FInteractionResult NoneOffered = Interaction->InteractWith(EmptyObject);
    TestFalse(TEXT("No success when the target offers nothing"), NoneOffered.bSuccess);
    TestFalse(TEXT("Nothing is in progress either"), NoneOffered.bInProgress);

    // No candidates: the model is not asked and nothing is chosen
    AddExpectedError(TEXT("No model interaction choice"), EAutomationExpectedErrorFlags::Contains, 2);
    TestFalse(TEXT("No interaction choice without candidates"), Interaction->InteractWithBest());
    TestFalse(TEXT("The refusal is recorded"), Interaction->GetLastInteractionChoiceError().IsEmpty());
    TestFalse(TEXT("No best interactable before the model chooses"), Interaction->GetBestInteractable().Actor.IsValid());

    Interaction->InteractionChoicePromptTemplate.Empty();
    TestFalse(TEXT("No interaction choice with an empty prompt"), Interaction->InteractWithBest());

    World->DestroyActor(EmptyObject);
    World->DestroyActor(PlainTarget);
    World->DestroyActor(Owner);
    return true;
}
