#pragma once

#include "GameFramework/HUD.h"
#include "RpgHud.generated.h"

UCLASS()
class RPGBOARD_API ARpgHud final : public AHUD
{
    GENERATED_BODY()

public:
    void SetStatus(const FString& Line);
    void DrawHUD() override;

private:
    FString Status;
};
