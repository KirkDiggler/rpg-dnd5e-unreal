#include "RpgPlayerController.h"

#include "Camera/CameraActor.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "RpgBoard.h"
#include "RpgMotion.h"
#include "RpgPlay.h"

void ARpgPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ARpgPlayerController::OnClick);
    InputComponent->BindKey(EKeys::E, IE_Pressed, this, &ARpgPlayerController::OnEndTurn);
    InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ARpgPlayerController::OnLeave);
}

void ARpgPlayerController::Adopt(ARpgBoard* InBoard, URpgPlay* InPlay)
{
    Board = InBoard;
    Play = InPlay;
    if (ACameraActor* Camera = Board->Camera()) SetViewTarget(Camera);
    bShowMouseCursor = true;
    DefaultMouseCursor = EMouseCursor::Crosshairs;
    SetInputMode(FInputModeGameAndUI());
}

void ARpgPlayerController::OnClick()
{
    FVector Origin, Direction;
    if (DeprojectMousePositionToWorld(Origin, Direction)) ClickRay(Origin, Direction);
}

void ARpgPlayerController::ClickAtCell(const FIntPoint& Cell)
{
    FVector2D Screen;
    FVector Origin, Direction;
    if (ProjectWorldLocationToScreen(FVector(CentreOf(Cell), 1.0), Screen) &&
        DeprojectScreenPositionToWorld(Screen.X, Screen.Y, Origin, Direction))
    {
        ClickRay(Origin, Direction);
    }
}

void ARpgPlayerController::ClickRay(const FVector& Origin, const FVector& Direction)
{
    if (!IsValid(Board) || !IsValid(Play)) return;
    FString Member;
    FIntPoint Cell;
    if (Board->FigureUnder(Origin, Direction, Member)) Play->ClickFigure(Member);
    else if (Board->FloorUnder(Origin, Direction, Cell)) Play->ClickCell(Cell);
}

void ARpgPlayerController::OnEndTurn() { if (IsValid(Play)) Play->EndTurn(); }
void ARpgPlayerController::OnLeave() { if (IsValid(Play)) Play->Leave(); }
