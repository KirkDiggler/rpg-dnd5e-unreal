#!/usr/bin/env bash
# Runs the automation tests and reads their results (the editor's exit status
# is not a verdict: it force-terminates itself when the queue is empty).
#   RpgClient.* and RpgBoard.* always, with no engine server needed.
#   RpgWire.*    when RPGWIRE_TEST_ADDR and RPGWIRE_TEST_INFO name the engine's
#                test server (the harness lives in rpg-api; see rpg-project#1155).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}"
FILTER="RpgClient.+RpgBoard."
if [ -n "${RPGWIRE_TEST_ADDR:-}" ] && [ -n "${RPGWIRE_TEST_INFO:-}" ]; then
  FILTER="Rpg"
else
  echo "test: RPGWIRE_TEST_ADDR not set; running the beat-reading's and board's tests only"
fi
LOG="${TMPDIR:-/tmp}/rpg-dnd5e-unreal-tests.log"
"$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" "$ROOT/RpgDnd5eUnreal.uproject" \
  -ExecCmds="Automation RunTests $FILTER" -TestExit="Automation Test Queue Empty" \
  -ReportExportPath="$ROOT/Saved/Automation" -nullrhi -unattended -NoSound -stdout -FullStdOutLogOutput > "$LOG" 2>&1 || true
grep -a -o -E 'Test Completed\. Result=\{[A-Za-z]+\} Name=\{[^}]+\}' "$LOG" | sed -E 's/Test Completed\. Result=\{([A-Za-z]+)\} Name=\{(.*)\}/\1  \2/' | sort -u
PASSED="$(grep -a -c -E 'Result=\{Success\}' "$LOG" || true)"
FAILED="$(grep -a -c -E 'Result=\{(Fail|FailUnexpectedError)\}' "$LOG" || true)"
ERRORS="$(grep -a -c 'LogAutomationController: Error' "$LOG" || true)"
echo "test: $PASSED passed, $FAILED failed, $ERRORS automation error line(s); log: $LOG"
[ "$PASSED" -gt 0 ] && [ "$FAILED" -eq 0 ] && [ "$ERRORS" -eq 0 ]
