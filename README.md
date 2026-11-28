# rpg-dnd5e-unreal

The Unreal client of the RPG engine. A second client beside
[rpg-dnd5e-web](https://github.com/KirkDiggler/rpg-dnd5e-web), shaped like
the first: generated code from the same protos, the same auth boundary,
the same seat gates, the same verbs in and record out. It opens no door of
its own, and the engine gains nothing for its sake.

The design is law in `rpg-project`,
[ideas/unreal-client/design.md](https://github.com/KirkDiggler/rpg-project/blob/main/ideas/unreal-client/design.md),
with its rulings; the story that carries the work is
[rpg-project#1237](https://github.com/KirkDiggler/rpg-project/issues/1237):
one engine, every client; generated, never written; intent in, record out;
the same door; no side channel.

## State

Built from [the spike](https://github.com/KirkDiggler/rpg-project/pull/1155),
which proves the concept on Linux with Unreal 5.9: a real game started
from Unreal as a real player on the real stack, a click on the floor
walks, a click on a figure swings, the world answers, and every figure
moves only as the record says.

The game mode launches into an existing session and shows the board and
figures as engine shapes. Click a known floor cell to walk or a figure to
swing; E ends the turn and Escape leaves. Content by reference, continuous
integration that builds the engine, and other platforms are not here yet.

## How the project is put together

An Unreal project is a **project** that lists **modules**, each a compiled
unit with its own dependencies, grouped where it helps into **plugins**. No
makefile: the engine's build tool reads a descriptor for each and writes
the build. For this repository they read:

```
RpgDnd5eUnreal.uproject            the project. Its name is the project's,
                                   every target's and the module's
Config/
  DefaultEngine.ini                which map opens, which Game Mode runs it
Source/
  RpgDnd5eUnreal.Target.cs         build the game: which modules, how
  RpgDnd5eUnrealEditor.Target.cs   build the editor's version of the same
  RpgDnd5eUnreal/
    RpgDnd5eUnreal.Build.cs        the game module's dependencies
    RpgDnd5eUnreal.h, .cpp         module entry point and console injections
    RpgGameMode.h, .cpp            opens the board for the command-line target
Plugins/RpgWire/
  RpgWire.uplugin                  the plugin: its modules, and when each loads
  Source/
    RpgWire/
      RpgWire.Build.cs             the core's dependencies, the libraries among them
      Public/                      the headers another module may include:
                                   RpgSession.h, RpgRecordStream.h
      Private/
        pb/                        the compiled contract, generated; 144 files
        RpgSession.cpp             sign in, start a game, send a verb
        RpgRecordStream.cpp        follow the record, live
        Tests/
    RpgClient/
      Public/RpgMotion.h           beat interpretation and hex coordinates
      Private/RpgMotion.cpp        what a record beat means for figures
      Private/Tests/
    RpgBoard/
      RpgBoard.Build.cs            the board's dependencies
      Public/, Private/            board, HUD, play loop and player controller
    ThirdParty/                    gRPC and protobuf, built by scripts/build-libs.sh; never committed
scripts/                           everything fetched or built goes through these
protos.ref                         the protos tag this client builds against
```

The game module is thin on purpose. The code is in the plugin, in three
modules:

| the module | owns | keeps |
|---|---|---|
| `RpgWire` | talking to the engine: a session, its verbs, the record as it arrives | plain C++; no Unreal type in `Public/` beyond the export macro; every call blocking, off the game thread |
| `RpgClient` | what a beat means: which cells a walk takes, where a swing lands | functions with no world in them, tested against beats the engine wrote |
| `RpgBoard` | showing the board, figures and status; passing play input to the wire | no core type in `Public/`; only record beats move figures |

Three Unreal rules give the tree its shape:

- **`Public/` is what other modules may include.** `Private/` is the
  module's own. `RpgSession.h` is public because the board module calls it.
- **Generated protobuf code cannot cross a module boundary.** Every Unreal
  module is its own shared library, and protobuf's generated symbols break
  across one. So `Private/pb/` compiles as part of `RpgWire`, not in a
  module of its own.
- **A `.Build.cs` is a module's dependency list.** It is C# the build tool
  runs; `RpgWire.Build.cs` is where the gRPC libraries get linked.

At run time, the game mode hosts the board's play loop. The loop sends verbs
through `RpgSession` and takes beats from `RpgRecordStream`; it gives each
beat to `RpgClient` for interpretation before the board shows its motion.

## Where the contract comes from

One merge to the protos repository makes one tag, and every client takes
the contract from that tag:

```mermaid
flowchart LR
    M[".proto on main"] -->|"merge: CI generates,<br/>tags vX.Y.Z"| T["generated branch, tag vX.Y.Z"]
    T -->|"module proxy, the tag's commit"| A["rpg-api (Go)"]
    T -->|"npm tarball of the tag,<br/>from GitHub"| W["rpg-dnd5e-web (TypeScript)"]
    T -->|"git fetch of the tag,<br/>one commit deep"| U["this client"]
    U -->|"buf build, then protoc and grpc_cpp_plugin<br/>built here from the same pin"| P["Plugins/RpgWire/Source/RpgWire/Private/pb/"]
```

The other two clients consume code the protos pipeline generates. This one
consumes the tag's *sources* and generates the C++ itself, with `protoc` and
`grpc_cpp_plugin` that `scripts/build-libs.sh` builds from the same commit as
the runtime (design rulings R3 and R8). Generated C++ is bound to the
protobuf and gRPC it links, so the generators and the runtime come from one
pin and cannot drift. A bump of the contract is a one-line change to
`protos.ref`, as the web's is to its `package.json`; a bump of the runtime
and its generators is a one-line change to `scripts/libs/vcpkg.pin`.

`Private/pb/` is never committed and never edited: a field with the wrong
name is fixed in the protos and regenerated.

The libraries in `Source/ThirdParty/` are built, not fetched.
`scripts/build-libs.sh` takes vcpkg at the commit in
`scripts/libs/vcpkg.pin` and builds gRPC, protobuf, abseil, re2, c-ares,
OpenSSL and zlib with the engine's own compiler against the engine's own
libc++, with the flags a module compiles with — no exceptions, no RTTI,
hidden visibility — through `scripts/libs/ue-linux.toolchain.cmake` and the
triplet beside it. `scripts/test-libs.sh` measures the result: no RTTI, no
exceptions, the pin written down.

## Build and test

Needs Linux, Unreal Engine 5.9, [`buf`](https://buf.build) and a C++
compiler for this machine, which builds the generators. No checkout of the
protos repository is needed.

```
scripts/bootstrap.sh     build the libraries (about ten minutes cold), generate
                         the contract, mount content if rpg-game-assets is
                         beside this repository
scripts/build-libs.sh    build gRPC and protobuf with the engine's own toolchain,
                         from the pin in scripts/libs/vcpkg.pin
scripts/test-libs.sh     the checks on the libraries: built by this
                         repository, built as the module is built, the pin written down
scripts/build.sh         build the editor target
scripts/test.sh          run the tests: beat-reading and board always; the verbs
                         against the engine's test server when RPGWIRE_TEST_ADDR
                         and RPGWIRE_TEST_INFO are set
scripts/perfile-check.py compile every hand-written .cpp on its own
scripts/play.sh <session> run tomb, range, walk or castle using UE_SPIKE_INFO
```

`UE_ROOT` says where the engine is, if not `~/UnrealEngine/UE_5.9`. To
work against a branch of the contract before it is published, set
`RPG_API_PROTOS_DIR` to a checkout of it: what is committed at its `HEAD`
is used, never a working tree.

The engine's test server is rpg-api's `TestUESpikeServe`, in
`internal/integration/session/`; it listens on `UE_SPIKE_ADDR` and writes
the runs' details to `UE_SPIKE_INFO`.

## Rules this repository keeps

- **No licensed content.** Nothing from Synty, no GLB or FBX, no engine
  content. Licensed models live in `rpg-game-assets`, the private store,
  and are mounted by `scripts/mount-content.sh`.
- **Nothing generated is edited.** The contract is generated from the
  protos at the tag in `protos.ref`; the libraries are fetched, pinned by
  hash.
- Work in a worktree; attribute as your team; the paper trail is issues
  and pull requests.

Start in [CLAUDE.md](CLAUDE.md).
