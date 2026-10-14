using UnrealBuildTool;
using System.Collections.Generic;

public class AeroSimEditorTarget : TargetRules
{
    public AeroSimEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("AeroSim");
    }
}
