using UnrealBuildTool;

public class AeroSimEditor : ModuleRules
{
    public AeroSimEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine"
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "AeroSim",
            "AssetRegistry",
            "AssetTools",
            "MaterialEditor",
            "MeshDescription",
            "StaticMeshDescription",
            "Json",
            "Projects",
            "UnrealEd"
        });
    }
}
