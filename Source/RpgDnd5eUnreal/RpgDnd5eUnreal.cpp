#include "RpgDnd5eUnreal.h"

#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "RpgGameMode.h"
#include "RpgPlay.h"

void FRpgDnd5eUnrealModule::StartupModule()
{
    for (const TCHAR* Verb : {TEXT("click"), TEXT("key"), TEXT("where"), TEXT("status"), TEXT("exit"), TEXT("shot")})
    {
        const FString Name = FString(TEXT("rpg.play.")) + Verb;
        Commands.Add(IConsoleManager::Get().RegisterConsoleCommand(*Name, TEXT("Inject play input or ask the board for evidence"),
            FConsoleCommandWithArgsDelegate::CreateLambda([this, Command = FString(Verb)](const TArray<FString>& Args)
            {
                Queue(Command, Args);
            })));
    }
    UE_LOG(LogRpgPlay, Display, TEXT("RpgDnd5eUnreal module loaded"));
}

void FRpgDnd5eUnrealModule::ShutdownModule()
{
    for (IConsoleCommand* Command : Commands) IConsoleManager::Get().UnregisterConsoleObject(Command, false);
    Commands.Reset();
    ReadyMode.Reset();
    Pending.Reset();
}

void FRpgDnd5eUnrealModule::Ready(ARpgGameMode* Mode)
{
    ReadyMode = Mode;
    ReadyAt = FPlatformTime::Seconds();
}

void FRpgDnd5eUnrealModule::NotReady(ARpgGameMode* Mode)
{
    if (ReadyMode.Get() != Mode) return;
    ReadyMode.Reset();
    Pending.Reset();
}

void FRpgDnd5eUnrealModule::Queue(const FString& Command, const TArray<FString>& Args)
{
    FInjection Request{Command, Args};
    if (!Request.Args.IsEmpty() && Request.Args.Last().StartsWith(TEXT("@")))
    {
        const FString At = Request.Args.Pop().Mid(1);
        if (!LexTryParseString(Request.Delay, *At) || !FMath::IsFinite(Request.Delay) || Request.Delay < 0.0)
        {
            UE_LOG(LogRpgPlay, Warning, TEXT("invalid injection time: %s"), *At);
            return;
        }
    }
    Pending.Add(MoveTemp(Request));
}

void FRpgDnd5eUnrealModule::Pump(ARpgGameMode* Mode)
{
    if (ReadyMode.Get() != Mode) return;
    const double Elapsed = FPlatformTime::Seconds() - ReadyAt;
    for (int32 Index = 0; Index < Pending.Num();)
    {
        if (Pending[Index].Delay > Elapsed)
        {
            ++Index;
            continue;
        }
        const FInjection Request = MoveTemp(Pending[Index]);
        Pending.RemoveAt(Index);
        Mode->Inject(Request.Command, Request.Args);
    }
}

IMPLEMENT_PRIMARY_GAME_MODULE(FRpgDnd5eUnrealModule, RpgDnd5eUnreal, "RpgDnd5eUnreal");
