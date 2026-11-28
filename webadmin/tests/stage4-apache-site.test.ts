import assert from "node:assert/strict";
import {
  chmodSync, existsSync, mkdtempSync, readFileSync, readdirSync, rmSync, statSync, symlinkSync, writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { test } from "node:test";
import type { TestContext } from "node:test";
import { fileURLToPath } from "node:url";
import { approvedStage4Origin, checkStage4ApacheSite, prepareStage4ApacheSite } from "../server/stage4-apache-site.ts";
import { PUBLIC_WEBADMIN_ORIGIN, UBUNTU_WEBADMIN_ORIGIN } from "../server/public-email-code.ts";

const template = fileURLToPath(new URL("../../config/apache2-webadmin-public.conf.example", import.meta.url));

function fixture(t: TestContext) {
  const directory = mkdtempSync(join(tmpdir(), "stage4-apache-test-"));
  t.after(() => rmSync(directory, { recursive: true }));
  return { directory, site: join(directory, "webadmin.conf") };
}

test("Stage IV creates a site only for the explicitly selected Ubuntu or Debian Origin", t => {
  const { site } = fixture(t);
  assert.equal(approvedStage4Origin("7178"), UBUNTU_WEBADMIN_ORIGIN);
  assert.equal(approvedStage4Origin("7278"), PUBLIC_WEBADMIN_ORIGIN);
  for (const port of ["", "07178", "443", "7278 ", "7178/"]) assert.equal(approvedStage4Origin(port), undefined);

  assert.equal(prepareStage4ApacheSite(UBUNTU_WEBADMIN_ORIGIN, template, false, site), "ready");
  assert.equal(existsSync(site), false);
  assert.equal(prepareStage4ApacheSite(UBUNTU_WEBADMIN_ORIGIN, template, true, site), "created");
  const ubuntu = readFileSync(site, "utf8");
  assert.match(ubuntu, /^Listen 7178$/m);
  assert.match(ubuntu, /^<VirtualHost \*:7178>$/m);
  assert.match(ubuntu, /RewriteCond %\{HTTP:Origin\} !\^https:\/\/[^\s]+:7178\$/);
  assert.equal(statSync(site).mode & 0o777, 0o644);
  assert.equal(checkStage4ApacheSite(UBUNTU_WEBADMIN_ORIGIN, site), true);

  const debianSite = site + ".debian";
  assert.equal(prepareStage4ApacheSite(PUBLIC_WEBADMIN_ORIGIN, template, true, debianSite), "created");
  assert.deepEqual(readFileSync(debianSite), readFileSync(template));
  assert.equal(checkStage4ApacheSite(PUBLIC_WEBADMIN_ORIGIN, debianSite), true);
});

test("a reinstalled or mismatched Stage IV site keeps the existing bytes, inode, mode and enabled link", t => {
  const { directory, site } = fixture(t);
  prepareStage4ApacheSite(UBUNTU_WEBADMIN_ORIGIN, template, true, site);
  const reviewed = readFileSync(site, "utf8").replace(/SSLCertificateFile "[^"]+"/,
    'SSLCertificateFile "/etc/dc24h/tls/current/fullchain.pem"') + "# Operator-managed TLS configuration\n";
  writeFileSync(site, reviewed);
  chmodSync(site, 0o640);
  const before = statSync(site);
  const enabled = join(directory, "enabled.conf");
  symlinkSync(site, enabled);
  for (let attempt = 0; attempt < 2; attempt++) {
    assert.equal(prepareStage4ApacheSite(UBUNTU_WEBADMIN_ORIGIN, template, false, site), "preserved");
    assert.equal(prepareStage4ApacheSite(UBUNTU_WEBADMIN_ORIGIN, template, true, site), "preserved");
  }
  assert.throws(() => prepareStage4ApacheSite(PUBLIC_WEBADMIN_ORIGIN, template, true, site), /does not match/);
  const after = statSync(site);
  assert.deepEqual([after.ino, after.mode, after.uid, after.gid, after.mtimeMs],
    [before.ino, before.mode, before.uid, before.gid, before.mtimeMs]);
  assert.equal(readFileSync(enabled, "utf8"), reviewed);
});

test("the site validator rejects mismatched listeners, vhosts, host and Origin rules or disabled TLS", t => {
  const { site } = fixture(t);
  const debian = readFileSync(template, "utf8");
  for (const altered of [
    debian.replace("Listen 7278", "Listen 7178"),
    debian.replace("<VirtualHost *:7278>", "<VirtualHost *:7178>"),
    debian.replace("ServerName server-747585786.dc24h.eu", "ServerName foreign.invalid"),
    debian.replace("!^server-747585786\\.dc24h\\.eu:7278$", "!^server-747585786\\.dc24h\\.eu:7178$"),
    debian.replace("!^https://", "!^http://"),
    debian.replace("SSLEngine On", "SSLEngine Off"),
    debian + "Listen 7178\n",
  ]) {
    assert.notEqual(altered, debian);
    writeFileSync(site, altered);
    assert.throws(() => checkStage4ApacheSite(PUBLIC_WEBADMIN_ORIGIN, site), /does not match/);
  }
});

test("failed site preparation leaves no partial file and refuses existing symlinks", t => {
  const { directory, site } = fixture(t);
  const broken = join(directory, "broken.conf");
  writeFileSync(broken, "Listen 80\n");
  assert.throws(() => prepareStage4ApacheSite(UBUNTU_WEBADMIN_ORIGIN, broken, true, site), /does not match/);
  assert.deepEqual(readdirSync(directory), ["broken.conf"]);
  symlinkSync(join(directory, "missing"), site);
  assert.throws(() => prepareStage4ApacheSite(UBUNTU_WEBADMIN_ORIGIN, template, true, site), /Unsafe Stage IV Apache site/);
  assert.equal(existsSync(join(directory, "missing")), false);
});
