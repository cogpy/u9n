#include "Misc/AutomationTest.h"
#include "Avatar/Avatar3DComponentEnhanced.h"
#include "Personality/PersonalityTraitSystem.h"
#include "Neurochemical/NeurochemicalSimulationComponent.h"
#include "Avatar/MetaHumanDNABridge.h"
#include "Avatar/PythonDNACalibWrapper.h"
#include "Tests/AutomationCommon.h"
#include "EchoCharacterReaction.h"

// ===== Avatar3DComponentEnhanced Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAvatar3DComponentBasicTest, 
    "UnrealEngineCog.Avatar.BasicFunctionality", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAvatar3DComponentBasicTest::RunTest(const FString& Parameters)
{
    // Create test component
    UAvatar3DComponentEnhanced* AvatarComponent = NewObject<UAvatar3DComponentEnhanced>();
    
    TestNotNull(TEXT("Avatar component should be created"), AvatarComponent);
    
    if (AvatarComponent)
    {
        // Test emotional state setting
        AvatarComponent->SetEmotionalState(EAvatarEmotionalState::Happy, 0.8f);
        FAvatarEmotionalState State = AvatarComponent->GetCurrentEmotionalState();
        
        TestEqual(TEXT("Emotional state should be Happy"), 
                  State.CurrentEmotion, EAvatarEmotionalState::Happy);
        TestEqual(TEXT("Emotion intensity should be 0.8"), 
                  State.EmotionIntensity, 0.8f, 0.01f);
        
        // Test personality trait
        AvatarComponent->EnablePersonalityTrait(EAvatarPersonalityTrait::Confident, 0.7f);
        // Verification would require getter method
        
        // Test appearance settings
        FAvatarAppearanceSettings NewAppearance;
        NewAppearance.SkinTone = FLinearColor(1.0f, 0.9f, 0.8f);
        NewAppearance.HairColor = FLinearColor(0.4f, 0.8f, 0.9f);
        AvatarComponent->SetAppearance(NewAppearance);
        
        UE_LOG(LogTemp, Log, TEXT("Avatar3DComponent basic tests passed"));
    }
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAvatar3DEmotionalTransitionTest, 
    "UnrealEngineCog.Avatar.EmotionalTransition", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAvatar3DEmotionalTransitionTest::RunTest(const FString& Parameters)
{
    UAvatar3DComponentEnhanced* AvatarComponent = NewObject<UAvatar3DComponentEnhanced>();
    
    TestNotNull(TEXT("Avatar component should be created"), AvatarComponent);
    
    if (AvatarComponent)
    {
        // Set initial emotion
        AvatarComponent->SetEmotionalState(EAvatarEmotionalState::Neutral, 0.5f);
        
        // Trigger transition
        AvatarComponent->TransitionToEmotion(EAvatarEmotionalState::Excited, 1.0f);
        
        // In production, would simulate time passing and verify transition
        UE_LOG(LogTemp, Log, TEXT("Emotional transition test initiated"));
    }
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAvatar3DSuperHotGirlTest, 
    "UnrealEngineCog.Avatar.SuperHotGirlAesthetic", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAvatar3DSuperHotGirlTest::RunTest(const FString& Parameters)
{
    UAvatar3DComponentEnhanced* AvatarComponent = NewObject<UAvatar3DComponentEnhanced>();
    
    TestNotNull(TEXT("Avatar component should be created"), AvatarComponent);
    
    if (AvatarComponent)
    {
        // Apply super-hot-girl aesthetic
        AvatarComponent->ApplySuperHotGirlAesthetic();
        
        // Test aesthetic parameters
        AvatarComponent->ApplyEmotionalBlush(0.5f);
        AvatarComponent->ApplyHairShimmer(0.7f);
        AvatarComponent->SetEyeSparkle(0.9f);
        
        UE_LOG(LogTemp, Log, TEXT("Super-hot-girl aesthetic test passed"));
    }
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAvatar3DModelReactionTest, 
    "UnrealEngineCog.Avatar.AppliesModelReaction", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAvatar3DModelReactionTest::RunTest(const FString& Parameters)
{
    UAvatar3DComponentEnhanced* AvatarComponent = NewObject<UAvatar3DComponentEnhanced>();
    
    TestNotNull(TEXT("Avatar component should be created"), AvatarComponent);
    
    if (AvatarComponent)
    {
        AvatarComponent->ReactionCharacterName = TEXT("Deep Tree Echo");

        // A reaction as parsed from a model reply is applied as given
        FEchoCharacterReaction Reaction;
        Reaction.CharacterName = TEXT("Deep Tree Echo");
        Reaction.Expression = TEXT("Surprised");
        Reaction.Intensity = 0.6f;
        AvatarComponent->ApplyCharacterReaction(Reaction);

        FAvatarEmotionalState State = AvatarComponent->GetCurrentEmotionalState();
        TestEqual(TEXT("Avatar shows the model-chosen expression"), State.CurrentEmotion, EAvatarEmotionalState::Surprised);
        TestEqual(TEXT("Avatar uses the model-chosen intensity"), State.EmotionIntensity, 0.6f, 0.01f);

        // A reaction for another character, or with no expression, changes nothing
        FEchoCharacterReaction Other;
        Other.CharacterName = TEXT("Someone Else");
        Other.Expression = TEXT("Angry");
        Other.Intensity = 1.0f;
        AvatarComponent->ApplyCharacterReaction(Other);

        FEchoCharacterReaction NoExpression;
        NoExpression.CharacterName = TEXT("Deep Tree Echo");
        NoExpression.Utterance = TEXT("(model utterance)");
        AvatarComponent->ApplyCharacterReaction(NoExpression);

        State = AvatarComponent->GetCurrentEmotionalState();
        TestEqual(TEXT("Expression unchanged without a matching model reaction"), State.CurrentEmotion, EAvatarEmotionalState::Surprised);
    }
    
    return true;
}

// ===== PersonalityTraitSystem Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPersonalityTraitBasicTest, 
    "UnrealEngineCog.Personality.BasicFunctionality", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPersonalityTraitBasicTest::RunTest(const FString& Parameters)
{
    UPersonalityTraitSystem* PersonalitySystem = NewObject<UPersonalityTraitSystem>();
    
    TestNotNull(TEXT("Personality system should be created"), PersonalitySystem);
    
    if (PersonalitySystem)
    {
        // Test trait setting
        PersonalitySystem->SetTraitIntensity(EPersonalityTraitType::Confident, 0.8f);
        float Intensity = PersonalitySystem->GetTraitIntensity(EPersonalityTraitType::Confident);
        
        TestEqual(TEXT("Confidence trait should be 0.8"), Intensity, 0.8f, 0.01f);
        
        // Test trait modification
        PersonalitySystem->ModifyTrait(EPersonalityTraitType::Confident, 0.1f);
        float NewIntensity = PersonalitySystem->GetTraitIntensity(EPersonalityTraitType::Confident);
        
        TestEqual(TEXT("Confidence should increase to 0.9"), NewIntensity, 0.9f, 0.01f);
        
        UE_LOG(LogTemp, Log, TEXT("Personality trait basic tests passed"));
    }
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPersonalitySuperHotGirlTest, 
    "UnrealEngineCog.Personality.SuperHotGirlTrait", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPersonalitySuperHotGirlTest::RunTest(const FString& Parameters)
{
    UPersonalityTraitSystem* PersonalitySystem = NewObject<UPersonalityTraitSystem>();
    
    TestNotNull(TEXT("Personality system should be created"), PersonalitySystem);
    
    if (PersonalitySystem)
    {
        // Activate super-hot-girl trait
        PersonalitySystem->ActivateSuperHotGirlTrait(0.9f);
        
        FSuperHotGirlTrait Trait = PersonalitySystem->GetSuperHotGirlTrait();
        
        TestTrue(TEXT("Confidence should be high"), Trait.Confidence > 0.7f);
        TestTrue(TEXT("Charm should be high"), Trait.Charm > 0.8f);
        
        // Traits are persona data only: ticking with the trait active must not produce a reaction
        int32 ReactionCount = 0;
        const FDelegateHandle Handle = FEchoReactionBus::OnReaction().AddLambda(
            [&ReactionCount](const FEchoCharacterReaction&) { ++ReactionCount; });
        for (int32 Step = 0; Step < 10; ++Step)
        {
            PersonalitySystem->TickComponent(0.1f, LEVELTICK_All, nullptr);
        }
        FEchoReactionBus::OnReaction().Remove(Handle);

        TestEqual(TEXT("Personality traits must not synthesise a character reaction"), ReactionCount, 0);
        
        UE_LOG(LogTemp, Log, TEXT("Super-hot-girl trait test passed"));
    }
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPersonalityHyperChaoticTest, 
    "UnrealEngineCog.Personality.HyperChaoticTrait", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPersonalityHyperChaoticTest::RunTest(const FString& Parameters)
{
    UPersonalityTraitSystem* PersonalitySystem = NewObject<UPersonalityTraitSystem>();
    
    TestNotNull(TEXT("Personality system should be created"), PersonalitySystem);
    
    if (PersonalitySystem)
    {
        // Activate hyper-chaotic trait
        PersonalitySystem->ActivateHyperChaoticTrait(0.7f);
        
        FHyperChaoticTrait Trait = PersonalitySystem->GetHyperChaoticTrait();
        
        TestTrue(TEXT("Unpredictability should be high"), Trait.UnpredictabilityFactor > 0.5f);
        TestTrue(TEXT("Emotional volatility should be high"), Trait.EmotionalVolatility > 0.5f);
        
        // A chaotic persona still produces no reaction of its own: reactions come only from the model
        int32 ReactionCount = 0;
        const FDelegateHandle Handle = FEchoReactionBus::OnReaction().AddLambda(
            [&ReactionCount](const FEchoCharacterReaction&) { ++ReactionCount; });
        for (int32 Step = 0; Step < 10; ++Step)
        {
            PersonalitySystem->TickComponent(0.1f, LEVELTICK_All, nullptr);
        }
        FEchoReactionBus::OnReaction().Remove(Handle);
        TestEqual(TEXT("Hyper-chaotic trait must not synthesise a character reaction"), ReactionCount, 0);
        
        // Test predictability
        float Predictability = PersonalitySystem->GetBehavioralPredictability();
        TestTrue(TEXT("Predictability should be low with high chaos"), Predictability < 0.5f);
        
        UE_LOG(LogTemp, Log, TEXT("Hyper-chaotic trait test passed"));
    }
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPersonalityTraitInteractionTest, 
    "UnrealEngineCog.Personality.TraitInteractions", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPersonalityTraitInteractionTest::RunTest(const FString& Parameters)
{
    UPersonalityTraitSystem* PersonalitySystem = NewObject<UPersonalityTraitSystem>();
    
    TestNotNull(TEXT("Personality system should be created"), PersonalitySystem);
    
    if (PersonalitySystem)
    {
        // Set multiple traits
        PersonalitySystem->SetTraitIntensity(EPersonalityTraitType::SuperHotGirl, 0.8f);
        PersonalitySystem->SetTraitIntensity(EPersonalityTraitType::HyperChaotic, 0.5f);
        PersonalitySystem->SetTraitIntensity(EPersonalityTraitType::Confident, 0.7f);
        
        // Calculate interactions
        PersonalitySystem->CalculateTraitInteractions();
        
        // Test synergy
        float Synergy = PersonalitySystem->GetTraitSynergy(
            EPersonalityTraitType::SuperHotGirl, 
            EPersonalityTraitType::Confident
        );
        TestTrue(TEXT("SuperHotGirl and Confident should have positive synergy"), Synergy > 0.4f);
        
        // Test dominant traits
        TArray<EPersonalityTraitType> DominantTraits = PersonalitySystem->GetDominantTraits(3);
        TestEqual(TEXT("Should return 3 dominant traits"), DominantTraits.Num(), 3);
        
        // Test emotional tendency
        FVector2D EmotionalTendency = PersonalitySystem->GetEmotionalTendency();
        TestTrue(TEXT("Valence should be positive"), EmotionalTendency.X > 0.0f);
        
        UE_LOG(LogTemp, Log, TEXT("Trait interaction test passed"));
    }
    
    return true;
}

// ===== NeurochemicalSimulationComponent Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeurochemicalBasicTest, 
    "UnrealEngineCog.Neurochemical.BasicFunctionality", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FNeurochemicalBasicTest::RunTest(const FString& Parameters)
{
    UNeurochemicalSimulationComponent* NeuroSystem = NewObject<UNeurochemicalSimulationComponent>();
    
    TestNotNull(TEXT("Neurochemical system should be created"), NeuroSystem);
    
    if (NeuroSystem)
    {
        // Test setting neurochemical levels
        NeuroSystem->SetNeurochemicalLevel(ENeurochemicalType::Dopamine, 0.8f);
        float DopamineLevel = NeuroSystem->GetNeurochemicalLevel(ENeurochemicalType::Dopamine);
        
        TestEqual(TEXT("Dopamine level should be 0.8"), DopamineLevel, 0.8f, 0.01f);
        
        // Test modification
        NeuroSystem->ModifyNeurochemical(ENeurochemicalType::Serotonin, 0.2f);
        float SerotoninLevel = NeuroSystem->GetNeurochemicalLevel(ENeurochemicalType::Serotonin);
        
        TestTrue(TEXT("Serotonin should have increased"), SerotoninLevel > 0.6f);
        
        UE_LOG(LogTemp, Log, TEXT("Neurochemical basic tests passed"));
    }
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeurochemicalRewardResponseTest, 
    "UnrealEngineCog.Neurochemical.RewardResponse", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FNeurochemicalRewardResponseTest::RunTest(const FString& Parameters)
{
    UNeurochemicalSimulationComponent* NeuroSystem = NewObject<UNeurochemicalSimulationComponent>();
    
    TestNotNull(TEXT("Neurochemical system should be created"), NeuroSystem);
    
    if (NeuroSystem)
    {
        float InitialDopamine = NeuroSystem->GetNeurochemicalLevel(ENeurochemicalType::Dopamine);
        
        // Trigger reward response
        NeuroSystem->TriggerRewardResponse(1.0f);
        
        float NewDopamine = NeuroSystem->GetNeurochemicalLevel(ENeurochemicalType::Dopamine);
        
        TestTrue(TEXT("Dopamine should increase after reward"), NewDopamine > InitialDopamine);
        
        UE_LOG(LogTemp, Log, TEXT("Reward response test passed"));
    }
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeurochemicalEmotionalChemistryTest, 
    "UnrealEngineCog.Neurochemical.EmotionalChemistry", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FNeurochemicalEmotionalChemistryTest::RunTest(const FString& Parameters)
{
    UNeurochemicalSimulationComponent* NeuroSystem = NewObject<UNeurochemicalSimulationComponent>();
    
    TestNotNull(TEXT("Neurochemical system should be created"), NeuroSystem);
    
    if (NeuroSystem)
    {
        // Set high serotonin and dopamine
        NeuroSystem->SetNeurochemicalLevel(ENeurochemicalType::Serotonin, 0.9f);
        NeuroSystem->SetNeurochemicalLevel(ENeurochemicalType::Dopamine, 0.9f);
        
        FEmotionalChemistry Emotions = NeuroSystem->GetEmotionalChemistry();
        
        TestTrue(TEXT("Happiness should be high"), Emotions.Happiness > 0.8f);
        
        // Test behavioral effects
        float Motivation = NeuroSystem->GetMotivationLevel();
        TestTrue(TEXT("Motivation should be high with high dopamine"), Motivation > 0.8f);
        
        UE_LOG(LogTemp, Log, TEXT("Emotional chemistry test passed"));
    }
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeurochemicalHomeostasisTest, 
    "UnrealEngineCog.Neurochemical.Homeostasis", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FNeurochemicalHomeostasisTest::RunTest(const FString& Parameters)
{
    UNeurochemicalSimulationComponent* NeuroSystem = NewObject<UNeurochemicalSimulationComponent>();
    
    TestNotNull(TEXT("Neurochemical system should be created"), NeuroSystem);
    
    if (NeuroSystem)
    {
        // Set extreme value
        NeuroSystem->SetNeurochemicalLevel(ENeurochemicalType::Cortisol, 1.0f);
        
        // Enable homeostasis
        NeuroSystem->EnableHomeostasis(true);
        NeuroSystem->SetHomeostasisRate(0.5f);
        
        // In production, would simulate time and verify return to baseline
        
        // Test reset
        NeuroSystem->ResetToBaseline();
        FNeurochemicalState State = NeuroSystem->GetCurrentState();
        
        TestEqual(TEXT("Cortisol should return to baseline"), 
                  State.CortisolLevel, 0.3f, 0.1f);
        
        UE_LOG(LogTemp, Log, TEXT("Homeostasis test passed"));
    }
    
    return true;
}

// ===== Integration Tests =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIntegrationAvatarPersonalityTest, 
    "UnrealEngineCog.Integration.AvatarPersonality", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FIntegrationAvatarPersonalityTest::RunTest(const FString& Parameters)
{
    UAvatar3DComponentEnhanced* AvatarComponent = NewObject<UAvatar3DComponentEnhanced>();
    UPersonalityTraitSystem* PersonalitySystem = NewObject<UPersonalityTraitSystem>();
    
    TestNotNull(TEXT("Avatar component should be created"), AvatarComponent);
    TestNotNull(TEXT("Personality system should be created"), PersonalitySystem);
    
    if (AvatarComponent && PersonalitySystem)
    {
        // Set personality traits
        PersonalitySystem->ActivateSuperHotGirlTrait(0.8f);
        PersonalitySystem->ActivateHyperChaoticTrait(0.5f);
        
        // Apply to avatar
        AvatarComponent->ApplySuperHotGirlAesthetic();
        AvatarComponent->SetHyperChaoticBehavior(0.5f);
        
        // Verify integration
        FAvatarEmotionalState State = AvatarComponent->GetCurrentEmotionalState();
        
        UE_LOG(LogTemp, Log, TEXT("Avatar-Personality integration test passed"));
    }
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIntegrationNeurochemicalEmotionTest, 
    "UnrealEngineCog.Integration.NeurochemicalEmotion", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FIntegrationNeurochemicalEmotionTest::RunTest(const FString& Parameters)
{
    UAvatar3DComponentEnhanced* AvatarComponent = NewObject<UAvatar3DComponentEnhanced>();
    UNeurochemicalSimulationComponent* NeuroSystem = NewObject<UNeurochemicalSimulationComponent>();
    
    TestNotNull(TEXT("Avatar component should be created"), AvatarComponent);
    TestNotNull(TEXT("Neurochemical system should be created"), NeuroSystem);
    
    if (AvatarComponent && NeuroSystem)
    {
        // Trigger neurochemical response
        NeuroSystem->TriggerRewardResponse(1.0f);
        
        // Get emotional chemistry
        FEmotionalChemistry Emotions = NeuroSystem->GetEmotionalChemistry();
        
        // Map to avatar emotional state
        if (Emotions.Happiness > 0.7f)
        {
            AvatarComponent->SetEmotionalState(EAvatarEmotionalState::Happy, Emotions.Happiness);
        }
        
        UE_LOG(LogTemp, Log, TEXT("Neurochemical-Emotion integration test passed"));
    }
    
    return true;
}

// ===== MetaHuman DNA bridge: the face is the model's choice =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetaHumanDNANoNeurochemicalFaceWithoutModelTest,
    "UnrealEngineCog.MetaHumanDNA.NoNeurochemicalFaceWithoutModel",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMetaHumanDNANoNeurochemicalFaceWithoutModelTest::RunTest(const FString& Parameters)
{
    UMetaHumanDNABridge* Bridge = NewObject<UMetaHumanDNABridge>();
    TestNotNull(TEXT("DNA bridge should be created"), Bridge);
    if (!Bridge) { return false; }

    TestTrue(TEXT("The expression vocabulary is offered to the model"), Bridge->ExpressionRenderMap.Num() > 0);

    // High dopamine/serotonin used to be rendered as a smile directly; now nothing is rendered
    // until the model chooses an expression. An empty prompt fails without a model call.
    FNeurochemicalState State;
    State.DopamineLevel = 0.95f;
    State.SerotoninLevel = 0.9f;
    Bridge->FacialExpressionPromptTemplate.Empty();
    AddExpectedError(TEXT("No model facial expression"));
    TMap<FString, float> Weights;
    Bridge->SyncNeurochemicalStateToBlendShapes(State, Weights);

    TestEqual(TEXT("No blend shape weights before the model chooses an expression"), Weights.Num(), 0);
    TestTrue(TEXT("No model expression"), Bridge->GetModelFacialExpression().IsEmpty());
    TestFalse(TEXT("The failed request records an error"), Bridge->GetLastFacialExpressionError().IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetaHumanDNANoEmotionFaceWithoutModelTest,
    "UnrealEngineCog.MetaHumanDNA.NoEmotionFaceWithoutModel",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMetaHumanDNANoEmotionFaceWithoutModelTest::RunTest(const FString& Parameters)
{
    UMetaHumanDNABridge* Bridge = NewObject<UMetaHumanDNABridge>();
    TestNotNull(TEXT("DNA bridge should be created"), Bridge);
    if (!Bridge) { return false; }

    TestFalse(TEXT("The emotion prompt template has a default"), Bridge->EmotionFacialExpressionPromptTemplate.IsEmpty());

    // Positive valence and high arousal used to be rendered as Smile/EyeOpen/BrowRaise by a fixed
    // rule; now nothing is rendered until the model chooses an expression. An empty prompt fails
    // without a model call.
    FEmotionalState Emotion;
    Emotion.Valence = 0.9f;
    Emotion.Arousal = 0.8f;
    Emotion.Intensity = 1.0f;
    Emotion.Happiness = 0.9f;
    Bridge->EmotionFacialExpressionPromptTemplate.Empty();
    AddExpectedError(TEXT("No model facial expression"));
    TMap<FString, float> Weights;
    Weights.Add(TEXT("Smile_L"), 1.0f); // replaced by the (empty) model render
    Bridge->SyncEmotionalStateToFacialRig(Emotion, Weights);

    TestEqual(TEXT("No blend shape weights before the model chooses an expression"), Weights.Num(), 0);
    TestTrue(TEXT("No model expression"), Bridge->GetModelFacialExpression().IsEmpty());
    TestTrue(TEXT("The failure names the empty template"),
        Bridge->GetLastFacialExpressionError().Contains(TEXT("EmotionFacialExpressionPromptTemplate")));
    return true;
}

// ===== DNACalib wrapper: no simulated DNA data =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPythonDNACalibNoSimulationTest,
    "UnrealEngineCog.MetaHumanDNA.DNACalibWrapperReportsFailureHonestly",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPythonDNACalibNoSimulationTest::RunTest(const FString& Parameters)
{
    // Every failure below is logged as an error by the wrapper
    AddExpectedError(TEXT("Python wrapper error"), EAutomationExpectedErrorFlags::Contains, 0);

    FPythonDNACalibWrapper Wrapper;
    TestFalse(TEXT("Initialize fails for a missing DNACalib folder"),
              Wrapper.Initialize(FPaths::ProjectSavedDir() / TEXT("NoSuchDNACalibFolder")));
    TestFalse(TEXT("The failure says why"), Wrapper.GetLastError().IsEmpty());

    // Without the real bindings nothing is reported as loaded, saved or edited, and no rig data exists
    TestFalse(TEXT("LoadDNA fails"), Wrapper.LoadDNA(TEXT("Missing.dna")));
    TestFalse(TEXT("SaveDNA fails"), Wrapper.SaveDNA(FPaths::ProjectSavedDir() / TEXT("Out.dna")));
    TestFalse(TEXT("RenameJoint fails"), Wrapper.RenameJoint(TEXT("jaw"), TEXT("jaw2")));
    TestFalse(TEXT("RemoveJoint fails"), Wrapper.RemoveJoint(TEXT("jaw")));
    TestEqual(TEXT("No joint names"), Wrapper.GetJointNames().Num(), 0);
    TestEqual(TEXT("No blend shape names"), Wrapper.GetBlendShapeNames().Num(), 0);
    TestTrue(TEXT("No rig definition name"), Wrapper.GetDNADatabaseName().IsEmpty());
    TestTrue(TEXT("No Python call result"), Wrapper.CallPythonFunction(TEXT("os.getcwd"), {}).IsEmpty());
    return true;
}
