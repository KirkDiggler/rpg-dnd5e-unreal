#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Misc/AutomationTest.h"
#include "ProceduralMeshComponent.h"
#include "RpgBoard.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgNativeDownedTest, "RpgClient.RpgBoard.Native.DownedReplacesNativeWithShape", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRpgNativeDownedTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("test world"), World)) return false;
    ARpgBoard* Board = World->SpawnActor<ARpgBoard>();
    ACharacter* Stub = World->SpawnActor<ACharacter>(FVector(0, 0, 88), FRotator::ZeroRotator);
    if (TestNotNull(TEXT("board"), Board) && TestNotNull(TEXT("stub character"), Stub))
    {
        const FString Member = TEXT("test-native");
        Board->Figures.Add(Member, Stub);
        Board->NativeFigures.Add(Member);
        Board->NativeClasses.Add(Member, Stub->GetClass());
        Board->SetFigureSide(Member, ERpgSide::Monster);
        Board->SetFigureDowned(Member, true);
        AActor* Replacement = Board->Figures.FindRef(Member);
        TestTrue(TEXT("native actor is replaced"), Replacement && Replacement != Stub);
        TestTrue(TEXT("replacement is the tipped-over S3 shape"), Replacement &&
            Replacement->FindComponentByClass<UProceduralMeshComponent>() &&
            FMath::IsNearlyEqual(Replacement->GetActorRotation().Roll, 90.0));
        Board->SetFigureDowned(Member, false);
        AActor* Restored = Board->Figures.FindRef(Member);
        TestTrue(TEXT("standing restores the native class"), Restored && Restored != Stub && Restored->IsA<ACharacter>());
    }
    World->DestroyWorld(false);
    return !HasAnyErrors();
}
#endif
