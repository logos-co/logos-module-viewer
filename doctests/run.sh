#!/usr/bin/env bash
#
# Execute the logos-module-viewer doc-test end-to-end and regenerate its Markdown.
#
# The spec (module-viewer-app.test.yaml) builds this commit's module-viewer app,
# builds the real logos-accounts-module as an installable module tree, then drives
# the viewer headlessly: it introspects the module with --list-methods and calls
# methods over the Logos IPC bridge with --call, asserting on the results.
#
# The runner is the shared `doctest` CLI
# (https://github.com/logos-co/logos-doctest), invoked directly via its flake.
# `doctest run` executes the steps, `doctest generate` renders the .md, and
# `doctest clean` strips build artifacts, keeping only the .md.
#
# To run against a local logos-doctest checkout instead of the published flake,
# set DOCTEST, e.g.:  DOCTEST="nix run path:../../logos-doctest --" ./run.sh
#
set -euo pipefail

# Run from this doctests/ directory regardless of where the script is invoked from.
cd "$(dirname "$0")"

# The doctest CLI. Override by exporting DOCTEST (space-separated command).
read -r -a DOCTEST <<< "${DOCTEST:-nix run github:logos-co/logos-doctest --}"
OUTPUT_DIR="./outputs"

# How to build the module-viewer app under test. Two modes:
#
#   LOCAL  (default) — build THIS working tree, including uncommitted changes.
#     run.sh exports MODULE_VIEWER_SRC=path:<repo-root>; the spec's build step
#     expands `${MODULE_VIEWER_SRC:-github:...}` to it. This is what you want while
#     developing: it needs no commit and no push.
#
#   REMOTE (REMOTE=1) — build the pinned GitHub commit instead. The spec falls back
#     to `github:logos-co/logos-module-viewer{release}`, pinned to $COMMIT via
#     --release-for. Use this in CI, or to reproduce exactly what a published build
#     would do. The commit must be pushed to the remote for nix to fetch it.
REPO_ROOT="$(cd .. && pwd)"
RELEASE_FOR=()
if [ "${REMOTE:-0}" = "1" ]; then
  unset MODULE_VIEWER_SRC
  COMMIT="${COMMIT-$(git rev-parse HEAD)}"
  if [ -n "${COMMIT}" ]; then
    RELEASE_FOR=(--release-for "logos-module-viewer=${COMMIT}")
    echo "==> REMOTE mode: pinning logos-module-viewer to ${COMMIT} (from GitHub)"
    if ! git branch -r --contains "${COMMIT}" >/dev/null 2>&1; then
      echo "WARNING: ${COMMIT} is not on any remote branch — nix cannot fetch it" >&2
      echo "         until you push; the build will fail to resolve." >&2
    fi
  else
    echo "==> REMOTE mode: building from latest logos-module-viewer master"
  fi
else
  export MODULE_VIEWER_SRC="path:${REPO_ROOT}"
  echo "==> LOCAL mode: building the working tree at ${REPO_ROOT}"
  echo "    (set REMOTE=1 to build the pinned GitHub commit instead)"
fi

echo "==> Clearing previous ${OUTPUT_DIR}/"
# A prior run copies artifacts out of the read-only nix store, so the directories
# land read-only too. Restore write permission before removing.
if [ -e "${OUTPUT_DIR}" ]; then
  chmod -R u+w "${OUTPUT_DIR}" 2>/dev/null || true
fi
rm -rf "${OUTPUT_DIR}"
mkdir -p "${OUTPUT_DIR}"

for spec in *.test.yaml; do
  name="$(basename "${spec%.test.yaml}")"
  echo "==> Running ${spec} into ${OUTPUT_DIR}/"
  # ${RELEASE_FOR[@]+...} guards the expansion so an empty array doesn't trip
  # `set -u` on older bash (e.g. macOS's stock 3.2).
  "${DOCTEST[@]}" run "${spec}" \
    --verbose \
    --continue-on-fail \
    ${RELEASE_FOR[@]+"${RELEASE_FOR[@]}"} \
    --output-dir "${OUTPUT_DIR}/"

  echo "==> Generating ${OUTPUT_DIR}/${name}.md"
  "${DOCTEST[@]}" generate "${spec}" \
    ${RELEASE_FOR[@]+"${RELEASE_FOR[@]}"} \
    -o "${OUTPUT_DIR}/${name}.md"
done

echo "==> Cleaning build artifacts from ${OUTPUT_DIR}/ (keeps .md)"
"${DOCTEST[@]}" clean "${OUTPUT_DIR}" --verbose

echo "==> Done. Rendered docs are in ${OUTPUT_DIR}/"
