// Stage IV only: connect to a tightly scoped Unix bridge, never to the hub's
// same-UID private socket/token. The bridge verifies SO_PEERCRED independently.
import { lstatSync } from "node:fs";
import { dirname, isAbsolute } from "node:path";
import type { PublicBackendCredentials } from "./public-backend.ts";

export function publicBridgeCredentials(env: NodeJS.ProcessEnv): PublicBackendCredentials {
  const socket = env.DC24H_STAGE4_BRIDGE_SOCKET;
  if (!socket || !isAbsolute(socket) || socket !== "/run/dc24h-stage4-bridge/bridge.sock" ||
      env.DC24H_API_SOCKET || env.DC24H_API_TOKEN_FILE) {
    throw Error("Stage IV dedicated bridge configuration required");
  }
  try {
    const endpoint = lstatSync(socket);
    const parent = lstatSync(dirname(socket));
    const gid = process.getegid?.();
    const uid = process.geteuid?.();
    if (!endpoint.isSocket() || !parent.isDirectory() ||
        gid === undefined || uid === undefined || uid === 0 ||
        parent.uid === 0 || parent.uid === uid ||
        endpoint.uid !== parent.uid || endpoint.gid !== gid || parent.gid !== gid ||
        (endpoint.mode & 0o777) !== 0o660 ||
        (parent.mode & 0o777) !== 0o750) {
      throw Error("Unsafe Stage IV bridge endpoint");
    }
  } catch {
    throw Error("Unsafe Stage IV bridge endpoint");
  }
  return { socket, token: "" }; // The hub bearer token lives only in the bridge.
}
