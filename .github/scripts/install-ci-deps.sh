#!/usr/bin/env bash
set -Eeuo pipefail

if (( $# == 0 )); then
  echo "Usage: $0 PACKAGE..." >&2
  exit 2
fi

# GitHub-hosted Ubuntu VMs use sudo; Debian CI containers execute as root.
# Do not grant passwordless elevation or require sudo inside a disposable image.
if (( EUID == 0 )); then
  as_root=()
else
  as_root=(sudo -n)
fi

# Hosted runners can expose an unreachable regional EC2 mirror via apt-mirrors.txt.
# Keep the normal Ubuntu suites and signing policy, but use the canonical mirror.
for sources in /etc/apt/apt-mirrors.txt /etc/apt/sources.list /etc/apt/sources.list.d/ubuntu.sources; do
  if [[ -f "$sources" ]]; then
    "${as_root[@]}" sed -i -E \
      's#https?://[[:alnum:].-]+\.ec2\.archive\.ubuntu\.com/ubuntu/?#http://archive.ubuntu.com/ubuntu/#g' \
      "$sources"
  fi
done

apt_network_options=(
  -o Acquire::Retries=1
  -o Acquire::http::Timeout=15
  -o Acquire::https::Timeout=15
)

# GitHub-hosted Ubuntu images sometimes include unrelated third-party APT feeds.
# A failure of one of those feeds (for example, Microsoft returning HTTP 403)
# must not block installation of packages that come from Ubuntu itself.
# Restrict BOTH update and install to the image's signed Ubuntu source file.
# This does not disable signature checks, ignore apt failures, or change
# repository configuration on the runner. Other environments retain their
# existing APT sources.
if [[ "${GITHUB_ACTIONS:-}" == "true" && "${RUNNER_ENVIRONMENT:-}" == "github-hosted" ]]; then
  . /etc/os-release
  if [[ "${ID:-}" == "ubuntu" ]]; then
    if [[ -f /etc/apt/sources.list.d/ubuntu.sources ]]; then
      ubuntu_apt_source=/etc/apt/sources.list.d/ubuntu.sources
    elif [[ -s /etc/apt/sources.list ]]; then
      ubuntu_apt_source=/etc/apt/sources.list
    else
      echo "No canonical Ubuntu APT source found on hosted runner" >&2
      exit 1
    fi
    apt_network_options+=(
      -o "Dir::Etc::sourcelist=${ubuntu_apt_source}"
      -o "Dir::Etc::sourceparts=-"
    )
  fi
fi

# Fail with an actionable timeout instead of consuming the whole CI job slot.
"${as_root[@]}" timeout --signal=TERM --kill-after=10s 300s \
  apt-get "${apt_network_options[@]}" update

"${as_root[@]}" env DEBIAN_FRONTEND=noninteractive \
  timeout --signal=TERM --kill-after=10s 1200s \
  apt-get "${apt_network_options[@]}" install -y --no-install-recommends "$@"
