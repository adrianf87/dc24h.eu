import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import { resolve } from "node:path";
import { localApiPlugin } from "./server/local-api.ts";

export default defineConfig({
  base: "./",
  plugins: [react(), localApiPlugin()],
  server: {
    host: "127.0.0.1",
    port: 5173,
    strictPort: true,
    cors: false,
    allowedHosts: ["localhost"],
    fs: {
      strict: true,
      allow: [import.meta.dirname],
      deny: [
        "**/.env*",
        "**/.git/**",
        "**/*.token",
        "**/*.password",
        "**/*.key",
        "**/*.pem",
        "**/*.crt",
      ],
    },
  },
  preview: { host: "127.0.0.1", port: 4173, strictPort: true },
  build: {
    sourcemap: false, target: "es2022",
    rollupOptions: { input: {
      local: resolve(import.meta.dirname, "index.html"),
      remote: resolve(import.meta.dirname, "remote.html"),
    } },
  },
});
