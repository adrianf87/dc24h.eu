# Apache2 HTTPS example — alpha-0.1.11

The shipped [HTTPS example](../../config/apache2-webadmin-https.conf.example) serves the static WebAdmin build through Apache2. It adds HTTP-to-HTTPS redirection, TLS 1.2/1.3, separate cipher lists, certificate/key paths and security headers. It is a configuration template; this release does not activate a site, obtain/renew a certificate, change DNS/firewall settings or reload a host service.

The C++ ADC/ADCS listeners and private Unix API remain independent. The example does not proxy API requests or provide browser login/roles. Access to the static panel is local by default (`Require local`), and both `/admin-api` and `/api` are denied. A production authenticated gateway and remote administrator access policy remain separate work. The Vite development adapter is not part of Apache or the production bundle.

## Files and prerequisites

- `config/apache2-webadmin-https.conf.example`: HTTP port 80 redirect and HTTPS port 443 virtual hosts, using `dc24h.eu` as the example hostname.
- `config/apache2-webadmin.conf.example`: existing standalone loopback HTTP preview on port 8080; this is a separate example.
- `webadmin/dist/`: prebuilt static assets; build with the supported Node/npm toolchain from [WEBADMIN.md](WEBADMIN.md).
- Apache2 >=2.4.42 with OpenSSL >=1.1.1, mod_ssl, mod_headers, mod_alias, mod_dir, mod_mime and authorization modules. Debian/Ubuntu standard Apache packages provide these modules.

Debian/Ubuntu `ports.conf` owns the Listen directives. The new site file intentionally contains no Listen 80/443 to avoid duplicate listeners. Enabling mod_ssl normally makes the distro's conditional HTTPS listener available; verify the actual local ports configuration. Do not copy the test harness's ephemeral Listen directives into a production site.

Default asset path: `/usr/local/share/dc24h/webadmin`. Default credential paths: `/etc/dc24h/webadmin-tls/fullchain.pem` and `/etc/dc24h/webadmin-tls/privkey.pem`. Customize all occurrences of the domain and paths. The full chain should contain the leaf certificate followed by its intermediates, and its names must cover the chosen hostname. Keep the private key outside the document root/Git, readable only by the appropriate Apache startup identity (normally root, mode 0600); workers do not need direct key-file access. Use credentials from your certificate provider or existing renewal setup. No certificate authority client, challenge handler or renewal job is included.

## Build and installation

```sh
npm --prefix webadmin ci
npm --prefix webadmin run build
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DDC24H_INSTALL_WEBADMIN=ON
cmake --build build-release --parallel 2
sudo cmake --install build-release --prefix /usr/local
```

CMake always installs the HTTPS example under `etc/dc24h`, alongside the INI example. `DC24H_INSTALL_WEBADMIN=ON` additionally installs already built frontend assets and the older loopback example. Default C++ builds still need neither Node nor Apache. Installation copies files only; no site/service is enabled automatically.

## Customize and enable on Debian/Ubuntu

Provision the actual certificate/key first, install the assets, then copy and edit the template. These are operator commands, not actions performed by this release:

```sh
sudo a2enmod ssl headers alias
sudo install -m 0644 /usr/local/etc/dc24h/apache2-webadmin-https.conf.example \
  /etc/apache2/sites-available/dc24h-webadmin-https.conf
sudoedit /etc/apache2/sites-available/dc24h-webadmin-https.conf
```

Keep `Require local` until an explicit administrator access policy is selected. If you later allow a trusted administrator network, edit the Directory policy deliberately; this does not add web sessions, roles or an API gateway. The static shell contains no token or live data by itself.

Verify certificate identity, validity and corresponding public keys. These commands print certificate metadata/public-key hashes only:

```sh
openssl x509 -in /etc/dc24h/webadmin-tls/fullchain.pem -noout -subject -issuer -dates
openssl x509 -in /etc/dc24h/webadmin-tls/fullchain.pem -pubkey -noout |
  openssl pkey -pubin -outform DER | openssl dgst -sha256
sudo openssl pkey -in /etc/dc24h/webadmin-tls/privkey.pem -pubout -outform DER |
  openssl dgst -sha256
```

The two public-key hashes must match. `apache2ctl configtest` checks syntax and missing files, but does not by itself guarantee certificate/key correspondence; the automated regression also verifies actual startup with a mismatched pair. After reviewing the certificate/key and customized configuration:

```sh
sudo a2ensite dc24h-webadmin-https
sudo apache2ctl configtest && sudo systemctl reload apache2
```

For a CA-trusted certificate covering `dc24h.eu`, a local check can use `curl --resolve dc24h.eu:443:127.0.0.1 https://dc24h.eu/`; this preserves certificate/name verification and does not require disabling TLS checks. Confirm the actual live certificate after renewal/reload. The template does not validate your external DNS, CA chain, expiry or other virtual-host policy automatically.

## Behavior and scope

The HTTP virtual host returns a permanent 301 redirect to a fixed `https://dc24h.eu/` destination, preserving paths/query strings. It does not construct the destination from the request Host header. The HTTPS virtual host selects TLS 1.2/1.3 and AEAD cipher suites, disables SSL compression and sets `SSLSessionTickets off`. Private API routes, hidden paths, common credential/configuration/log/SQL file suffixes, directory listing and symlink following are denied. Serve only the built asset directory, not the repository.

CSP permits same-origin bundled scripts/styles/fonts/connections and forbids frames/objects. Additional headers set no-sniff, no-referrer and DENY framing. HTTPS responses (including errors) carry host-only HSTS `max-age=300`; no includeSubDomains/preload policy is imposed. This short example lifetime can be changed by the operator after validating their HTTPS deployment. These headers do not implement API authentication or constitute a security audit.

Existing port-443/default/SNI virtual hosts and global Apache settings can affect real deployments; review the selected site rather than replacing unrelated configuration. The example does not configure HTTP/2, OCSP stapling, automatic certificate renewal, production audit logging or a reverse proxy.

## Automated verification

```sh
ctest --test-dir build -R '^apache2.https$' --output-on-failure
```

CMake adds the test when Apache2/mod_ssl are available and explicitly reports absence otherwise. CI installs Apache2. The Python test uses a separate temporary ServerRoot, local ephemeral ports, disposable certificate/key, fixture assets/logs/cache/PID and dropped-privilege workers. It never loads or changes host sites, trust stores or privileged ports. Only paths/ports are substituted into the shipped template; its TLS/access/header policy is exercised unchanged.

Seven cases verify trusted TLS 1.2/1.3 and static content, canonical redirects and query preservation, HTTPS-only HSTS/security headers, private/hidden/secret/index/symlink denial, wrong-host/untrusted-certificate rejection, legacy TLS rejection, missing credentials and actual startup rejection of a mismatched key. This is not an external scan or proof of every target distribution's behavior. Local/CI verification scope is recorded in [TESTING.md](TESTING.md).

Sources: [Apache mod_ssl](https://httpd.apache.org/docs/2.4/mod/mod_ssl.html), [Apache TLS how-to](https://httpd.apache.org/docs/2.4/ssl/ssl_howto.html), [canonical redirects](https://httpd.apache.org/docs/2.4/rewrite/remapping.html).
