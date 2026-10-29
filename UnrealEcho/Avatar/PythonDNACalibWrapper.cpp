// PythonDNACalibWrapper.cpp
// Deep Tree Echo - Python DNACalib Integration Implementation
// Copyright (c) 2025 Deep Tree Echo Project
//
// Every operation runs the real MetaHuman DNACalib Python bindings (dna, dnacalib) in a Python
// process and parses its JSON reply. There is no simulation mode: without Python or the bindings
// the wrapper reports failure with the reason.

#include "PythonDNACalibWrapper.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogPythonDNA, Log, All);

namespace
{
    // Bridge script run by the wrapper: one operation per process, one JSON object on stdout.
    const char* const DNACalibBridgeScript = R"PY(# Deep Tree Echo - DNACalib bridge (written by FPythonDNACalibWrapper; runs the real dna/dnacalib bindings)
# usage: python dnacalib_bridge.py <bindings_dir|-> <op> [args...]; prints one JSON object
import json, sys

def out(obj):
    sys.stdout.write(json.dumps(obj))
    sys.stdout.flush()

def fail(msg):
    out({"ok": False, "error": msg})
    sys.exit(1)

def main(argv):
    if len(argv) < 2:
        fail("usage: dnacalib_bridge.py <bindings_dir|-> <op> [args...]")
    if argv[0] != "-":
        for p in argv[0].split(";"):
            if p:
                sys.path.insert(0, p)
    try:
        import dna
        import dnacalib
    except Exception as e:
        fail("cannot import the dna/dnacalib Python bindings: %s" % e)
    op, args = argv[1], argv[2:]

    def check(what):
        if not dna.Status.isOk():
            fail("%s: %s" % (what, dna.Status.get().message))

    def load(path):
        stream = dna.FileStream(path, dna.FileStream.AccessMode_Read, dna.FileStream.OpenMode_Binary)
        reader = dna.BinaryStreamReader(stream, dna.DataLayer_All)
        reader.read()
        check("reading " + path)
        return dnacalib.DNACalibDNAReader(reader)

    def save(reader, path):
        stream = dna.FileStream(path, dna.FileStream.AccessMode_Write, dna.FileStream.OpenMode_Binary)
        writer = dna.BinaryStreamWriter(stream)
        writer.setFrom(reader)
        writer.write()
        check("writing " + path)

    def names(count, get):
        return [get(i) for i in range(count)]

    def index_of(count, get, name, kind):
        for i in range(count):
            if get(i) == name:
                return i
        fail("%s '%s' not found in the DNA" % (kind, name))

    if op == "probe":
        out({"ok": True})
        return
    if not args:
        fail("missing DNA path")
    path = args[0]
    r = load(path)
    if op == "info":
        out({"ok": True, "db_name": r.getDBName(),
             "joints": names(r.getJointCount(), r.getJointName),
             "blend_shapes": names(r.getBlendShapeChannelCount(), r.getBlendShapeChannelName),
             "meshes": names(r.getMeshCount(), r.getMeshName),
             "lods": r.getLODCount()})
        return
    if op == "save":
        if len(args) < 2:
            fail("save needs an output path")
        save(r, args[1])
        out({"ok": True})
        return
    if op == "rename_joint" and len(args) == 3:
        index_of(r.getJointCount(), r.getJointName, args[1], "joint")
        dnacalib.RenameJointCommand(args[1], args[2]).run(r)
    elif op == "remove_joint" and len(args) == 2:
        dnacalib.RemoveJointCommand(index_of(r.getJointCount(), r.getJointName, args[1], "joint")).run(r)
    elif op == "rename_mesh" and len(args) == 3:
        index_of(r.getMeshCount(), r.getMeshName, args[1], "mesh")
        dnacalib.RenameMeshCommand(args[1], args[2]).run(r)
    elif op == "remove_mesh" and len(args) == 2:
        dnacalib.RemoveMeshCommand(index_of(r.getMeshCount(), r.getMeshName, args[1], "mesh")).run(r)
    elif op == "clear_blend_shapes" and len(args) == 1:
        dnacalib.ClearBlendShapesCommand().run(r)
    elif op == "remove_lod" and len(args) == 2:
        lod = int(args[1])
        count = r.getLODCount()
        if lod < 0 or lod >= count or count < 2:
            fail("cannot remove LOD %d from a DNA with %d LODs" % (lod, count))
        cmd = dnacalib.SetLODsCommand()
        cmd.setLODs([i for i in range(count) if i != lod])
        cmd.run(r)
    else:
        fail("unknown operation or wrong arguments: %s %s" % (op, args[1:]))
    save(r, path)
    out({"ok": True})

if __name__ == "__main__":
    try:
        main(sys.argv[1:])
    except SystemExit:
        raise
    except Exception as e:
        fail("%s: %s" % (type(e).__name__, e))
)PY";

    FString QuoteArg(const FString& Arg)
    {
        return TEXT("\"") + Arg.Replace(TEXT("\""), TEXT("\\\"")) + TEXT("\"");
    }

    TArray<FString> ReadStringArray(const TSharedPtr<FJsonObject>& Json, const TCHAR* Field)
    {
        TArray<FString> Result;
        const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
        if (Json.IsValid() && Json->TryGetArrayField(Field, Items) && Items)
        {
            for (const TSharedPtr<FJsonValue>& Item : *Items)
            {
                FString Value;
                if (Item.IsValid() && Item->TryGetString(Value))
                {
                    Result.Add(Value);
                }
            }
        }
        return Result;
    }
}

// ========================================
// Constructor / Destructor
// ========================================

FPythonDNACalibWrapper::FPythonDNACalibWrapper()
    : bInitialized(false)
    , bProbing(false)
    , bInfoValid(false)
{
}

FPythonDNACalibWrapper::~FPythonDNACalibWrapper()
{
    if (bInitialized)
    {
        Shutdown();
    }
}

// ========================================
// Initialization
// ========================================

bool FPythonDNACalibWrapper::Initialize(const FString& DNACalibPath)
{
    if (bInitialized)
    {
        UE_LOG(LogPythonDNA, Warning, TEXT("Python wrapper already initialized"));
        return true;
    }

    DNACalibRootPath = DNACalibPath;
    if (!IFileManager::Get().DirectoryExists(*DNACalibPath))
    {
        SetLastError(FString::Printf(TEXT("DNACalib path does not exist: %s"), *DNACalibPath));
        return false;
    }

    PythonExecutable = FPlatformMisc::GetEnvironmentVariable(TEXT("DNACALIB_PYTHON"));
    if (PythonExecutable.IsEmpty())
    {
        PythonExecutable = PLATFORM_WINDOWS ? TEXT("python") : TEXT("python3");
    }

    // Write the bridge script
    const FString WorkDir = FPaths::ProjectSavedDir() / TEXT("DNACalib");
    IFileManager::Get().MakeDirectory(*WorkDir, true);
    BridgeScriptPath = WorkDir / TEXT("dnacalib_bridge.py");
    if (!FFileHelper::SaveStringToFile(FString(UTF8_TO_TCHAR(DNACalibBridgeScript)), *BridgeScriptPath))
    {
        SetLastError(FString::Printf(TEXT("cannot write the DNACalib bridge script to %s"), *BridgeScriptPath));
        return false;
    }

    // Find the bindings: an explicit path, else the DNACalib folder and its lib/ subfolders
    TArray<FString> Candidates;
    const FString EnvPath = FPlatformMisc::GetEnvironmentVariable(TEXT("DNACALIB_PYTHON_PATH"));
    if (!EnvPath.IsEmpty())
    {
        Candidates.Add(EnvPath);
    }
    else
    {
        const TCHAR* Platform = PLATFORM_WINDOWS ? TEXT("windows") : TEXT("linux");
        Candidates.Add(DNACalibPath);
        Candidates.Add(DNACalibPath / TEXT("lib"));
        for (const TCHAR* Maya : { TEXT("Maya2024"), TEXT("Maya2023"), TEXT("Maya2022") })
        {
            Candidates.Add(DNACalibPath / TEXT("lib") / Maya / Platform);
        }
    }

    TArray<FString> ProbeErrors;
    bProbing = true; // a candidate that fails is not an error yet
    for (const FString& Candidate : Candidates)
    {
        BindingsPath = Candidate;
        TSharedPtr<FJsonObject> Reply;
        const bool bFound = RunBridge(TEXT("probe"), {}, Reply);
        if (bFound)
        {
            bProbing = false;
            bInitialized = true;
            LastError.Reset();
            UE_LOG(LogPythonDNA, Log, TEXT("DNACalib Python bindings found in %s (%s)"), *BindingsPath, *PythonExecutable);
            return true;
        }
        ProbeErrors.Add(FString::Printf(TEXT("%s: %s"), *Candidate, *LastError));
    }
    bProbing = false;

    BindingsPath.Reset();
    SetLastError(FString::Printf(TEXT("the dna/dnacalib Python bindings could not be imported with %s (build them from MetaHuman-DNA-Calibration/dnacalib or set DNACALIB_PYTHON_PATH). Tried: %s"),
        *PythonExecutable, *FString::Join(ProbeErrors, TEXT(" | "))));
    return false;
}

void FPythonDNACalibWrapper::Shutdown()
{
    if (!WorkingDNAPath.IsEmpty())
    {
        IFileManager::Get().Delete(*WorkingDNAPath, false, true, true);
    }
    WorkingDNAPath.Reset();
    LoadedDNAPath.Reset();
    bInfoValid = false;
    CachedDBName.Reset();
    CachedJointNames.Reset();
    CachedBlendShapeNames.Reset();
    CachedMeshNames.Reset();

    if (bInitialized)
    {
        UE_LOG(LogPythonDNA, Log, TEXT("Python DNACalib wrapper shut down"));
    }
    bInitialized = false;
}

// ========================================
// Process plumbing
// ========================================

bool FPythonDNACalibWrapper::RunPythonFile(const FString& ScriptPath, const TArray<FString>& Args, FString& OutStdOut)
{
    FString Params = QuoteArg(ScriptPath);
    for (const FString& Arg : Args)
    {
        Params += TEXT(" ") + QuoteArg(Arg);
    }

    int32 ReturnCode = -1;
    FString StdErr;
    OutStdOut.Reset();
    if (!FPlatformProcess::ExecProcess(*PythonExecutable, *Params, &ReturnCode, &OutStdOut, &StdErr))
    {
        SetLastError(FString::Printf(TEXT("could not run %s"), *PythonExecutable));
        return false;
    }
    if (ReturnCode != 0 && OutStdOut.TrimStartAndEnd().IsEmpty())
    {
        SetLastError(FString::Printf(TEXT("%s exited with code %d: %s"), *PythonExecutable, ReturnCode, *StdErr.Left(500)));
        return false;
    }
    return true;
}

bool FPythonDNACalibWrapper::RunBridge(const FString& Op, const TArray<FString>& Args, TSharedPtr<FJsonObject>& OutJson)
{
    TArray<FString> AllArgs;
    AllArgs.Add(BindingsPath.IsEmpty() ? FString(TEXT("-")) : BindingsPath);
    AllArgs.Add(Op);
    AllArgs.Append(Args);

    FString StdOut;
    if (!RunPythonFile(BridgeScriptPath, AllArgs, StdOut))
    {
        return false;
    }

    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(StdOut);
    if (!FJsonSerializer::Deserialize(Reader, OutJson) || !OutJson.IsValid())
    {
        SetLastError(FString::Printf(TEXT("DNACalib bridge '%s' gave no JSON reply: %s"), *Op, *StdOut.Left(300)));
        return false;
    }
    bool bOk = false;
    if (!OutJson->TryGetBoolField(TEXT("ok"), bOk) || !bOk)
    {
        FString Error;
        OutJson->TryGetStringField(TEXT("error"), Error);
        SetLastError(FString::Printf(TEXT("DNACalib %s failed: %s"), *Op, Error.IsEmpty() ? TEXT("(no reason given)") : *Error));
        return false;
    }
    return true;
}

bool FPythonDNACalibWrapper::RefreshInfo()
{
    if (bInfoValid)
    {
        return true;
    }
    if (!bInitialized || WorkingDNAPath.IsEmpty())
    {
        SetLastError(TEXT("No DNA loaded"));
        return false;
    }

    TSharedPtr<FJsonObject> Reply;
    if (!RunBridge(TEXT("info"), { WorkingDNAPath }, Reply))
    {
        return false;
    }
    Reply->TryGetStringField(TEXT("db_name"), CachedDBName);
    CachedJointNames = ReadStringArray(Reply, TEXT("joints"));
    CachedBlendShapeNames = ReadStringArray(Reply, TEXT("blend_shapes"));
    CachedMeshNames = ReadStringArray(Reply, TEXT("meshes"));
    bInfoValid = true;
    return true;
}

bool FPythonDNACalibWrapper::RunEdit(const FString& Op, const TArray<FString>& Args, const FString& Description)
{
    if (!bInitialized || WorkingDNAPath.IsEmpty())
    {
        SetLastError(TEXT("No DNA loaded"));
        return false;
    }

    TArray<FString> AllArgs;
    AllArgs.Add(WorkingDNAPath);
    AllArgs.Append(Args);
    TSharedPtr<FJsonObject> Reply;
    if (!RunBridge(Op, AllArgs, Reply))
    {
        return false;
    }

    bInfoValid = false; // the working copy changed
    UE_LOG(LogPythonDNA, Log, TEXT("%s"), *Description);
    return true;
}

// ========================================
// DNA File Operations
// ========================================

bool FPythonDNACalibWrapper::LoadDNA(const FString& DNAPath)
{
    if (!bInitialized)
    {
        SetLastError(TEXT("Wrapper not initialized"));
        return false;
    }
    if (!FPaths::FileExists(DNAPath))
    {
        SetLastError(FString::Printf(TEXT("DNA file not found: %s"), *DNAPath));
        return false;
    }

    // Read the DNA with the bindings and write the working copy edits will be applied to
    const FString NewWorking = FPaths::ProjectSavedDir() / TEXT("DNACalib") /
        FString::Printf(TEXT("working_%s.dna"), *FGuid::NewGuid().ToString());
    TSharedPtr<FJsonObject> Reply;
    if (!RunBridge(TEXT("save"), { DNAPath, NewWorking }, Reply))
    {
        return false;
    }

    if (!WorkingDNAPath.IsEmpty())
    {
        IFileManager::Get().Delete(*WorkingDNAPath, false, true, true);
    }
    WorkingDNAPath = NewWorking;
    LoadedDNAPath = DNAPath;
    bInfoValid = false;
    if (!RefreshInfo())
    {
        return false;
    }

    UE_LOG(LogPythonDNA, Log, TEXT("Loaded DNA file: %s (%s, %d joints, %d blend shapes, %d meshes)"), *DNAPath,
        *CachedDBName, CachedJointNames.Num(), CachedBlendShapeNames.Num(), CachedMeshNames.Num());
    return true;
}

bool FPythonDNACalibWrapper::SaveDNA(const FString& DNAPath)
{
    if (!bInitialized || WorkingDNAPath.IsEmpty())
    {
        SetLastError(TEXT("No DNA loaded"));
        return false;
    }

    TSharedPtr<FJsonObject> Reply;
    if (!RunBridge(TEXT("save"), { WorkingDNAPath, DNAPath }, Reply))
    {
        return false;
    }
    UE_LOG(LogPythonDNA, Log, TEXT("Saved DNA file: %s"), *DNAPath);
    return true;
}

FString FPythonDNACalibWrapper::GetDNADatabaseName()
{
    // The rig definition the DNA reports (reader.getDBName()), or empty if it could not be read
    return RefreshInfo() ? CachedDBName : FString();
}

// ========================================
// Rig Structure Access
// ========================================

TArray<FString> FPythonDNACalibWrapper::GetJointNames()
{
    return RefreshInfo() ? CachedJointNames : TArray<FString>();
}

TArray<FString> FPythonDNACalibWrapper::GetBlendShapeNames()
{
    return RefreshInfo() ? CachedBlendShapeNames : TArray<FString>();
}

TArray<FString> FPythonDNACalibWrapper::GetMeshNames()
{
    return RefreshInfo() ? CachedMeshNames : TArray<FString>();
}

int32 FPythonDNACalibWrapper::GetJointCount()
{
    return GetJointNames().Num();
}

int32 FPythonDNACalibWrapper::GetBlendShapeCount()
{
    return GetBlendShapeNames().Num();
}

int32 FPythonDNACalibWrapper::GetMeshCount()
{
    return GetMeshNames().Num();
}

// ========================================
// DNA Manipulation
// ========================================

bool FPythonDNACalibWrapper::RenameJoint(const FString& OldName, const FString& NewName)
{
    return RunEdit(TEXT("rename_joint"), { OldName, NewName }, FString::Printf(TEXT("Renamed joint: %s -> %s"), *OldName, *NewName));
}

bool FPythonDNACalibWrapper::RemoveJoint(const FString& JointName)
{
    return RunEdit(TEXT("remove_joint"), { JointName }, FString::Printf(TEXT("Removed joint: %s"), *JointName));
}

bool FPythonDNACalibWrapper::RenameMesh(const FString& OldName, const FString& NewName)
{
    return RunEdit(TEXT("rename_mesh"), { OldName, NewName }, FString::Printf(TEXT("Renamed mesh: %s -> %s"), *OldName, *NewName));
}

bool FPythonDNACalibWrapper::RemoveMesh(const FString& MeshName)
{
    return RunEdit(TEXT("remove_mesh"), { MeshName }, FString::Printf(TEXT("Removed mesh: %s"), *MeshName));
}

bool FPythonDNACalibWrapper::ClearBlendShapes()
{
    return RunEdit(TEXT("clear_blend_shapes"), {}, TEXT("Cleared all blend shapes"));
}

bool FPythonDNACalibWrapper::RemoveLOD(int32 LODIndex)
{
    return RunEdit(TEXT("remove_lod"), { FString::FromInt(LODIndex) }, FString::Printf(TEXT("Removed LOD: %d"), LODIndex));
}

// ========================================
// Advanced Operations
// ========================================

bool FPythonDNACalibWrapper::ExecutePythonScript(const FString& Script)
{
    if (!bInitialized)
    {
        SetLastError(TEXT("Wrapper not initialized"));
        return false;
    }

    // Run the script with the bindings importable; success means it exited cleanly
    const FString ScriptPath = FPaths::ProjectSavedDir() / TEXT("DNACalib") /
        FString::Printf(TEXT("script_%s.py"), *FGuid::NewGuid().ToString());
    const FString Prelude = FString::Printf(TEXT("import sys\nfor _p in %s.split(';'):\n    sys.path.insert(0, _p)\n"),
        *QuoteArg(BindingsPath.Replace(TEXT("\\"), TEXT("/"))));
    if (!FFileHelper::SaveStringToFile(Prelude + Script, *ScriptPath))
    {
        SetLastError(FString::Printf(TEXT("cannot write the script to %s"), *ScriptPath));
        return false;
    }

    int32 ReturnCode = -1;
    FString StdOut, StdErr;
    const bool bRan = FPlatformProcess::ExecProcess(*PythonExecutable, *QuoteArg(ScriptPath), &ReturnCode, &StdOut, &StdErr);
    IFileManager::Get().Delete(*ScriptPath, false, true, true);
    if (!bRan || ReturnCode != 0)
    {
        SetLastError(FString::Printf(TEXT("Python script failed (exit %d): %s"), ReturnCode, *StdErr.Left(500)));
        return false;
    }
    UE_LOG(LogPythonDNA, Log, TEXT("Executed Python script (%d chars)"), Script.Len());
    return true;
}

FString FPythonDNACalibWrapper::CallPythonFunction(const FString& FunctionName, const TArray<FString>& Args)
{
    if (!bInitialized)
    {
        SetLastError(TEXT("Wrapper not initialized"));
        return FString();
    }

    // FunctionName is "module.function"; arguments are passed as strings; the result is str(result)
    int32 Dot = INDEX_NONE;
    if (!FunctionName.FindLastChar(TEXT('.'), Dot) || Dot <= 0)
    {
        SetLastError(FString::Printf(TEXT("CallPythonFunction needs module.function, got '%s'"), *FunctionName));
        return FString();
    }

    const FString ScriptPath = FPaths::ProjectSavedDir() / TEXT("DNACalib") /
        FString::Printf(TEXT("call_%s.py"), *FGuid::NewGuid().ToString());
    const FString Script = FString::Printf(TEXT(
        "import sys, json, importlib\n"
        "for _p in sys.argv[1].split(';'):\n    sys.path.insert(0, _p)\n"
        "_m = importlib.import_module(sys.argv[2])\n"
        "_r = getattr(_m, sys.argv[3])(*sys.argv[4:])\n"
        "sys.stdout.write(json.dumps({'ok': True, 'result': str(_r)}))\n"));
    if (!FFileHelper::SaveStringToFile(Script, *ScriptPath))
    {
        SetLastError(FString::Printf(TEXT("cannot write the call script to %s"), *ScriptPath));
        return FString();
    }

    TArray<FString> CallArgs = { BindingsPath, FunctionName.Left(Dot), FunctionName.Mid(Dot + 1) };
    CallArgs.Append(Args);
    FString StdOut;
    const bool bRan = RunPythonFile(ScriptPath, CallArgs, StdOut);
    IFileManager::Get().Delete(*ScriptPath, false, true, true);
    if (!bRan)
    {
        return FString();
    }

    TSharedPtr<FJsonObject> Reply;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(StdOut);
    FString Result;
    if (!FJsonSerializer::Deserialize(Reader, Reply) || !Reply.IsValid() || !Reply->TryGetStringField(TEXT("result"), Result))
    {
        SetLastError(FString::Printf(TEXT("Python call %s failed: %s"), *FunctionName, *StdOut.Left(300)));
        return FString();
    }
    return Result;
}

// ========================================
// Error Handling
// ========================================

void FPythonDNACalibWrapper::SetLastError(const FString& Error)
{
    LastError = Error;
    if (!bProbing)
    {
        UE_LOG(LogPythonDNA, Error, TEXT("Python wrapper error: %s"), *Error);
    }
}
