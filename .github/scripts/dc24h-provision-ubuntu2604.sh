#!/usr/bin/env bash
set -Eeuo pipefail

container="ubuntu2604-github-runner"
image="ubuntu:26.04"
started_by_script=0

cleanup() {
  result=$?
  trap - EXIT
  if [ "$started_by_script" -eq 1 ] && \
     [ "$(docker inspect -f '{{.State.Running}}' "$container")" = true ]; then
    echo "Stopping $container after provisioning"
    docker stop --timeout 60 "$container" || result=1
  fi
  if docker container inspect "$container" >/dev/null 2>&1 && \
     [ "$(docker inspect -f '{{.State.Running}}' "$container")" != false ]; then
    echo "::error::$container is still running" >&2
    result=1
  fi
  exit "$result"
}
trap cleanup EXIT
trap 'exit 130' INT TERM

# This new environment must not interrupt other container-based CI jobs.
for sibling in \
  ubuntu2204-github-runner ubuntu2404-github-runner \
  debian12-github-runner debian13-github-runner; do
  if [ "$(docker inspect -f '{{.State.Running}}' "$sibling")" = true ]; then
    echo "::error::Another test container $sibling is already running" >&2
    exit 1
  fi
done

if docker container inspect "$container" >/dev/null 2>&1; then
  current_image="$(docker inspect -f '{{.Config.Image}}' "$container")"
  if [ "$current_image" != "$image" ]; then
    echo "::error::Container $container already exists with image $current_image" >&2
    exit 1
  fi
  if [ "$(docker inspect -f '{{.State.Running}}' "$container")" = true ]; then
    echo "::error::$container is already running; refusing to interrupt it" >&2
    exit 1
  fi
  echo "Using existing $container"
else
  docker pull "$image"
  docker create \
    --name "$container" \
    --hostname "$container" \
    --network bridge \
    --restart=no \
    --cpuset-cpus=0-15 \
    --cpus=16 \
    --memory=32g \
    --memory-swap=32g \
    "$image" sleep infinity
fi

docker update --cpuset-cpus=0-15 --cpus=16 \
  --memory=32g --memory-swap=32g --restart=no "$container"

started_by_script=1
docker start "$container"

docker exec "$container" bash -euc '
  export DEBIAN_FRONTEND=noninteractive
  apt-get update
  apt-get install -y --no-install-recommends \
    sudo ca-certificates curl procps tar
  if ! id github-runner >/dev/null 2>&1; then
    useradd --create-home --shell /bin/bash github-runner
  fi
  printf "%s\\n" "github-runner ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/90-github-runner
  chmod 0440 /etc/sudoers.d/90-github-runner
  visudo -cf /etc/sudoers.d/90-github-runner
'

docker exec "$container" bash -ec '
  . /etc/os-release
  test "$ID" = ubuntu
  test "$VERSION_ID" = 26.04
  printf "Installed: %s\\n" "$PRETTY_NAME"
'
test "$(docker exec "$container" nproc)" -eq 16
test "$(docker exec "$container" cat /sys/fs/cgroup/memory.max)" = 34359738368
test "$(docker inspect -f '{{.HostConfig.CpusetCpus}}' "$container")" = 0-15
test "$(docker inspect -f '{{.HostConfig.NanoCpus}}' "$container")" = 16000000000
test "$(docker inspect -f '{{.HostConfig.Memory}}' "$container")" = 34359738368
test "$(docker inspect -f '{{.HostConfig.MemorySwap}}' "$container")" = 34359738368
test "$(docker inspect -f '{{.HostConfig.RestartPolicy.Name}}' "$container")" = no
test "$(docker exec --user github-runner "$container" sudo -n whoami)" = root
docker exec --user github-runner "$container" \
  curl -fsSI --max-time 20 https://github.com >/dev/null

echo "Ubuntu 26.04 installed; user github-runner has passwordless sudo."
echo "Docker bridge internet OK; 16 CPU threads and 32 GiB RAM configured."
echo "Container will be stopped after provisioning."
