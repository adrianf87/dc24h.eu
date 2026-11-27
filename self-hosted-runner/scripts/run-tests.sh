#!/usr/bin/env bash
# Self-hosted runner qualification entry point. No automatic host changes.
set -euo pipefail

runner_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
mode="${1:-offline}"
if [ "$#" -gt 1 ]; then
    printf 'Usage: bash self-hosted-runner/scripts/run-tests.sh [offline|host-read-only|all]\n' >&2
    exit 2
fi

run_offline() {
    python3 -m unittest discover -s "$runner_dir/tests" -p 'test_*_offline.py' -v
}
run_host_read_only() {
    python3 -m unittest discover -s "$runner_dir/tests" -p 'test_host_read_only.py' -v
}

case "$mode" in
    offline)
        run_offline
        ;;
    host-read-only)
        printf '%s\n' 'Manual read-only qualification of the approved Debian 13 runner.'
        run_host_read_only
        ;;
    all)
        run_offline
        run_host_read_only
        ;;
    *)
        printf 'Unknown mode: %s\n' "$mode" >&2
        exit 2
        ;;
esac
