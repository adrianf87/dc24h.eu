// Child-only fault harness. No user identities, secrets or production paths.
import { openRemoteAuditStore } from "../server/remote-audit-store.ts";

const [mode, path] = process.argv.slice(2);
if (!path || (mode !== "fsync-error" && mode !== "crash-after-fsync")) throw Error("Invalid fault case");
const audit = openRemoteAuditStore(path);
if (mode === "crash-after-fsync") {
  audit.append("login_refused", 1_700_000_000_000);
  process.kill(process.pid, "SIGKILL");
  throw Error("Crash injection did not terminate process");
}
try {
  audit.append("login_refused", 1_700_000_000_000);
  throw Error("Injected fsync error was not observed");
} catch (error) {
  if (!String(error).includes("EIO") || audit.health().healthy || audit.health().dropped !== 1)
    throw Error("Audit did not fail closed on fsync EIO");
  try { audit.append("login_ok", 1_700_000_000_001); throw Error("Audit writer resumed after failure"); }
  catch (retryError) { if (!String(retryError).includes("unavailable")) throw retryError; }
  process.stdout.write("injected fsync EIO refused and writer locked\n");
} finally { audit.close(); }
