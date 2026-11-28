#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RpgMotion.h"
#include "RpgBoard.generated.h"

class ACameraActor;
class UProceduralMeshComponent;

enum class ERpgSide : uint8 { Player, Npc, Monster };
struct FRpgWall { FString Id; FVector Centre; FVector Size; double Yaw = 0.0; bool Door = false; };
struct FRpgShotSubject { FString What; FBox Box; bool Point = false; FString Hex; };
RPGBOARD_API ERpgSide SideOf(int Kind, bool HasClassRef, bool HasMonsterRef);
RPGBOARD_API bool ReadSighted(const FString& FieldsJson, const FString& Observer, TArray<FString>& OutGained, TArray<FString>& OutLost);
RPGBOARD_API FString StatusLine(const FString& WhoseTurn, bool TurnClock, const FString& LastWords);

UCLASS()
class RPGBOARD_API ARpgBoard final : public AActor
{
    GENERATED_BODY()

public:
    ARpgBoard();
    static constexpr double kWalkSeconds = 0.40;
    static constexpr double kSwingSeconds = 0.50;

    void BuildFloor(const TArray<FIntPoint>& Cells, const FIntPoint& StartCell);
    void ShotSubjects(TArray<FRpgShotSubject>& Out) const;
    void BuildWalls(const TArray<FRpgWall>& Walls);
    void BuildGround();
    void SetFigureWord(const FString& Member, const FString& Word);
    void SetFigureDowned(const FString& Member, bool Downed);
    void PlaceFigure(const FString& Member, ERpgSide Side, const FIntPoint& Cell);
    void MoveMarkerTo(const FString& Whose);
    void Enqueue(const TArray<FMotionStep>& Steps);
    const FIntPoint* CellOf(const FString& Member) const;
    bool FigureUnder(const FVector& Origin, const FVector& Direction, FString& Member) const;
    bool FloorUnder(const FVector& Origin, const FVector& Direction, FIntPoint& Cell) const;
    ACameraActor* Camera() const;
    void Clear();

    void Tick(float DeltaSeconds) override;

private:
    friend class URpgPlay;
    struct FMotion
    {
        FMotionStep Step;
        FVector Start = FVector::ZeroVector;
        FVector End = FVector::ZeroVector;
        double Elapsed = 0.0;
    };
    void UpdateMarker();
    void ApplyColour(const FString& Member);
    void RefitCamera();
    bool HasFigure(const FString& Member) const;
    void RemoveFigure(const FString& Member);
    void SetFigureSide(const FString& Member, ERpgSide Side);
    void BeginMotion(const FMotionStep& Step);
    TMap<FString, FIntPoint> Cells;
    FIntPoint StartCell = FIntPoint::ZeroValue;
    TArray<FIntPoint> KnownCells;
    FString Marked;
    TArray<FMotionStep> Waiting;
    TMap<FString, FMotion> Playing;
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> Floor;
    UPROPERTY() TObjectPtr<ACameraActor> ViewCamera;
    UPROPERTY() TObjectPtr<AActor> Marker;
    TMap<FString, ERpgSide> Sides;
    TMap<FString, FString> Words;
    TMap<FString, bool> Downed;
    FBox FloorBounds = FBox(ForceInit);
    UPROPERTY() TArray<TObjectPtr<AActor>> Walls;
    UPROPERTY() TObjectPtr<AActor> Ground;
    UPROPERTY() TMap<FString, TObjectPtr<AActor>> Figures;
};
