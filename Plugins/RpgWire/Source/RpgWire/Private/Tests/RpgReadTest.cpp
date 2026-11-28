#if WITH_DEV_AUTOMATION_TESTS
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "RpgRead.h"
#include "dnd5e/api/session/v1alpha1/types.pb.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgReadMemberTest, "RpgWire.Read.AMembersEveryFieldComesFromTheWire", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRpgReadMemberTest::RunTest(const FString& Parameters)
{
    namespace v1 = dnd5e::api::session::v1alpha1;
    v1::PublicMemberInfo wire;
    wire.set_id("subject-id"); wire.set_kind(v1::MEMBER_KIND_WORLD); wire.set_name("subject-name");
    wire.set_monster_ref("monster-ref"); wire.set_class_ref("class-ref"); wire.set_faction("faction-word");
    rpgwire::Member out;
    rpgwire::ReadMember(wire, out);
    TestTrue(TEXT("id"), out.id == "subject-id");
    TestTrue(TEXT("kind"), out.kind == rpgwire::Member::Kind::World);
    TestTrue(TEXT("name"), out.name == "subject-name");
    TestTrue(TEXT("monster ref"), out.monster_ref == "monster-ref");
    TestTrue(TEXT("class ref"), out.class_ref == "class-ref");
    TestTrue(TEXT("faction"), out.faction == "faction-word");
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgReadSightingTest, "RpgWire.Read.ASightingsEveryFieldComesFromTheWire", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRpgReadSightingTest::RunTest(const FString& Parameters)
{
    namespace v1 = dnd5e::api::session::v1alpha1;
    v1::Sighting wire;
    wire.set_subject("subject-id"); wire.set_name("subject-name"); wire.set_kind(v1::MEMBER_KIND_MONSTER); wire.set_stance("allied");
    wire.mutable_seen()->mutable_position()->set_x(-3); wire.mutable_seen()->mutable_position()->set_y(7); wire.mutable_seen()->set_standing(v1::STANDING_DOWNED);
    rpgwire::Sighting out;
    rpgwire::ReadSighting(wire, out);
    TestTrue(TEXT("subject"), out.id == "subject-id");
    TestTrue(TEXT("name"), out.name == "subject-name");
    TestTrue(TEXT("kind"), out.kind == rpgwire::Member::Kind::Monster);
    TestTrue(TEXT("stance"), out.stance == "allied");
    TestTrue(TEXT("cell"), out.cell == rpgwire::Cell(-3, 7));
    TestEqual(TEXT("standing"), out.standing, 2);
    v1::Sighting empty;
    rpgwire::Sighting emptyOut;
    rpgwire::ReadSighting(empty, emptyOut);
    TestTrue(TEXT("empty stance"), emptyOut.stance.empty());
    TestEqual(TEXT("unknown standing"), emptyOut.standing, 0);
    return !HasAnyErrors();
}
#endif
