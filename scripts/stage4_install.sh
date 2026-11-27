#!/bin/sh
set -eu

if [ "$(id -u)" -ne 0 ] || [ "$#" -ne 2 ]; then
    echo "Usage: stage4_install.sh NODE24_DIRECTORY CMAKE_BUILD_DIRECTORY" >&2
    exit 2
fi
node_dir=$(realpath "$1")
build=$(realpath "$2")
source=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
target=/opt/dc24h
node="$node_dir/bin/node"
case "$("$node" --version)" in
    v24.*) ;;
    *) echo "Node 24 LTS required" >&2; exit 2 ;;
esac
for unit in dc24h-twilight-stage4-webadmin dc24h-twilight-stage4-bridge dc24h-twilight-stage4-auth; do
    if systemctl is-active --quiet "$unit"; then
        echo "Stop the Stage IV units before installing a replacement" >&2
        exit 2
    fi
done
export PATH="$node_dir/bin:/usr/bin:/bin"
(
    cd "$source/webadmin"
    npm ci --no-audit --no-fund
    npm run typecheck
    npm run build:public:ci
    npm run build:stage4-init
)
install -d -m 0755 "$target/bin" "$target/node24/bin" "$target/scripts" \
    "$target/database/stage4" "$target/webadmin/dist" /usr/local/share/dc24h/stage4 /etc/sysusers.d
install -m 0755 "$node" "$target/node24/bin/node"
for program in dc24h-twilight dc24h-twilight-owner dc24h-twilight-account-admin; do
    install -m 0755 "$build/$program" "$target/bin/$program"
done
install -m 0755 "$source/scripts/dc24h-stage4-init" /usr/local/sbin/dc24h-stage4-init
install -m 0644 "$source/scripts/stage4_webadmin_bridge.py" "$target/scripts/"
install -m 0644 "$source"/database/stage4/*.sql "$target/database/stage4/"
install -m 0644 "$source"/webadmin/dist/public-main.mjs "$source"/webadmin/dist/stage4-init.mjs "$target/webadmin/dist/"
install -m 0644 "$source"/webadmin/package.json "$source"/webadmin/package-lock.json "$target/webadmin/"
(
    cd "$target/webadmin"
    npm ci --omit=dev --no-audit --no-fund
)
install -m 0644 "$source"/webadmin/public-stage4/index.html "$source"/webadmin/public-stage4/panel.css \
    "$source"/webadmin/public-stage4/panel.js /usr/local/share/dc24h/stage4/
install -m 0644 "$source/config/dc24h-stage4-webadmin.sysusers.example" /etc/sysusers.d/dc24h-stage4-webadmin.conf
systemd-sysusers /etc/sysusers.d/dc24h-stage4-webadmin.conf
for unit in dc24h-twilight-stage4-webadmin dc24h-twilight-stage4-bridge dc24h-twilight-stage4-auth; do
    install -m 0644 "$source/config/$unit.service.example" "/etc/systemd/system/$unit.service"
done
# Check the Stage IV units themselves; unrelated host units can emit warnings
# (for example obsolete CPUAccounting= on newer systemd releases).
# Ubuntu 22.04 ships systemd 249 and does not have --recursive-errors.
printf '%s\n' 'Stage IV: verifying installed systemd units'
if systemd-analyze --help | grep -Fq -- '--recursive-errors='; then
    systemd-analyze --recursive-errors=no verify /etc/systemd/system/dc24h-twilight-stage4-*.service
else
    systemd-analyze verify /etc/systemd/system/dc24h-twilight-stage4-*.service
fi
printf '%s\n' 'Stage IV: reloading systemd manager'
systemctl daemon-reload
install -m 0644 "$source/config/apache2-webadmin-public.conf.example" \
    /etc/apache2/sites-available/dc24h-stage4-webadmin.conf

# Debian's optional msmtp AppArmor profile needs this single service-owned path.
profile=/etc/apparmor.d/usr.bin.msmtp
local_policy=/etc/apparmor.d/local/usr.bin.msmtp
rule="owner /var/lib/dc24h-stage4/.msmtprc r,"
printf '%s\n' 'Stage IV: reviewing optional msmtp AppArmor profile'
# The disposable GitHub-hosted matrix validates the packaged installation,
# not the GitHub image's host-wide AppArmor policy. Leave that host policy
# untouched; this exception requires BOTH runner identity markers explicitly
# passed through sudo by the workflow. Debian VPS and self-hosted deployments
# are never exempt from installing/reloading an active msmtp policy.
if [ "${GITHUB_ACTIONS:-}" = "true" ] &&
   [ "${RUNNER_ENVIRONMENT:-}" = "github-hosted" ]; then
    printf '%s\n' 'Stage IV: hosted CI: no host AppArmor policy changes'
elif [ -f "$profile" ] && command -v apparmor_parser >/dev/null 2>&1; then
    install -d -m 0755 /etc/apparmor.d/local
    if ! grep -Fqx "$rule" "$local_policy" 2>/dev/null; then
        printf '\n%s\n' "$rule" >> "$local_policy"
    fi
    if [ -d /sys/kernel/security/apparmor ] &&
       [ ! -e /etc/apparmor.d/disable/usr.bin.msmtp ] &&
       [ ! -L /etc/apparmor.d/disable/usr.bin.msmtp ]; then
        if ! apparmor_parser -r "$profile"; then
            printf '%s\n' 'Stage IV: failed to reload active msmtp AppArmor profile' >&2
            exit 1
        fi
    fi
    printf '%s\n' 'Stage IV: msmtp AppArmor policy installed'
fi
