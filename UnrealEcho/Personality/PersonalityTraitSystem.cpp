#include "PersonalityTraitSystem.h"
#include "Kismet/KismetMathLibrary.h"

UPersonalityTraitSystem::UPersonalityTraitSystem()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    
    // Initialize configuration
    TraitDevelopmentSpeed = 0.1f;
    TraitInteractionStrength = 0.5f;
    
    // Initialize default traits
    FPersonalityTrait SuperHotGirlTraitData;
    SuperHotGirlTraitData.TraitType = EPersonalityTraitType::SuperHotGirl;
    SuperHotGirlTraitData.Intensity = 0.8f;
    SuperHotGirlTraitData.Stability = 0.9f;
    SuperHotGirlTraitData.DevelopmentRate = 0.05f;
    PersonalityTraits.Add(EPersonalityTraitType::SuperHotGirl, SuperHotGirlTraitData);
    
    FPersonalityTrait HyperChaoticTraitData;
    HyperChaoticTraitData.TraitType = EPersonalityTraitType::HyperChaotic;
    HyperChaoticTraitData.Intensity = 0.3f;
    HyperChaoticTraitData.Stability = 0.3f;
    HyperChaoticTraitData.DevelopmentRate = 0.2f;
    PersonalityTraits.Add(EPersonalityTraitType::HyperChaotic, HyperChaoticTraitData);
    
    FPersonalityTrait DeepTreeEchoTraitData;
    DeepTreeEchoTraitData.TraitType = EPersonalityTraitType::DeepTreeEcho;
    DeepTreeEchoTraitData.Intensity = 1.0f;
    DeepTreeEchoTraitData.Stability = 1.0f;
    DeepTreeEchoTraitData.DevelopmentRate = 0.01f;
    PersonalityTraits.Add(EPersonalityTraitType::DeepTreeEcho, DeepTreeEchoTraitData);
}

void UPersonalityTraitSystem::BeginPlay()
{
    Super::BeginPlay();
    
    UE_LOG(LogTemp, Log, TEXT("PersonalityTraitSystem initialized for %s"), 
           *GetOwner()->GetName());
}

void UPersonalityTraitSystem::TickComponent(float DeltaTime, ELevelTick TickType, 
                                             FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    // Trait values are persona data (model input). The tick only resolves deterministic
    // trait interactions; it never triggers expressions, gestures or emotional events.
    // The character's visible reactions come exclusively from model replies broadcast on
    // FEchoReactionBus.
    ProcessTraitInteractions(DeltaTime);
}

// ===== Trait Management =====

void UPersonalityTraitSystem::SetTraitIntensity(EPersonalityTraitType TraitType, float Intensity)
{
    float ClampedIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
    
    if (PersonalityTraits.Contains(TraitType))
    {
        PersonalityTraits[TraitType].Intensity = ClampedIntensity;
    }
    else
    {
        FPersonalityTrait NewTrait;
        NewTrait.TraitType = TraitType;
        NewTrait.Intensity = ClampedIntensity;
        PersonalityTraits.Add(TraitType, NewTrait);
    }
    
    UE_LOG(LogTemp, Log, TEXT("Trait %d set to intensity %.2f"), (int32)TraitType, ClampedIntensity);
}

float UPersonalityTraitSystem::GetTraitIntensity(EPersonalityTraitType TraitType) const
{
    if (PersonalityTraits.Contains(TraitType))
    {
        return PersonalityTraits[TraitType].Intensity;
    }
    return 0.0f;
}

void UPersonalityTraitSystem::ModifyTrait(EPersonalityTraitType TraitType, float DeltaIntensity)
{
    float CurrentIntensity = GetTraitIntensity(TraitType);
    SetTraitIntensity(TraitType, CurrentIntensity + DeltaIntensity);
}

void UPersonalityTraitSystem::DevelopTrait(EPersonalityTraitType TraitType, float DevelopmentAmount)
{
    if (PersonalityTraits.Contains(TraitType))
    {
        FPersonalityTrait& Trait = PersonalityTraits[TraitType];
        float Development = DevelopmentAmount * Trait.DevelopmentRate;
        Trait.Intensity = FMath::Clamp(Trait.Intensity + Development, 0.0f, 1.0f);
        
        UE_LOG(LogTemp, Log, TEXT("Trait %d developed by %.3f to %.2f"), 
               (int32)TraitType, Development, Trait.Intensity);
    }
}

// ===== Super Hot Girl Trait =====

void UPersonalityTraitSystem::ActivateSuperHotGirlTrait(float Intensity)
{
    SetTraitIntensity(EPersonalityTraitType::SuperHotGirl, Intensity);
    
    // Update sub-parameters based on intensity
    SuperHotGirlTrait.Confidence = 0.7f + (Intensity * 0.3f);
    SuperHotGirlTrait.Charm = 0.8f + (Intensity * 0.2f);
    SuperHotGirlTrait.Playfulness = 0.5f + (Intensity * 0.5f);
    SuperHotGirlTrait.Elegance = 0.6f + (Intensity * 0.4f);
    SuperHotGirlTrait.Expressiveness = 0.7f + (Intensity * 0.3f);
    
    UE_LOG(LogTemp, Log, TEXT("Super Hot Girl trait activated at intensity %.2f"), Intensity);
}

void UPersonalityTraitSystem::SetSuperHotGirlParameters(float Confidence, float Charm, float Playfulness)
{
    SuperHotGirlTrait.Confidence = FMath::Clamp(Confidence, 0.0f, 1.0f);
    SuperHotGirlTrait.Charm = FMath::Clamp(Charm, 0.0f, 1.0f);
    SuperHotGirlTrait.Playfulness = FMath::Clamp(Playfulness, 0.0f, 1.0f);
}

// ===== Hyper Chaotic Trait =====

void UPersonalityTraitSystem::ActivateHyperChaoticTrait(float Intensity)
{
    SetTraitIntensity(EPersonalityTraitType::HyperChaotic, Intensity);
    
    // Update sub-parameters based on intensity
    HyperChaoticTrait.UnpredictabilityFactor = Intensity;
    HyperChaoticTrait.EmotionalVolatility = 0.3f + (Intensity * 0.7f);
    HyperChaoticTrait.ImpulsivityLevel = 0.2f + (Intensity * 0.8f);
    HyperChaoticTrait.PatternBreaking = 0.4f + (Intensity * 0.6f);
    HyperChaoticTrait.CognitiveFluctuation = 0.2f + (Intensity * 0.8f);
    
    UE_LOG(LogTemp, Log, TEXT("Hyper Chaotic trait activated at intensity %.2f"), Intensity);
}

void UPersonalityTraitSystem::SetHyperChaoticParameters(float Unpredictability, float Volatility, float Impulsivity)
{
    HyperChaoticTrait.UnpredictabilityFactor = FMath::Clamp(Unpredictability, 0.0f, 1.0f);
    HyperChaoticTrait.EmotionalVolatility = FMath::Clamp(Volatility, 0.0f, 1.0f);
    HyperChaoticTrait.ImpulsivityLevel = FMath::Clamp(Impulsivity, 0.0f, 1.0f);
}

// ===== Trait Interactions =====

void UPersonalityTraitSystem::CalculateTraitInteractions()
{
    // Super Hot Girl + Hyper Chaotic = Unpredictably Charming
    float SuperHotIntensity = GetTraitIntensity(EPersonalityTraitType::SuperHotGirl);
    float ChaoticIntensity = GetTraitIntensity(EPersonalityTraitType::HyperChaotic);
    
    if (SuperHotIntensity > 0.5f && ChaoticIntensity > 0.3f)
    {
        // Increase playfulness and unpredictability
        SuperHotGirlTrait.Playfulness = FMath::Min(1.0f, SuperHotGirlTrait.Playfulness + (ChaoticIntensity * 0.2f));
        HyperChaoticTrait.MicroExpressionFrequency = FMath::Min(1.0f, HyperChaoticTrait.MicroExpressionFrequency + (SuperHotIntensity * 0.1f));
    }
    
    // Confident + Chaotic = Bold Unpredictability
    float ConfidentIntensity = GetTraitIntensity(EPersonalityTraitType::Confident);
    if (ConfidentIntensity > 0.6f && ChaoticIntensity > 0.3f)
    {
        HyperChaoticTrait.ImpulsivityLevel = FMath::Min(1.0f, HyperChaoticTrait.ImpulsivityLevel + (ConfidentIntensity * 0.15f));
    }
}

float UPersonalityTraitSystem::GetTraitSynergy(EPersonalityTraitType Trait1, EPersonalityTraitType Trait2) const
{
    float Intensity1 = GetTraitIntensity(Trait1);
    float Intensity2 = GetTraitIntensity(Trait2);
    
    // Define synergy relationships
    if ((Trait1 == EPersonalityTraitType::SuperHotGirl && Trait2 == EPersonalityTraitType::Confident) ||
        (Trait2 == EPersonalityTraitType::SuperHotGirl && Trait1 == EPersonalityTraitType::Confident))
    {
        return Intensity1 * Intensity2 * 1.5f; // Strong positive synergy
    }
    
    if ((Trait1 == EPersonalityTraitType::HyperChaotic && Trait2 == EPersonalityTraitType::Mysterious) ||
        (Trait2 == EPersonalityTraitType::HyperChaotic && Trait1 == EPersonalityTraitType::Mysterious))
    {
        return Intensity1 * Intensity2 * 1.3f; // Moderate positive synergy
    }
    
    return Intensity1 * Intensity2; // Neutral synergy
}

TArray<EPersonalityTraitType> UPersonalityTraitSystem::GetDominantTraits(int32 Count) const
{
    TArray<EPersonalityTraitType> DominantTraits;
    TArray<TPair<EPersonalityTraitType, float>> SortedTraits;
    
    // Collect all traits with their intensities
    for (const auto& TraitPair : PersonalityTraits)
    {
        SortedTraits.Add(TPair<EPersonalityTraitType, float>(TraitPair.Key, TraitPair.Value.Intensity));
    }
    
    // Sort by intensity
    SortedTraits.Sort([](const TPair<EPersonalityTraitType, float>& A, const TPair<EPersonalityTraitType, float>& B) {
        return A.Value > B.Value;
    });
    
    // Return top N traits
    for (int32 i = 0; i < FMath::Min(Count, SortedTraits.Num()); i++)
    {
        DominantTraits.Add(SortedTraits[i].Key);
    }
    
    return DominantTraits;
}

// ===== Behavioral Output =====

FVector2D UPersonalityTraitSystem::GetEmotionalTendency() const
{
    float Valence = 0.0f;
    float Arousal = 0.5f;
    
    // Super Hot Girl tends toward positive valence
    float SuperHotIntensity = GetTraitIntensity(EPersonalityTraitType::SuperHotGirl);
    Valence += SuperHotIntensity * 0.6f;
    Arousal += SuperHotIntensity * 0.3f;
    
    // Hyper Chaotic increases arousal (deterministic tendency; no random valence walk)
    float ChaoticIntensity = GetTraitIntensity(EPersonalityTraitType::HyperChaotic);
    Arousal += ChaoticIntensity * 0.4f;
    
    // Confident increases both
    float ConfidentIntensity = GetTraitIntensity(EPersonalityTraitType::Confident);
    Valence += ConfidentIntensity * 0.3f;
    Arousal += ConfidentIntensity * 0.2f;
    
    return FVector2D(FMath::Clamp(Valence, -1.0f, 1.0f), FMath::Clamp(Arousal, 0.0f, 1.0f));
}

float UPersonalityTraitSystem::GetBehavioralPredictability() const
{
    float Predictability = 1.0f;
    
    // Hyper Chaotic reduces predictability
    float ChaoticIntensity = GetTraitIntensity(EPersonalityTraitType::HyperChaotic);
    Predictability -= ChaoticIntensity * 0.7f;
    
    // Confident increases predictability slightly
    float ConfidentIntensity = GetTraitIntensity(EPersonalityTraitType::Confident);
    Predictability += ConfidentIntensity * 0.2f;
    
    return FMath::Clamp(Predictability, 0.0f, 1.0f);
}

float UPersonalityTraitSystem::GetSocialEngagementLevel() const
{
    float Engagement = 0.5f;
    
    // Super Hot Girl increases social engagement
    float SuperHotIntensity = GetTraitIntensity(EPersonalityTraitType::SuperHotGirl);
    Engagement += SuperHotIntensity * 0.4f;
    
    // Playful increases engagement
    float PlayfulIntensity = GetTraitIntensity(EPersonalityTraitType::Playful);
    Engagement += PlayfulIntensity * 0.3f;
    
    // Mysterious reduces engagement slightly
    float MysteriousIntensity = GetTraitIntensity(EPersonalityTraitType::Mysterious);
    Engagement -= MysteriousIntensity * 0.2f;
    
    return FMath::Clamp(Engagement, 0.0f, 1.0f);
}

// ===== Internal Methods =====

void UPersonalityTraitSystem::ProcessTraitInteractions(float DeltaTime)
{
    CalculateTraitInteractions();
}
