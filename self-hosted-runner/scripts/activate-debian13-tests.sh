#!/usr/bin/env bash
# Overlay the versioned Debian 13 test snapshot onto the checkout's tests/
# before CMake, CTest, Python and WebAdmin integration tests execute.
set -Eeuo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/../.." && pwd)"
snapshot="$repo_root/self-hosted-runner/tests/debian-13"
destination="$repo_root/tests"

if [[ ! -f /etc/os-release ]]; then
  printf 'Cannot verify Debian 13: /etc/os-release is absent\n' >&2
  exit 1
fi
# shellcheck disable=SC1091
. /etc/os-release
if [[ "${ID:-}" != debian || "${VERSION_ID:-}" != 13 ]]; then
  printf 'Debian 13 test snapshot requires Debian 13, found %s %s\n' "${ID:-unknown}" "${VERSION_ID:-unknown}" >&2
  exit 1
fi
if [[ "${GITHUB_ACTIONS:-false}" == true ]] &&
   { [[ "${RUNNER_ENVIRONMENT:-}" != self-hosted ]] ||
     [[ "$(hostname -s)" != server-747585786 ]] ||
     [[ "$(id -un)" != github-runner ]]; }; then
  printf 'Unexpected runner for Debian 13 CI test snapshot\n' >&2
  exit 1
fi
if [[ ! -d "$snapshot" || ! -d "$destination" ]]; then
  printf 'Missing Debian 13 snapshot or repository tests directory\n' >&2
  exit 1
fi

# Fail if application tests were added without updating the dedicated snapshot.
count=0
while IFS= read -r -d '' file; do
  relative="${file#"$destination"/}"
  if [[ ! -f "$snapshot/$relative" ]]; then
    printf 'Debian 13 snapshot missing test: %s\n' "$relative" >&2
    exit 1
  fi
  (( count += 1 ))
done < <(find "$destination" -type f -print0)
if (( count < 267 )); then
  printf 'Expected at least 267 application test files, found %s\n' "$count" >&2
  exit 1
fi

cp -a -- "$snapshot/." "$destination/"
printf 'Activated Debian 13 test snapshot: verified %s source test files\n' "$count"
