#include "RpgBoard.h"
#include "RpgLook.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "RpgPlay.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgSideTest, "RpgBoard.Figure.TheSideComesFromTheKindThenTheReferences",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSideTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("player kind overrides creature reference"), SideOf(1, false, true) == ERpgSide::Player);
    TestTrue(TEXT("monster kind overrides class reference"), SideOf(2, true, false) == ERpgSide::Monster);
    TestTrue(TEXT("world kind overrides both references"), SideOf(3, true, true) == ERpgSide::Npc);
    TestTrue(TEXT("a class reference identifies a player"), SideOf(0, true, false) == ERpgSide::Player);
    TestTrue(TEXT("a creature reference identifies a monster"), SideOf(0, false, true) == ERpgSide::Monster);
    TestTrue(TEXT("neither reference identifies an NPC"), SideOf(0, false, false) == ERpgSide::Npc);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgSightTest, "RpgBoard.Sight.ThePinnedPayloadNamesWhoWasGainedAndLostForItsObserverOnly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSightTest::RunTest(const FString& Parameters)
{
    const FString First(TEXT("{\"beat\":\"sighted\",\"gained\":[\"pedlar\"],\"knowledge_changed\":true,\"observer\":\"alice\"}"));
    const FString Loss(TEXT("{\"beat\":\"sighted\",\"gained\":[],\"lost\":[\"gate-1\"],\"observer\":\"alice\"}"));
    TArray<FString> Gained, Lost;
    TestTrue(TEXT("Alice gains the pedlar"), ReadSighted(First, TEXT("alice"), Gained, Lost));
    TestTrue(TEXT("only the pedlar was gained"), Gained == TArray<FString>{TEXT("pedlar")} && Lost.IsEmpty());
    TestTrue(TEXT("Alice loses a guard"), ReadSighted(Loss, TEXT("alice"), Gained, Lost));
    TestTrue(TEXT("only gate-1 was lost"), Gained.IsEmpty() && Lost == TArray<FString>{TEXT("gate-1")});
    TestFalse(TEXT("another observer cannot use the loss"), ReadSighted(Loss, TEXT("bob"), Gained, Lost));
    TestTrue(TEXT("refusal leaves outputs untouched"), Gained.IsEmpty() && Lost == TArray<FString>{TEXT("gate-1")});
    TestFalse(TEXT("malformed JSON is refused"), ReadSighted(TEXT("{"), TEXT("alice"), Gained, Lost));
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

    bool ApplyShape(AStaticMeshActor& Actor, const TCHAR* Path, const FLinearColor& Color)
    {
        UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Path);
        if (!Mesh) return false;
        UStaticMeshComponent* Component = Actor.GetStaticMeshComponent();
        Component->SetStaticMesh(Mesh);
        if (UMaterial* Material = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
        {
            UMaterialInstanceDynamic* Tint = UMaterialInstanceDynamic::Create(Material, Component);
            Tint->SetVectorParameterValue(TEXT("Color"), Color);
            Component->SetMaterial(0, Tint);
        }
        return true;
    }

    AStaticMeshActor* SpawnShape(UWorld* World, const FVector& Location, const FVector& Scale)
    {
        AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator);
        if (Actor)
        {
            Actor->SetMobility(EComponentMobility::Movable);
            Actor->SetActorScale3D(Scale);
        }
        return Actor;
    }

    bool ReadNames(const FJsonObject& Payload, const TCHAR* Key, TArray<FString>& Names)
    {
        if (!Payload.HasField(Key)) return true;
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Payload.TryGetArrayField(Key, Values)) return false;
        for (const auto& Value : *Values)
        {
            FString Name;
            if (!Value->TryGetString(Name) || Name.IsEmpty()) return false;
            Names.Add(MoveTemp(Name));
        }
        return true;
    }
}

namespace RpgLook
{
FLinearColor FigureColour(ERpgSide Side, const FString& Word, bool Active, bool Downed)
{
    if (Downed) return kDowned;
    if (Word.Equals(TEXT("hostile"), ESearchCase::IgnoreCase)) Side = ERpgSide::Monster;
    else if (Word.Equals(TEXT("allied"), ESearchCase::IgnoreCase)) Side = ERpgSide::Npc;
    if (Side == ERpgSide::Player) return Active ? kPlayerActive : kPlayer;
    if (Side == ERpgSide::Monster) return Active ? kMonsterActive : kMonster;
    return Active ? kNpcActive : kNpc;
}
FString HexOf(const FLinearColor& Colour) { return TEXT("#") + Colour.ToFColor(true).ToHex().Left(6); }
FVector BoxCentre(double X, double Y, double Elevation, double Height) { return FVector(X, Y, Elevation + Height / 2.0) * kCmPerFoot; }
FVector BoxSize(double Depth, double Width, double Height) { return FVector(Depth, Width, Height) * kCmPerFoot; }
}

ERpgSide SideOf(int Kind, bool HasClassRef, bool HasMonsterRef)
{
    if (Kind == 1 || (Kind == 0 && HasClassRef)) return ERpgSide::Player;
    if (Kind == 2 || (Kind == 0 && HasMonsterRef)) return ERpgSide::Monster;
    return ERpgSide::Npc;
}

bool ReadSighted(const FString& FieldsJson, const FString& Observer, TArray<FString>& OutGained, TArray<FString>& OutLost)
{
    TSharedPtr<FJsonObject> Payload;
    FString Beat, NamedObserver;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FieldsJson), Payload) || !Payload.IsValid() ||
        !Payload->TryGetStringField(TEXT("beat"), Beat) || Beat != TEXT("sighted") ||
        !Payload->TryGetStringField(TEXT("observer"), NamedObserver) || NamedObserver != Observer) return false;
    TArray<FString> Gained, Lost;
    if (!ReadNames(*Payload, TEXT("gained"), Gained) || !ReadNames(*Payload, TEXT("lost"), Lost)) return false;
    OutGained = MoveTemp(Gained);
    OutLost = MoveTemp(Lost);
    return true;
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

void ARpgBoard::BuildFloor(const TArray<FIntPoint>& InCells, const FIntPoint& InStartCell)
{
    KnownCells = InCells;
    StartCell = InStartCell;
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
        const FColor Tint = (Cell == StartCell ? RpgLook::kFloorStart : RpgLook::kFloor).ToFColor(true);
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
    UMaterial* FloorMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
    Floor->SetMaterial(0, FloorMaterial);
    UE_LOG(LogRpgPlay, Display, TEXT("FLOOR cells=%d material=%s"), KnownCells.Num(), FloorMaterial ? TEXT("VertexColorMaterial") : TEXT("default"));
    Floor->CreateMeshSection(0, Vertices, Triangles, Normals, {}, Colors, {}, true);
    FloorBounds = CellBox.IsValid ? CellBox.ExpandBy(kRadius) : FBox(ForceInit);
    RefitCamera();
}

void ARpgBoard::RefitCamera()
{
    FBox Revealed = FloorBounds;
    if (!Revealed.IsValid) return;
    const FVector Middle = Revealed.GetCenter();
    const double Width = FMath::Max(Revealed.GetSize().X, 400.0);
    const double Depth = FMath::Max(Revealed.GetSize().Y, 400.0);
    const double HalfHorizontal = FMath::DegreesToRadians(42.0 / 2.0);
    const double HalfVertical = FMath::Atan(FMath::Tan(HalfHorizontal) / (16.0 / 9.0));
    const double Margin = 400.0;
    const double Distance = FMath::Max((Width / 2.0 + Margin) / FMath::Tan(HalfHorizontal),
        (Depth / 2.0 + Margin) / FMath::Tan(HalfVertical));
    const double Pitch = FMath::DegreesToRadians(60.0);
    const FVector Location = Middle + FVector(0.0, -Distance * FMath::Cos(Pitch), Distance * FMath::Sin(Pitch));
    const FRotator Rotation(-60.0, 90.0, 0.0);
    if (!ViewCamera) ViewCamera = GetWorld()->SpawnActor<ACameraActor>(Location, Rotation);
    else ViewCamera->SetActorLocationAndRotation(Location, Rotation);
    if (ViewCamera)
    {
        auto* Camera = ViewCamera->GetCameraComponent();
        Camera->SetFieldOfView(42.0f);
        Camera->PostProcessSettings.bOverride_AutoExposureMethod = true;
        Camera->PostProcessSettings.AutoExposureMethod = AEM_Manual;
        Camera->PostProcessSettings.bOverride_AutoExposureBias = true;
        Camera->PostProcessSettings.AutoExposureBias = 7.62f;
    }
}

void ARpgBoard::ShotSubjects(TArray<FRpgShotSubject>& Out) const
{
    Out.Reset();
    for (const auto& Pair : Figures)
    {
        if (!IsValid(Pair.Value.Get())) continue;
        const ERpgSide* Side = Sides.Find(Pair.Key);
        if (!Side) continue;
        Out.Add({Pair.Key, Pair.Value->GetComponentsBoundingBox(true), false, RpgLook::HexOf(RpgLook::FigureColour(*Side, Words.FindRef(Pair.Key), Pair.Key == Marked, Downed.FindRef(Pair.Key)))});
    }
    auto OccupiedAt = [this](const FIntPoint& Cell)
    {
        for (const auto& Figure : Figures)
            if (IsValid(Figure.Value.Get()) && CellAt(FVector2D(Figure.Value->GetActorLocation())) == Cell) return true;
        return false;
    };
    auto CellBox = [](const FIntPoint& Cell) { const FVector Point = At(Cell, 2.0); return FBox(Point - FVector(1, 1, 0), Point + FVector(1, 1, 0)); };
    if (!OccupiedAt(StartCell)) Out.Add({TEXT("start"), CellBox(StartCell), true, RpgLook::HexOf(RpgLook::kFloorStart)});
    int Added = 0;
    for (const FIntPoint& Cell : KnownCells)
    {
        if (Cell == StartCell) continue;
        if (OccupiedAt(Cell)) continue;
        Out.Add({FString::Printf(TEXT("cell-%d-%d"), Cell.X, Cell.Y), CellBox(Cell), true, RpgLook::HexOf(RpgLook::kFloor)});
        if (++Added == 2) break;
    }
}

void ARpgBoard::PlaceFigure(const FString& Member, ERpgSide Side, const FIntPoint& Cell)
{
    if (HasFigure(Member)) return;
    if (AStaticMeshActor* Figure = SpawnShape(GetWorld(), At(Cell, 51.0), FVector(0.60, 0.60, 1.0)))
    {
        Figures.Add(Member, Figure);
        SetFigureSide(Member, Side);
    }
}

bool ARpgBoard::HasFigure(const FString& Member) const { return IsValid(Figures.FindRef(Member).Get()); }

void ARpgBoard::RemoveFigure(const FString& Member)
{
    AStaticMeshActor* Figure = Figures.FindRef(Member);
    if (IsValid(Figure)) Figure->Destroy();
    Figures.Remove(Member);
    Sides.Remove(Member);
    Words.Remove(Member);
    Downed.Remove(Member);
    Playing.Remove(Member);
    Waiting.RemoveAll([&Member](const FMotionStep& Step)
    {
        return Step.member == Member && Step.kind != FMotionStep::Kind::Turn;
    });
    UpdateMarker();
}

void ARpgBoard::SetFigureSide(const FString& Member, ERpgSide Side)
{
    AStaticMeshActor* Figure = Figures.FindRef(Member);
    const ERpgSide* Previous = Sides.Find(Member);
    if (!IsValid(Figure) || (Previous && *Previous == Side)) return;
    const TCHAR* Path = TEXT("/Engine/BasicShapes/Sphere.Sphere");
    const TCHAR* Name = TEXT("npc");
    FLinearColor Tint = RpgLook::FigureColour(Side, Words.FindRef(Member), Member == Marked, Downed.FindRef(Member));
    if (Side == ERpgSide::Player)
    {
        Path = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
        Name = TEXT("player");

    }
    else if (Side == ERpgSide::Monster)
    {
        Path = TEXT("/Engine/BasicShapes/Cube.Cube");
        Name = TEXT("monster");

    }
    if (ApplyShape(*Figure, Path, Tint))
    {
        Sides.Add(Member, Side);
        ApplyColour(Member);
        UE_LOG(LogRpgPlay, Display, TEXT("FIGURE %s side=%s"), *Member, Name);
    }
}

void ARpgBoard::ApplyColour(const FString& Member)
{
    AStaticMeshActor* Figure = Figures.FindRef(Member);
    const ERpgSide* Side = Sides.Find(Member);
    if (!IsValid(Figure) || !Side) return;
    const FLinearColor Colour = RpgLook::FigureColour(*Side, Words.FindRef(Member), Member == Marked, Downed.FindRef(Member));
    UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Figure->GetStaticMeshComponent()->GetMaterial(0));
    if (Material) Material->SetVectorParameterValue(TEXT("Color"), Colour);
    UE_LOG(LogRpgPlay, Display, TEXT("COLOUR %s %s"), *Member, *RpgLook::HexOf(Colour));
}
void ARpgBoard::SetFigureWord(const FString& Member, const FString& Word)
{
    if (!HasFigure(Member)) return;
    Words.Add(Member, Word);
    ApplyColour(Member);
}
void ARpgBoard::SetFigureDowned(const FString& Member, bool IsDowned)
{
    AStaticMeshActor* Figure = Figures.FindRef(Member);
    if (!IsValid(Figure)) return;
    Downed.Add(Member, IsDowned);
    FRotator Rotation = Figure->GetActorRotation();
    Rotation.Roll = IsDowned ? 90.0 : 0.0;
    Figure->SetActorRotation(Rotation);
    FVector Location = Figure->GetActorLocation();
    Location.Z = IsDowned ? 31.0 : 51.0;
    Figure->SetActorLocation(Location);
    ApplyColour(Member);
}
void ARpgBoard::MoveMarkerTo(const FString& Whose)
{
    const FString Old = Marked;
    Marked = Whose;
    ApplyColour(Old);
    ApplyColour(Marked);
    if (!Marker && !Marked.IsEmpty())
    {
        Marker = SpawnShape(GetWorld(), FVector::ZeroVector, FVector(1.8, 1.8, 0.02));
        if (Marker)
        {
            ApplyShape(*Marker, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), RpgLook::kPlayerActive);
            Marker->SetActorEnableCollision(false);
        }
    }
    UpdateMarker();
}
void ARpgBoard::UpdateMarker()
{
    if (!Marker) return;
    const AStaticMeshActor* Figure = Marked.IsEmpty() ? nullptr : Figures.FindRef(Marked).Get();
    Marker->SetActorHiddenInGame(!IsValid(Figure));
    if (IsValid(Figure)) { FVector Pos = Figure->GetActorLocation(); Pos.Z = 1.0; Marker->SetActorLocation(Pos); }
}
void ARpgBoard::BuildGround()
{
    if (IsValid(Ground.Get())) Ground->Destroy();
    Ground = SpawnShape(GetWorld(), FVector(0, 0, -2), FVector(400, 400, 1));
    if (Ground && ApplyShape(*Ground, TEXT("/Engine/BasicShapes/Plane.Plane"), RpgLook::kGround))
    {
        Ground->SetActorEnableCollision(false);
        UE_LOG(LogRpgPlay, Display, TEXT("GROUND ok"));
    }
}
void ARpgBoard::BuildWalls(const TArray<FRpgWall>& InWalls)
{
    for (const auto& Wall : Walls) if (IsValid(Wall.Get())) Wall->Destroy();
    Walls.Reset();
    for (const FRpgWall& Wall : InWalls)
    {
        AStaticMeshActor* Actor = SpawnShape(GetWorld(), Wall.Centre, Wall.Size / 100.0);
        if (!Actor) continue;
        Actor->SetActorRotation(FRotator(0, Wall.Yaw, 0));
        if (!ApplyShape(*Actor, TEXT("/Engine/BasicShapes/Cube.Cube"), Wall.Door ? RpgLook::kWood : RpgLook::kStone)) { Actor->Destroy(); continue; }
        Walls.Add(Actor);
    }
    UE_LOG(LogRpgPlay, Display, TEXT("WALLS drawn=%d"), Walls.Num());
    RefitCamera();
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
    Sides.Reset();
    Words.Reset();
    Downed.Reset();
    for (const auto& Wall : Walls) if (IsValid(Wall.Get())) Wall->Destroy();
    Walls.Reset();
    if (IsValid(Ground.Get())) Ground->Destroy();
    Ground = nullptr;
    FloorBounds = FBox(ForceInit);
    KnownCells.Reset();
    if (IsValid(Marker.Get())) Marker->Destroy();
    Marker = nullptr;
    Floor->ClearAllMeshSections();
}
