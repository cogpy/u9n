// Automation tests: Echobeats reports nothing derived from its untrained (random) feature weights.

#include "Misc/AutomationTest.h"
#include "EchobeatsGamingIntegration.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEchobeatsUntrainedFeaturesTest,
    "DeepTreeEcho.Echobeats.UntrainedFeaturesAreNotMeasurements",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FEchobeatsUntrainedFeaturesTest::RunTest(const FString& Parameters)
{
    UEchobeatsGamingIntegration* Echobeats = NewObject<UEchobeatsGamingIntegration>();
    TestNotNull(TEXT("Echobeats should be created"), Echobeats);
    if (!Echobeats) { return false; }

    TestFalse(TEXT("Fresh feature weights are untrained"), Echobeats->AreFeatureWeightsTrained());
    Echobeats->MinLearningUpdatesForTrainedFeatures = 0;
    TestFalse(TEXT("A zero threshold does not make them trained"), Echobeats->AreFeatureWeightsTrained());

    // Untrained features match no pattern (and so broadcast none)
    TArray<float> Features;
    Features.Init(0.9f, 32);
    TestEqual(TEXT("No patterns from untrained features"), Echobeats->MatchPatterns(Features).Num(), 0);

    // No invented situation factors: nothing without evidence, only evidenced factors otherwise
    const TMap<FString, float> Empty = Echobeats->AssessSituation({});
    TestEqual(TEXT("No factors without evidence"), Empty.Num(), 0);
    const TMap<FString, float> Threat = Echobeats->AssessSituation({ TEXT("threat_flank") });
    TestTrue(TEXT("Evidenced threat is assessed"), Threat.Contains(TEXT("threat_level")));
    TestFalse(TEXT("Resource status is never defaulted"), Threat.Contains(TEXT("resource_status")));
    TestFalse(TEXT("Position advantage is never defaulted"), Threat.Contains(TEXT("position_advantage")));

    TestTrue(TEXT("No integrated decision from untrained weights"), Echobeats->GetIntegratedDecision().IsEmpty());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
