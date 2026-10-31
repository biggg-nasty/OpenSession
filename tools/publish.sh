#!/usr/bin/env bash
# Publishes this repository to GitHub as a public repo under the logged-in account.
# Requires: gh (brew install gh) and `gh auth login` done once.
set -euo pipefail
cd "$(dirname "$0")/.."
NAME="${1:-OpenSession}"
gh auth status >/dev/null 2>&1 || { echo "Run: gh auth login"; exit 1; }
OWNER="$(gh api user --jq .login)"
if ! git remote get-url origin >/dev/null 2>&1; then
    gh repo create "$NAME" --public --description "Recording-first open-source multitrack DAW (JUCE + Tracktion Engine, GPL-3.0-or-later)" \
        --source . --remote origin --push
else
    git push -u origin main
fi
echo "Published: https://github.com/$OWNER/$NAME"
echo "Check that the LICENSE badge shows GPL-3.0 and that the submodule link resolves (third_party/tracktion_engine)."
