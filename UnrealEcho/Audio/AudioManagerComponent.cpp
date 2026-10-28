#include "Audio/AudioManagerComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWaveProcedural.h"
#include "Personality/PersonalityTraitSystem.h"
#include "EchoCharacterReaction.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "HAL/PlatformMisc.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogAvatarAudio, Log, All);

UAudioManagerComponent::UAudioManagerComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    
    // Initialize default values
    BasePitch = 1.0f;
    BaseSpeed = 1.0f;
    MusicFadeSpeed = 1.0f;
    TtsVoice = TEXT("alloy");
    TtsSampleRate = 24000;
    
    CurrentEmotion = EAvatarEmotionalState::Neutral;
    CurrentEmotionalIntensity = 0.5f;
    CurrentCognitiveLoad = 0.0f;
    CurrentChaosFactor = 0.0f;
}

void UAudioManagerComponent::BeginPlay()
{
    Super::BeginPlay();
    
    InitializeAudioComponents();
    ApplyTtsEnvironment();

    // Voice and reaction sound effects follow only model-produced reactions.
    ReactionHandle = FEchoReactionBus::OnReaction().AddUObject(this, &UAudioManagerComponent::HandleReaction);

    if (!IsTextToSpeechConfigured())
    {
        UE_LOG(LogAvatarAudio, Warning,
            TEXT("No TTS model configured (set ECHO_TTS_BASE_URL and ECHO_TTS_MODEL): utterances are captioned only, no voice audio is played"));
    }
}

void UAudioManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    FEchoReactionBus::OnReaction().Remove(ReactionHandle);
    ReactionHandle.Reset();
    Super::EndPlay(EndPlayReason);
}

void UAudioManagerComponent::ApplyTtsEnvironment()
{
    const FString EnvUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("ECHO_TTS_BASE_URL"));
    const FString EnvModel = FPlatformMisc::GetEnvironmentVariable(TEXT("ECHO_TTS_MODEL"));
    const FString EnvVoice = FPlatformMisc::GetEnvironmentVariable(TEXT("ECHO_TTS_VOICE"));
    FString EnvKey = FPlatformMisc::GetEnvironmentVariable(TEXT("ECHO_TTS_API_KEY"));
    if (EnvKey.IsEmpty())
    {
        EnvKey = FPlatformMisc::GetEnvironmentVariable(TEXT("ECHO_MODEL_API_KEY"));
    }
    if (!EnvUrl.IsEmpty()) TtsBaseUrl = EnvUrl;
    if (!EnvModel.IsEmpty()) TtsModel = EnvModel;
    if (!EnvVoice.IsEmpty()) TtsVoice = EnvVoice;
    if (!EnvKey.IsEmpty() && TtsApiKey.IsEmpty()) TtsApiKey = EnvKey;
    TtsBaseUrl.RemoveFromEnd(TEXT("/"));
}

bool UAudioManagerComponent::IsTextToSpeechConfigured() const
{
    return !TtsBaseUrl.IsEmpty() && !TtsModel.IsEmpty();
}

void UAudioManagerComponent::HandleReaction(const FEchoCharacterReaction& Reaction)
{
    if (!ReactionCharacterName.IsEmpty() && Reaction.CharacterName != ReactionCharacterName)
    {
        return;
    }

    if (!Reaction.Utterance.IsEmpty())
    {
        Speak(Reaction.Utterance, Reaction.Intensity);
    }

    // Sound effects keyed by the model-chosen gesture / expression (designer-assigned assets).
    if (!Reaction.Gesture.IsEmpty())
    {
        PlayGestureSound(FName(*Reaction.Gesture));
    }
    if (!Reaction.Expression.IsEmpty())
    {
        const UEnum* EmotionEnum = StaticEnum<EAvatarEmotionalState>();
        const int64 Value = EmotionEnum ? EmotionEnum->GetValueByNameString(Reaction.Expression) : INDEX_NONE;
        if (Value != INDEX_NONE)
        {
            PlayEmotionalSound(static_cast<EAvatarEmotionalState>(Value), Reaction.Intensity);
        }
    }
}

void UAudioManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    UpdateVoiceModulation(DeltaTime);
    UpdateMusicMixing(DeltaTime);
    UpdateAmbientMixing(DeltaTime);
}

void UAudioManagerComponent::InitializeAudioComponents()
{
    // Create voice audio component
    VoiceAudioComponent = NewObject<UAudioComponent>(this, TEXT("VoiceAudioComponent"));
    if (VoiceAudioComponent)
    {
        VoiceAudioComponent->RegisterComponent();
        VoiceAudioComponent->AttachToComponent(GetOwner()->GetRootComponent(), 
                                               FAttachmentTransformRules::KeepRelativeTransform);
        VoiceAudioComponent->bAutoActivate = false;
    }
    
    // Create music layer components (4 layers)
    for (int32 i = 0; i < 4; i++)
    {
        FString ComponentName = FString::Printf(TEXT("MusicLayer_%d"), i);
        UAudioComponent* MusicLayer = NewObject<UAudioComponent>(this, FName(*ComponentName));
        
        if (MusicLayer)
        {
            MusicLayer->RegisterComponent();
            MusicLayer->AttachToComponent(GetOwner()->GetRootComponent(), 
                                         FAttachmentTransformRules::KeepRelativeTransform);
            MusicLayer->bAutoActivate = false;
            MusicLayerComponents.Add(MusicLayer);
        }
    }
    
    // Create ambient audio component
    AmbienceComponent = NewObject<UAudioComponent>(this, TEXT("AmbienceComponent"));
    if (AmbienceComponent)
    {
        AmbienceComponent->RegisterComponent();
        AmbienceComponent->AttachToComponent(GetOwner()->GetRootComponent(), 
                                            FAttachmentTransformRules::KeepRelativeTransform);
        AmbienceComponent->bAutoActivate = false;
        
        if (CognitiveAmbience)
        {
            AmbienceComponent->SetSound(CognitiveAmbience);
            AmbienceComponent->Play();
        }
    }
}

// Voice System Implementation

void UAudioManagerComponent::Speak(const FString& Text, float EmotionalIntensity)
{
    if (Text.TrimStartAndEnd().IsEmpty())
    {
        return;
    }

    // Captions always carry the exact text being said.
    OnSpeechCaption.Broadcast(Text);

    if (!IsTextToSpeechConfigured())
    {
        UE_LOG(LogAvatarAudio, Verbose, TEXT("No TTS model configured; utterance captioned without voice audio"));
        return;
    }

    TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("model"), TtsModel);
    Body->SetStringField(TEXT("input"), Text);
    Body->SetStringField(TEXT("voice"), TtsVoice);
    Body->SetStringField(TEXT("response_format"), TEXT("pcm"));
    FString Payload;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
    FJsonSerializer::Serialize(Body, Writer);

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(TtsBaseUrl + TEXT("/audio/speech"));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    if (!TtsApiKey.IsEmpty())
    {
        Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + TtsApiKey);
    }
    Request->SetContentAsString(Payload);

    TWeakObjectPtr<UAudioManagerComponent> WeakThis(this);
    Request->OnProcessRequestComplete().BindLambda(
        [WeakThis, EmotionalIntensity](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
        {
            UAudioManagerComponent* Self = WeakThis.Get();
            if (!Self)
            {
                return;
            }
            if (!bConnected || !Response.IsValid())
            {
                UE_LOG(LogAvatarAudio, Error, TEXT("TTS request failed: no response; utterance not voiced"));
                return;
            }
            const int32 Code = Response->GetResponseCode();
            if (Code < 200 || Code >= 300)
            {
                UE_LOG(LogAvatarAudio, Error, TEXT("TTS request failed: HTTP %d: %s; utterance not voiced"),
                    Code, *Response->GetContentAsString().Left(256));
                return;
            }
            Self->PlaySynthesizedSpeech(Response->GetContent(), EmotionalIntensity);
        });

    if (!Request->ProcessRequest())
    {
        UE_LOG(LogAvatarAudio, Error, TEXT("TTS request could not be started; utterance not voiced"));
    }
}

void UAudioManagerComponent::PlaySynthesizedSpeech(const TArray<uint8>& Pcm16, float EmotionalIntensity)
{
    if (!VoiceAudioComponent || Pcm16.Num() < 2)
    {
        UE_LOG(LogAvatarAudio, Error, TEXT("TTS returned no audio; utterance not voiced"));
        return;
    }

    USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this);
    Wave->SetSampleRate(static_cast<uint32>(TtsSampleRate));
    Wave->NumChannels = 1;
    Wave->bLooping = false;
    Wave->QueueAudio(Pcm16.GetData(), Pcm16.Num() - (Pcm16.Num() % 2));

    // Prosody follows the model-chosen intensity.
    VoiceAudioComponent->SetPitchMultiplier(BasePitch * FMath::Lerp(0.95f, 1.05f, EmotionalIntensity));
    VoiceAudioComponent->SetSound(Wave);
    VoiceAudioComponent->Play();
}

void UAudioManagerComponent::SetVoicePitch(float Pitch)
{
    BasePitch = FMath::Clamp(Pitch, 0.5f, 2.0f);
    
    if (VoiceAudioComponent)
    {
        VoiceAudioComponent->SetPitchMultiplier(BasePitch);
    }
}

void UAudioManagerComponent::SetVoiceSpeed(float Speed)
{
    BaseSpeed = FMath::Clamp(Speed, 0.5f, 2.0f);
}

void UAudioManagerComponent::UpdateVoiceModulation(float DeltaTime)
{
    if (!VoiceAudioComponent || !VoiceAudioComponent->IsPlaying())
    {
        return;
    }
    
    // Get personality system for confidence
    UPersonalityTraitSystem* PersonalitySystem = GetOwner()->FindComponentByClass<UPersonalityTraitSystem>();
    const float Confidence = PersonalitySystem ? PersonalitySystem->GetSuperHotGirlTrait().Confidence : 0.7f;
    
    // Modulate pitch based on current emotion
    float TargetPitch = BasePitch;
    switch (CurrentEmotion)
    {
        case EAvatarEmotionalState::Happy:
            TargetPitch = BasePitch * 1.1f;
            break;
        case EAvatarEmotionalState::Sad:
            TargetPitch = BasePitch * 0.9f;
            break;
        case EAvatarEmotionalState::Excited:
            TargetPitch = BasePitch * 1.15f;
            break;
        case EAvatarEmotionalState::Angry:
            TargetPitch = BasePitch * 0.95f;
            break;
        default:
            TargetPitch = BasePitch;
    }
    
    // Smooth interpolation
    float CurrentPitch = VoiceAudioComponent->GetPitchMultiplier();
    float NewPitch = FMath::FInterpTo(CurrentPitch, TargetPitch, DeltaTime, 2.0f);
    VoiceAudioComponent->SetPitchMultiplier(NewPitch);
    
    // Modulate volume based on confidence
    float TargetVolume = FMath::Lerp(0.7f, 1.0f, Confidence);
    float CurrentVolume = VoiceAudioComponent->GetVolumeMultiplier();
    float NewVolume = FMath::FInterpTo(CurrentVolume, TargetVolume, DeltaTime, 1.0f);
    VoiceAudioComponent->SetVolumeMultiplier(NewVolume);
}

// Sound Effects System Implementation

void UAudioManagerComponent::PlayGestureSound(FName GestureName)
{
    USoundBase** Sound = GestureSounds.Find(GestureName);
    if (Sound && *Sound)
    {
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), *Sound, GetOwner()->GetActorLocation());
    }
}

void UAudioManagerComponent::PlayEmotionalSound(EAvatarEmotionalState Emotion, float Intensity)
{
    USoundBase** Sound = EmotionalSounds.Find(Emotion);
    if (Sound && *Sound)
    {
        UAudioComponent* EmotionAudio = UGameplayStatics::SpawnSoundAttached(
            *Sound,
            GetOwner()->GetRootComponent(),
            NAME_None,
            FVector::ZeroVector,
            EAttachLocation::KeepRelativeOffset,
            false
        );
        
        if (EmotionAudio)
        {
            EmotionAudio->SetVolumeMultiplier(Intensity * 0.5f);  // Subtle
            EmotionAudio->Play();
        }
    }
}

void UAudioManagerComponent::PlayCognitiveSound(float CognitiveLoad)
{
    if (CognitiveProcessingSound)
    {
        UAudioComponent* CogAudio = UGameplayStatics::SpawnSoundAttached(
            CognitiveProcessingSound,
            GetOwner()->GetRootComponent(),
            TEXT("Head"),  // Attach to head bone
            FVector::ZeroVector,
            EAttachLocation::SnapToTarget,
            false
        );
        
        if (CogAudio)
        {
            // Modulate pitch based on cognitive load
            CogAudio->SetPitchMultiplier(0.8f + CognitiveLoad * 0.4f);
            CogAudio->SetVolumeMultiplier(CognitiveLoad * 0.3f);
            CogAudio->Play();
        }
    }
}

// Music System Implementation

void UAudioManagerComponent::UpdateMusicLayers(EAvatarEmotionalState Emotion, float Intensity)
{
    if (MusicLayerComponents.Num() < 4)
    {
        return;
    }
    
    CurrentEmotion = Emotion;
    CurrentEmotionalIntensity = Intensity;
    
    // Layer 0: Base (always at full volume)
    if (MusicLayerComponents[0])
    {
        MusicLayerComponents[0]->SetVolumeMultiplier(1.0f);
        if (!MusicLayerComponents[0]->IsPlaying())
        {
            MusicLayerComponents[0]->Play();
        }
    }
    
    // Layer 1: Emotional theme
    USoundBase** EmotionMusic = MusicThemes.Find(Emotion);
    if (EmotionMusic && *EmotionMusic && MusicLayerComponents[1])
    {
        if (MusicLayerComponents[1]->GetSound() != *EmotionMusic)
        {
            MusicLayerComponents[1]->SetSound(*EmotionMusic);
            MusicLayerComponents[1]->Play();
        }
        MusicLayerComponents[1]->SetVolumeMultiplier(Intensity);
    }
    
    // Layer 2: Intensity layer (high energy)
    if (MusicLayerComponents[2])
    {
        float IntensityVolume = FMath::Max(0.0f, Intensity - 0.5f) * 2.0f;
        MusicLayerComponents[2]->SetVolumeMultiplier(IntensityVolume);
        
        if (IntensityVolume > 0.1f && !MusicLayerComponents[2]->IsPlaying())
        {
            MusicLayerComponents[2]->Play();
        }
    }
    
    // Layer 3: Chaos layer
    if (MusicLayerComponents[3])
    {
        float ChaosVolume = CurrentChaosFactor * 0.7f;
        MusicLayerComponents[3]->SetVolumeMultiplier(ChaosVolume);
        
        if (ChaosVolume > 0.1f && !MusicLayerComponents[3]->IsPlaying())
        {
            MusicLayerComponents[3]->Play();
        }
    }
}

void UAudioManagerComponent::SetMusicIntensity(float Intensity)
{
    CurrentEmotionalIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
    UpdateMusicLayers(CurrentEmotion, CurrentEmotionalIntensity);
}

void UAudioManagerComponent::UpdateMusicMixing(float DeltaTime)
{
    // Smooth volume transitions for all music layers
    for (UAudioComponent* Layer : MusicLayerComponents)
    {
        if (Layer)
        {
            // Volume transitions are handled in UpdateMusicLayers
            // This function could add additional smoothing if needed
        }
    }
}

// Ambient System Implementation

void UAudioManagerComponent::UpdateAmbientSound(float CognitiveLoad, float ChaosFactor)
{
    CurrentCognitiveLoad = FMath::Clamp(CognitiveLoad, 0.0f, 1.0f);
    CurrentChaosFactor = FMath::Clamp(ChaosFactor, 0.0f, 1.0f);
    
    if (!AmbienceComponent)
    {
        return;
    }
    
    // Blend between calm and intense ambience based on cognitive load
    float BaseVolume = 0.3f + CurrentCognitiveLoad * 0.4f;
    AmbienceComponent->SetVolumeMultiplier(BaseVolume);
    
    // Modulate pitch based on chaos factor
    float BasePitchValue = 1.0f + CurrentChaosFactor * 0.2f;
    AmbienceComponent->SetPitchMultiplier(BasePitchValue);
    
    // Ensure ambience is playing
    if (!AmbienceComponent->IsPlaying())
    {
        AmbienceComponent->Play();
    }
}

void UAudioManagerComponent::UpdateAmbientMixing(float DeltaTime)
{
    // Additional ambient mixing logic
    // Could add spatial audio effects, reverb modulation, etc.
}

void UAudioManagerComponent::PlayEchoResonanceSound(float Intensity)
{
    if (EchoResonanceSound)
    {
        UAudioComponent* EchoAudio = UGameplayStatics::SpawnSoundAttached(
            EchoResonanceSound,
            GetOwner()->GetRootComponent(),
            NAME_None,
            FVector::ZeroVector,
            EAttachLocation::KeepRelativeOffset,
            false
        );
        
        if (EchoAudio)
        {
            EchoAudio->SetVolumeMultiplier(Intensity * 0.6f);
            EchoAudio->SetPitchMultiplier(0.9f + Intensity * 0.2f);
            EchoAudio->Play();
        }
    }
}
