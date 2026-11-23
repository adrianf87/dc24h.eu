import { createServer } from "node:http";
import { credentials } from "./local-api.ts";
import { remoteGateway } from "./remote-gateway.ts";
import { loadTotpVerifier } from "./remote-mfa.ts";
import { remoteAuditRecord } from "./remote-audit.ts";

// Explicitly local-only. A public listener or MFA-free deployment is not
// supported by this development release.
if (process.env.DC24H_REMOTE_PUBLIC || process.env.DC24H_REMOTE_BIND) {
  throw new Error("Public remote gateway is disabled");
}
const origin = process.env.DC24H_REMOTE_ORIGIN;
if (!origin) throw new Error("DC24H_REMOTE_ORIGIN is required");
const port = Number(process.env.DC24H_REMOTE_PORT ?? "4182");
if (!Number.isInteger(port) || port < 1024 || port > 65535) throw new Error("Invalid local gateway port");
const config = credentials(process.env);
if (!config) throw new Error("Local C++ API credentials required");
const mfaFile = process.env.DC24H_REMOTE_MFA_FILE;
const replayFile = process.env.DC24H_REMOTE_MFA_REPLAY_FILE;
if (!mfaFile || !replayFile) throw new Error("Private TOTP enrollment and replay files required");
// Both files are checked for ownership, mode and symlinks before listening.
const mfa = loadTotpVerifier(mfaFile, replayFile);
const app = createServer(remoteGateway(config, {
  origin,
  mfaVerify: mfa.verify,
  // systemd-journald receives one structured, bounded record per event.
  // No passwords, TOTP, cookies, raw request bodies or actor IDs enter the log.
  audit: (event) => process.stdout.write(remoteAuditRecord(event, Date.now())),
}));
app.requestTimeout = 5000;
app.headersTimeout = 6000;
app.maxRequestsPerSocket = 100;
app.listen(port, "127.0.0.1", () => {
  process.stdout.write("[remote-gateway] local-only listener ready\n");
});
process.once("SIGTERM", () => { app.close(); mfa.close(); });
