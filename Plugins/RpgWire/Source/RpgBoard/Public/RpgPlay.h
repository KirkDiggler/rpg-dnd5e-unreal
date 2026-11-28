#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RpgPlay.generated.h"

RPGBOARD_API DECLARE_LOG_CATEGORY_EXTERN(LogRpgPlay, Log, All);

class ARpgBoard;

struct RPGBOARD_API FRpgTarget
{
    FString Address, Session, Member, Player, ShotOnJoin;
    static bool Parse(const FString& CommandLine, FRpgTarget& Out, FString& Why);
};

UCLASS()
class RPGBOARD_API URpgPlay final : public UObject
{
    GENERATED_BODY()

public:
    using FStatus = TFunction<void(const FString&)>;
    using FReady = TFunction<void(bool Ok, const FString& Detail)>;
    using FLeft = TFunction<void()>;

    static URpgPlay* Create(UObject* Outer, ARpgBoard* Board, FStatus OnStatus, FReady OnReady, FLeft OnLeft);
    void Join(const FRpgTarget& Target);
    void SetShotOnJoin(const FString& Path);
    void ClickCell(const FIntPoint& Cell);
    void ClickFigure(const FString& Member);
    void EndTurn();
    void Leave();
    bool WhoseTurn(FString& Out) const;
    void ReportStatus() const;
    void BeginDestroy() override;

private:
    struct FState;
    struct FAct;
    TSharedPtr<FState, ESPMode::ThreadSafe> State;
    UPROPERTY() TObjectPtr<ARpgBoard> Board;
    FStatus OnStatus;
    FReady OnReady;
    FLeft OnLeft;
    FString TurnMember;
    FString LastWords;
    FString ShotOnJoin;
    bool bJoining = false;
    bool bJoined = false;
    bool bReady = false;
    bool bActing = false;
    bool bLeaving = false;

    void AcceptBeat(const FString& Beat, const FString& Fields, const FString& Act);
    void TryReady();
    void ApplySight(const TArray<FString>& Gained, const TArray<FString>& Lost);
    void RefreshBoard();
    void RefreshView();
    void ApplyCachedWord(const FString& Member);
    void FailJoin(const FString& Why);
    void Act(FAct Request);
    bool CanAct(const TCHAR* Input) const;
    void PublishStatus() const;
};
