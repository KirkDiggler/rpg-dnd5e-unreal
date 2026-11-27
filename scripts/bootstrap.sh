#!/usr/bin/env bash
# Everything the repository does not hold: the compiled contract, the
# libraries, and the licensed content if there is any. Run once after cloning.
set -euo pipefail
cd "$(dirname "$0")/.."
scripts/build-libs.sh
scripts/fetch-protos.sh
scripts/mount-content.sh
cat <<NEXT

The first run built the libraries: about ten minutes, once.
Next: scripts/build.sh, then scripts/test.sh
Needs: Unreal Engine 5.9 (UE_ROOT, default ~/UnrealEngine/UE_5.9), on Linux.
NEXT
