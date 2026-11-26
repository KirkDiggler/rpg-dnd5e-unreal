// RpgClient: the client's side of the record, what a beat means for figures
// and which cells a walk takes, in functions with no world in them.
using UnrealBuildTool;

public class RpgClient : ModuleRules
{
    public RpgClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bEnableExceptions = false;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "Json", "RpgWire" });
    }
}
