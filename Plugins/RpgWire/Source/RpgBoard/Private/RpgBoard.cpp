#include "RpgBoard.h"
#include "RpgLook.h"
#include "RpgManifest.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "Materials/Material.h"
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

    struct FShape
    {
        TArray<FVector> Vertices;
        TArray<int32> Triangles;
        TArray<FVector> Normals;
        void Triangle(int32 A, int32 B, int32 C) { Triangles.Append({A, B, C, A, C, B}); }
        void Vertex(const FVector& P, const FVector& N) { Vertices.Add(P); Normals.Add(N); }
    };
    FShape MakeBox(FVector Size)
    {
        FShape Shape;
        const FVector H = Size / 2.0;
        for (int32 I = 0; I < 8; ++I) Shape.Vertex(FVector((I & 1) ? H.X : -H.X, (I & 2) ? H.Y : -H.Y, (I & 4) ? H.Z : -H.Z), FVector::UpVector);
        for (const auto& T : {FIntVector(0,2,3), FIntVector(0,3,1), FIntVector(4,5,7), FIntVector(4,7,6),
                              FIntVector(0,1,5), FIntVector(0,5,4), FIntVector(2,6,7), FIntVector(2,7,3),
                              FIntVector(0,4,6), FIntVector(0,6,2), FIntVector(1,3,7), FIntVector(1,7,5)}) Shape.Triangle(T.X,T.Y,T.Z);
        return Shape;
    }
    FShape MakeCylinder(double Radius, double Height, int32 Segments = 24)
    {
        FShape Shape;
        for (int32 I = 0; I < Segments; ++I)
        {
            const double A = 2.0 * PI * I / Segments;
            const FVector N(FMath::Cos(A), FMath::Sin(A), 0);
            Shape.Vertex(FVector(N.X * Radius, N.Y * Radius, -Height / 2), N);
            Shape.Vertex(FVector(N.X * Radius, N.Y * Radius, Height / 2), N);
        }
        const int32 Bottom = Shape.Vertices.Num(); Shape.Vertex(FVector(0,0,-Height/2), -FVector::UpVector);
        const int32 Top = Shape.Vertices.Num(); Shape.Vertex(FVector(0,0,Height/2), FVector::UpVector);
        for (int32 I = 0; I < Segments; ++I)
        {
            const int32 A = I * 2, B = ((I + 1) % Segments) * 2;
            Shape.Triangle(A,B,B+1); Shape.Triangle(A,B+1,A+1);
            Shape.Triangle(Bottom,B,A); Shape.Triangle(Top,A+1,B+1);
        }
        return Shape;
    }
    FShape MakeSphere(double Radius, int32 Segments = 16, int32 Rings = 8)
    {
        FShape Shape;
        for (int32 R = 0; R <= Rings; ++R)
        {
            const double Phi = PI * R / Rings;
            for (int32 I = 0; I < Segments; ++I)
            {
                const double A = 2.0 * PI * I / Segments;
                const FVector N(FMath::Sin(Phi)*FMath::Cos(A), FMath::Sin(Phi)*FMath::Sin(A), FMath::Cos(Phi));
                Shape.Vertex(N * Radius, N);
            }
        }
        for (int32 R = 0; R < Rings; ++R) for (int32 I = 0; I < Segments; ++I)
        {
            const int32 A = R*Segments+I, B = R*Segments+(I+1)%Segments, C = A+Segments, D = B+Segments;
            Shape.Triangle(A,C,D); Shape.Triangle(A,D,B);
        }
        return Shape;
    }
    UProceduralMeshComponent* MeshOf(AActor* Actor) { return Actor ? Actor->FindComponentByClass<UProceduralMeshComponent>() : nullptr; }
    void ApplyShape(AActor& Actor, const FShape& Shape, const FLinearColor& Colour, bool Collision = true)
    {
        auto* Mesh = MeshOf(&Actor);
        if (!Mesh) return;
        TArray<FColor> Colours;
        Colours.Init(Colour.ToFColor(false), Shape.Vertices.Num());
        Mesh->CreateMeshSection(0, Shape.Vertices, Shape.Triangles, Shape.Normals, {}, Colours, {}, Collision);
        Mesh->SetMaterial(0, LoadObject<UMaterial>(nullptr, TEXT("/Engine/EngineDebugMaterials/VertexColorViewMode_ColorOnly.VertexColorViewMode_ColorOnly")));
    }
    AActor* SpawnShape(UWorld* World, const FVector& Location)
    {
        AActor* Actor = World->SpawnActor<AActor>(Location, FRotator::ZeroRotator);
        if (!Actor) return nullptr;
        auto* Mesh = NewObject<UProceduralMeshComponent>(Actor);
        Actor->SetRootComponent(Mesh);
        Actor->AddInstanceComponent(Mesh);
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->bUseComplexAsSimpleCollision = true;
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Mesh->RegisterComponent();
        Actor->SetActorLocation(Location);
        return Actor;
    }
    FShape FigureShape(ERpgSide Side)
    {
        if (Side == ERpgSide::Player) return MakeCylinder(30,100);
        if (Side == ERpgSide::Monster) return MakeBox(FVector(60,60,100));
        return MakeSphere(50);
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

void ARpgBoard::SetManifest(const FRpgManifest& InManifest)
{
    Manifest = MakeShared<FRpgManifest>(InManifest);
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
        const FColor Tint = (Cell == StartCell ? RpgLook::kFloorStart : RpgLook::kFloor).ToFColor(false);
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
    UMaterial* FloorMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Engine/EngineDebugMaterials/VertexColorViewMode_ColorOnly.VertexColorViewMode_ColorOnly"));
    Floor->SetMaterial(0, FloorMaterial);
    UE_LOG(LogRpgPlay, Display, TEXT("FLOOR cells=%d material=%s"), KnownCells.Num(), FloorMaterial ? TEXT("VertexColorViewMode_ColorOnly") : TEXT("default"));
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
        Camera->PostProcessSettings.AutoExposureBias = 9.95f;
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
        if (NativeFigures.Contains(Pair.Key))
        {
            // Collision capsules and Blueprint demo cameras are not rendered subjects.
            FBox Bounds(ForceInit);
            TInlineComponentArray<UMeshComponent*> Meshes(Pair.Value.Get());
            for (const UMeshComponent* Mesh : Meshes)
                if (Mesh->IsVisible() && !Mesh->bHiddenInGame) Bounds += Mesh->Bounds.GetBox();
            Out.Add({Pair.Key, Bounds, false, TEXT("NATIVE")});
        }
        else
        {
            Out.Add({Pair.Key, Pair.Value->GetComponentsBoundingBox(true), false,
                RpgLook::HexOf(RpgLook::FigureColour(*Side, Words.FindRef(Pair.Key), Pair.Key == Marked, Downed.FindRef(Pair.Key)))});
        }
    }
    auto OccupiedAt = [this](const FIntPoint& Cell)
    {
        for (const auto& Figure : Figures)
            if (IsValid(Figure.Value.Get()) && CellAt(FVector2D(Figure.Value->GetActorLocation())) == Cell) return true;
        return false;
    };
    auto CellBox = [](const FIntPoint& Cell) { const FVector Point = At(Cell, 2.0); return FBox(Point - FVector(1, 1, 0), Point + FVector(1, 1, 0)); };
    Out.Add({TEXT("start"), CellBox(StartCell), true, RpgLook::HexOf(RpgLook::kFloorStart)});
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
    PlaceFigure(Member, Side, Cell, nullptr);
}

AActor* ARpgBoard::SpawnNative(UClass* Class, const FVector& Location) const
{
    if (!Class || !Class->IsChildOf(ACharacter::StaticClass())) return nullptr;
    FActorSpawnParameters Parameters;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ACharacter* Figure = GetWorld()->SpawnActor<ACharacter>(Class, Location, FRotator(0, RpgLook::kKingdomCameraFacingYaw, 0), Parameters);
    if (!Figure) return nullptr;
    Figure->GetCharacterMovement()->DisableMovement(); // RpgMotion owns board transforms.
    FVector Placed = Location;
    Placed.Z = Figure->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    Figure->SetActorLocation(Placed, false, nullptr, ETeleportType::TeleportPhysics);
    return Figure;
}

void ARpgBoard::PlaceFigure(const FString& Member, ERpgSide Side, const FIntPoint& Cell, const rpgwire::Member* Identity)
{
    if (HasFigure(Member)) return;
    const FString Asset = Manifest && Identity ? Manifest->StandingFor(*Identity) : FString();
    if (!Asset.IsEmpty())
    {
        const FString ClassPath = Asset + TEXT(".") + FPaths::GetBaseFilename(Asset) + TEXT("_C");
        if (AActor* Figure = SpawnNative(StaticLoadClass(AActor::StaticClass(), nullptr, ClassPath), At(Cell, 0.0)))
        {
            Figures.Add(Member, Figure);
            NativeFigures.Add(Member);
            NativeClasses.Add(Member, Figure->GetClass());
            SetFigureSide(Member, Side);
            UE_LOG(LogRpgPlay, Display, TEXT("FIGURE %s native=%s"), *Member, *Asset);
            UE_LOG(LogRpgPlay, Display, TEXT("FOOTING %s capsule=%.1f yaw=%.1f"), *Member, Figure->GetActorLocation().Z, Figure->GetActorRotation().Yaw);
            return;
        }
    }
    if (AActor* Figure = SpawnShape(GetWorld(), At(Cell, 50.0)))
    {
        Figures.Add(Member, Figure);
        SetFigureSide(Member, Side);
        const TCHAR* Shape = Side == ERpgSide::Player ? TEXT("cylinder") : Side == ERpgSide::Monster ? TEXT("box") : TEXT("sphere");
        UE_LOG(LogRpgPlay, Display, TEXT("SHAPE %s %s"), *Member, Shape);
        UE_LOG(LogRpgPlay, Display, TEXT("FIGURE %s shape"), *Member);
    }
}

bool ARpgBoard::HasFigure(const FString& Member) const { return IsValid(Figures.FindRef(Member).Get()); }

void ARpgBoard::RemoveFigure(const FString& Member)
{
    AActor* Figure = Figures.FindRef(Member);
    if (IsValid(Figure)) Figure->Destroy();
    Figures.Remove(Member);
    NativeFigures.Remove(Member);
    NativeClasses.Remove(Member);
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
    AActor* Figure = Figures.FindRef(Member);
    const ERpgSide* Previous = Sides.Find(Member);
    if (!IsValid(Figure) || (Previous && *Previous == Side)) return;
    const TCHAR* Name = Side == ERpgSide::Player ? TEXT("player") : Side == ERpgSide::Monster ? TEXT("monster") : TEXT("npc");
    Sides.Add(Member, Side);
    ApplyColour(Member);
    UE_LOG(LogRpgPlay, Display, TEXT("FIGURE %s side=%s"), *Member, Name);
}

void ARpgBoard::ApplyColour(const FString& Member)
{
    AActor* Figure = Figures.FindRef(Member);
    const ERpgSide* Side = Sides.Find(Member);
    if (!IsValid(Figure) || !Side || NativeFigures.Contains(Member)) return;
    const FLinearColor Colour = RpgLook::FigureColour(*Side, Words.FindRef(Member), Member == Marked, Downed.FindRef(Member));
    ApplyShape(*Figure, FigureShape(*Side), Colour);
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
    AActor* Figure = Figures.FindRef(Member);
    if (!IsValid(Figure)) return;
    const bool WasDowned = Downed.FindRef(Member);
    if (WasDowned == IsDowned) return;
    if (UClass* NativeClass = NativeClasses.FindRef(Member))
    {
        FVector Location = Figure->GetActorLocation();
        Location.Z = 50.0;
        AActor* Next = IsDowned ? SpawnShape(GetWorld(), Location) : SpawnNative(NativeClass, Location);
        if (!Next) return;
        Figure->Destroy();
        Figures.Add(Member, Next);
        Figure = Next;
        if (IsDowned)
        {
            NativeFigures.Remove(Member);
            UE_LOG(LogRpgPlay, Display, TEXT("FIGURE %s downed=shape"), *Member);
        }
        else
        {
            NativeFigures.Add(Member);
            Downed.Add(Member, false);
            UE_LOG(LogRpgPlay, Display, TEXT("FIGURE %s native=%s"), *Member, *NativeClass->GetOutermost()->GetName());
            if (FMotion* Motion = Playing.Find(Member)) Motion->Start.Z = Motion->End.Z = Figure->GetActorLocation().Z;
            UpdateMarker();
            return;
        }
    }
    Downed.Add(Member, IsDowned);
    FRotator Rotation = Figure->GetActorRotation();
    Rotation.Roll = IsDowned ? 90.0 : 0.0;
    Figure->SetActorRotation(Rotation);
    FVector Location = Figure->GetActorLocation();
    Location.Z = IsDowned && Sides.FindRef(Member) != ERpgSide::Npc ? 30.0 : 50.0;
    Figure->SetActorLocation(Location);
    if (FMotion* Motion = Playing.Find(Member)) Motion->Start.Z = Motion->End.Z = Location.Z;
    ApplyColour(Member);
    UpdateMarker();
}
void ARpgBoard::MoveMarkerTo(const FString& Whose)
{
    const FString Old = Marked;
    Marked = Whose;
    ApplyColour(Old);
    ApplyColour(Marked);
    if (!Marker && !Marked.IsEmpty())
    {
        Marker = SpawnShape(GetWorld(), FVector::ZeroVector);
        if (Marker)
        {
            ApplyShape(*Marker, MakeCylinder(90, 2), RpgLook::kPlayerActive, false);
            Marker->SetActorEnableCollision(false);
        }
    }
    UpdateMarker();
}
void ARpgBoard::UpdateMarker()
{
    if (!Marker) return;
    const AActor* Figure = Marked.IsEmpty() ? nullptr : Figures.FindRef(Marked).Get();
    Marker->SetActorHiddenInGame(!IsValid(Figure));
    if (IsValid(Figure)) { FVector Pos = Figure->GetActorLocation(); Pos.Z = 1.0; Marker->SetActorLocation(Pos); }
}
void ARpgBoard::BuildGround()
{
    if (IsValid(Ground.Get())) Ground->Destroy();
    Ground = SpawnShape(GetWorld(), FVector(0, 0, -2));
    if (Ground)
    {
        ApplyShape(*Ground, MakeBox(FVector(40000, 40000, 2)), RpgLook::kGround, false);
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
        AActor* Actor = SpawnShape(GetWorld(), Wall.Centre);
        if (!Actor) continue;
        Actor->SetActorRotation(FRotator(0, Wall.Yaw, 0));
        ApplyShape(*Actor, MakeBox(Wall.Size), Wall.Door ? RpgLook::kWood : RpgLook::kStone);
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
    AActor* Figure = Figures.FindRef(Step.member);
    if (!IsValid(Figure)) return;
    FMotion& Motion = Playing.Add(Step.member);
    Motion.Step = Step;
    Motion.Start = Figure->GetActorLocation();
    Motion.End = At(Step.to, Motion.Start.Z);
    if (Step.kind == FMotionStep::Kind::Swing)
    {
        const AActor* Target = Figures.FindRef(Step.target);
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
        AActor* Figure = Figures.FindRef(It.Key());
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
        UPrimitiveComponent* Surface = MeshOf(Pair.Value.Get());
        if (auto* Character = Cast<ACharacter>(Pair.Value.Get())) Surface = Character->GetCapsuleComponent();
        if (Surface && Surface->LineTraceComponent(Hit, Origin, End, FCollisionQueryParams()) && Hit.Distance < Nearest)
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
    NativeFigures.Reset();
    NativeClasses.Reset();
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
