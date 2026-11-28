#include "RpgBoard.h"
#include "RpgLook.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgColourTest, "RpgBoard.Look.TheColourComesFromTheWordThenTheSideAndGreysWhenDowned", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRpgColourTest::RunTest(const FString& Parameters)
{
    using namespace RpgLook;
    TestTrue(TEXT("monster"), FigureColour(ERpgSide::Monster, TEXT(""), false, false) == kMonster);
    TestTrue(TEXT("hostile monster"), FigureColour(ERpgSide::Monster, TEXT("hostile"), false, false) == kMonster);
    TestTrue(TEXT("hostile player"), FigureColour(ERpgSide::Player, TEXT("hostile"), false, false) == kMonster);
    TestTrue(TEXT("allied monster"), FigureColour(ERpgSide::Monster, TEXT("allied"), false, false) == kNpc);
    TestTrue(TEXT("neutral monster"), FigureColour(ERpgSide::Monster, TEXT("neutral"), false, false) == kMonster);
    TestTrue(TEXT("unknown word"), FigureColour(ERpgSide::Monster, TEXT("friendly"), false, false) == kMonster);
    TestTrue(TEXT("active player"), FigureColour(ERpgSide::Player, TEXT(""), true, false) == kPlayerActive);
    TestTrue(TEXT("active npc"), FigureColour(ERpgSide::Npc, TEXT(""), true, false) == kNpcActive);
    TestTrue(TEXT("downed"), FigureColour(ERpgSide::Player, TEXT(""), true, true) == kDowned);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgBoxTest, "RpgBoard.Look.WallBoxesAreFeetTurnedToCentimetres", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRpgBoxTest::RunTest(const FString& Parameters)
{
    using namespace RpgLook;
    TestTrue(TEXT("size"), BoxSize(10, 5, 10).Equals(FVector(346.41, 173.21, 346.41), 0.01));
    TestTrue(TEXT("origin centre"), FMath::IsNearlyEqual(BoxCentre(0, 0, 0, 10).Z, 173.21, 0.01));
    TestTrue(TEXT("offset centre"), BoxCentre(3, 4, 2, 10).Equals(FVector(103.92, 138.56, 242.49), 0.01));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgHexTest, "RpgBoard.Look.HexIsUpperCasesRGB", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRpgHexTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("monster"), RpgLook::HexOf(RpgLook::kMonster), FString(TEXT("#E53E3E")));
    TestEqual(TEXT("downed"), RpgLook::HexOf(RpgLook::kDowned), FString(TEXT("#666666")));
    return !HasAnyErrors();
}
#endif
