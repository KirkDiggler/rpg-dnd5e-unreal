#!/usr/bin/env bash
# Exercise the board against an already-running engine fixture. The fixture is
# mutable; use a fresh server before a scenario pass.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}"
SCENARIO="${1:-}"
MODE="${2:-}"
usage() { echo 'usage: scripts/play.sh <tomb|range|walk|castle> [--play]' >&2; exit 1; }
[ "$#" -le 2 ] && { [ -z "$MODE" ] || [ "$MODE" = --play ]; } || usage

case "$SCENARIO" in
  tomb)   MEMBER_KEY=fighter; EXIT_AT=12; ACTS='rpg.play.click 2 1 @3,rpg.play.key E @6' ;;
  range)  MEMBER_KEY=archer;  EXIT_AT=6;  ACTS='rpg.play.click 4 1 @3' ;;
  walk)   MEMBER_KEY=walker;  EXIT_AT=10; ACTS='rpg.play.click 3 1 @3,rpg.play.where ue-walker @8' ;;
  castle) MEMBER_KEY=castle_member; EXIT_AT=6; ACTS='' ;;
  *) usage ;;
esac

HOST_KEY="${MEMBER_KEY}_host"
if [ "$SCENARIO" = castle ]; then HOST_KEY=castle_host; fi
INFO="${UE_SPIKE_INFO:-$HOME/UnrealEngine/.ue-spike-info.json}"
read_key() { jq -er --arg key "$1" '.[$key] | strings | select(length > 0)' "$INFO"; }
if ! ADDR="$(read_key addr)" || ! SESSION="$(read_key "$SCENARIO")" ||
   ! MEMBER="$(read_key "$MEMBER_KEY")" || ! PLAYER="$(read_key "$HOST_KEY")"; then
  echo "FAIL fixture info ($INFO)"
  exit 1
fi

OUT="$ROOT/Saved/PlayShots"
FRAME="$OUT/$SCENARIO.png"
LOG="$OUT/$SCENARIO.log"
mkdir -p "$OUT"
EDITOR=("$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" "$ROOT/RpgDnd5eUnreal.uproject"
  /Engine/Maps/Entry -game -NoSound -stdout -FullStdOutLogOutput
  "-RpgAddr=$ADDR" "-RpgSession=$SESSION" "-RpgMember=$MEMBER" "-RpgPlayer=$PLAYER")
RESULT=0
if [ "$MODE" = --play ]; then
  LOG="$OUT/$SCENARIO-play.log"
  PLAYFRAME="$OUT/$SCENARIO-play.png"
  if [ -e "$PLAYFRAME" ]; then unlink "$PLAYFRAME"; fi
  "${EDITOR[@]}" -windowed -ResX=1280 -ResY=720 "-RpgShotOnJoin=$PLAYFRAME" > "$LOG" 2>&1 || RESULT=$?
else
  # A prior run must not stand in for a frame this run actually produced.
  if [ -e "$FRAME" ]; then unlink "$FRAME"; fi
  COMMANDS="${ACTS:+$ACTS,}rpg.play.shot '$FRAME' @$((EXIT_AT - 1)),rpg.play.exit @$EXIT_AT"
  timeout --kill-after=5s 90s "${EDITOR[@]}" -RenderOffscreen -unattended "-ExecCmds=$COMMANDS" > "$LOG" 2>&1 || RESULT=$?
fi

FAILS=0
check() {
  local fact="$1"; shift
  if "$@"; then printf 'PASS %s\n' "$fact"; else printf 'FAIL %s\n' "$fact"; FAILS=$((FAILS + 1)); fi
}
logged() { grep -aqF -- "LogRpgPlay: Display: $1" "$LOG"; }
matches() { grep -aqE -- "LogRpgPlay: Display: $1" "$LOG"; }
not_logged() { ! logged "$1"; }
counted() { [ "$(grep -acF -- "LogRpgPlay: Display: $1" "$LOG")" -eq "$2" ]; }
refusal_shown() {
  local words
  words="$(sed -n 's/^.*LogRpgPlay: Display: REFUSED attack \(.*[^[:space:]]\)[[:space:]]*$/\1/p' "$LOG" | head -n 1)"
  [ -n "$words" ] && grep -aF 'LogRpgPlay: Display: STATUS ' "$LOG" | grep -F -- "$words" >/dev/null
}

if [ "$MODE" = --play ]; then
  # SIGHT lines are emitted only for this observer's parsed gained entries.
  # Correlate all three facts to the same member, not three unrelated lines.
  facts="$(awk -v observer="$MEMBER" '
    /LogRpgPlay: Display: / {
      sub(/^.*LogRpgPlay: Display: /, ""); sub(/\r$/, "")
      if ($1 == "SIGHT" && $2 == "observer=" observer && $3 ~ /^gained=/) {
        sight[substr($3, 8)] = 1
      } else if ($1 == "JOINED" && sight[$2]) {
        joined[$2] = 1; seenJoin = 1
      } else if ($1 == "ROSTER" && $3 ~ /^members=/) {
        n = split(substr($3, 9), roster, ",")
        for (i = 1; i <= n; ++i) if (joined[roster[i]]) resolved[roster[i]] = 1
      } else if ($1 == "FIGURE" && $3 == "side=monster" && resolved[$2]) {
        monster[$2] = 1; seenMonster = 1
      } else if ($1 == "MOTION" && $2 == "walk" && monster[$3]) {
        seenWalk = 1
      } else if ($1 == "LOST") {
        delete sight[$2]; delete joined[$2]; delete resolved[$2]; delete monster[$2]
      }
    }
    END { print seenJoin+0, seenMonster+0, seenWalk+0 }
  ' "$LOG")"
  read -r JOINED MONSTER WALKED <<< "$facts"
  check frame test -s "$PLAYFRAME"
  check 'sighted member joined' test "$JOINED" -eq 1
  check 'admitted member resolved to monster from roster' test "$MONSTER" -eq 1
  check 'admitted monster walked' test "$WALKED" -eq 1
  if [ "$RESULT" -ne 0 ]; then
    echo "FAIL game process (status $RESULT; log: $LOG)"
    FAILS=$((FAILS + 1))
  fi
  exit "$FAILS"
fi

check 'floor material' matches 'FLOOR cells=[0-9]+ material=VertexColorMaterial'
check 'no missing object' bash -c '! grep -aq "Failed to find object" "$1"' _ "$LOG"
check 'ground' logged 'GROUND ok'
check "process exited within timeout (status $RESULT)" test "$RESULT" -eq 0
check 'play target' logged "PLAY addr=$ADDR session=$SESSION member=$MEMBER"
case "$SCENARIO" in
  tomb)
    check 'fighter active colour' logged 'COLOUR ue-fighter #63B3ED'
    check 'dummy colour' logged 'COLOUR ue-dummy #E53E3E'
    check 'dummy active colour' logged 'COLOUR ue-dummy #FC8181'
    check 'fighter inactive colour' logged 'COLOUR ue-fighter #3182CE'
    check 'walls reported' matches 'WALLS drawn=[0-9]+'
    check 'roster names the fighter and skeleton'  matches 'ROSTER n=2 members=(ue-fighter,ue-dummy|ue-dummy,ue-fighter)\r?$'
    check 'turn clock' logged 'AFFORD clock=2 '
    check 'attack sent' logged 'VERB attack ue-fighter ue-dummy'
    check 'attack beat' matches 'BEAT (struck|missed) act='
    check 'fighter is player' logged 'FIGURE ue-fighter side=player'
    check 'dummy is monster' logged 'FIGURE ue-dummy side=monster'
    check 'fighter swing' logged 'MOTION swing ue-fighter'
    check 'end turn sent' logged 'VERB end-turn ue-fighter'
    check 'turn ended beat' logged 'BEAT turn-ended '
    check 'turn marker motion' matches 'MOTION turn [^[:space:]]+'
    ;;
  range)
    check 'archer active colour' logged 'COLOUR ue-archer #63B3ED'
    check 'far colour' logged 'COLOUR ue-far #E53E3E'
    check 'turn clock' logged 'AFFORD clock=2 '
    check 'attack sent' logged 'VERB attack ue-archer ue-far'
    check 'attack refused with words' matches 'REFUSED attack [^[:space:]]'
    check 'refusal shown' refusal_shown
    check 'no swing' not_logged 'MOTION swing'
    ;;
  walk)
    check 'walker active colour' logged 'COLOUR ue-walker #63B3ED'
    check 'world clock' logged 'AFFORD clock=1 '
    check 'move sent' logged 'VERB move ue-walker (1,1)…(3,1)'
    check 'two moved beats' counted 'BEAT moved ' 2
    check 'two walk motions' counted 'MOTION walk ue-walker' 2
    check 'walker reached the cell' logged 'CELL ue-walker 3 1'
    check 'no turn ended beat' not_logged 'BEAT turn-ended '
    ;;
  castle)
    check 'alice active colour' logged 'COLOUR alice #63B3ED'
    check 'pedlar colour' logged 'COLOUR pedlar #E53E3E'
    check 'walls drawn' matches 'WALLS drawn=[1-9][0-9]*'
    check 'nonempty roster' matches 'ROSTER n=[1-9][0-9]* '
    check 'pedlar is monster' logged 'FIGURE pedlar side=monster'
    check 'nonempty atlas' matches 'ATLAS cells=[1-9][0-9]* '
    check 'clock reported' matches 'AFFORD clock=[12] '
    ;;
esac
check 'exit logged' logged 'EXIT'
check 'no refused exit' not_logged 'REFUSED exit'
check frame test -s "$FRAME"
exit "$FAILS"
