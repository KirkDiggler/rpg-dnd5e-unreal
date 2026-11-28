#if WITH_DEV_AUTOMATION_TESTS
#include "RpgManifest.h"
#include "RpgSession.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
struct FManifestFile
{
    FString Path = FPaths::ProjectSavedDir() / (FGuid::NewGuid().ToString() + TEXT("-manifest.json"));
    ~FManifestFile() { IFileManager::Get().Delete(*Path); }
    bool Write() const
    {
        return FFileHelper::SaveStringToFile(TEXT(R"({"players":{"fighter":"/Game/Test/Rider"},"npcs":{"merchant-01":{"standing":"/Game/Test/Merchant","downed":null}}})"), *Path);
    }
};
}

#define RPG_MANIFEST_TEST(Name) IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManifest##Name, "RpgClient.RpgBoard.Manifest." #Name, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

RPG_MANIFEST_TEST(ResolvesPlayerByClassRef)
bool FManifestResolvesPlayerByClassRef::RunTest(const FString& Parameters)
{
    const FManifestFile File;
    if (!TestTrue(TEXT("write fixture"), File.Write())) return false;
    rpgwire::Member Member;
    Member.kind = rpgwire::Member::Kind::Player;
    Member.class_ref = "fighter";
    Member.appearance_ref = "merchant-01";
    TestEqual(TEXT("player resolves by class, not appearance"), FRpgManifest::Load(File.Path).StandingFor(Member), FString(TEXT("/Game/Test/Rider")));
    return !HasAnyErrors();
}

RPG_MANIFEST_TEST(ResolvesNpcByAppearanceRef)
bool FManifestResolvesNpcByAppearanceRef::RunTest(const FString& Parameters)
{
    const FManifestFile File;
    if (!TestTrue(TEXT("write fixture"), File.Write())) return false;
    rpgwire::Member Member;
    Member.kind = rpgwire::Member::Kind::Monster;
    Member.class_ref = "fighter";
    Member.monster_ref = "not-a-key";
    Member.appearance_ref = "merchant-01";
    TestEqual(TEXT("monster resolves the exact appearance"), FRpgManifest::Load(File.Path).StandingFor(Member), FString(TEXT("/Game/Test/Merchant")));
    return !HasAnyErrors();
}

RPG_MANIFEST_TEST(UnknownRefIsEmpty)
bool FManifestUnknownRefIsEmpty::RunTest(const FString& Parameters)
{
    const FManifestFile File;
    if (!TestTrue(TEXT("write fixture"), File.Write())) return false;
    const FRpgManifest Manifest = FRpgManifest::Load(File.Path);
    rpgwire::Member Member;
    Member.kind = rpgwire::Member::Kind::Monster;
    Member.monster_ref = "merchant-01";
    Member.appearance_ref = "Merchant-01";
    TestTrue(TEXT("no case folding or monster ref fallback"), Manifest.StandingFor(Member).IsEmpty());
    Member.kind = rpgwire::Member::Kind::Player;
    Member.class_ref = "not-a-key";
    TestTrue(TEXT("no fallback for unknown class"), Manifest.StandingFor(Member).IsEmpty());
    return !HasAnyErrors();
}

RPG_MANIFEST_TEST(MissingFileIsEmpty)
bool FManifestMissingFileIsEmpty::RunTest(const FString& Parameters)
{
    const FManifestFile File;
    rpgwire::Member Member;
    Member.kind = rpgwire::Member::Kind::Player;
    Member.class_ref = "fighter";
    TestTrue(TEXT("missing file resolves nothing"), FRpgManifest::Load(File.Path).StandingFor(Member).IsEmpty());
    return !HasAnyErrors();
}

RPG_MANIFEST_TEST(WorldKindIsEmpty)
bool FManifestWorldKindIsEmpty::RunTest(const FString& Parameters)
{
    const FManifestFile File;
    if (!TestTrue(TEXT("write fixture"), File.Write())) return false;
    rpgwire::Member Member;
    Member.kind = rpgwire::Member::Kind::World;
    Member.class_ref = "fighter";
    Member.appearance_ref = "merchant-01";
    TestTrue(TEXT("world members resolve neither index"), FRpgManifest::Load(File.Path).StandingFor(Member).IsEmpty());
    return !HasAnyErrors();
}
#undef RPG_MANIFEST_TEST
#endif
