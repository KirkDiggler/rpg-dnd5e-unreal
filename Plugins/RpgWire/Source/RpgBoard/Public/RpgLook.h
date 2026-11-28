#pragma once
#include "CoreMinimal.h"
#include "RpgBoard.h"
namespace RpgLook
{
inline const FLinearColor kPlayer = FLinearColor::FromSRGBColor(FColor(0x31,0x82,0xCE)), kPlayerActive = FLinearColor::FromSRGBColor(FColor(0x63,0xB3,0xED));
inline const FLinearColor kMonster = FLinearColor::FromSRGBColor(FColor(0xE5,0x3E,0x3E)), kMonsterActive = FLinearColor::FromSRGBColor(FColor(0xFC,0x81,0x81));
inline const FLinearColor kNpc = FLinearColor::FromSRGBColor(FColor(0xD6,0x9E,0x2E)), kNpcActive = FLinearColor::FromSRGBColor(FColor(0xF6,0xD5,0x5C));
inline const FLinearColor kDowned = FLinearColor::FromSRGBColor(FColor(0x66,0x66,0x66)), kGround = FLinearColor::FromSRGBColor(FColor(0x2D,0x31,0x38));
inline const FLinearColor kStone{0.55f,0.54f,0.50f}, kWood{0.30f,0.19f,0.10f};
inline const FLinearColor kFloor = FLinearColor::FromSRGBColor(FColor(0x9E,0xA3,0xA8)), kFloorStart = FLinearColor::FromSRGBColor(FColor(0xC9,0xA6,0x6B));
inline constexpr double kSqrt3 = 1.7320508075688772, kFeetPerHex = 5.0, kCmPerFoot = kSqrt3 / kFeetPerHex * 100.0;
RPGBOARD_API FLinearColor FigureColour(ERpgSide Side, const FString& Word, bool Active, bool Downed);
RPGBOARD_API FString HexOf(const FLinearColor& Colour);
RPGBOARD_API FVector BoxCentre(double OriginXFeet, double OriginYFeet, double ElevationFeet, double HeightFeet);
RPGBOARD_API FVector BoxSize(double DepthFeet, double WidthFeet, double HeightFeet);
}
