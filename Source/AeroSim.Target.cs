using UnrealBuildTool;
using System.Collections.Generic;

public class AeroSimTarget : TargetRules
{
    public AeroSimTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("AeroSim");
    }
}
