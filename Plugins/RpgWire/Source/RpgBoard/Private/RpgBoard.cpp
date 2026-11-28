#include "RpgBoard.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgSideTest, "RpgBoard.Figure.TheSideComesFromTheReferences",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSideTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("a class reference identifies a player"), SideOf(true, false) == ERpgSide::Player);
    TestTrue(TEXT("a creature reference identifies a monster"), SideOf(false, true) == ERpgSide::Monster);
    TestTrue(TEXT("neither reference identifies an NPC"), SideOf(false, false) == ERpgSide::Npc);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgStatusTest, "RpgBoard.Status.TheLineNamesWhoseTurnTheClockAndTheLastWords",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgStatusTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("the line shows the turn clock and last words"),
        StatusLine(TEXT("ue-fighter"), true, TEXT("no target in reach")),
        FString(TEXT("turn: ue-fighter · turn clock · no target in reach")));
    TestEqual(TEXT("the line shows the world clock"),
        StatusLine(TEXT("ue-fighter"), false, TEXT("no target in reach")),
        FString(TEXT("turn: ue-fighter · world clock · no target in reach")));
    TestEqual(TEXT("empty words leave no extra separator"), StatusLine(TEXT("ue-fighter"), true, FString()),
        FString(TEXT("turn: ue-fighter · turn clock")));
    return !HasAnyErrors();
}
#endif

namespace
{
    constexpr double kRadius = 100.0;
    constexpr double kTraceDistance = 10000000.0;
    constexpr double kLungeCm = 40.0;

    FVector At(const FIntPoint& Cell, double Z)
    {
        return FVector(CentreOf(Cell), Z);
    }

    AStaticMeshActor* Shape(UWorld* World, const TCHAR* Path, const FVector& Location,
        const FVector& Scale, const FLinearColor& Color)
    {
        UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Path);
        if (!Mesh) return nullptr;
        AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator);
        if (!Actor) return nullptr;
        Actor->SetMobility(EComponentMobility::Movable);
        Actor->SetActorScale3D(Scale);
        UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
        Component->SetStaticMesh(Mesh);
        if (UMaterial* Material = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
        {
            UMaterialInstanceDynamic* Tint = UMaterialInstanceDynamic::Create(Material, Component);
            Tint->SetVectorParameterValue(TEXT("Color"), Color);
            Component->SetMaterial(0, Tint);
        }
        return Actor;
    }
}

ERpgSide SideOf(bool HasClassRef, bool HasMonsterRef)
{
    if (HasClassRef) return ERpgSide::Player;
    if (HasMonsterRef) return ERpgSide::Monster;
    return ERpgSide::Npc;
}

FString StatusLine(const FString& WhoseTurn, bool TurnClock, const FString& LastWords)
{
    FString Line = FString::Printf(TEXT("turn: %s · %s clock"), *WhoseTurn, TurnClock ? TEXT("turn") : TEXT("world"));
    if (!LastWords.IsEmpty()) Line += TEXT(" · ") + LastWords;
    return Line;
}

ARpgBoard::ARpgBoard()
{
    PrimaryActorTick.bCanEverTick = true;
    Floor = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("KnownCells"));
    SetRootComponent(Floor);
    Floor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Floor->SetCollisionObjectType(ECC_WorldStatic);
    Floor->bUseComplexAsSimpleCollision = true;
}

void ARpgBoard::BuildFloor(const TArray<FIntPoint>& KnownCells)
{
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FColor> Colors;
    FBox CellBox(ForceInit);
    for (const FIntPoint& Cell : KnownCells)
    {
        const FVector Centre = At(Cell, 0.0);
        CellBox += Centre;
        const int32 Base = Vertices.Num();
        const FColor Tint = Cell == StartCell ? FColor(170, 143, 107) : FColor(115, 115, 115);
        for (int32 Corner = 0; Corner < 6; ++Corner)
        {
            const double Angle = FMath::DegreesToRadians(30.0 + 60.0 * Corner);
            Vertices.Add(Centre + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0) * kRadius + FVector::UpVector);
            Normals.Add(FVector::UpVector);
            Colors.Add(Tint);
        }
        for (int32 Corner = 1; Corner < 5; ++Corner)
        {
            Triangles.Append({Base, Base + Corner, Base + Corner + 1, Base, Base + Corner + 1, Base + Corner});
        }
    }
    Floor->SetMaterial(0, LoadObject<UMaterial>(nullptr, TEXT("/Engine/EngineMaterials/VertexColorMaterial.VertexColorMaterial")));
    Floor->CreateMeshSection(0, Vertices, Triangles, Normals, {}, Colors, {}, true);
    if (!CellBox.IsValid) return;

    const FBox Revealed = CellBox.ExpandBy(kRadius);
    const FVector Middle = Revealed.GetCenter();
    const double Width = FMath::Max(Revealed.GetSize().X, 400.0);
    const double Depth = FMath::Max(Revealed.GetSize().Y, 400.0);
    const double HalfHorizontal = FMath::DegreesToRadians(42.0 / 2.0);
    const double HalfVertical = FMath::Atan(FMath::Tan(HalfHorizontal) / (16.0 / 9.0));
    const double Margin = 400.0;
    const double Distance = FMath::Max((Width / 2.0 + Margin) / FMath::Tan(HalfHorizontal),
        (Depth / 2.0 + Margin) / FMath::Tan(HalfVertical));
    const double Pitch = FMath::DegreesToRadians(60.0);
    ViewCamera = GetWorld()->SpawnActor<ACameraActor>(
        Middle + FVector(0.0, -Distance * FMath::Cos(Pitch), Distance * FMath::Sin(Pitch)), FRotator(-60.0, 90.0, 0.0));
    if (ViewCamera) ViewCamera->GetCameraComponent()->SetFieldOfView(42.0f);
}

void ARpgBoard::PlaceFigure(const FString& Member, ERpgSide Side, const FIntPoint& Cell)
{
    if (Figures.Contains(Member)) return;
    const TCHAR* Path = TEXT("/Engine/BasicShapes/Sphere.Sphere");
    FLinearColor Tint(0.46f, 0.46f, 0.46f);
    if (Side == ERpgSide::Player)
    {
        Path = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
        Tint = FLinearColor(0.06f, 0.25f, 0.95f);
    }
    else if (Side == ERpgSide::Monster)
    {
        Path = TEXT("/Engine/BasicShapes/Cube.Cube");
        Tint = FLinearColor(0.85f, 0.09f, 0.08f);
    }
    if (AStaticMeshActor* Figure = Shape(GetWorld(), Path, At(Cell, 51.0), FVector(0.60, 0.60, 1.0), Tint))
    {
        Figures.Add(Member, Figure);
    }
}

void ARpgBoard::MoveMarkerTo(const FString& Whose)
{
    Marked = Whose;
    if (!Marker && !Marked.IsEmpty())
    {
        Marker = Shape(GetWorld(), TEXT("/Engine/BasicShapes/Cone.Cone"), FVector::ZeroVector,
            FVector(0.27), FLinearColor::Yellow);
        if (Marker) Marker->SetActorEnableCollision(false);
    }
    UpdateMarker();
}

void ARpgBoard::UpdateMarker()
{
    if (!Marker) return;
    const AStaticMeshActor* Figure = Marked.IsEmpty() ? nullptr : Figures.FindRef(Marked).Get();
    Marker->SetActorHiddenInGame(!IsValid(Figure));
    if (IsValid(Figure)) Marker->SetActorLocation(Figure->GetActorLocation() + FVector(0.0, 0.0, 220.0));
}

void ARpgBoard::Enqueue(const TArray<FMotionStep>& Steps)
{
    Waiting.Append(Steps);
}

void ARpgBoard::BeginMotion(const FMotionStep& Step)
{
    if (Step.kind == FMotionStep::Kind::Turn)
    {
        MoveMarkerTo(Step.member);
        return;
    }
    AStaticMeshActor* Figure = Figures.FindRef(Step.member);
    if (!IsValid(Figure)) return;
    FMotion& Motion = Playing.Add(Step.member);
    Motion.Step = Step;
    Motion.Start = Figure->GetActorLocation();
    Motion.End = At(Step.to, Motion.Start.Z);
    if (Step.kind == FMotionStep::Kind::Swing)
    {
        const AStaticMeshActor* Target = Figures.FindRef(Step.target);
        const FVector Toward = IsValid(Target) ? Target->GetActorLocation() - Motion.Start : Motion.End - Motion.Start;
        Motion.End = Toward.GetSafeNormal2D();
        if (!Motion.End.IsNearlyZero()) Figure->SetActorRotation(Motion.End.Rotation());
    }
}

void ARpgBoard::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    for (auto It = Playing.CreateIterator(); It; ++It)
    {
        FMotion& Motion = It.Value();
        AStaticMeshActor* Figure = Figures.FindRef(It.Key());
        if (!IsValid(Figure))
        {
            It.RemoveCurrent();
            continue;
        }
        Motion.Elapsed += DeltaSeconds;
        const bool Walk = Motion.Step.kind == FMotionStep::Kind::Walk;
        const double Alpha = FMath::Min(1.0, Motion.Elapsed / (Walk ? kWalkSeconds : kSwingSeconds));
        Figure->SetActorLocation(Walk ? FMath::Lerp(Motion.Start, Motion.End, Alpha)
            : Motion.Start + Motion.End * (Alpha == 1.0 ? 0.0 : FMath::Sin(Alpha * PI) * kLungeCm));
        if (Alpha == 1.0) It.RemoveCurrent();
    }
    for (int32 Index = 0; Index < Waiting.Num();)
    {
        const FMotionStep& Step = Waiting[Index];
        if (Step.kind != FMotionStep::Kind::Turn && Playing.Contains(Step.member))
        {
            ++Index;
            continue;
        }
        BeginMotion(Step);
        Waiting.RemoveAt(Index);
    }
    UpdateMarker();
}

const FIntPoint* ARpgBoard::CellOf(const FString& Member) const { return Cells.Find(Member); }

bool ARpgBoard::FigureUnder(const FVector& Origin, const FVector& Direction, FString& Member) const
{
    const FVector End = Origin + Direction * kTraceDistance;
    double Nearest = kTraceDistance;
    bool Found = false;
    for (const auto& Pair : Figures)
    {
        if (!IsValid(Pair.Value.Get())) continue;
        FHitResult Hit;
        if (Pair.Value->GetStaticMeshComponent()->LineTraceComponent(Hit, Origin, End, FCollisionQueryParams()) && Hit.Distance < Nearest)
        {
            Nearest = Hit.Distance;
            Member = Pair.Key;
            Found = true;
        }
    }
    return Found;
}

bool ARpgBoard::FloorUnder(const FVector& Origin, const FVector& Direction, FIntPoint& Cell) const
{
    FHitResult Hit;
    if (!Floor->LineTraceComponent(Hit, Origin, Origin + Direction * kTraceDistance, FCollisionQueryParams())) return false;
    Cell = CellAt(FVector2D(Hit.ImpactPoint));
    return true;
}

ACameraActor* ARpgBoard::Camera() const { return ViewCamera; }

void ARpgBoard::Clear()
{
    Waiting.Reset();
    Playing.Reset();
    Cells.Reset();
    for (const auto& Pair : Figures)
    {
        if (IsValid(Pair.Value.Get())) Pair.Value->Destroy();
    }
    Figures.Reset();
    if (IsValid(Marker.Get())) Marker->Destroy();
    Marker = nullptr;
    Floor->ClearAllMeshSections();
}
