using System.IO;
using UnrealBuildTool;

public class RpgBoard : ModuleRules
{
    public RpgBoard(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bEnableExceptions = false;
        bUseRTTI = false;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "RpgWire", "RpgClient" });
        PrivateDependencyModuleNames.AddRange(new[] { "ProceduralMeshComponent", "Json" });

        // Ahead of every source file, the generated contract included: see RpgPbGuard.h.
        ForceIncludeFiles.Add(Path.Combine(ModuleDirectory, "..", "RpgWire", "Private", "RpgPbGuard.h"));
        PrivateDefinitions.Add("GOOGLE_PROTOBUF_NO_RTTI");
    }
}
