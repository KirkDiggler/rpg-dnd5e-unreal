#include "RpgGameMode.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "RpgBoard.h"
#include "RpgDnd5eUnreal.h"
#include "RpgHud.h"
#include "RpgPlay.h"
#include "RpgPlayerController.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#endif

namespace
{
    FRpgDnd5eUnrealModule& GameModule()
    {
        return FModuleManager::GetModuleChecked<FRpgDnd5eUnrealModule>(TEXT("RpgDnd5eUnreal"));
    }
}

ARpgGameMode::ARpgGameMode()
{
    PlayerControllerClass = ARpgPlayerController::StaticClass();
    HUDClass = ARpgHud::StaticClass();
    DefaultPawnClass = nullptr;
    PrimaryActorTick.bCanEverTick = true;
}

void ARpgGameMode::StartPlay()
{
    Super::StartPlay();
    FRpgTarget Target;
    FString Why;
    if (!FRpgTarget::Parse(FCommandLine::Get(), Target, Why))
    {
        SetStatus(Why);
        UE_LOG(LogRpgPlay, Display, TEXT("PLAY refused %s"), *Why);
        return;
    }

    UWorld* World = GetWorld();
    if (ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector(0.0, 0.0, 500.0), FRotator(-48.0, 32.0, 0.0)))
    {
        Sun->GetLightComponent()->SetIntensity(6.0f);
    }
    if (ASkyLight* Sky = World->SpawnActor<ASkyLight>(FVector(0.0, 0.0, 300.0), FRotator::ZeroRotator))
    {
        Sky->GetLightComponent()->SetIntensity(1.0f);
    }
    Board = World->SpawnActor<ARpgBoard>(FVector::ZeroVector, FRotator::ZeroRotator);
    const TWeakObjectPtr<ARpgGameMode> Weak(this);
    Play = URpgPlay::Create(this, Board,
        [Weak](const FString& Line) { if (ARpgGameMode* Mode = Weak.Get()) Mode->SetStatus(Line); },
        [Weak](bool Ok, const FString& Detail)
        {
            ARpgGameMode* Mode = Weak.Get();
            if (!Mode) return;
            if (!Ok)
            {
                UE_LOG(LogRpgPlay, Display, TEXT("PLAY refused %s"), *Detail);
                return;
            }
            Mode->bReady = true;
            if (auto* Controller = Cast<ARpgPlayerController>(Mode->GetWorld()->GetFirstPlayerController()))
            {
                Controller->Adopt(Mode->Board, Mode->Play);
            }
            GameModule().Ready(Mode);
        },
        [Weak] { if (ARpgGameMode* Mode = Weak.Get()) Mode->Left(); });
    Play->Join(Target);
}

void ARpgGameMode::SetStatus(const FString& Line) const
{
    const APlayerController* Controller = GetWorld()->GetFirstPlayerController();
    if (auto* Hud = Controller ? Cast<ARpgHud>(Controller->GetHUD()) : nullptr) Hud->SetStatus(Line);
}

void ARpgGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    if (auto* Controller = Cast<ARpgPlayerController>(NewPlayer); Controller && bReady) Controller->Adopt(Board, Play);
}

void ARpgGameMode::Left()
{
    GameModule().NotReady(this);
#if WITH_EDITOR
    if (GIsEditor && GUnrealEd)
    {
        GUnrealEd->RequestEndPlayMap();
        return;
    }
#endif
    FPlatformMisc::RequestExit(false);
}

void ARpgGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
    if (bReady) GameModule().NotReady(this);
    bReady = false;
    Super::EndPlay(Reason);
}

void ARpgGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bReady) GameModule().Pump(this);
}

void ARpgGameMode::Inject(const FString& Command, const TArray<FString>& Args)
{
    if (!bReady || !Play || !Board) return;
    auto* Controller = Cast<ARpgPlayerController>(GetWorld()->GetFirstPlayerController());
    if (Command == TEXT("click") && Args.Num() == 2 && Controller)
    {
        FIntPoint Cell;
        if (LexTryParseString(Cell.X, *Args[0]) && LexTryParseString(Cell.Y, *Args[1])) Controller->ClickAtCell(Cell);
    }
    else if (Command == TEXT("key") && Args.Num() == 1 && Controller)
    {
        if (Args[0] == TEXT("E")) Controller->OnEndTurn();
        else if (Args[0] == TEXT("Escape")) Controller->OnLeave();
    }
    else if (Command == TEXT("exit") && Args.IsEmpty())
    {
        Play->Leave();
    }
    else if (Command == TEXT("where") && Args.Num() == 1)
    {
        if (const FIntPoint* Cell = Board->CellOf(Args[0]))
            UE_LOG(LogRpgPlay, Display, TEXT("CELL %s %d %d"), *Args[0], Cell->X, Cell->Y);
    }
    else if (Command == TEXT("status") && Args.IsEmpty())
    {
        Play->ReportStatus();
    }
    else if (Command == TEXT("shot") && Args.Num() == 1 && !FApp::IsNullRHIRequested() && !FPaths::IsRelative(Args[0]))
    {
        FScreenshotRequest::RequestScreenshot(Args[0], false, false);
        UE_LOG(LogRpgPlay, Display, TEXT("SHOT %s"), *Args[0]);
    }
}
