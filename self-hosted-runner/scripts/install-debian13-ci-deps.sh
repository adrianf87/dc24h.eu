#!/usr/bin/env bash
# Debian 13 package prerequisites for the OWNER-approved self-hosted test VPS.
set -Eeuo pipefail

if (( $# == 0 )); then
  printf 'Usage: bash self-hosted-runner/scripts/install-debian13-ci-deps.sh PACKAGE...\n' >&2
  exit 2
fi

# No Ubuntu sources, source rewrites, or GitHub-hosted runner assumptions.
# The OWNER has approved sudo package installation on this test VPS.
. /etc/os-release
if [[ "${ID:-}" != "debian" || "${VERSION_ID:-}" != "13" ]]; then
  printf 'Expected Debian 13, got %s %s\n' "${ID:-unknown}" "${VERSION_ID:-unknown}" >&2
  exit 1
fi
if [[ "$(hostname -s)" != "server-747585786" || "$(id -un)" != "github-runner" ]]; then
  printf 'Expected the approved github-runner account on server-747585786\n' >&2
  exit 1
fi

apt_network_options=(
  -o Acquire::Retries=3
  -o Acquire::http::Timeout=30
  -o Acquire::https::Timeout=30
)

sudo -n apt-get "${apt_network_options[@]}" update -qq
sudo -n env DEBIAN_FRONTEND=noninteractive apt-get "${apt_network_options[@]}" install -y --no-install-recommends "$@"
