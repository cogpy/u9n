#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sound/SoundBase.h"
#include "AudioManagerComponent.generated.h"

// Forward declarations
class UAudioComponent;
struct FEchoCharacterReaction;

/** Text the avatar is saying (for captions/subtitles). Only model-produced utterances reach Speak(). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAvatarSpeechCaption, const FString&, Text);

UENUM(BlueprintType)
enum class EAvatarEmotionalState : uint8
{
    Neutral UMETA(DisplayName = "Neutral"),
    Happy UMETA(DisplayName = "Happy"),
    Sad UMETA(DisplayName = "Sad"),
    Excited UMETA(DisplayName = "Excited"),
    Angry UMETA(DisplayName = "Angry"),
    Flirty UMETA(DisplayName = "Flirty"),
    Surprised UMETA(DisplayName = "Surprised")
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALENGINE_API UAudioManagerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAudioManagerComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Voice System

    /**
     * Voices Text. Text must be the character's model-produced utterance (the component itself
     * speaks only utterances arriving on FEchoReactionBus). The text is always forwarded to
     * OnSpeechCaption; audio is played only when a TTS model synthesises this exact text
     * (OpenAI-compatible POST {TtsBaseUrl}/audio/speech, raw 16-bit mono PCM). Without a TTS
     * model no voice audio is played.
     */
    UFUNCTION(BlueprintCallable, Category = "Audio|Voice")
    void Speak(const FString& Text, float EmotionalIntensity = 0.5f);

    /** True when a TTS endpoint and model are configured. */
    UFUNCTION(BlueprintPure, Category = "Audio|Voice")
    bool IsTextToSpeechConfigured() const;

    UPROPERTY(BlueprintAssignable, Category = "Audio|Voice")
    FOnAvatarSpeechCaption OnSpeechCaption;

    UFUNCTION(BlueprintCallable, Category = "Audio|Voice")
    void SetVoicePitch(float Pitch);

    UFUNCTION(BlueprintCallable, Category = "Audio|Voice")
    void SetVoiceSpeed(float Speed);

    // Sound Effects
    UFUNCTION(BlueprintCallable, Category = "Audio|SFX")
    void PlayGestureSound(FName GestureName);

    UFUNCTION(BlueprintCallable, Category = "Audio|SFX")
    void PlayEmotionalSound(EAvatarEmotionalState Emotion, float Intensity);

    UFUNCTION(BlueprintCallable, Category = "Audio|SFX")
    void PlayCognitiveSound(float CognitiveLoad);

    UFUNCTION(BlueprintCallable, Category = "Audio|SFX")
    void PlayEchoResonanceSound(float Intensity);

    // Music System
    UFUNCTION(BlueprintCallable, Category = "Audio|Music")
    void UpdateMusicLayers(EAvatarEmotionalState Emotion, float Intensity);

    UFUNCTION(BlueprintCallable, Category = "Audio|Music")
    void SetMusicIntensity(float Intensity);

    // Ambient System
    UFUNCTION(BlueprintCallable, Category = "Audio|Ambient")
    void UpdateAmbientSound(float CognitiveLoad, float ChaosFactor);

protected:
    // Voice System Properties

    /** TTS endpoint base URL (e.g. https://api.openai.com/v1); ECHO_TTS_BASE_URL overrides it. */
    UPROPERTY(EditAnywhere, Category = "Audio|Voice|TTS")
    FString TtsBaseUrl;

    /** TTS model name; ECHO_TTS_MODEL overrides it. */
    UPROPERTY(EditAnywhere, Category = "Audio|Voice|TTS")
    FString TtsModel;

    /** TTS voice name; ECHO_TTS_VOICE overrides it. */
    UPROPERTY(EditAnywhere, Category = "Audio|Voice|TTS")
    FString TtsVoice;

    /** Bearer token; ECHO_TTS_API_KEY overrides it (falls back to ECHO_MODEL_API_KEY). */
    UPROPERTY(EditAnywhere, Category = "Audio|Voice|TTS")
    FString TtsApiKey;

    /** Sample rate of the PCM the endpoint returns (OpenAI "pcm" is 24 kHz). */
    UPROPERTY(EditAnywhere, Category = "Audio|Voice|TTS", meta = (ClampMin = "8000"))
    int32 TtsSampleRate;

    /** Only reactions from this character are voiced; empty = every reaction on the bus. */
    UPROPERTY(EditAnywhere, Category = "Audio|Reaction")
    FString ReactionCharacterName;

    UPROPERTY()
    UAudioComponent* VoiceAudioComponent;

    UPROPERTY(EditAnywhere, Category = "Audio|Voice", meta = (ClampMin = "0.5", ClampMax = "2.0"))
    float BasePitch;

    UPROPERTY(EditAnywhere, Category = "Audio|Voice", meta = (ClampMin = "0.5", ClampMax = "2.0"))
    float BaseSpeed;

    // Sound Effect Libraries
    UPROPERTY(EditAnywhere, Category = "Audio|SFX")
    TMap<FName, USoundBase*> GestureSounds;

    UPROPERTY(EditAnywhere, Category = "Audio|SFX")
    TMap<EAvatarEmotionalState, USoundBase*> EmotionalSounds;

    UPROPERTY(EditAnywhere, Category = "Audio|SFX")
    USoundBase* CognitiveProcessingSound;

    UPROPERTY(EditAnywhere, Category = "Audio|SFX")
    USoundBase* EchoResonanceSound;

    // Music System Properties
    UPROPERTY(EditAnywhere, Category = "Audio|Music")
    TMap<EAvatarEmotionalState, USoundBase*> MusicThemes;

    UPROPERTY()
    TArray<UAudioComponent*> MusicLayerComponents;

    UPROPERTY(EditAnywhere, Category = "Audio|Music", meta = (ClampMin = "0.1", ClampMax = "5.0"))
    float MusicFadeSpeed;

    // Ambient System Properties
    UPROPERTY(EditAnywhere, Category = "Audio|Ambient")
    USoundBase* CognitiveAmbience;

    UPROPERTY(EditAnywhere, Category = "Audio|Ambient")
    USoundBase* ChaosAmbience;

    UPROPERTY()
    UAudioComponent* AmbienceComponent;

private:
    void InitializeAudioComponents();
    void ApplyTtsEnvironment();
    void HandleReaction(const FEchoCharacterReaction& Reaction);
    void PlaySynthesizedSpeech(const TArray<uint8>& Pcm16, float EmotionalIntensity);

    FDelegateHandle ReactionHandle;
    void UpdateVoiceModulation(float DeltaTime);
    void UpdateMusicMixing(float DeltaTime);
    void UpdateAmbientMixing(float DeltaTime);

    // Current state
    EAvatarEmotionalState CurrentEmotion;
    float CurrentEmotionalIntensity;
    float CurrentCognitiveLoad;
    float CurrentChaosFactor;
};
