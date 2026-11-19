#!/usr/bin/env bash
# Run C++ and WebAdmin on the Debian 13 self-hosted runner host.
set -Eeuo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$root"

. /etc/os-release
echo "Host ID=${ID:-} VERSION_ID=${VERSION_ID:-} PRETTY_NAME=${PRETTY_NAME:-}"
test "${ID:-}" = "debian"
test "${VERSION_ID:-}" = "13"

sudo -n true
sudo -n apt-get update
sudo -n env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
  build-essential cmake python3 libutf8proc-dev libgcrypt20-dev \
  libssl-dev libmariadb-dev openssl ncdc mariadb-server apache2
sudo -n service mariadb start
sudo -n service apache2 start

command -v g++
command -v cmake
command -v python3
command -v ncdc
command -v node
node --version

workers="$(nproc)"
if [ "$workers" -gt 16 ]; then
  workers=16
fi
if [ "$workers" -lt 1 ]; then
  workers=1
fi
echo "Workers: $workers"

for build_type in Debug Release; do
  echo "=== Debian 13 host: C++ $build_type ==="
  cmake -S . -B "build/$build_type" \
    -DCMAKE_BUILD_TYPE="$build_type" \
    -DBUILD_TESTING=ON -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
  cmake --build "build/$build_type" --parallel "$workers"
  ctest --test-dir "build/$build_type" --parallel "$workers" --output-on-failure
done

cd webadmin
echo "=== Debian 13 host: WebAdmin ==="
npm ci --no-audit --no-fund
npm run build
node --import tsx --test --test-concurrency="$workers" tests/*.test.ts
npx playwright install --with-deps chromium
npm run test:e2e -- --workers="$workers" --fully-parallel
test -x ../build/Release/dc24h
npm run test:integration -- ../build/Release/dc24h
