#include "RpgHud.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"

void ARpgHud::SetStatus(const FString& Line) { Status = Line; }

void ARpgHud::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas || !GEngine || Status.IsEmpty()) return;
    UFont* Font = GEngine->GetMediumFont();
    float Width = 0.0f, Height = 0.0f;
    Canvas->TextSize(Font, Status, Width, Height);
    DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.68f), 12.0f, 12.0f, Width + 20.0f, Height + 16.0f);
    DrawText(Status, FLinearColor::White, 22.0f, 20.0f, Font);
}
