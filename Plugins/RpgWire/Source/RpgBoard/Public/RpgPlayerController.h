#pragma once

#include "GameFramework/PlayerController.h"
#include "RpgPlayerController.generated.h"

class ARpgBoard;
class URpgPlay;

UCLASS()
class RPGBOARD_API ARpgPlayerController final : public APlayerController
{
    GENERATED_BODY()

public:
    void SetupInputComponent() override;
    void Adopt(ARpgBoard* InBoard, URpgPlay* InPlay);
    void ClickAtCell(const FIntPoint& Cell);
    void OnEndTurn();
    void OnLeave();

private:
    void OnClick();
    void ClickRay(const FVector& Origin, const FVector& Direction);
    UPROPERTY() TObjectPtr<ARpgBoard> Board;
    UPROPERTY() TObjectPtr<URpgPlay> Play;
};
