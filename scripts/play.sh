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
tomb_walls_match_atlas() {
  awk '/LogRpgPlay: Display: ATLAS cells=/ { if (seen) bad=1; if (match($0, /placed=[0-9]+/)) { n=substr($0,RSTART+7,RLENGTH-7)+0; seen=1 } else bad=1 }
       /LogRpgPlay: Display: WALLS drawn=/ && seen { if (match($0, /drawn=[0-9]+/)) { w=substr($0,RSTART+6,RLENGTH-6)+0; if (!((n==0 && w==0) || (n>0 && w>0))) bad=1; pairs++ } else bad=1; seen=0 }
       END { exit (bad || seen || !pairs) }' "$LOG"
}
after_end_turn() {
  awk -v wanted="$1" '/LogRpgPlay: Display: VERB end-turn / { ended=1 } ended && index($0, "LogRpgPlay: Display: " wanted) { found=1 } END { exit !found }' "$LOG"
}
refusal_shown() {
  local words
  words="$(sed -n 's/^.*LogRpgPlay: Display: REFUSED attack \(.*[^[:space:]]\)[[:space:]]*$/\1/p' "$LOG" | head -n 1)"
  [ -n "$words" ] && grep -aF 'LogRpgPlay: Display: STATUS ' "$LOG" | grep -F -- "$words" >/dev/null
}

KINGDOM=/Game/PolygonFantasyKingdom/Meshes/CharactersUE4
MOUNTED=0
if [ -f "$ROOT/Config/RpgGameAssetsManifest.json" ] && [ -d "$ROOT/Content/PolygonFantasyKingdom" ]; then MOUNTED=1; fi
native_or_colour() {
  local name="$1" asset="$2" colour="$3"
  if [ "$MOUNTED" -eq 1 ]; then
    check "$name native" logged "FIGURE $name native=$KINGDOM/$asset"
  else
    check "$name colour" logged "COLOUR $name $colour"
  fi
}
castle_guard_frame() {
  awk -v prefix="$KINGDOM/BP_Chr_Soldier_" '
    /LogRpgPlay: Display: / {
      sub(/^.*LogRpgPlay: Display: /, ""); sub(/\r$/, "")
      if ($1 == "SHOT") exit
      if ($1 == "JOINED" && $2 ~ /^gate-/) guards[$2] = 1
      if ($1 == "LOST") delete guards[$2]
      if ($1 == "FIGURE" && $2 ~ /^gate-/ && index($3, "native=") == 1)
        soldier[$2] = index($3, "native=" prefix) == 1
      if ($1 == "PIXEL" && $7 == "NATIVE") pixel[$2] = 1
    }
    END {
      for (guard in guards) {
        n++
        if (!soldier[guard] || !pixel[guard]) bad = 1
      }
      exit (!n || bad)
    }' "$LOG"
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
        for (i = 1; i <= n; ++i) resolved[roster[i]] = 1
      } else if ($1 == "FIGURE" && $3 == "side=monster" && resolved[$2] && sight[$2]) {
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
  check palette python3 "$ROOT/scripts/lookcheck.py" "$PLAYFRAME" "$LOG"
  check 'sighted member joined' test "$JOINED" -eq 1
  check 'admitted member resolved to monster from roster' test "$MONSTER" -eq 1
  check 'admitted monster walked' test "$WALKED" -eq 1
  if [ "$SCENARIO" = castle ] && [ "$MOUNTED" -eq 1 ]; then
    check 'castle guards in this frame resolved to native soldiers' castle_guard_frame
  fi
  if [ "$RESULT" -ne 0 ]; then
    echo "FAIL game process (status $RESULT; log: $LOG)"
    FAILS=$((FAILS + 1))
  fi
  exit "$FAILS"
fi

check 'floor material' matches 'FLOOR cells=[0-9]+ material=VertexColorViewMode_ColorOnly'
check 'no missing object' bash -c '! grep -aq "Failed to find object" "$1"' _ "$LOG"
check 'ground' logged 'GROUND ok'
check "process exited within timeout (status $RESULT)" test "$RESULT" -eq 0
check 'play target' logged "PLAY addr=$ADDR session=$SESSION member=$MEMBER"
case "$SCENARIO" in
  tomb)
    native_or_colour ue-fighter BP_Chr_Rider_01 '#63B3ED'
    check 'dummy uses a stand-in' logged 'FIGURE ue-dummy shape'
    check 'dummy colour' logged 'COLOUR ue-dummy #E53E3E'
    check 'dummy active colour after end turn' after_end_turn 'COLOUR ue-dummy #FC8181'
    if [ "$MOUNTED" -eq 0 ]; then check 'fighter inactive colour after end turn' after_end_turn 'COLOUR ue-fighter #3182CE'; fi
    check 'walls match atlas placement' tomb_walls_match_atlas
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
    native_or_colour ue-archer BP_Chr_Rider_01 '#63B3ED'
    check 'far skeleton uses a stand-in' logged 'FIGURE ue-far shape'
    check 'far colour' logged 'COLOUR ue-far #E53E3E'
    check 'turn clock' logged 'AFFORD clock=2 '
    check 'attack sent' logged 'VERB attack ue-archer ue-far'
    check 'attack refused with words' matches 'REFUSED attack [^[:space:]]'
    check 'refusal shown' refusal_shown
    check 'no swing' not_logged 'MOTION swing'
    ;;
  walk)
    native_or_colour ue-walker BP_Chr_Rider_01 '#63B3ED'
    check 'world clock' logged 'AFFORD clock=1 '
    check 'move sent' logged 'VERB move ue-walker (1,1)…(3,1)'
    check 'two moved beats' counted 'BEAT moved ' 2
    check 'two walk motions' counted 'MOTION walk ue-walker' 2
    check 'walker reached the cell' logged 'CELL ue-walker 3 1'
    check 'no turn ended beat' not_logged 'BEAT turn-ended '
    ;;
  castle)
    native_or_colour alice BP_Chr_Rider_01 '#63B3ED'
    native_or_colour pedlar BP_Chr_Merchant_01 '#E53E3E'
    if [ "$MOUNTED" -eq 1 ]; then
      check 'two castle native subjects' matches 'PIXEL (alice|pedlar) .* NATIVE\r?$'
      check 'exactly two native footprints' test "$(grep -acE 'PIXEL .+ NATIVE\r?$' "$LOG")" -eq 2
    fi
    check 'walls drawn' matches 'WALLS drawn=7\r?$'
    check 'nonempty roster' matches 'ROSTER n=[1-9][0-9]* '
    check 'pedlar is monster' logged 'FIGURE pedlar side=monster'
    check 'nonempty atlas' matches 'ATLAS cells=[1-9][0-9]* '
    check 'clock reported' matches 'AFFORD clock=[12] '
    ;;
esac
check 'exit logged' logged 'EXIT'
check 'no refused exit' not_logged 'REFUSED exit'
check frame test -s "$FRAME"
check palette python3 "$ROOT/scripts/lookcheck.py" "$FRAME" "$LOG"
exit "$FAILS"
