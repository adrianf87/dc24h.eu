import {
  constants,
  openSync,
  fstatSync,
  readSync,
  closeSync,
  lstatSync,
  realpathSync,
} from "node:fs";
import { isAbsolute, dirname, relative } from "node:path";
import { request } from "node:http";
import type { IncomingMessage, ServerResponse } from "node:http";
import type { Plugin } from "vite";
import { parseSnapshot } from "../src/lib/status.ts";

import { accountMiddleware } from "./account-api.ts";

interface Credentials {
  socket: string;
  token: string;
}
export function credentials(
  env: NodeJS.ProcessEnv,
  webRoot?: string,
): Credentials | null {
  const socket = env.DC24H_API_SOCKET,
    file = env.DC24H_API_TOKEN_FILE;
  if (!socket && !file) return null;
  if (!socket || !file || !isAbsolute(socket) || !isAbsolute(file))
    throw new Error("Invalid local API adapter configuration");
  const uid = process.geteuid?.();
  let fd: number | undefined;
  const buffer = Buffer.alloc(67);
  try {
    if (webRoot) {
      const location = relative(realpathSync(webRoot), realpathSync(file));
      if (location !== ".." && !location.startsWith("../")) throw new Error();
    }
    const endpoint = lstatSync(socket),
      parent = lstatSync(dirname(socket));
    if (
      !endpoint.isSocket() ||
      endpoint.uid !== uid ||
      endpoint.mode & 0o077 ||
      !parent.isDirectory() ||
      parent.uid !== uid ||
      parent.mode & 0o077
    )
      throw new Error();
    fd = openSync(
      file,
      constants.O_RDONLY | constants.O_NOFOLLOW | constants.O_NONBLOCK,
    );
    const stat = fstatSync(fd);
    if (
      !stat.isFile() ||
      stat.uid !== uid ||
      stat.nlink !== 1 ||
      stat.mode & 0o077 ||
      stat.size < 64 ||
      stat.size > 66
    )
      throw new Error();
    let count = 0,
      read = 0;
    do {
      read = readSync(fd, buffer, count, buffer.length - count, null);
      count += read;
    } while (read && count < buffer.length);
    const token = buffer
      .subarray(0, count)
      .toString("ascii")
      .replace(/\r?\n$/, "");
    if (
      count > 66 ||
      !/^[0-9a-fA-F]{64}$/.test(token) ||
      buffer.subarray(0, count).some((byte) => byte > 127)
    )
      throw new Error();
    return { socket, token };
  } catch {
    throw new Error("Invalid local API adapter configuration");
  } finally {
    buffer.fill(0);
    if (fd !== undefined) closeSync(fd);
  }
}
function reply(res: ServerResponse, code: number, error: string) {
  res.writeHead(code, {
    "Content-Type": "application/json",
    "Cache-Control": "no-store",
    "X-Content-Type-Options": "nosniff",
  });
  res.end(JSON.stringify({ error }));
}
export function middleware(config: Credentials | null) {
  let active = 0;
  const accounts = accountMiddleware(config);
  return (req: IncomingMessage, res: ServerResponse, next: () => void) => {
    if (!req.url?.startsWith("/admin-api")) {
      next();
      return;
    }
    const host = req.headers.host ?? "",
      peer = req.socket.remoteAddress;
    if (
      !["127.0.0.1", "::1", "::ffff:127.0.0.1"].includes(peer ?? "") ||
      !/^(127\.0\.0\.1|localhost|\[::1\])(:\d{1,5})?$/.test(host) ||
      (req.headers.origin && req.headers.origin !== `http://${host}`) ||
      (req.headers["sec-fetch-site"] &&
        !["same-origin", "none"].includes(
          String(req.headers["sec-fetch-site"]),
        ))
    ) {
      reply(res, 403, "forbidden");
      return;
    }
    if (req.url.startsWith("/admin-api/v1/accounts") || req.url === "/admin-api/v1/admin" || req.url.startsWith("/admin-api/v1/bans") || req.url === "/admin-api/v1/audit") { accounts(req, res); return; }
    if (req.method !== "GET") {
      res.setHeader("Allow", "GET");
      reply(res, 405, "method_not_allowed");
      return;
    }
    if (
      req.url !== "/admin-api/v1/status" &&
      req.url !== "/admin-api/v1/health"
    ) {
      reply(res, 404, "not_found");
      return;
    }
    if (
      req.headers["transfer-encoding"] ||
      (req.headers["content-length"] && req.headers["content-length"] !== "0")
    ) {
      reply(res, 400, "bad_request");
      return;
    }
    if (!config) {
      reply(res, 503, "api_unconfigured");
      return;
    }
    if (active >= 8) {
      reply(res, 429, "api_busy");
      return;
    }
    active++;
    let done = false;
    const upstream = request({
      socketPath: config.socket,
      path: req.url.replace("/admin-api/", "/api/"),
      method: "GET",
      agent: false,
      headers: {
        Host: "localhost",
        Authorization: `Bearer ${config.token}`,
        Accept: "application/json",
        Connection: "close",
      },
    });
    const timer = setTimeout(
      () => upstream.destroy(new Error("API timeout")),
      2000,
    );
    const finish = () => {
      if (!done) {
        done = true;
        active--;
        clearTimeout(timer);
      }
    };
    res.once("close", () => {
      finish();
      upstream.destroy();
    });
    upstream.on("response", (response) => {
      const chunks: Buffer[] = [];
      let size = 0;
      response.on("data", (chunk: Buffer) => {
        size += chunk.length;
        if (size > 16384) {
          upstream.destroy(new Error("API response too large"));
          return;
        }
        chunks.push(chunk);
      });
      response.on("error", () => {
        if (!done) {
          finish();
          reply(res, 502, "api_unavailable");
        }
      });
      response.on("end", () => {
        if (done) return;
        finish();
        if (response.statusCode !== 200) {
          reply(
            res,
            502,
            response.statusCode === 401
              ? "api_unauthorized"
              : "api_unavailable",
          );
          return;
        }
        try {
          if (!response.headers["content-type"]?.startsWith("application/json"))
            throw new Error();
          const data: unknown = JSON.parse(
            Buffer.concat(chunks).toString("utf8"),
          );
          const result =
            req.url === "/admin-api/v1/status"
              ? parseSnapshot(data)
              : data &&
                  typeof data === "object" &&
                  "status" in data &&
                  data.status === "ok"
                ? { status: "ok" }
                : null;
          if (!result) throw new Error();
          res.writeHead(200, {
            "Content-Type": "application/json",
            "Cache-Control": "no-store",
            "X-Content-Type-Options": "nosniff",
          });
          res.end(JSON.stringify(result));
        } catch {
          reply(res, 502, "invalid_api_response");
        }
      });
    });
    upstream.on("error", () => {
      if (!done) {
        finish();
        reply(res, 502, "api_unavailable");
      }
    });
    upstream.end();
  };
}
export function localApiPlugin(): Plugin {
  return {
    name: "dc24h-local-development-api",
    configureServer(server) {
      server.middlewares.use(
        middleware(credentials(process.env, server.config.root)),
      );
    },
    configurePreviewServer(server) {
      server.middlewares.use(middleware(null));
    },
  };
}
