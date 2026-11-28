#!/bin/bash
# Install and restore disposable vhosts on the GitHub-hosted Ubuntu matrix.
set -euo pipefail
test "$(id -u)" = 0
test "${GITHUB_ACTIONS:-}" = true && test "${RUNNER_ENVIRONMENT:-}" = github-hosted || {
    echo 'Stage IV installer fixture requires a disposable GitHub-hosted runner' >&2
    exit 2
}
test "$#" = 2
node_dir=$(realpath "$1")
build=$(realpath "$2")
repo=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
site=/etc/apache2/sites-available/dc24h-stage4-webadmin.conf
enabled=/etc/apache2/sites-enabled/dc24h-stage4-webadmin.conf
test ! -e "$site" && test ! -L "$site" && test ! -e "$enabled" && test ! -L "$enabled"
fixture=$(mktemp -d)
trap 'rm -rf -- "$fixture"' EXIT

install_stage4() {
    sh "$repo/scripts/stage4_install.sh" "$node_dir" "$build" "$1"
}

openssl req -x509 -newkey rsa:2048 -nodes -days 1 -subj /CN=stage4-install.test \
    -out "$fixture/fixture.pem" -keyout "$fixture/fixture.key" >"$fixture/openssl.log" 2>&1
chmod 0600 "$fixture/fixture.key"

verify_apache() {
    sed -E -e "s@^([[:space:]]*SSLCertificateFile) .*@\1 \"$fixture/fixture.pem\"@" \
        -e "s@^([[:space:]]*SSLCertificateKeyFile) .*@\1 \"$fixture/fixture.key\"@" \
        "$site" > "$fixture/site.conf"
    {
        printf 'ServerRoot "%s"\nDefaultRuntimeDir "%s"\nErrorLog "%s/apache-error.log"\nServerName localhost\n' \
            "$fixture" "$fixture" "$fixture"
        for module in mpm_event authz_core ssl rewrite headers proxy proxy_http dir; do
            printf 'LoadModule %s_module /usr/lib/apache2/modules/mod_%s.so\n' "$module" "$module"
        done
        printf 'Include "%s/site.conf"\n' "$fixture"
    } > "$fixture/apache.conf"
    /usr/sbin/apache2 -t -f "$fixture/apache.conf"
}

# Start with a reviewed Ubuntu site of its own, including mode and custom TLS paths.
sed -e 's/:7278/:7178/g' -e 's/^Listen 7278$/Listen 7178/' \
    -e 's|/etc/letsencrypt/live/server-747585786.dc24h.eu/|/etc/dc24h/tls/current/|' \
    "$repo/config/apache2-webadmin-public.conf.example" > "$fixture/ubuntu.conf"
printf '\n# Existing operator configuration\n' >> "$fixture/ubuntu.conf"
install -m 0640 "$fixture/ubuntu.conf" "$site"
before=$(stat -c '%i %a %u %g %y' "$site")
install_stage4 7178
install_stage4 7178
cmp "$site" "$fixture/ubuntu.conf"
test "$(stat -c '%i %a %u %g %y' "$site")" = "$before"
verify_apache

if install_stage4 7278 >"$fixture/mismatch.log" 2>&1; then
    echo 'Installer accepted the Debian port with the existing Ubuntu vhost' >&2
    exit 1
fi
grep -Fq 'Stage IV Apache site does not match the selected HTTPS Origin' "$fixture/mismatch.log"
cmp "$site" "$fixture/ubuntu.conf"
test "$(stat -c '%i %a %u %g %y' "$site")" = "$before"
echo 'PASS Ubuntu 7178 install, reinstall and mismatch refusal preserved bytes and metadata'

mv -- "$site" "$fixture/ubuntu-original.conf"
mkdir -p "$fixture/failed-node/bin"
ln -s "$node_dir/bin/node" "$fixture/failed-node/bin/node"
printf '#!/bin/sh\nexit 27\n' > "$fixture/failed-node/bin/npm"
chmod 0755 "$fixture/failed-node/bin/npm"
result=0
sh "$repo/scripts/stage4_install.sh" "$fixture/failed-node" "$build" 7278 || result=$?
test "$result" = 27
test ! -e "$site" && test ! -L "$site"
echo 'PASS failed installation left no new Apache configuration'

install_stage4 7278
cmp "$site" "$repo/config/apache2-webadmin-public.conf.example"
verify_apache
echo 'PASS the explicit historical Debian 7278 site is valid Apache configuration'

# Restore only the original fixture site. The separate database fixture checks
# recovery of the matching private configuration, accounts and sessions.
mv -T -- "$fixture/ubuntu-original.conf" "$site"
cmp "$site" "$fixture/ubuntu.conf"
test "$(stat -c '%a %u %g' "$site")" = '640 0 0'
test ! -e "$enabled" && test ! -L "$enabled"
verify_apache
echo 'PASS rollback restored the Ubuntu 7178 vhost and left Apache activation unchanged'
