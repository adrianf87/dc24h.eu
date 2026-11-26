#!/usr/bin/env bash
set -euo pipefail
umask 022
source_dir=$1
build=$2
out=$3
cd "$source_dir"

isolated() {
    unshare --net --mount --pid --fork --kill-child --mount-proc bash -ec '
        mount -t tmpfs -o mode=1777,size=4G tmpfs /tmp
        ip link set lo up
        if [[ $1 == ipv4 ]]; then
            sysctl -qw net.ipv6.conf.lo.disable_ipv6=1
        fi
        shift
        ip -brief address show dev lo
        exec "$@"
    ' bash "$@"
}

run_ctest() {
    local family=$1
    local name=$2
    shift 2
    isolated "$family" ctest --test-dir "$build" --verbose --parallel 16         --output-junit "$out/ctest-$name.raw.xml" "$@" >"$out/ctest-$name.raw.log" 2>&1
}

# Run the real transient service directly against the host manager.
DC24H_REQUIRE_SYSTEMD_HOST=1 ctest --test-dir "$build" --verbose     --output-junit "$out/ctest-systemd.raw.xml" -R '^systemd\.sandbox$'     >"$out/ctest-systemd.raw.log" 2>&1
python3 "$out/systemd_mount_probe.py" >"$out/systemd-probe.log" 2>&1
DC24H_REQUIRE_IPV6_LOOPBACK=1 run_ctest dual full -E '^systemd\.sandbox$'
DC24H_REQUIRE_IPV6_LOOPBACK=1 run_ctest dual ipv6-strict     -R '^(network.process|adc.ctm.process|adc.rcm.process|stage1.ipv6_loopback_policy)$'
run_ctest ipv4 ipv4 -R '^(adc.ctm.process|adc.rcm.process|stage1.ipv6_loopback_policy)$'
expected=0
DC24H_REQUIRE_IPV6_LOOPBACK=1 run_ctest ipv4 ipv4-strict-negative     -R '^adc\.(ctm|rcm)\.process$' || expected=$?
test "$expected" -eq 8
