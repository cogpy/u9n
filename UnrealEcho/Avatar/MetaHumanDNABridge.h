// MetaHumanDNABridge.h
// Deep Tree Echo - MetaHuman DNA Integration
// Copyright (c) 2025 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "../Neurochemical/NeurochemicalSimulationComponent.h"
#include "../Cognitive/EmotionalState.h"
#include "MetaHumanDNABridge.generated.h"

// Forward declarations
class FPythonDNACalibWrapper;

/**
 * How one facial expression label is rendered: blend shape weights at full intensity.
 * Rendering only; the label itself is chosen by the model.
 */
USTRUCT(BlueprintType)
struct FDNAExpressionRender
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DNA")
    TMap<FString, float> BlendShapeWeights;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDNAFacialExpressionChosen, const FString&, Expression, float, Intensity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDNAFacialExpressionFailed, const FString&, Error);

/**
 * DNA Version Enum
 * Represents different MetaHuman DNA rig definitions
 */
UENUM(BlueprintType)
enum class EDNAVersion : uint8
{
    Unknown     UMETA(DisplayName = "Unknown"),
    DHI         UMETA(DisplayName = "DHI (Legacy)"),
    MH4         UMETA(DisplayName = "MH.4 (2023+)")
};

/**
 * DNA Joint Information
 * Stores joint metadata from DNA file
 */
USTRUCT(BlueprintType)
struct FDNAJointInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DNA")
    FString JointName;

    UPROPERTY(BlueprintReadOnly, Category = "DNA")
    int32 JointIndex;

    UPROPERTY(BlueprintReadOnly, Category = "DNA")
    FTransform NeutralTransform;

    UPROPERTY(BlueprintReadOnly, Category = "DNA")
    int32 ParentIndex;

    FDNAJointInfo()
        : JointIndex(-1)
        , NeutralTransform(FTransform::Identity)
        , ParentIndex(-1)
    {}
};

/**
 * DNA Blend Shape Information
 * Stores blend shape metadata from DNA file
 */
USTRUCT(BlueprintType)
struct FDNABlendShapeInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DNA")
    FString BlendShapeName;

    UPROPERTY(BlueprintReadOnly, Category = "DNA")
    int32 BlendShapeIndex;

    UPROPERTY(BlueprintReadOnly, Category = "DNA")
    FString TargetMeshName;

    UPROPERTY(BlueprintReadOnly, Category = "DNA")
    int32 VertexCount;

    FDNABlendShapeInfo()
        : BlendShapeIndex(-1)
        , VertexCount(0)
    {}
};

/**
 * DNA Mesh Information
 * Stores mesh metadata from DNA file
 */
USTRUCT(BlueprintType)
struct FDNAMeshInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DNA")
    FString MeshName;

    UPROPERTY(BlueprintReadOnly, Category = "DNA")
    int32 MeshIndex;

    UPROPERTY(BlueprintReadOnly, Category = "DNA")
    int32 VertexCount;

    UPROPERTY(BlueprintReadOnly, Category = "DNA")
    TArray<FString> BlendShapeNames;

    FDNAMeshInfo()
        : MeshIndex(-1)
        , VertexCount(0)
    {}
};

/**
 * UMetaHumanDNABridge
 * 
 * Bridge between MetaHuman DNA Calibration tools and Deep Tree Echo avatar system.
 * Provides DNA file loading, manipulation, and integration with Unreal Engine skeletal meshes.
 * 
 * Key Features:
 * - Load and parse DNA files using DNACalib Python API
 * - Extract facial rig structure and blend shape data
 * - Map DNA joints to Unreal Engine skeleton
 * - Manipulate DNA files (rename, remove, modify)
 * - Sync neurochemical and emotional states to facial expressions
 * 
 * Integration Points:
 * - Avatar3DComponentEnhanced: Real-time facial animation
 * - NeurochemicalSimulationComponent: Neurochemical to blend shape mapping
 * - PersonalityTraitSystem: Personality influence on facial behavior
 */
UCLASS(BlueprintType, Blueprintable)
class UNREALECHO_API UMetaHumanDNABridge : public UObject
{
    GENERATED_BODY()

public:
    UMetaHumanDNABridge();
    virtual ~UMetaHumanDNABridge();

    // ========================================
    // DNA File Operations
    // ========================================

    /**
     * Load DNA file from disk
     * @param DNAPath - Absolute path to .dna file
     * @return True if successfully loaded
     */
    UFUNCTION(BlueprintCallable, Category = "DNA|File Operations")
    bool LoadDNAFile(const FString& DNAPath);

    /**
     * Save DNA file to disk
     * @param DNAPath - Absolute path to save .dna file
     * @return True if successfully saved
     */
    UFUNCTION(BlueprintCallable, Category = "DNA|File Operations")
    bool SaveDNAFile(const FString& DNAPath);

    /**
     * Get DNA version (DHI or MH.4)
     * @return DNA version enum
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "DNA|File Operations")
    EDNAVersion GetDNAVersion() const { return DNAVersion; }

    /**
     * Get DNA version as string
     * @return "DHI" or "MH.4"
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "DNA|File Operations")
    FString GetDNAVersionString() const;

    /**
     * Check if DNA file is loaded
     * @return True if DNA is loaded
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "DNA|File Operations")
    bool IsDNALoaded() const { return bDNALoaded; }

    /**
     * Get loaded DNA file path
     * @return Path to loaded DNA file
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "DNA|File Operations")
    FString GetLoadedDNAPath() const { return LoadedDNAPath; }

    // ========================================
    // Rig Structure Access
    // ========================================

    /**
     * Get all joint names from DNA
     * @return Array of joint names
     */
    UFUNCTION(BlueprintCallable, Category = "DNA|Rig Structure")
    TArray<FString> GetJointNames() const;

    /**
     * Get all mesh names from DNA
     * @return Array of mesh names
     */
    UFUNCTION(BlueprintCallable, Category = "DNA|Rig Structure")
    TArray<FString> GetMeshNames() const;

    /**
     * Get all blend shape names from DNA
     * @return Array of blend shape names
     */
    UFUNCTION(BlueprintCallable, Category = "DNA|Rig Structure")
    TArray<FString> GetBlendShapeNames() const;

    /**
     * Get joint information by name
     * @param JointName - Name of joint
     * @param OutJointInfo - Output joint information
     * @return True if joint found
     */
    UFUNCTION(BlueprintCallable, Category = "DNA|Rig Structure")
    bool GetJointInfo(const FString& JointName, FDNAJointInfo& OutJointInfo) const;

    /**
     * Get blend shape information by name
     * @param BlendShapeName - Name of blend shape
     * @param OutBlendShapeInfo - Output blend shape information
     * @return True if blend shape found
     */
    UFUNCTION(BlueprintCallable, Category = "DNA|Rig Structure")
    bool GetBlendShapeInfo(const FString& BlendShapeName, FDNABlendShapeInfo& OutBlendShapeInfo) const;

    /**
     * Get mesh information by name
     * @param MeshName - Name of mesh
     * @param OutMeshInfo - Output mesh information
     * @return True if mesh found
     */
    UFUNCTION(BlueprintCallable, Category = "DNA|Rig Structure")
    bool GetMeshInfo(const FString& MeshName, FDNAMeshInfo& OutMeshInfo) const;

    /**
     * Get neutral joint transform
     * @param JointName - Name of joint
     * @return Neutral transform of joint
     */
    UFUNCTION(BlueprintCallable, Category = "DNA|Rig Structure")
    FTransform GetNeutralJointTransform(const FString& JointName) const;

    // ========================================
    // DNA Manipulation
    // ========================================

    /**
     * Rename joint in DNA
     * @param OldName - Current joint name
     * @param NewName - New joint name
     * @return True if successfully renamed
     */
    UFUNCTION(BlueprintCallable, Category = "DNA|Manipulation")
    bool RenameJoint(const FString& OldName, const FString& NewName);

    /**
     * Remove joint from DNA
     * @param JointName - Name of joint to remove
     * @return True if successfully removed
     */
    UFUNCTION(BlueprintCallable, Category = "DNA|Manipulation")
    bool RemoveJoint(const FString& JointName);

    /**
     * Rename blend shape in DNA
     * @param OldName - Current blend shape name
     * @param NewName - New blend shape name
     * @return True if successfully renamed
     */
    UFUNCTION(BlueprintCallable, Category = "DNA|Manipulation")
    bool RenameBlendShape(const FString& OldName, const FString& NewName);

    /**
     * Remove blend shape from DNA
     * @param BlendShapeName - Name of blend shape to remove
     * @return True if successfully removed
     */
    UFUNCTION(BlueprintCallable, Category = "DNA|Manipulation")
    bool RemoveBlendShape(const FString& BlendShapeName);

    /**
     * Modify blend shape deltas
     * @param BlendShapeName - Name of blend shape
     * @param Deltas - Array of vertex deltas
     * @return True if successfully modified
     */
    UFUNCTION(BlueprintCallable, Category = "DNA|Manipulation")
    bool ModifyBlendShapeDeltas(const FString& BlendShapeName, const TArray<FVector>& Deltas);

    /**
     * Clear all blend shape data from DNA
     * @return True if successfully cleared
     */
    UFUNCTION(BlueprintCallable, Category = "DNA|Manipulation")
    bool ClearAllBlendShapes();

    /**
     * Remove LOD from DNA
     * @param LODIndex - Index of LOD to remove
     * @return True if successfully removed
     */
    UFUNCTION(BlueprintCallable, Category = "DNA|Manipulation")
    bool RemoveLOD(int32 LODIndex);

    // ========================================
    // Integration with Avatar System
    // ========================================

    /**
     * Apply DNA to skeletal mesh
     * Maps DNA joints to Unreal Engine skeleton
     * @param SkeletalMesh - Target skeletal mesh component
     * @return True if successfully applied
     */
    UFUNCTION(BlueprintCallable, Category = "DNA|Integration")
    bool ApplyDNAToSkeleton(USkeletalMeshComponent* SkeletalMesh);

    /**
     * Sync neurochemical state to blend shapes. Asks the model to choose the facial expression
     * for State (RequestFacialExpressionFromNeurochemicals) and outputs the blend shape weights
     * rendered from the model's last chosen expression - empty until the model has answered.
     * @param State - Current neurochemical state (model input)
     * @param OutBlendShapeWeights - Weights rendered from the model's expression (0-1), or empty
     */
    UFUNCTION(BlueprintCallable, Category = "DNA|Integration")
    void SyncNeurochemicalStateToBlendShapes(const FNeurochemicalState& State, TMap<FString, float>& OutBlendShapeWeights);

    /**
     * Ask the model to choose the facial expression (a key of ExpressionRenderMap) and its
     * intensity [0,1] given the neurochemical levels (FacialExpressionPromptTemplate). On a valid
     * reply the expression is rendered through ExpressionRenderMap and OnFacialExpressionChosen
     * fires. Failures: Error log, GetLastFacialExpressionError, OnFacialExpressionFailed, no change.
     * Returns false when nothing was requested.
     */
    UFUNCTION(BlueprintCallable, Category = "DNA|Integration")
    bool RequestFacialExpressionFromNeurochemicals(const FNeurochemicalState& State);

    /** The model's last chosen facial expression, or empty */
    UFUNCTION(BlueprintPure, Category = "DNA|Integration")
    FString GetModelFacialExpression() const { return ModelFacialExpression; }

    /** Intensity of the model's last chosen facial expression (0 if none) */
    UFUNCTION(BlueprintPure, Category = "DNA|Integration")
    float GetModelFacialExpressionIntensity() const { return ModelFacialExpressionIntensity; }

    /** Blend shape weights rendered from the model's last expression, or empty */
    UFUNCTION(BlueprintPure, Category = "DNA|Integration")
    TMap<FString, float> GetModelExpressionBlendShapeWeights() const { return ModelExpressionWeights; }

    /** Why the last facial expression request failed, or empty */
    UFUNCTION(BlueprintPure, Category = "DNA|Integration")
    FString GetLastFacialExpressionError() const { return LastFacialExpressionError; }

    /** Expression labels the model may choose (the keys) and how each is rendered */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DNA|Model")
    TMap<FString, FDNAExpressionRender> ExpressionRenderMap;

    /**
     * Instructions to the model for choosing the facial expression. Slots: {neurochemicals},
     * {current}, {expressions}.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DNA|Model", meta = (MultiLine = true))
    FString FacialExpressionPromptTemplate;

    /** The model chose the facial expression */
    UPROPERTY(BlueprintAssignable, Category = "DNA|Events")
    FOnDNAFacialExpressionChosen OnFacialExpressionChosen;

    /** A facial expression request failed; nothing changed */
    UPROPERTY(BlueprintAssignable, Category = "DNA|Events")
    FOnDNAFacialExpressionFailed OnFacialExpressionFailed;

    /**
     * Sync emotional state to facial rig
     * Maps emotional state to facial expression blend shapes
     * @param Emotion - Current emotional state
     * @param OutBlendShapeWeights - Output blend shape weights (0-1)
     */
    UFUNCTION(BlueprintCallable, Category = "DNA|Integration")
    void SyncEmotionalStateToFacialRig(const FEmotionalState& Emotion, TMap<FString, float>& OutBlendShapeWeights);

    /**
     * Get blend shape weight for specific blend shape
     * @param BlendShapeName - Name of blend shape
     * @return Current weight (0-1)
     */
    UFUNCTION(BlueprintCallable, Category = "DNA|Integration")
    float GetBlendShapeWeight(const FString& BlendShapeName) const;

    /**
     * Set blend shape weight for specific blend shape
     * @param BlendShapeName - Name of blend shape
     * @param Weight - Target weight (0-1)
     */
    UFUNCTION(BlueprintCallable, Category = "DNA|Integration")
    void SetBlendShapeWeight(const FString& BlendShapeName, float Weight);

    /**
     * Get all current blend shape weights
     * @return Map of blend shape names to weights
     */
    UFUNCTION(BlueprintCallable, Category = "DNA|Integration")
    TMap<FString, float> GetAllBlendShapeWeights() const;

    /**
     * Set all blend shape weights
     * @param Weights - Map of blend shape names to weights
     */
    UFUNCTION(BlueprintCallable, Category = "DNA|Integration")
    void SetAllBlendShapeWeights(const TMap<FString, float>& Weights);

    // ========================================
    // Advanced Features
    // ========================================

    /**
     * Initialize Python DNACalib wrapper
     * @param DNACalibPath - Path to MetaHuman-DNA-Calibration directory
     * @return True if successfully initialized
     */
    UFUNCTION(BlueprintCallable, Category = "DNA|Advanced")
    bool InitializePythonWrapper(const FString& DNACalibPath);

    /**
     * Shutdown Python DNACalib wrapper
     */
    UFUNCTION(BlueprintCallable, Category = "DNA|Advanced")
    void ShutdownPythonWrapper();

    /**
     * Get DNA statistics
     * @param OutJointCount - Number of joints
     * @param OutMeshCount - Number of meshes
     * @param OutBlendShapeCount - Number of blend shapes
     */
    UFUNCTION(BlueprintCallable, Category = "DNA|Advanced")
    void GetDNAStatistics(int32& OutJointCount, int32& OutMeshCount, int32& OutBlendShapeCount) const;

    /**
     * Validate DNA integrity
     * @return True if DNA is valid
     */
    UFUNCTION(BlueprintCallable, Category = "DNA|Advanced")
    bool ValidateDNAIntegrity() const;

protected:
    // DNA file state
    UPROPERTY()
    bool bDNALoaded;

    UPROPERTY()
    FString LoadedDNAPath;

    UPROPERTY()
    EDNAVersion DNAVersion;

    // Cached DNA data
    UPROPERTY()
    TArray<FDNAJointInfo> JointInfoCache;

    UPROPERTY()
    TArray<FDNABlendShapeInfo> BlendShapeInfoCache;

    UPROPERTY()
    TArray<FDNAMeshInfo> MeshInfoCache;

    // Joint and blend shape mappings
    TMap<FString, int32> JointNameToIndexMap;
    TMap<FString, int32> BlendShapeNameToIndexMap;
    TMap<FString, int32> MeshNameToIndexMap;

    // Current blend shape weights
    TMap<FString, float> CurrentBlendShapeWeights;

    // Python DNACalib wrapper
    TSharedPtr<FPythonDNACalibWrapper> PythonWrapper;

    // Helper functions
    void BuildCacheMaps();
    void ClearCache();
    bool LoadDNAData();
    bool ParseDNAVersion();
    void InitializeBlendShapeWeights();

    // The model's facial expression and its rendering
    FString ModelFacialExpression;
    float ModelFacialExpressionIntensity = 0.0f;
    TMap<FString, float> ModelExpressionWeights;
    FString LastFacialExpressionError;
    int32 FacialExpressionRequestSerial = 0;

    void HandleFacialExpressionReply(int32 Serial, bool bSuccess, const FString& TextOrError);
    void FailFacialExpressionRequest(const FString& Error);

    // Emotional state (dimensional values) to blend shape rendering
    void MapEmotionalStateToBlendShapes(const FEmotionalState& Emotion, TMap<FString, float>& OutWeights);

    // Blend shape name mappings for different DNA versions
    TMap<FString, FString> GetDHIBlendShapeNames() const;
    TMap<FString, FString> GetMH4BlendShapeNames() const;
};
