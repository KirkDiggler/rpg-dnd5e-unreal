#include "RpgPlay.h"

#include "Async/Async.h"
#include "Containers/StringConv.h"
#include "Misc/Parse.h"
#include "RpgLook.h"
#include "RpgBoard.h"
#include "RpgMotion.h"
#include "RpgRecordStream.h"
#include "RpgSession.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

DEFINE_LOG_CATEGORY(LogRpgPlay);

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgTargetTest, "RpgBoard.Target.FourSwitchesParseAndAPartialOneIsRefused",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgTargetTest::RunTest(const FString& Parameters)
{
    FRpgTarget Target;
    FString Why;
    TestTrue(TEXT("all four switches parse"),
        FRpgTarget::Parse(TEXT("-RpgAddr=127.0.0.1:50151 -RpgSession=tomb -RpgMember=fighter -RpgPlayer=host"), Target, Why));
    TestEqual(TEXT("address"), Target.Address, FString(TEXT("127.0.0.1:50151")));
    TestEqual(TEXT("session"), Target.Session, FString(TEXT("tomb")));
    TestEqual(TEXT("member"), Target.Member, FString(TEXT("fighter")));
    TestEqual(TEXT("player"), Target.Player, FString(TEXT("host")));
    TestFalse(TEXT("two switches are not a target"), FRpgTarget::Parse(TEXT("-RpgAddr=x -RpgSession=y"), Target, Why));
    TestTrue(TEXT("refusal names the missing member"), Why.Contains(TEXT("RpgMember")));
    return !HasAnyErrors();
}
#endif

namespace
{
    constexpr int kAttack = 1;
    constexpr int kMove = 2;
    constexpr int kEndTurn = 3;

    FString Read(const std::string& Value) { return FString(UTF8_TO_TCHAR(Value.c_str())); }
    std::string Wire(const FString& Value) { return std::string(TCHAR_TO_UTF8(*Value)); }

    FString Row(const rpgwire::Afforded& Offered, int Verb)
    {
        const rpgwire::Declaration* Withheld = nullptr;
        for (const rpgwire::Declaration& Declaration : Offered.declarations)
        {
            if (Declaration.verb != Verb) continue;
            if (Declaration.available) return TEXT("available");
            if (!Withheld) Withheld = &Declaration;
        }
        return Withheld ? TEXT("withheld:") + Read(Withheld->why) : FString(TEXT("none"));
    }

    void ReportAfford(const rpgwire::Afforded& Offered)
    {
        UE_LOG(LogRpgPlay, Display, TEXT("AFFORD clock=%d attack=%s move=%s end-turn=%s"), Offered.clock,
            *Row(Offered, kAttack), *Row(Offered, kMove), *Row(Offered, kEndTurn));
    }

    using FRoster = TArray<TPair<FString, ERpgSide>>;

    FRoster ReadRoster(const std::vector<rpgwire::Member>& Members)
    {
        FRoster Roster;
        for (const rpgwire::Member& Member : Members)
        {
            Roster.Emplace(Read(Member.id), SideOf(static_cast<int>(Member.kind), !Member.class_ref.empty(), !Member.monster_ref.empty()));
        }
        return Roster;
    }

    void ReportRoster(const FRoster& Roster)
    {
        TArray<FString> Names;
        for (const auto& Member : Roster) Names.Add(Member.Key);
        UE_LOG(LogRpgPlay, Display, TEXT("ROSTER n=%d members=%s"), Names.Num(), *FString::Join(Names, TEXT(",")));
    }

    TArray<FIntPoint> ReadCells(const rpgwire::Atlas& Atlas)
    {
        TArray<FIntPoint> Cells;
        for (const rpgwire::Cell& Cell : Atlas.cells) Cells.Emplace(Cell.first, Cell.second);
        return Cells;
    }

    TArray<FRpgWall> ReadWalls(const rpgwire::Atlas& Atlas)
    {
        TArray<FRpgWall> Walls;
        for (const auto& Placed : Atlas.placed)
        {
            std::string Parent = Placed.id;
            const auto Cut = Parent.rfind("/span/");
            const bool Span = Cut != std::string::npos;
            if (Span) Parent.resize(Cut);
            auto Info = Atlas.authored.find(Parent);
            if (Info == Atlas.authored.end() && !Atlas.dungeon_key.empty()) Info = Atlas.authored.find(Atlas.dungeon_key + "/" + Parent);
            if (Info == Atlas.authored.end()) continue;
            if (!Info->second.door && (Span != (Info->second.openings > 0))) continue;
            FRpgWall Wall;
            Wall.Id = Read(Placed.id);
            Wall.Centre = RpgLook::BoxCentre(Placed.origin_x, Placed.origin_y, Info->second.elevation, Info->second.height);
            Wall.Size = RpgLook::BoxSize(Placed.depth, Placed.width, Info->second.height);
            Wall.Yaw = Placed.facing;
            Wall.Door = Info->second.door;
            Walls.Add(MoveTemp(Wall));
        }
        return Walls;
    }
    void ReportFactions(const std::vector<rpgwire::Member>& Members)
    {
        TArray<FString> Values;
        for (const auto& Member : Members) if (!Member.faction.empty()) Values.Add(Read(Member.id) + TEXT(":") + Read(Member.faction));
        if (!Values.IsEmpty()) UE_LOG(LogRpgPlay, Display, TEXT("FACTIONS %s"), *FString::Join(Values, TEXT(",")));
    }
    FString CellText(const FIntPoint& Cell)
    {
        return FString::Printf(TEXT("(%d,%d)"), Cell.X, Cell.Y);
    }
}

struct URpgPlay::FState
{
    explicit FState(const FRpgTarget& Target)
        : Options{Wire(Target.Address), Wire(Target.Session), Wire(Target.Player), Wire(Target.Member)}
    {}

    rpgwire::RecordStream::Options Options;
    // Blocking RPCs and stream stop execute only on workers.
    TSharedPtr<rpgwire::Session, ESPMode::ThreadSafe> Session;
    TSharedPtr<rpgwire::RecordStream, ESPMode::ThreadSafe> Stream;
    // These read models belong to the game thread after marshal.
    rpgwire::Afforded Afforded_;
    FRoster Roster;
    TArray<FRpgWall> Walls;
    TMap<FString, TPair<FString, bool>> Words;
    bool bViewBusy = false;
    bool bViewPending = false;
    TArray<TPair<TArray<FString>, TArray<FString>>> OpeningSight;
    bool bReadRoster = false;
    bool bReadAtlas = false;
    bool bRefreshing = false;
    TArray<FIntPoint> KnownCells;
    FIntPoint Start = FIntPoint::ZeroValue;
};

struct URpgPlay::FAct
{
    int Verb = 0;
    FString Member;
    std::vector<rpgwire::Cell> Path;
    FString Arg;
};

bool FRpgTarget::Parse(const FString& CommandLine, FRpgTarget& Out, FString& Why)
{
    FRpgTarget Parsed;
    struct FSwitch { const TCHAR* Key; FString FRpgTarget::* Field; };
    for (const FSwitch& Switch : {FSwitch{TEXT("RpgAddr="), &FRpgTarget::Address},
        FSwitch{TEXT("RpgSession="), &FRpgTarget::Session}, FSwitch{TEXT("RpgMember="), &FRpgTarget::Member},
        FSwitch{TEXT("RpgPlayer="), &FRpgTarget::Player}})
    {
        FString& Field = Parsed.*Switch.Field;
        if (!FParse::Value(*CommandLine, Switch.Key, Field) || Field.IsEmpty())
        {
            Why = FString::Printf(TEXT("missing -%s"), Switch.Key);
            return false;
        }
    }
    FParse::Value(*CommandLine, TEXT("RpgShotOnJoin="), Parsed.ShotOnJoin);
    Out = MoveTemp(Parsed);
    Why.Reset();
    return true;
}

URpgPlay* URpgPlay::Create(UObject* Outer, ARpgBoard* InBoard, FStatus InStatus, FReady InReady, FLeft InLeft)
{
    URpgPlay* Play = NewObject<URpgPlay>(Outer);
    Play->Board = InBoard;
    Play->OnStatus = MoveTemp(InStatus);
    Play->OnReady = MoveTemp(InReady);
    Play->OnLeft = MoveTemp(InLeft);
    return Play;
}

void URpgPlay::SetOnFirstJoin(TFunction<void()> Callback) { OnFirstJoin = MoveTemp(Callback); }

void URpgPlay::Join(const FRpgTarget& Target)
{
    if (State) return;
    bJoining = true;
    TurnMember = Target.Member;
    State = MakeShared<FState, ESPMode::ThreadSafe>(Target);
    UE_LOG(LogRpgPlay, Display, TEXT("PLAY addr=%s session=%s member=%s"), *Target.Address, *Target.Session, *Target.Member);
    const TWeakObjectPtr<URpgPlay> Weak(this);
    const TSharedPtr<FState, ESPMode::ThreadSafe> Joining = State;
    Async(EAsyncExecution::ThreadPool, [Weak, Joining]
    {
        const auto& Options = Joining->Options;
        auto Session = MakeShared<rpgwire::Session, ESPMode::ThreadSafe>(rpgwire::Session::Options{Options.address, Options.player_id});
        std::vector<rpgwire::Member> Members;
        rpgwire::Atlas Atlas;
        rpgwire::Afforded Offered;
        std::string Error;
        if (!Session->Roster(Options.session, Options.member, &Members, &Error) ||
            !Session->Atlas(Options.session, Options.member, &Atlas, &Error) ||
            !Session->Afford(Options.session, Options.member, &Offered, &Error))
        {
            AsyncTask(ENamedThreads::GameThread, [Weak, Why = Read(Error)]
            {
                if (URpgPlay* Play = Weak.Get()) Play->FailJoin(Why);
            });
            return;
        }

        // The reader can start delivering before its constructor returns. The
        // readiness check waits for both this payload and the record's positions.
        auto Stream = MakeShared<rpgwire::RecordStream, ESPMode::ThreadSafe>(Options,
            [Weak](const rpgwire::RecordBeat& Beat)
            {
                AsyncTask(ENamedThreads::GameThread, [Weak, Name = Read(Beat.beat), Fields = Read(Beat.fields_json),
                    CausedBy = Read(Beat.act)]
                {
                    if (URpgPlay* Play = Weak.Get()) Play->AcceptBeat(Name, Fields, CausedBy);
                });
            },
            [Weak](const rpgwire::StreamEnd& End)
            {
                AsyncTask(ENamedThreads::GameThread, [Weak, Why = Read(End.message)]
                {
                    if (URpgPlay* Play = Weak.Get(); Play && !Play->bLeaving)
                    {
                        if (!Play->bReady) Play->FailJoin(Why);
                        else
                        {
                            Play->LastWords = Why;
                            Play->PublishStatus();
                        }
                    }
                });
            });
        AsyncTask(ENamedThreads::GameThread, [Weak, Session = MoveTemp(Session), Stream = MoveTemp(Stream),
            Members = MoveTemp(Members), Atlas = MoveTemp(Atlas), Offered = MoveTemp(Offered)]
        {
            URpgPlay* Play = Weak.Get();
            if (!Play || !Play->bJoining)
            {
                Async(EAsyncExecution::ThreadPool, [Stream] { Stream->Stop(); });
                return;
            }
            FState& Joined = *Play->State;
            Joined.Session = Session;
            Joined.Stream = Stream;
            Joined.Afforded_ = Offered;
            Joined.Start = FIntPoint(Atlas.start.first, Atlas.start.second);
            Joined.KnownCells = ReadCells(Atlas);
            Joined.Roster = ReadRoster(Members);
            Joined.Walls = ReadWalls(Atlas);
            ReportRoster(Joined.Roster);
            ReportFactions(Members);
            UE_LOG(LogRpgPlay, Display, TEXT("ATLAS cells=%d placed=%d"), Joined.KnownCells.Num(), static_cast<int32>(Atlas.placed.size()));
            Play->bJoined = true;
            Play->TryReady();
        });
    });
}

void URpgPlay::FailJoin(const FString& Why)
{
    if (!bJoining) return;
    bJoining = false;
    LastWords = Why;
    PublishStatus();
    if (OnReady) OnReady(false, Why);
}

void URpgPlay::TryReady()
{
    if (!bJoining || !bJoined || !IsValid(Board)) return;
    for (const auto& Member : State->Roster)
    {
        if (!Board->CellOf(Member.Key)) return;
    }
    Board->BuildFloor(State->KnownCells, State->Start);
    Board->BuildWalls(State->Walls);
    Board->BuildGround();
    for (const auto& Member : State->Roster) { Board->PlaceFigure(Member.Key, Member.Value, *Board->CellOf(Member.Key)); ApplyCachedWord(Member.Key); }
    Board->MoveMarkerTo(TurnMember);
    bJoining = false;
    bReady = true;
    for (const auto& Delta : State->OpeningSight) ApplySight(Delta.Key, Delta.Value);
    State->OpeningSight.Reset();
    RefreshView();
    RefreshBoard();
    ReportAfford(State->Afforded_);
    PublishStatus();
    if (OnReady) OnReady(true, FString());
}

void URpgPlay::AcceptBeat(const FString& Beat, const FString& Fields, const FString& CausedBy)
{
    if (bLeaving || (!bJoining && !bReady) || !IsValid(Board)) return;
    UE_LOG(LogRpgPlay, Display, TEXT("BEAT %s act=%s"), *Beat, *CausedBy);
    if (Beat == TEXT("sighted"))
    {
        TArray<FString> Gained, Lost;
        if (ReadSighted(Fields, Read(State->Options.member), Gained, Lost))
        {
            if (bReady) ApplySight(Gained, Lost);
            else State->OpeningSight.Emplace(MoveTemp(Gained), MoveTemp(Lost));
        }
    }
    if (Beat == TEXT("cells_revealed")) State->bReadAtlas = true;

    TArray<FMotionStep> Steps;
    PlanMotion(Beat, Fields, Board->Cells, Steps);
    Steps.RemoveAll([this](const FMotionStep& Step)
    {
        return Step.kind != FMotionStep::Kind::Turn && !Board->HasFigure(Step.member);
    });
    for (const FMotionStep& Step : Steps)
    {
        switch (Step.kind)
        {
        case FMotionStep::Kind::Walk:
            UE_LOG(LogRpgPlay, Display, TEXT("MOTION walk %s %s→%s"), *Step.member, *CellText(Step.from), *CellText(Step.to));
            break;
        case FMotionStep::Kind::Swing:
            UE_LOG(LogRpgPlay, Display, TEXT("MOTION swing %s"), *Step.member);
            break;
        case FMotionStep::Kind::Turn:
            UE_LOG(LogRpgPlay, Display, TEXT("MOTION turn %s"), *Step.member);
            TurnMember = Step.member;
            if (bReady) PublishStatus();
            break;
        }
    }
    if (bReady)
    {
        Board->Enqueue(Steps);
        RefreshBoard();
    }
    else TryReady();
}

void URpgPlay::ApplySight(const TArray<FString>& Gained, const TArray<FString>& Lost)
{
    for (const FString& Member : Lost)
    {
        if (!Board->HasFigure(Member)) continue;
        Board->RemoveFigure(Member);
        UE_LOG(LogRpgPlay, Display, TEXT("LOST %s"), *Member);
    }
    const FString Observer = Read(State->Options.member);
    for (const FString& Member : Gained)
    {
        if (Board->HasFigure(Member)) continue;
        if (const FIntPoint* Cell = Board->CellOf(Member))
        {
            UE_LOG(LogRpgPlay, Display, TEXT("SIGHT observer=%s gained=%s"), *Observer, *Member);
            Board->PlaceFigure(Member, ERpgSide::Npc, *Cell);
            if (Board->HasFigure(Member))
            {
                UE_LOG(LogRpgPlay, Display, TEXT("JOINED %s %d %d"), *Member, Cell->X, Cell->Y);
                ApplyCachedWord(Member);
                if (OnFirstJoin)
                {
                    auto Callback = MoveTemp(OnFirstJoin);
                    OnFirstJoin = nullptr;
                    Callback();
                }
                State->bReadRoster = true;
            }
        }
    }
    if (bReady && (!Gained.IsEmpty() || !Lost.IsEmpty())) RefreshView();
}

void URpgPlay::RefreshBoard()
{
    const auto Active = State;
    if (!bReady || bLeaving || Active->bRefreshing || (!Active->bReadRoster && !Active->bReadAtlas)) return;
    const bool WantRoster = Active->bReadRoster;
    const bool WantAtlas = Active->bReadAtlas;
    Active->bReadRoster = Active->bReadAtlas = false;
    Active->bRefreshing = true;
    const TWeakObjectPtr<URpgPlay> Weak(this);
    Async(EAsyncExecution::ThreadPool, [Weak, Active, WantRoster, WantAtlas]
    {
        const auto& Options = Active->Options;
        std::vector<rpgwire::Member> Members;
        rpgwire::Atlas Atlas;
        std::string RosterError, AtlasError;
        const bool GotRoster = WantRoster && Active->Session->Roster(Options.session, Options.member, &Members, &RosterError);
        const bool GotAtlas = WantAtlas && Active->Session->Atlas(Options.session, Options.member, &Atlas, &AtlasError);
        AsyncTask(ENamedThreads::GameThread, [Weak, GotRoster, GotAtlas, Roster = ReadRoster(Members), Cells = ReadCells(Atlas),
            Start = FIntPoint(Atlas.start.first, Atlas.start.second), Placed = Atlas.placed.size(), Walls = ReadWalls(Atlas), Members = MoveTemp(Members),
            RosterWhy = Read(RosterError), AtlasWhy = Read(AtlasError)]
        {
            URpgPlay* Play = Weak.Get();
            if (!Play || Play->bLeaving || !IsValid(Play->Board)) return;
            if (GotRoster)
            {
                ReportRoster(Roster);
                ReportFactions(Members);
                for (const auto& Member : Roster) Play->Board->SetFigureSide(Member.Key, Member.Value);
            }
            if (GotAtlas)
            {
                Play->Board->BuildFloor(Cells, Start);
                UE_LOG(LogRpgPlay, Display, TEXT("ATLAS cells=%d placed=%d"), Cells.Num(), static_cast<int32>(Placed));
                Play->Board->BuildWalls(Walls);
            }
            if (!RosterWhy.IsEmpty() || !AtlasWhy.IsEmpty())
            {
                Play->LastWords = RosterWhy.IsEmpty() ? AtlasWhy : RosterWhy;
                Play->PublishStatus();
            }
            Play->State->bRefreshing = false;
            Play->RefreshBoard();
        });
    });
}

void URpgPlay::ApplyCachedWord(const FString& Member)
{
    if (const auto* Word = State->Words.Find(Member))
    {
        Board->SetFigureWord(Member, Word->Key);
        Board->SetFigureDowned(Member, Word->Value);
    }
}
void URpgPlay::RefreshView()
{
    if (!bReady || bLeaving || !State || !State->Session) return;
    if (State->bViewBusy) { State->bViewPending = true; return; }
    State->bViewBusy = true;
    const auto Active = State;
    const TWeakObjectPtr<URpgPlay> Weak(this);
    Async(EAsyncExecution::ThreadPool, [Weak, Active]
    {
        std::vector<rpgwire::Sighting> Sightings;
        std::string Error;
        const bool Ok = Active->Session->View(Active->Options.session, Active->Options.member, &Sightings, &Error);
        AsyncTask(ENamedThreads::GameThread, [Weak, Active, Ok, Sightings = MoveTemp(Sightings), Why = Read(Error)]
        {
            URpgPlay* Play = Weak.Get();
            if (!Play || Play->bLeaving || !IsValid(Play->Board)) return;
            if (Ok)
            {
                Active->Words.Reset();
                int32 Count = 0;
                for (const auto& Sighting : Sightings)
                {
                    const FString Id = Read(Sighting.id), Word = Read(Sighting.stance);
                    if (!Word.IsEmpty()) ++Count;
                    Active->Words.Add(Id, TPair<FString, bool>(Word, Sighting.standing == 2));
                    Play->ApplyCachedWord(Id);
                }
                UE_LOG(LogRpgPlay, Display, TEXT("VIEW sightings=%d words=%d"), static_cast<int32>(Sightings.size()), Count);
            }
            else { Play->LastWords = Why; Play->PublishStatus(); }
            Active->bViewBusy = false;
            if (Active->bViewPending) { Active->bViewPending = false; Play->RefreshView(); }
        });
    });
}

bool URpgPlay::CanAct(const TCHAR* Input) const
{
    if (bReady && !bActing && !bLeaving) return true;
    UE_LOG(LogRpgPlay, Display, TEXT("DROP %s"), Input);
    return false;
}

void URpgPlay::ClickCell(const FIntPoint& Cell)
{
    if (!CanAct(TEXT("click"))) return;
    const FIntPoint* Here = Board->CellOf(Read(State->Options.member));
    if (!Here || *Here == Cell)
    {
        UE_LOG(LogRpgPlay, Display, TEXT("DROP click %s"), Here ? TEXT("same-cell") : TEXT("no member cell"));
        return;
    }
    FAct Request;
    Request.Verb = kMove;
    Request.Arg = FString::Printf(TEXT("%s…%s"), *CellText(*Here), *CellText(Cell));
    const TArray<FIntPoint> Line = HexLine(*Here, Cell);
    for (int32 Index = 1; Index < Line.Num(); ++Index) Request.Path.emplace_back(Line[Index].X, Line[Index].Y);
    Act(MoveTemp(Request));
}

void URpgPlay::ClickFigure(const FString& Member)
{
    if (!CanAct(TEXT("click"))) return;
    FAct Request;
    Request.Verb = kAttack;
    Request.Member = Member;
    Request.Arg = Member;
    Act(MoveTemp(Request));
}

void URpgPlay::EndTurn()
{
    if (!CanAct(TEXT("key"))) return;
    FAct Request;
    Request.Verb = kEndTurn;
    Act(MoveTemp(Request));
}

void URpgPlay::Act(FAct Request)
{
    bActing = true;
    const TCHAR* Verb = Request.Verb == kAttack ? TEXT("attack") : Request.Verb == kMove ? TEXT("move") : TEXT("end-turn");
    const FString Actor = Read(State->Options.member);
    UE_LOG(LogRpgPlay, Display, TEXT("VERB %s %s%s"), Verb, *Actor,
        *(Request.Arg.IsEmpty() ? FString() : TEXT(" ") + Request.Arg));
    const auto Declaration = rpgwire::DeclarationFor(State->Afforded_, Request.Verb);
    const TWeakObjectPtr<URpgPlay> Weak(this);
    const TSharedPtr<FState, ESPMode::ThreadSafe> Active = State;
    Async(EAsyncExecution::ThreadPool, [Weak, Active, Request = MoveTemp(Request), Declaration, Verb]
    {
        const auto& Options = Active->Options;
        auto& Session = *Active->Session;
        std::string Error;
        std::string Words;
        bool Ok;
        if (Request.Verb == kMove)
        {
            rpgwire::Walk Walk;
            Ok = Session.Move(Options.session, Options.member, Request.Path, Declaration, &Walk, &Error);
            if (Ok && !Walk.continues) Words = Walk.stop_reason;
        }
        else if (Request.Verb == kAttack)
        {
            rpgwire::Swing Swing;
            Ok = Session.Attack(Options.session, Options.member, Wire(Request.Member), Declaration, &Swing, &Error);
        }
        else
        {
            std::string Next;
            bool Wrapped = false;
            Ok = Session.EndTurn(Options.session, Options.member, Declaration, &Next, &Wrapped, &Error);
        }
        if (!Ok) Words = Error;
        rpgwire::Afforded Offered;
        std::string AffordError;
        const bool Refreshed = Session.Afford(Options.session, Options.member, &Offered, &AffordError);
        AsyncTask(ENamedThreads::GameThread, [Weak, Verb, Ok, Refreshed, Offered = MoveTemp(Offered),
            Last = Read(Words), RefreshWhy = Read(AffordError)]
        {
            URpgPlay* Play = Weak.Get();
            if (!Play) return;
            if (!Ok) UE_LOG(LogRpgPlay, Display, TEXT("REFUSED %s %s"), Verb, *Last);
            if (Refreshed)
            {
                Play->State->Afforded_ = Offered;
                ReportAfford(Offered);
            }
            Play->LastWords = (!Refreshed && Last.IsEmpty()) ? RefreshWhy : Last;
            Play->bActing = false;
            Play->PublishStatus();
            Play->RefreshView();
        });
    });
}

void URpgPlay::Leave()
{
    if (!CanAct(TEXT("key"))) return;
    bLeaving = true;
    const auto Active = State;
    const TWeakObjectPtr<URpgPlay> Weak(this);
    UE_LOG(LogRpgPlay, Display, TEXT("VERB exit %s"), *Read(Active->Options.member));
    Async(EAsyncExecution::ThreadPool, [Weak, Active]
    {
        const auto& Options = Active->Options;
        std::string Error;
        const bool Ok = Active->Session->Exit(Options.session, Options.member, &Error);
        Active->Stream->Stop();
        AsyncTask(ENamedThreads::GameThread, [Weak, Ok, Why = Read(Error)]
        {
            URpgPlay* Play = Weak.Get();
            if (!Play) return;
            if (!Ok)
            {
                UE_LOG(LogRpgPlay, Display, TEXT("REFUSED exit %s"), *Why);
                Play->LastWords = Why;
                Play->PublishStatus();
            }
            if (IsValid(Play->Board)) Play->Board->Clear();
            UE_LOG(LogRpgPlay, Display, TEXT("EXIT"));
            if (Play->OnLeft) Play->OnLeft();
        });
    });
}

bool URpgPlay::WhoseTurn(FString& Out) const
{
    Out = TurnMember;
    return !Out.IsEmpty();
}

void URpgPlay::ReportStatus() const
{
    UE_LOG(LogRpgPlay, Display, TEXT("STATUS %s"), *StatusLine(TurnMember, State && State->Afforded_.clock == 2, LastWords));
}

void URpgPlay::PublishStatus() const
{
    ReportStatus();
    if (OnStatus) OnStatus(StatusLine(TurnMember, State && State->Afforded_.clock == 2, LastWords));
}

void URpgPlay::BeginDestroy()
{
    if (!bLeaving && State && State->Stream)
    {
        Async(EAsyncExecution::ThreadPool, [Stream = State->Stream] { Stream->Stop(); });
    }
    Super::BeginDestroy();
}
