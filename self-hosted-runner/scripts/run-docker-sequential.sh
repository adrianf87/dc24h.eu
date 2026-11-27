#!/usr/bin/env bash
# Run five independent application-test containers in a strictly serial order.
# This script runs on the approved self-hosted runner, never inside a container.
set -Eeuo pipefail

if (( $# != 0 )); then
  echo 'Usage: bash self-hosted-runner/scripts/run-docker-sequential.sh' >&2
  exit 2
fi

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
test -f "$repo_root/CMakeLists.txt"
test -f "$repo_root/self-hosted-runner/scripts/test-app-in-container.sh"

if [[ "${RUNNER_ENVIRONMENT:-}" != 'self-hosted' || \
      "$(hostname -s)" != 'server-747585786' || \
      "$(id -un)" != 'github-runner' ]]; then
  echo 'Only github-runner on the approved server-747585786 self-hosted runner may run this job.' >&2
  exit 1
fi

for program in docker flock python3; do
  command -v "$program" >/dev/null || { echo "Missing required command: $program" >&2; exit 1; }
done
docker info >/dev/null

# The same advisory lock is shared by every invocation on this VPS. It prevents
# simultaneous application-test containers even across separate workflow runs.
exec 9>/tmp/dc24h-app-docker-sequential.lock
flock -x 9

# CPU affinity limits logical CPU IDs, not the time quota (--cpus/--cpu-quota).
# Use 16 IDs available to this account instead of assuming IDs always start at 0.
cpuset="$(python3 - <<'PY'
import os
available = sorted(os.sched_getaffinity(0))
if len(available) < 16:
    raise SystemExit(f'Need 16 available logical CPUs; found {len(available)}')
print(','.join(str(cpu) for cpu in available[:16]))
PY
)"

distributions=(debian-12 debian-13 ubuntu-22.04 ubuntu-24.04 ubuntu-26.04)
images=(debian:12 debian:13 ubuntu:22.04 ubuntu:24.04 ubuntu:26.04)
result_dir="${RUNNER_TEMP:-/tmp}/dc24h-docker-results"
mkdir -p "$result_dir"

active_container=''
cleanup() {
  if [[ -n "$active_container" ]]; then
    docker rm -f "$active_container" >/dev/null 2>&1 || true
    active_container=''
  fi
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

overall_result=0
results=()
echo "Sequential Docker testing: 16 logical CPU IDs ($cpuset), 32 GiB RAM per container, no CPU time quota."

for index in "${!distributions[@]}"; do
  distro="${distributions[$index]}"
  image="${images[$index]}"
  echo "===== $((index + 1))/5: $distro ($image) ====="
  mkdir -p "$result_dir/$distro"

  if ! docker pull "$image"; then
    printf 'FAIL: image pull failed\n' > "$result_dir/$distro/result.txt"
    results+=("$distro: FAIL (image pull)")
    overall_result=1
    continue
  fi

  active_container="dc24h-ci-${GITHUB_RUN_ID:-manual}-${GITHUB_RUN_ATTEMPT:-1}-${distro//./-}-$$"
  if ! docker run -d \
      --name "$active_container" \
      --cpuset-cpus "$cpuset" \
      --memory 32g --memory-swap 32g \
      --pids-limit 4096 --security-opt no-new-privileges \
      --network bridge \
      --env CI=true --env "DC24H_TEST_DISTRO=$distro" \
      --env 'RUNNER_TEMP=/tmp/dc24h-ci-results' \
      "$image" sleep infinity >/dev/null; then
    printf 'FAIL: container start failed\n' > "$result_dir/$distro/result.txt"
    results+=("$distro: FAIL (container start)")
    overall_result=1
    cleanup
    continue
  fi

  # Fail closed if Docker silently ignored the requested CPU/memory settings.
  applied_cpuset="$(docker inspect --format '{{.HostConfig.CpusetCpus}}' "$active_container")"
  applied_memory="$(docker inspect --format '{{.HostConfig.Memory}}' "$active_container")"
  applied_quota="$(docker inspect --format '{{.HostConfig.CpuQuota}}' "$active_container")"
  applied_nanocpus="$(docker inspect --format '{{.HostConfig.NanoCpus}}' "$active_container")"
  if [[ "$applied_cpuset" != "$cpuset" || "$applied_memory" != '34359738368' || \
        "$applied_quota" != '0' || "$applied_nanocpus" != '0' ]]; then
    printf 'FAIL: unexpected Docker resource settings (cpuset=%s memory=%s cpu_quota=%s nano_cpus=%s)\n' \
      "$applied_cpuset" "$applied_memory" "$applied_quota" "$applied_nanocpus" > "$result_dir/$distro/result.txt"
    results+=("$distro: FAIL (resources)")
    overall_result=1
    cleanup
    continue
  fi

  # Copy source into the disposable container; do not bind-mount the live VPS,
  # Docker socket, MariaDB state, service directories or any host secret paths.
  if ! docker exec "$active_container" mkdir -p /workspace /tmp/dc24h-ci-results || \
     ! docker cp "$repo_root/." "$active_container:/workspace/"; then
    printf 'FAIL: workspace transfer failed\n' > "$result_dir/$distro/result.txt"
    results+=("$distro: FAIL (workspace transfer)")
    overall_result=1
    cleanup
    continue
  fi

  if docker exec --workdir /workspace "$active_container" \
      bash /workspace/self-hosted-runner/scripts/test-app-in-container.sh; then
    results+=("$distro: PASS")
  else
    results+=("$distro: FAIL")
    overall_result=1
  fi

  docker cp "$active_container:/tmp/dc24h-ci-results/." "$result_dir/$distro/" || {
    echo "WARN: report copy failed for $distro" >&2
    overall_result=1
  }
  cleanup
  echo "Stopped and removed $distro container before advancing."
done

printf '%s\n' "${results[@]}" | tee "$result_dir/summary.txt"
if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
  {
    echo '### Sequential application Docker tests'
    echo 'Order: Debian 12, Debian 13, Ubuntu 22.04, Ubuntu 24.04, Ubuntu 26.04.'
    echo 'Each container: 16 logical CPU IDs via cpuset, 32 GiB memory, no CPU-time quota.'
    printf -- '- %s\n' "${results[@]}"
  } >> "$GITHUB_STEP_SUMMARY"
fi
exit "$overall_result"
