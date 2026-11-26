# Working in rpg-dnd5e-unreal

Read this first, whatever you are. It points; it does not repeat.

## What this is

The Unreal client of the RPG engine, a client like the web's and no more:
it sends references and verbs and renders what the engine sends back. It
computes no rule. The law is the design doc in rpg-project,
`ideas/unreal-client/design.md`; read its Law and Rulings before changing
a seam. The workspace's own rules are in the root `AGENTS.md` of game-dev.

## Where things are

| you are changing | it lives | it must keep |
|---|---|---|
| how the client talks to the engine | `Plugins/RpgWire/Source/RpgWire/` | plain C++: no Unreal type in `Public/` beyond the export macro; every call blocking, run off the game thread |
| what a record beat means | `Plugins/RpgWire/Source/RpgClient/` | a function with no world in it, tested against beats the engine wrote |
| how a beat is shown | a later module, not yet here | motion only from what the record says, never from a verb's response |
| what is generated or vendored | `scripts/`, never by hand | `Private/pb/` and `Source/ThirdParty/` are ignored for a reason |

## Before you build

`scripts/bootstrap.sh`, once. It fetches the compiled contract from the
protos repository beside this one and the libraries from a pinned
archive. Both are stopgaps the design PR's open rulings replace (R2, R3).

## Verify, and say what you ran

- `scripts/build.sh` — the editor target.
- `scripts/test.sh` — the beat-reading's tests; with the engine's test
  server running (`RPGWIRE_TEST_ADDR`, `RPGWIRE_TEST_INFO`), the verbs'.
  The editor's exit status is not the verdict; read the per-test results.
- `scripts/perfile-check.py` — every `.cpp` compiled alone. Unreal builds
  a module amalgamated, which lets a file borrow an include from its
  neighbour; the editor and this check do not.

A claim that it builds says on which machine, with which engine version,
from which commit.

## Rules

- **Never commit licensed content**: no Synty, no GLB or FBX converted
  from it, no engine content. `rpg-game-assets` is the store; evidence
  frames with licensed models in them stay local.
- **Never edit what is generated.** Change the protos, or the script.
- **One writer per worktree.** Attribute as the active team, in its
  charter's signature form.
- Gaps are issues in the repository that owns them.

## Open

On the design PR's checklists: R2 (the `descriptor` rename), R3 (the
libraries' build), R4 (where the engine runs), the engine version, and the
interaction model, O1, which is this client's own design.
