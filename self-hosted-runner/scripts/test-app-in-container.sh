#!/usr/bin/env bash
# Executed only inside one disposable Debian/Ubuntu container.
set -Eeuo pipefail

distro="${DC24H_TEST_DISTRO:?DC24H_TEST_DISTRO is required}"
. /etc/os-release
case "$distro" in
  debian-12)     expected_id=debian; expected_version=12 ;;
  debian-13)     expected_id=debian; expected_version=13 ;;
  ubuntu-22.04) expected_id=ubuntu; expected_version=22.04 ;;
  ubuntu-24.04) expected_id=ubuntu; expected_version=24.04 ;;
  ubuntu-26.04) expected_id=ubuntu; expected_version=26.04 ;;
  *) echo "Unexpected distribution $distro" >&2; exit 2 ;;
esac
if [[ "$ID" != "$expected_id" || "$VERSION_ID" != "$expected_version" ]]; then
  echo "Image mismatch: expected $expected_id $expected_version, got $ID $VERSION_ID" >&2
  exit 1
fi

report_dir="${RUNNER_TEMP:-/tmp/dc24h-ci-results}"
mkdir -p "$report_dir"
on_exit() {
  result=$?
  printf 'Distribution: %s\nResult: %s\n' "$distro" "$result" > "$report_dir/result.txt"
}
trap on_exit EXIT

echo "Testing on $PRETTY_NAME: C++/CTest, WebAdmin TypeScript, unit, browser and integration tests."
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y --no-install-recommends \
  ca-certificates curl xz-utils tar git \
  build-essential cmake pkg-config python3 python3-dev \
  libutf8proc-dev libgcrypt20-dev libssl-dev libmariadb-dev \
  openssl libstdc++6

# Official Node.js 24 tarball with its published SHA-256, not a host binary.
# Node 24 is required for the Stage IV public WebAdmin build.
node_checksums="$(curl -fsSL https://nodejs.org/dist/latest-v24.x/SHASUMS256.txt)"
node_line="$(printf '%s\n' "$node_checksums" | awk '$2 ~ /^node-v24\.[0-9.]+-linux-x64\.tar\.xz$/ {print; exit}')"
if [[ -z "$node_line" ]]; then
  echo 'Cannot resolve the official Node.js 24 Linux x64 archive.' >&2
  exit 1
fi
read -r node_sha node_archive <<< "$node_line"
curl -fsSL "https://nodejs.org/dist/latest-v24.x/$node_archive" -o "/tmp/$node_archive"
( cd /tmp && printf '%s  %s\n' "$node_sha" "$node_archive" | sha256sum -c - )
tar -xJf "/tmp/$node_archive" -C /usr/local --strip-components 1
rm -f "/tmp/$node_archive"
node --version
npm --version

# No host /etc, database directory, systemd socket or Docker socket is mounted.
# Keep the former main CI coverage: both C++ build types and two sanitizers.
for build_type in Debug Release; do
  cmake -S /workspace -B "/workspace/build/$build_type" \
    -DCMAKE_BUILD_TYPE="$build_type" -DBUILD_TESTING=ON
  cmake --build "/workspace/build/$build_type" --parallel 16
  ctest --test-dir "/workspace/build/$build_type" --parallel 4 --output-on-failure \
    --output-junit "$report_dir/ctest-$build_type.xml"
done

for sanitizer in address undefined; do
  cmake -S /workspace -B "/workspace/build/sanitizer-$sanitizer" \
    -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DDC24H_SANITIZER="$sanitizer"
  cmake --build "/workspace/build/sanitizer-$sanitizer" --parallel 16
  ctest --test-dir "/workspace/build/sanitizer-$sanitizer" --output-on-failure \
    --output-junit "$report_dir/ctest-$sanitizer.xml" \
    -R '^(memory\.safety|adc\.(parser|security|syntax|sch|rcm|ctm)|network\.unit|transfers\.(protocol|tokens)|security\.unit|api\.unit|beta\.baseline)$'
done

cd /workspace/webadmin
npm ci --no-audit --no-fund
npm run typecheck
npm run build
npm run build:public:ci
npm run build:stage4-init
npm run build:enrollment:ci
npm test

# Browser execution must be real; unsupported image dependencies cause FAIL,
# not an invented PASS or implicit SKIPPED result.
npx playwright install --with-deps chromium
npm run test:e2e -- --workers=2
npm run test:integration -- ../build/Release/dc24h-twilight
