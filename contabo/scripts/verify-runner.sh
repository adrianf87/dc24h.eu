#!/usr/bin/env bash
set -euo pipefail
test "${RUNNER_NAME:-}" = server-747585786
test "${RUNNER_ENVIRONMENT:-}" = self-hosted
test "$(hostname)" = server-747585786
test "$(uname -m)" = x86_64
test "$(id -un)" = github-runner
test "$(id -u)" -ne 0
# shellcheck disable=SC1091
source /etc/os-release
test "$ID" = ubuntu && test "$VERSION_ID" = 24.04
printf 'Verified Contabo runner: %s, %s, Ubuntu %s, user %s\n' \
  "$RUNNER_NAME" "$(uname -m)" "$VERSION_ID" "$(id -un)"
