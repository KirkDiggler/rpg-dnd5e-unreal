using UnrealBuildTool;
using System.Collections.Generic;

public class RpgDnd5eUnrealEditorTarget : TargetRules
{
    public RpgDnd5eUnrealEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V8;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("RpgDnd5eUnreal");
    }
}
