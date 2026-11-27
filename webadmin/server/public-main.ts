// Explicitly gated startup adapter. No public TCP listener is ever opened by
// Node: a separately approved Apache 7278 vhost would proxy fixed API paths.
import { createServer } from "node:http";
import { publicBridgeCredentials } from "./public-bridge-credentials.ts";
import { publicBackend } from "./public-backend.ts";
import { publicGateway } from "./public-gateway.ts";
import { PUBLIC_WEBADMIN_ORIGIN } from "./public-email-code.ts";
import { createMsmtpSender } from "./public-msmtp.ts";
import { openVerifiedEmailCatalog } from "./public-verified-email.ts";
import { openRemoteRateLimitStore } from "./remote-rate-limit-store.ts";
import { openRemoteAuditStore } from "./remote-audit-store.ts";

if (process.env.DC24H_STAGE4_STARTUP_ACK !== "OWNER_WEBADMIN_PUBLIC_ACTIVATION_GO")
  throw Error("Stage IV startup refused: separate OWNER activation acknowledgement required");
if (process.env.DC24H_STAGE4_ORIGIN !== PUBLIC_WEBADMIN_ORIGIN)
  throw Error("Stage IV fixed public origin mismatch");
const config = publicBridgeCredentials(process.env);
const emailPath = process.env.DC24H_STAGE4_VERIFIED_EMAIL_FILE;
const auditPath = process.env.DC24H_STAGE4_AUDIT_FILE;
const limiterPath = process.env.DC24H_STAGE4_RATE_LIMIT_FILE;
if (!emailPath || !auditPath || !limiterPath) throw Error("Pre-provisioned private catalogs required");

const email = openVerifiedEmailCatalog(emailPath);
const audit = openRemoteAuditStore(auditPath);
// Rate-limit integrity is tied to a separate service-local random key,
// NEVER the C++ private token (which WebAdmin cannot read).
const limiterKey = process.env.DC24H_STAGE4_LIMITER_KEY;
if (!limiterKey || !/^[0-9a-f]{64}$/.test(limiterKey))
  throw Error("Stage IV independent limiter key required");
const limiter = openRemoteRateLimitStore(limiterPath, limiterKey);
const app = createServer(publicGateway({
  backend: publicBackend(config),
  email,
  sendCode: createMsmtpSender(),
  limiter,
  auditHealthy: () => audit.health().healthy && audit.verify(),
  audit: event => audit.append(event, Date.now()),
}));
app.requestTimeout = 5000;
app.headersTimeout = 6000;
app.maxRequestsPerSocket = 50;
app.listen(4278, "127.0.0.1", () => process.stdout.write("[stage4] loopback gateway ready\n"));
process.once("SIGTERM", () => { app.close(); audit.close(); });
