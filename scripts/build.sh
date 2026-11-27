#!/usr/bin/env bash
# Builds the editor target, which builds everything: the game module, the
# plugin's two modules and the generated contract (about two minutes clean).
# Close any running editor on this project first: Unreal builds a hot-reload
# library beside a running editor, and the next run loads that one instead.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}"
TARGET="$ROOT/Plugins/RpgWire/Source/RpgWire/Private/pb/SOURCE"
[ -f "$TARGET" ] || { echo "build: no compiled contract; run scripts/bootstrap.sh first" >&2; exit 1; }
exec "$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh" RpgDnd5eUnrealEditor "$ROOT/RpgDnd5eUnreal.uproject" "${1:-Development}" Linux
