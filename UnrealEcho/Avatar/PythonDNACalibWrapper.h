// PythonDNACalibWrapper.h
// Deep Tree Echo - Python DNACalib Integration
// Copyright (c) 2025 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"

/**
 * FPythonDNACalibWrapper
 *
 * C++ interface to the MetaHuman DNA Calibration Python bindings (dna / dnacalib, built from
 * MetaHuman-DNA-Calibration/dnacalib: PyDNA, PyDNACalib). Each operation runs the real bindings in
 * a Python process (a bridge script this wrapper writes to the project's Intermediate folder) and
 * reads its JSON result. Nothing is simulated: if Python or the bindings are unavailable,
 * Initialize fails, every operation fails, and GetLastError says why.
 *
 * Edits (rename/remove/clear/LOD) are applied to a working copy of the loaded DNA; SaveDNA writes
 * the working copy through the bindings' BinaryStreamWriter.
 *
 * Configuration (environment):
 * - DNACALIB_PYTHON: Python executable (default python3, or python on Windows)
 * - DNACALIB_PYTHON_PATH: directories (';'-separated) holding dna.py/_py3dna and dnacalib.py/_py3dnacalib;
 *   otherwise <DNACalibPath>, <DNACalibPath>/lib and <DNACalibPath>/lib/Maya20xx/<platform> are tried.
 *
 * Usage:
 * ```cpp
 * TSharedPtr<FPythonDNACalibWrapper> Wrapper = MakeShared<FPythonDNACalibWrapper>();
 * if (Wrapper->Initialize("/path/to/MetaHuman-DNA-Calibration"))
 * {
 *     Wrapper->LoadDNA("/path/to/character.dna");
 *     TArray<FString> Joints = Wrapper->GetJointNames();
 *     Wrapper->SaveDNA("/path/to/modified.dna");
 * }
 * ```
 */
class UNREALECHO_API FPythonDNACalibWrapper
{
public:
    FPythonDNACalibWrapper();
    ~FPythonDNACalibWrapper();

    // ========================================
    // Initialization
    // ========================================

    /**
     * Initialize Python interpreter and load DNACalib modules
     * @param DNACalibPath - Path to MetaHuman-DNA-Calibration directory
     * @return True if successfully initialized
     */
    bool Initialize(const FString& DNACalibPath);

    /**
     * Shutdown Python interpreter
     */
    void Shutdown();

    /**
     * Check if wrapper is initialized
     * @return True if initialized
     */
    bool IsInitialized() const { return bInitialized; }

    // ========================================
    // DNA File Operations
    // ========================================

    /**
     * Load DNA file
     * @param DNAPath - Path to .dna file
     * @return True if successfully loaded
     */
    bool LoadDNA(const FString& DNAPath);

    /**
     * Save DNA file
     * @param DNAPath - Path to save .dna file
     * @return True if successfully saved
     */
    bool SaveDNA(const FString& DNAPath);

    /**
     * Get DNA database name (DHI or MH.4)
     * @return Database name string
     */
    FString GetDNADatabaseName();

    // ========================================
    // Rig Structure Access
    // ========================================

    /**
     * Get all joint names from DNA
     * @return Array of joint names
     */
    TArray<FString> GetJointNames();

    /**
     * Get all blend shape names from DNA
     * @return Array of blend shape names
     */
    TArray<FString> GetBlendShapeNames();

    /**
     * Get all mesh names from DNA
     * @return Array of mesh names
     */
    TArray<FString> GetMeshNames();

    /**
     * Get joint count
     * @return Number of joints in DNA
     */
    int32 GetJointCount();

    /**
     * Get blend shape count
     * @return Number of blend shapes in DNA
     */
    int32 GetBlendShapeCount();

    /**
     * Get mesh count
     * @return Number of meshes in DNA
     */
    int32 GetMeshCount();

    // ========================================
    // DNA Manipulation
    // ========================================

    /**
     * Rename joint in DNA
     * @param OldName - Current joint name
     * @param NewName - New joint name
     * @return True if successfully renamed
     */
    bool RenameJoint(const FString& OldName, const FString& NewName);

    /**
     * Remove joint from DNA
     * @param JointName - Name of joint to remove
     * @return True if successfully removed
     */
    bool RemoveJoint(const FString& JointName);

    /**
     * Rename mesh in DNA
     * @param OldName - Current mesh name
     * @param NewName - New mesh name
     * @return True if successfully renamed
     */
    bool RenameMesh(const FString& OldName, const FString& NewName);

    /**
     * Remove mesh from DNA
     * @param MeshName - Name of mesh to remove
     * @return True if successfully removed
     */
    bool RemoveMesh(const FString& MeshName);

    /**
     * Clear all blend shape data
     * @return True if successfully cleared
     */
    bool ClearBlendShapes();

    /**
     * Remove LOD from DNA
     * @param LODIndex - Index of LOD to remove
     * @return True if successfully removed
     */
    bool RemoveLOD(int32 LODIndex);

    // ========================================
    // Advanced Operations
    // ========================================

    /**
     * Execute arbitrary Python script
     * @param Script - Python script to execute
     * @return True if successfully executed
     */
    bool ExecutePythonScript(const FString& Script);

    /**
     * Call Python function with arguments
     * @param FunctionName - Name of function to call
     * @param Args - Array of string arguments
     * @return Result string from Python function
     */
    FString CallPythonFunction(const FString& FunctionName, const TArray<FString>& Args);

    /**
     * Get last Python error message
     * @return Error message string
     */
    FString GetLastError() const { return LastError; }

private:
    // Initialization state
    bool bInitialized;
    bool bProbing;
    FString DNACalibRootPath;
    FString LastError;

    /** Python executable and the bindings directory that imported successfully */
    FString PythonExecutable;
    FString BindingsPath;

    /** The bridge script (written by Initialize) */
    FString BridgeScriptPath;

    /** The loaded DNA's source file and the working copy edits are applied to */
    FString LoadedDNAPath;
    FString WorkingDNAPath;

    /** Rig structure read from the working copy by the bindings (valid when bInfoValid) */
    bool bInfoValid;
    FString CachedDBName;
    TArray<FString> CachedJointNames;
    TArray<FString> CachedBlendShapeNames;
    TArray<FString> CachedMeshNames;

    /** Runs the bridge script with Op and Args; on success OutJson holds its reply */
    bool RunBridge(const FString& Op, const TArray<FString>& Args, TSharedPtr<class FJsonObject>& OutJson);

    /** Runs a Python file with the bindings on sys.path; returns stdout */
    bool RunPythonFile(const FString& ScriptPath, const TArray<FString>& Args, FString& OutStdOut);

    /** Reads joint/blend shape/mesh names and the DB name from the working copy */
    bool RefreshInfo();

    /** Applies an edit operation to the working copy */
    bool RunEdit(const FString& Op, const TArray<FString>& Args, const FString& Description);

    // Error handling
    void SetLastError(const FString& Error);
};
