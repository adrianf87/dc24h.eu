import { createServer } from "node:http";
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { setTimeout as sleep } from "node:timers/promises";
import { publicBridgeCredentials } from "./public-bridge-credentials.ts";
import { publicBackend } from "./public-backend.ts";
import { databasePublicGateway } from "./public-gateway.ts";
import { isWebAdminOrigin } from "./stage4-apache-site.ts";
import { PUBLIC_WEBADMIN_ORIGIN, UBUNTU_WEBADMIN_ORIGIN } from "./public-email-code.ts";
import { createMsmtpSender, dedicatedMsmtpConfigReady } from "./public-msmtp.ts";
import { installerDatabase, openStage4Store, stage4Identifier } from "./stage4-store.ts";
import type { RuntimeConfiguration } from "./stage4-store.ts";

async function main() {
  const origin = process.env.DC24H_STAGE4_ORIGIN;
  if (process.env.DC24H_STAGE4_STARTUP_ACK !== "OWNER_WEBADMIN_PUBLIC_ACTIVATION_GO" ||
      !isWebAdminOrigin(origin) || !process.geteuid?.())
    throw Error("Stage IV activation and dedicated service identity are required");
  const credentials = process.env.CREDENTIALS_DIRECTORY;
  if (!credentials || !dedicatedMsmtpConfigReady()) throw Error("Private Stage IV runtime settings unavailable");
  const settings = JSON.parse(readFileSync(join(credentials, "stage4-runtime.json"), "utf8")) as RuntimeConfiguration;
  const deploymentMatches = settings.origin === undefined ?
    [PUBLIC_WEBADMIN_ORIGIN, UBUNTU_WEBADMIN_ORIGIN].includes(origin) && settings.database === "dc24h_stage4_production" :
    settings.origin === origin && installerDatabase(settings.database);
  if (settings.environment !== "production" || !deploymentMatches ||
      !/^\/run\/(?:[a-z0-9_-]+\/)+[a-z0-9_.-]+\.sock$/.test(settings.socketPath) ||
      !stage4Identifier.test(settings.user) || !settings.user.endsWith("_production") ||
      typeof settings.password !== "string" || settings.password.length < 20 ||
      !/^[0-9a-f]{64}$/.test(settings.key)) throw Error("Stage IV production database settings refused");
  const config = publicBridgeCredentials(process.env);
  const backend = publicBackend(config);
  const db = openStage4Store(settings);
  try {
    await db.check();
    for (let attempt = 0; !await backend.health(); attempt++) {
      if (attempt === 9) throw Error("Stage IV account bridge unavailable");
      await sleep(500);
    }
  } catch (error) {
    await db.close();
    throw error;
  }
  const app = createServer(databasePublicGateway({ backend, db, origin, sendCode: createMsmtpSender() }));
  const cleanup = setInterval(() => { void db.clean().catch(() => {}); }, 60_000);
  cleanup.unref();
  app.requestTimeout = 10_000;
  app.headersTimeout = 10_000;
  app.maxRequestsPerSocket = 50;
  app.once("error", () => {
    process.stderr.write("Stage IV gateway listener failed\n");
    process.exitCode = 1;
    clearInterval(cleanup);
    void db.close();
  });
  app.listen(4278, "127.0.0.1", () => process.stdout.write("[stage4] loopback gateway ready\n"));
  process.once("SIGTERM", () => {
    clearInterval(cleanup);
    app.close(() => { void db.close(); });
  });
}

main().catch(() => {
  process.stderr.write("Stage IV startup FAIL: check private configuration, database and account bridge\n");
  process.exitCode = 1;
});
