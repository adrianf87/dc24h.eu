#!/usr/bin/env bash
set -Eeuo pipefail

if [ "$#" -ne 3 ]; then
  echo "Usage: $0 CONTAINER OS_ID VERSION_ID" >&2
  exit 2
fi

container="$1"
expected_os="$2"
expected_version="$3"
checkout_dir="${GITHUB_WORKSPACE:?GITHUB_WORKSPACE is required}"
test_dir=""
container_started=0

case "$container:$expected_os:$expected_version" in
  ubuntu2204-github-runner:ubuntu:22.04|\
  ubuntu2404-github-runner:ubuntu:24.04|\
  ubuntu2604-github-runner:ubuntu:26.04|\
  debian12-github-runner:debian:12|\
  debian13-github-runner:debian:13) ;;
  *) echo "Unrecognized container or OS: $container" >&2; exit 2 ;;
esac

cleanup() {
  result=$?
  trap - EXIT

  if [ "$container_started" -eq 1 ]; then
    case "$test_dir" in
      /home/github-runner/dc24h-ci.*)
        if [ "$(docker inspect -f '{{.State.Running}}' "$container")" = true ]; then
          docker exec --user github-runner "$container" rm -rf -- "$test_dir" \
            || echo "Warning: could not remove $test_dir" >&2
        fi
        ;;
    esac

    echo "Stopping $container after tests"
    if [ "$(docker inspect -f '{{.State.Running}}' "$container")" = true ]; then
      if ! docker stop --timeout 60 "$container"; then
        echo "Failed to stop $container" >&2
        result=1
      fi
    fi
    if [ "$(docker inspect -f '{{.State.Running}}' "$container")" != false ]; then
      echo "::error::Container $container is still running after tests" >&2
      result=1
    fi
  fi

  exit "$result"
}
trap cleanup EXIT
trap 'exit 130' INT TERM

# Never start this system while any of the other five test containers is running.
for candidate in \
  ubuntu2204-github-runner ubuntu2404-github-runner \
  ubuntu2604-github-runner \
  debian12-github-runner debian13-github-runner; do
  if [ "$(docker inspect -f '{{.State.Running}}' "$candidate")" = true ]; then
    echo "Container $candidate is already running; refusing overlapping tests" >&2
    exit 1
  fi
done

echo "Applying 16 CPU threads and 32 GiB RAM to $container"
docker update --cpuset-cpus=0-15 --cpus=16 --memory=32g --memory-swap=32g "$container"
echo "Starting only $container"
docker start "$container"
container_started=1

docker exec \
  --env EXPECTED_OS="$expected_os" \
  --env EXPECTED_VERSION="$expected_version" \
  "$container" bash -c \
  '. /etc/os-release; test "$ID" = "$EXPECTED_OS" && test "$VERSION_ID" = "$EXPECTED_VERSION"'

test "$(docker exec "$container" nproc)" -eq 16
test "$(docker exec "$container" cat /sys/fs/cgroup/memory.max)" -eq 34359738368
docker exec --user github-runner "$container" sudo -n true

test_dir="$(docker exec --user github-runner "$container" \
  mktemp -d /home/github-runner/dc24h-ci.XXXXXXXX)"

echo "Copying checked-out source to $container:$test_dir"
tar -C "$checkout_dir" --exclude='./.git' -cf - . \
  | docker exec -i --user github-runner "$container" \
      tar -C "$test_dir" -xf -

docker exec --user github-runner "$container" sudo -n apt-get update
docker exec --user github-runner "$container" sudo -n env DEBIAN_FRONTEND=noninteractive \
  apt-get install -y --no-install-recommends \
  build-essential cmake python3 libutf8proc-dev libgcrypt20-dev \
  libssl-dev libmariadb-dev openssl ncdc mariadb-server apache2

docker exec --user github-runner "$container" sudo -n service mariadb start
docker exec --user github-runner "$container" sudo -n service apache2 start

failed=0
for build_type in Debug Release; do
  echo "=== $container: C++ $build_type ==="

  if ! docker exec --user github-runner --workdir "$test_dir" \
    --env CXX=g++ --env CI=true --env GITHUB_ACTIONS=true "$container" \
    cmake -S . -B "build/$build_type" \
    -DCMAKE_BUILD_TYPE="$build_type" \
    -DBUILD_TESTING=ON -DDC24H_REQUIRE_INTEGRATION_TESTS=ON; then
    failed=1
    continue
  fi

  if ! docker exec --user github-runner --workdir "$test_dir" \
    --env CI=true --env GITHUB_ACTIONS=true "$container" \
    cmake --build "build/$build_type" --parallel 16; then
    failed=1
    continue
  fi

  # Run up to 16 CTest test processes within this one active container.
  if ! docker exec --user github-runner --workdir "$test_dir" \
    --env CI=true --env GITHUB_ACTIONS=true "$container" \
    ctest --test-dir "build/$build_type" \
    --parallel 16 --output-on-failure; then
    failed=1
  fi
done

# Node.js 22 comes from actions/setup-node on the HOST runner. Copy the
# standalone runtime into this container; no GitHub Runner runs inside it.
node_binary="$(readlink -f "$(command -v node)")"
node_root="$(dirname "$(dirname "$node_binary")")"
if [ ! -x "$node_root/bin/node" ] || [ ! -f "$node_root/lib/node_modules/npm/bin/npm-cli.js" ]; then
  echo "::error::Host Node.js 22 runtime, including npm, was not found" >&2
  exit 1
fi
docker exec --user github-runner "$container" mkdir -p "$test_dir/.node"
tar -C "$node_root" -cf - . \
  | docker exec -i --user github-runner "$container" \
      tar -C "$test_dir/.node" -xf -

webadmin_path="$test_dir/.node/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
docker exec --user github-runner --env PATH="$webadmin_path" "$container" node --version

run_webadmin() {
  local label="$1"
  shift
  echo "=== $container: WebAdmin $label ==="
  if ! docker exec --user github-runner --workdir "$test_dir/webadmin" \
    --env PATH="$webadmin_path" --env CI=true --env GITHUB_ACTIONS=true \
    "$container" "$@"; then
    echo "::error::$container WebAdmin $label failed" >&2
    failed=1
    return 1
  fi
}

if run_webadmin "npm ci" npm ci --no-audit --no-fund; then
  run_webadmin "TypeScript and production build" npm run build || true
  run_webadmin "unit tests (16 workers)" \
    bash -c 'exec node --import tsx --test --test-concurrency=16 tests/*.test.ts' || true

  if run_webadmin "Chromium installation" npx playwright install --with-deps chromium; then
    run_webadmin "Playwright E2E (16 workers)" \
      npm run test:e2e -- --workers=16 --fully-parallel || true

    if docker exec "$container" test -x "$test_dir/build/Release/dc24h"; then
      run_webadmin "live C++ API integration" \
        npm run test:integration -- ../build/Release/dc24h || true
    else
      echo "::error::$container WebAdmin live integration needs a Release dc24h binary" >&2
      failed=1
    fi
  fi
fi

exit "$failed"
