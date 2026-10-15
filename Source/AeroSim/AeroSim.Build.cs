using UnrealBuildTool;

public class AeroSim : ModuleRules
{
    public AeroSim(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "ApplicationCore",
            "Engine",
            "EnhancedInput",
            "InputCore"
        });
    }
}
