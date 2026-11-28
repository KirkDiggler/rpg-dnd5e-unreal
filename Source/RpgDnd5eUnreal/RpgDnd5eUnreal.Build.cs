using UnrealBuildTool;

public class RpgDnd5eUnreal : ModuleRules
{
    public RpgDnd5eUnreal(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
        PrivateDependencyModuleNames.AddRange(new[] { "RpgBoard" });
        if (Target.bBuildEditor) PrivateDependencyModuleNames.Add("UnrealEd");
    }
}
