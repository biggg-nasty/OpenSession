#!/usr/bin/env bash
# Gets a machine ready for the demo (docs/demo-script.md):
#   1. builds the Release app (configuring build-release/ and fetching submodules if needed),
#   2. runs the deterministic self-test suite,
#   3. regenerates the synthetic fallback session at demo/DemoSession.osession (File > Open demo session),
#   4. checks that it exports, and prints what the audio device really reports.
# Usage: tools/demo-prep.sh [demo-seconds]     (default 48)
set -euo pipefail
cd "$(dirname "$0")/.."

SECONDS_OF_DEMO="${1:-48}"
APP_BUNDLE="build-release/OpenSession_artefacts/Release/OpenSession.app"
APP="$APP_BUNDLE/Contents/MacOS/OpenSession"
DEMO="demo/DemoSession.osession"
CHECK_DIR="$(mktemp -d -t opensession-demo-check)"

step() { printf '\n==> %s\n' "$1"; }

step "Build (Release)"
if [ ! -f build-release/build.ninja ]; then
    tools/bootstrap.sh Release
fi
cmake --build build-release -j

step "Self-test suite"
"$APP" --run-tests 2>/dev/null | grep -E '^\[doctest\] (test cases|assertions|Status)'

step "Demo session ($SECONDS_OF_DEMO s of synthesised material)"
# The demo session is generated content (see demo/README.md); anything recorded into it while
# rehearsing is discarded here so every demo starts from the same state.
rm -rf "$DEMO"
"$APP" --make-demo "$DEMO" "$SECONDS_OF_DEMO" 2>/dev/null | tail -1

step "Export check"
"$APP" --export-mix "$DEMO" "$CHECK_DIR/mix.wav" 2>/dev/null | tail -1
"$APP" --export-stems "$DEMO" "$CHECK_DIR/stems" 2>/dev/null | tail -n +1
rm -rf "$CHECK_DIR"

step "Audio device as reported"
"$APP" --print-audio-settings 2>/dev/null || echo "(no audio device could be opened - check System Settings > Sound)"

cat <<EOF

Ready. Launch with:
  open "$APP_BUNDLE"
Then follow docs/demo-script.md. Before going live: plug in the interface/headphones, set input
gain so the track meter peaks around -12 dBFS, and grant microphone access if macOS asks.
EOF
