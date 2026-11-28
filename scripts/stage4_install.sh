#!/bin/sh
set -eu

if [ "$(id -u)" -ne 0 ] || [ "$#" -ne 3 ]; then
    echo "Usage: stage4_install.sh NODE24_DIRECTORY CMAKE_BUILD_DIRECTORY PUBLIC_PORT (7178 or 7278)" >&2
    exit 2
fi
node_dir=$(realpath "$1")
build=$(realpath "$2")
public_port=$3
source=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
target=/opt/dc24h
node="$node_dir/bin/node"
case "$("$node" --version)" in
    v24.*) ;;
    *) echo "Node 24 LTS required" >&2; exit 2 ;;
esac
units="dc24h-stage4-webadmin dc24h-stage4-bridge dc24h-stage4-auth"
unit_directory=/etc/systemd/system

refuse() {
    echo "$1" >&2
    exit 2
}

apache_site() {
    "$node" --input-type=module - "$source" "$public_port" "$1" <<'JS'
import { pathToFileURL } from "node:url";
const [source, port, mode] = process.argv.slice(2);
const { approvedStage4Origin, prepareStage4ApacheSite } =
  await import(pathToFileURL(source + "/webadmin/server/stage4-apache-site.ts"));
try {
  const origin = approvedStage4Origin(port);
  if (!origin) throw Error("Select Stage IV public port 7178 (Ubuntu) or 7278 (historical Debian)");
  const state = prepareStage4ApacheSite(origin, source + "/config/apache2-webadmin-public.conf.example",
    mode === "publish");
  if (mode === "publish") console.log(`Stage IV: Apache site ${state} for ${origin}`);
} catch (error) {
  console.error("Stage IV: " + error.message);
  process.exitCode = 1;
}
JS
}

check_units() {
    for unit in $units; do
        old="dc24h-twilight-${unit#dc24h-}.service"
        active=$(systemctl show -p ActiveState --value "$old") ||
            refuse "Cannot read the state of $old"
        [ "$active" = inactive ] || refuse "Stop $old before installing replacement units"
        enabled=$(systemctl show -p UnitFileState --value "$old") ||
            refuse "Cannot read the enablement of $old"
        case "$enabled" in
            disabled|masked) ;;
            "")
                load=$(systemctl show -p LoadState --value "$old") ||
                    refuse "Cannot read the load state of $old"
                [ "$load" = not-found ] || refuse "Disable or retire $old before installation"
                ;;
            *) refuse "Disable or retire $old before installation" ;;
        esac

        name="$unit.service"
        destination="$unit_directory/$name"
        active=$(systemctl show -p ActiveState --value "$name") ||
            refuse "Cannot read the state of $name"
        [ "$active" = inactive ] || refuse "Stop $name before installing replacement units"
        fragment=$(systemctl show -p FragmentPath --value "$name") ||
            refuse "Cannot read the installed path of $name"
        dropins=$(systemctl show -p DropInPaths --value "$name") ||
            refuse "Cannot read drop-ins for $name"
        [ -z "$dropins" ] || refuse "Review the existing drop-ins for $name"
        [ -z "$fragment" ] || [ "$fragment" = "$destination" ] ||
            refuse "Review the existing unit $fragment"
        if [ -L "$destination" ] ||
           { [ -e "$destination" ] && ! cmp -s "$source/config/$name.example" "$destination"; }; then
            refuse "Refusing to replace existing unit $destination"
        fi
    done
}

apache_site check
check_units
export PATH="$node_dir/bin:/usr/bin:/bin"
(
    cd "$source/webadmin"
    npm ci --no-audit --no-fund
    npm run typecheck
    npm run build:public:ci
    npm run build:stage4-init
)
check_units
apache_site check
install -d -m 0755 "$target/bin" "$target/node24/bin" "$target/scripts" \
    "$target/database/stage4" "$target/webadmin/dist" /usr/local/share/dc24h/stage4 /etc/sysusers.d
install -m 0755 "$node" "$target/node24/bin/node"
for program in dc24h dc24h-owner dc24h-account-admin; do
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
    "$source"/webadmin/public-stage4/panel.js "$source"/webadmin/public-stage4/panel-react.js \
    /usr/local/share/dc24h/stage4/
install -m 0644 "$source/config/dc24h-stage4-webadmin.sysusers.example" /etc/sysusers.d/dc24h-stage4-webadmin.conf
systemd-sysusers /etc/sysusers.d/dc24h-stage4-webadmin.conf
for unit in $units; do
    destination="$unit_directory/$unit.service"
    if [ ! -e "$destination" ] && [ ! -L "$destination" ]; then
        # Publish only complete unit files, with no replacement if a unit
        # appears between preflight and install.
        staging=$(mktemp "$unit_directory/.dc24h-stage4.XXXXXX")
        trap 'rm -f -- "$staging"' EXIT
        install -m 0644 "$source/config/$unit.service.example" "$staging"
        ln -T -- "$staging" "$destination"
        rm -- "$staging"
        trap - EXIT
    fi
    [ ! -L "$destination" ] && cmp -s "$source/config/$unit.service.example" "$destination" ||
        refuse "Refusing to use unexpected unit $destination"
done
# Check the Stage IV units themselves; unrelated host units can emit warnings
# (for example obsolete CPUAccounting= on newer systemd releases).
# Ubuntu 22.04 ships systemd 249 and does not have --recursive-errors.
printf '%s\n' 'Stage IV: verifying installed systemd units'
if systemd-analyze --help | grep -Fq -- '--recursive-errors='; then
    systemd-analyze --recursive-errors=no verify "$unit_directory/dc24h-stage4-webadmin.service" "$unit_directory/dc24h-stage4-bridge.service" "$unit_directory/dc24h-stage4-auth.service"
else
    systemd-analyze verify "$unit_directory/dc24h-stage4-webadmin.service" "$unit_directory/dc24h-stage4-bridge.service" "$unit_directory/dc24h-stage4-auth.service"
fi
printf '%s\n' 'Stage IV: reloading systemd manager'
systemctl daemon-reload

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

# Leave an existing vhost untouched. Publish a fresh site only after the rest
# of the installation succeeds, without enabling or reloading Apache.
apache_site publish
