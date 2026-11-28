#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "UObject/WeakObjectPtr.h"

class ARpgGameMode;
struct IConsoleCommand;

class FRpgDnd5eUnrealModule final : public FDefaultGameModuleImpl
{
public:
    void StartupModule() override;
    void ShutdownModule() override;
    void Ready(ARpgGameMode* Mode);
    void NotReady(ARpgGameMode* Mode);
    void Pump(ARpgGameMode* Mode);

private:
    struct FInjection
    {
        FString Command;
        TArray<FString> Args;
        double Delay = 0.0;
    };
    void Queue(const FString& Command, const TArray<FString>& Args);
    TArray<IConsoleCommand*> Commands;
    TArray<FInjection> Pending;
    TWeakObjectPtr<ARpgGameMode> ReadyMode;
    double ReadyAt = 0.0;
};
