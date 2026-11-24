import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { fileURLToPath } from "node:url";
import test from "node:test";

test("live passkey helper releases an open Python command pipe on failure", async () => {
  const executable = fileURLToPath(new URL("./beta07-live-passkey-client.mjs", import.meta.url));
  const child = spawn(process.execPath, [executable], {
    stdio: ["pipe", "pipe", "ignore"],
  });
  let stdout = "";
  child.stdout.setEncoding("utf8");
  child.stdout.on("data", (chunk: string) => { stdout += chunk; });
  try {
    // Keep stdin open exactly as the Python harness does when it waits for
    // the completed helper. A leaked readline handle would time out here.
    child.stdin.write("{}\n");
    const code = await new Promise<number | null>((resolve, reject) => {
      const timer = setTimeout(() => reject(new Error("Passkey helper retained open stdin")), 4000);
      child.once("close", exitCode => { clearTimeout(timer); resolve(exitCode); });
      child.once("error", error => { clearTimeout(timer); reject(error); });
    });
    assert.equal(code, 1);
    assert.equal(stdout, "client_error:fixture_parameters\n");
  } finally {
    child.kill("SIGTERM");
    child.stdin.destroy();
  }
});
