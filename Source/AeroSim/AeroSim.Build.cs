using UnrealBuildTool;
using System;
using System.Text.RegularExpressions;

public class AeroSim : ModuleRules
{
    public AeroSim(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        string Revision = Environment.GetEnvironmentVariable("AEROSIM_SOURCE_REVISION") ?? "UNKNOWN";
        string Fingerprint = Environment.GetEnvironmentVariable("AEROSIM_SOURCE_FINGERPRINT") ?? "UNKNOWN";
        if (!Regex.IsMatch(Revision, "^[a-f0-9]{40}$")) Revision = "UNKNOWN";
        if (!Regex.IsMatch(Fingerprint, "^[a-f0-9]{64}$")) Fingerprint = "UNKNOWN";
        PublicDefinitions.Add("AEROSIM_SOURCE_REVISION=\"" + Revision + "\"");
        PublicDefinitions.Add("AEROSIM_SOURCE_FINGERPRINT=\"" + Fingerprint + "\"");
        PublicDefinitions.Add("AEROSIM_SOURCE_DIRTY=" + (Environment.GetEnvironmentVariable("AEROSIM_SOURCE_DIRTY") == "0" ? "0" : "1"));

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "ApplicationCore",
            "Engine",
            "EnhancedInput",
            "InputCore",
            "Json",
            "CommonUI",
            "UMG",
            "Slate",
            "SlateCore",
            "RHI",
            "RenderCore",
            "ProceduralMeshComponent",
            // DEV-FLORA: runtime-built instanced tree archetype meshes (UStaticMesh::BuildFromMeshDescriptions)
            "MeshDescription",
            "StaticMeshDescription"
        });
    }
}
