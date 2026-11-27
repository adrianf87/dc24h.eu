#!/usr/bin/env bash
# Read-only Debian package/runner inventory; NEVER reads tokens or service secrets.
set -euo pipefail
umask 077
export LC_ALL=C

for required in dpkg-query awk sort sha256sum date; do
    command -v "$required" >/dev/null 2>&1 || {
        printf 'Missing required tool: %s\n' "$required" >&2
        exit 1
    }
done

runner_docs_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
local_date="$(date +%d%m%Y)"
utc_stamp="$(date -u +%Y%m%dT%H%M%SZ)"
snapshot_dir="$runner_docs_dir/$local_date/inventory/$utc_stamp"

if [ -e "$snapshot_dir" ]; then
    printf 'Inventory already exists: %s\n' "$snapshot_dir" >&2
    exit 1
fi
mkdir -p -m 0700 -- "$snapshot_dir"
warnings="$snapshot_dir/inventory-warnings.txt"
: > "$warnings"

{
    printf 'Collected UTC: %s\n' "$(date -u +%FT%TZ)"
    printf 'Hostname: %s\n' "$(hostname -s 2>/dev/null || printf unknown)"
    if [ -r /etc/os-release ]; then
        awk -F= '$1 == "PRETTY_NAME" || $1 == "VERSION_ID" || $1 == "VERSION_CODENAME" {print}' /etc/os-release
    fi
    printf 'Kernel: %s\n' "$(uname -r)"
    printf 'Architecture: %s\n' "$(dpkg --print-architecture)"
    printf 'Logical CPUs: %s\n' "$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf unknown)"
    if [ -r /proc/meminfo ]; then
        awk '/^MemTotal:/ {printf "MemTotal: %s %s\n", $2, $3}' /proc/meminfo
    fi
    if command -v df >/dev/null 2>&1; then
        df -Pk / | awk 'NR == 2 {printf "Root filesystem (1K blocks): total=%s used=%s available=%s\n", $2, $3, $4}'
    fi
    if id github-runner >/dev/null 2>&1; then
        printf 'Runner account: github-runner (exists)\n'
    else
        printf 'Runner account: github-runner (NOT FOUND)\n'
    fi
} > "$snapshot_dir/system.txt"

# Authoritative local package database: names and versions only.
dpkg-query -W -f='${binary:Package}\t${Version}\t${db:Status-Status}\n' |
    awk -F '\t' '$3 == "installed" {print $1 "\t" $2}' |
    sort -u > "$snapshot_dir/installed-packages.tsv"

if command -v apt-mark >/dev/null 2>&1; then
    if ! apt-mark showmanual 2>> "$warnings" | sort -u > "$snapshot_dir/apt-manual.txt"; then
        printf 'Could not fully read manually installed package list\n' >> "$warnings"
    fi
else
    printf 'apt-mark unavailable\n' >> "$warnings"
    : > "$snapshot_dir/apt-manual.txt"
fi

# Read only structured dpkg events. APT command histories can contain secrets.
{
    for logfile in /var/log/dpkg.log /var/log/dpkg.log.*; do
        [ -f "$logfile" ] || continue
        if [ ! -r "$logfile" ]; then
            printf 'Unreadable dpkg log: %s\n' "$logfile" >> "$warnings"
            continue
        fi
        if [[ "$logfile" == *.gz ]]; then
            if ! gzip -cd -- "$logfile" 2>> "$warnings"; then
                printf 'Could not fully decompress dpkg log: %s\n' "$logfile" >> "$warnings"
            fi
        else
            if ! sed -n 'p' "$logfile" 2>> "$warnings"; then
                printf 'Could not fully read dpkg log: %s\n' "$logfile" >> "$warnings"
            fi
        fi
    done
} | awk '$3 ~ /^(install|upgrade|remove|purge)$/ {
    printf "%s\t%s\t%s\t%s\t%s\t%s\n", $1, $2, $3, $4, $5, $6
}' | sort -u > "$snapshot_dir/dpkg-package-events.tsv"

if [ ! -s "$snapshot_dir/dpkg-package-events.tsv" ]; then
    printf 'No retained package events; removal history incomplete\n' >> "$warnings"
fi

awk -F '\t' 'NR == FNR {installed[$1] = 1; next}
    ($3 == "remove" || $3 == "purge") && !($4 in installed) {print}
' "$snapshot_dir/installed-packages.tsv" "$snapshot_dir/dpkg-package-events.tsv" |
    sort -u > "$snapshot_dir/removed-not-installed.tsv"

: > "$snapshot_dir/tool-versions.tsv"
tool_version() {
    local tool="$1"
    shift
    if command -v "$tool" >/dev/null 2>&1; then
        local version
        version="$("$tool" "$@" 2>/dev/null | head -n 1 || true)"
        version="${version//$'\t'/ }"
        printf '%s\t%s\n' "$tool" "${version:-version not reported}" >> "$snapshot_dir/tool-versions.tsv"
    fi
}
tool_version git --version
tool_version bash --version
tool_version python3 --version
tool_version node --version
tool_version npm --version
tool_version cmake --version
tool_version g++ --version
tool_version docker --version
tool_version podman --version
tool_version msmtp --version
tool_version mariadb --version
tool_version apache2 -v
tool_version openssl version

(
    cd "$snapshot_dir"
    sha256sum system.txt installed-packages.tsv apt-manual.txt \
        dpkg-package-events.tsv removed-not-installed.tsv tool-versions.tsv \
        inventory-warnings.txt > SHA256SUMS
)

printf 'Inventory saved to %s\n' "$snapshot_dir"
printf 'Installed package rows: %s\n' "$(wc -l < "$snapshot_dir/installed-packages.tsv")"
printf 'Removed/purged events for currently absent packages: %s\n' "$(wc -l < "$snapshot_dir/removed-not-installed.tsv")"
printf '%s\n' 'Review inventory reports before committing. No service changes, tokens, passwords, or private configurations read.'
