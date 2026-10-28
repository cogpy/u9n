// Copyright Deep Tree Echo. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DeepTreeEcho : ModuleRules
{
	public DeepTreeEcho(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Deep Tree Echo source directories (relative to repository root)
		string DeepTreeEchoRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../DeepTreeEcho"));
		string ReservoirEchoRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../ReservoirEcho"));

		// Include paths for Deep Tree Echo components
		PublicIncludePaths.AddRange(
			new string[] {
				Path.Combine(DeepTreeEchoRoot, "Core"),
				Path.Combine(DeepTreeEchoRoot, "Reservoir"),
				Path.Combine(DeepTreeEchoRoot, "4ECognition"),
				Path.Combine(DeepTreeEchoRoot, "ActiveInference"),
				Path.Combine(DeepTreeEchoRoot, "Avatar"),
				Path.Combine(DeepTreeEchoRoot, "Cognitive"),
				Path.Combine(DeepTreeEchoRoot, "Cosmos"),
				Path.Combine(DeepTreeEchoRoot, "Entelechy"),
				Path.Combine(DeepTreeEchoRoot, "Evolution"),
				Path.Combine(DeepTreeEchoRoot, "Goals"),
				Path.Combine(DeepTreeEchoRoot, "Integration"),
				Path.Combine(DeepTreeEchoRoot, "Memory"),
				Path.Combine(DeepTreeEchoRoot, "Metamodel"),
				Path.Combine(DeepTreeEchoRoot, "Model"),
				Path.Combine(DeepTreeEchoRoot, "Sensorimotor"),
				Path.Combine(DeepTreeEchoRoot, "System5"),
				Path.Combine(DeepTreeEchoRoot, "UnrealBridge"),
				Path.Combine(DeepTreeEchoRoot, "Wisdom"),
				// Sys6 Operad Architecture
				Path.Combine(DeepTreeEchoRoot, "Sys6"),
				// Taskflow parallel task scheduling
				Path.Combine(DeepTreeEchoRoot, "Taskflow"),
			}
		);

		// Include paths for ReservoirCpp library (Eigen-based ESN)
		PrivateIncludePaths.AddRange(
			new string[] {
				Path.Combine(ReservoirEchoRoot, "reservoircpp_cpp/include"),
				Path.Combine(ReservoirEchoRoot, "external/eigen-3.4.0"),
				// Taskflow header-only library for parallel task graphs
				Path.Combine(ReservoirEchoRoot, "external/taskflow-3.8.0"),
			}
		);

		// Core Unreal Engine dependencies
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Json",  // EchoModelClient's public header exposes FJsonObject
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				// EchoModelClient: characters speak only through a model endpoint
				"HTTP",
			}
		);

		// Enable exceptions for Eigen library
		bEnableExceptions = true;

		// Disable unity builds for better debugging
		bUseUnity = false;

		// Compiler definitions
		PublicDefinitions.Add("DEEPTREEECHO_API=");
		PublicDefinitions.Add("EIGEN_MPL2_ONLY");  // Use only MPL2-licensed parts of Eigen
	}
}
