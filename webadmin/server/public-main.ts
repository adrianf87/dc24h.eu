// Explicitly gated startup adapter. No public TCP listener is ever opened by
// Node: a separately approved Apache 7278 vhost would proxy fixed API paths.
import { createServer } from "node:http";
import { credentials } from "./local-api.ts";
import { publicBackend } from "./public-backend.ts";
import { publicGateway } from "./public-gateway.ts";
import { PUBLIC_WEBADMIN_ORIGIN, PUBLIC_WEBADMIN_RP_ID,
  requirePublicWebAuthnIdentity } from "./public-two-factor.ts";
import { openPasskeyStore } from "./remote-passkeys.ts";
import { passkeyCeremonies } from "./remote-webauthn.ts";
import { openRemoteRateLimitStore } from "./remote-rate-limit-store.ts";
import { openRemoteAuditStore } from "./remote-audit-store.ts";

if (process.env.DC24H_STAGE4_STARTUP_ACK !== "OWNER_WEBADMIN_PUBLIC_ACTIVATION_GO")
  throw Error("Stage IV startup refused: separate OWNER activation acknowledgement required");
requirePublicWebAuthnIdentity(PUBLIC_WEBADMIN_ORIGIN, PUBLIC_WEBADMIN_RP_ID);
if (process.env.DC24H_STAGE4_ORIGIN !== PUBLIC_WEBADMIN_ORIGIN ||
    process.env.DC24H_STAGE4_RP_ID !== PUBLIC_WEBADMIN_RP_ID)
  throw Error("Stage IV fixed origin / RP ID mismatch");
const config = credentials(process.env);
if (!config) throw Error("Private C++ API credentials required");
const passkeyPath = process.env.DC24H_STAGE4_PASSKEY_FILE;
const auditPath = process.env.DC24H_STAGE4_AUDIT_FILE;
const limiterPath = process.env.DC24H_STAGE4_RATE_LIMIT_FILE;
if (!passkeyPath || !auditPath || !limiterPath) throw Error("Pre-provisioned private catalogs required");

const store = openPasskeyStore(passkeyPath, PUBLIC_WEBADMIN_RP_ID);
const audit = openRemoteAuditStore(auditPath);
const limiter = openRemoteRateLimitStore(limiterPath, config.token);
const app = createServer(publicGateway({
  backend: publicBackend(config),
  passkeys: { store, ceremonies: passkeyCeremonies(store, PUBLIC_WEBADMIN_ORIGIN, PUBLIC_WEBADMIN_RP_ID) },
  limiter,
  auditHealthy: () => audit.health().healthy && audit.verify(),
  audit: event => audit.append(event, Date.now()),
}));
app.requestTimeout = 5000;
app.headersTimeout = 6000;
app.maxRequestsPerSocket = 50;
app.listen(4278, "127.0.0.1", () => process.stdout.write("[stage4] loopback gateway ready\n"));
process.once("SIGTERM", () => { app.close(); audit.close(); });
