import { createServer } from "node:http";
import { credentials } from "./local-api.ts";
import { remoteGateway } from "./remote-gateway.ts";

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
const app = createServer(remoteGateway(config, {
  origin,
  audit: (event) => process.stdout.write("[remote-gateway] event=" + event + "\n"),
}));
app.requestTimeout = 5000;
app.headersTimeout = 6000;
app.maxRequestsPerSocket = 100;
app.listen(port, "127.0.0.1", () => {
  process.stdout.write("[remote-gateway] local-only listener ready\n");
});
