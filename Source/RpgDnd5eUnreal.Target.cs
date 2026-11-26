using UnrealBuildTool;
using System.Collections.Generic;

public class RpgDnd5eUnrealTarget : TargetRules
{
    public RpgDnd5eUnrealTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V8;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("RpgDnd5eUnreal");
    }
}
