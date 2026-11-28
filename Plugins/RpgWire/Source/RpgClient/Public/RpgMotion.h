// RpgMotion: the two decisions a record-driven client makes about a beat, in
// functions with no world in them.
//
// The record says what happened; how it looks is the client's (design.md,
// O1). These keep the reading of it apart from the showing of it, so the
// reading can be tested against captured beats and the showing replaced.
#pragma once

#include "CoreMinimal.h"
#include "HAL/Platform.h"

/// One thing a figure does because of a beat.
struct RPGCLIENT_API FMotionStep
{
    enum class Kind { Walk, Swing, Turn };

    Kind kind = Kind::Walk;
    FString member;                     // who walks, swings, or acts next
    FString target;                     // a swing's target
    FIntPoint from = FIntPoint::ZeroValue; // axial (q, r)
    FIntPoint to = FIntPoint::ZeroValue;
};

/// Reads one beat and appends what it means for figures. `cells` is where
/// each figure stands, as the record has said so far: `joined_at` and `moved`
/// set it, and a `moved` beat names only the cell arrived at, so the cell it
/// left comes from here. Returns false for a beat that moves nobody (and
/// for one whose payload cannot be read), true otherwise.
RPGCLIENT_API bool PlanMotion(const FString& beat, const FString& fields_json, TMap<FString, FIntPoint>& cells,
                TArray<FMotionStep>& out);

/// The cells a walk from `from` to `to` takes on the hex board, both ends
/// included, each one adjacent to the next: the straight line in cube
/// coordinates, rounded. The engine prices and may refuse the path; this
/// only names it.
RPGCLIENT_API TArray<FIntPoint> HexLine(const FIntPoint& from, const FIntPoint& to);

/// Pointy-topped hexes of 100 cm circumradius. CentreOf names the point
/// at the centre of (q,r): (√3·100·(q + r/2), 150·r) in centimetres.
RPGCLIENT_API FVector2D CentreOf(const FIntPoint& Cell);
RPGCLIENT_API FIntPoint CellAt(const FVector2D& centimetres);
