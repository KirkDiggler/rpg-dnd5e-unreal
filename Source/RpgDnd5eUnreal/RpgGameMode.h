#pragma once

#include "GameFramework/GameModeBase.h"
#include "RpgGameMode.generated.h"

class ARpgBoard;
class URpgPlay;

UCLASS()
class ARpgGameMode final : public AGameModeBase
{
    GENERATED_BODY()

public:
    ARpgGameMode();
    void StartPlay() override;
    void PostLogin(APlayerController* NewPlayer) override;
    void EndPlay(const EEndPlayReason::Type Reason) override;
    void Tick(float DeltaSeconds) override;
    void Inject(const FString& Command, const TArray<FString>& Args);

private:
    void SetStatus(const FString& Line) const;
    void Left();
    UPROPERTY() TObjectPtr<ARpgBoard> Board;
    UPROPERTY() TObjectPtr<URpgPlay> Play;
    bool bReady = false;
};
