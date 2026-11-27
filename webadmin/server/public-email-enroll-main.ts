// Run manually from the VPS console as the dedicated service account, with
// the public gateway STOPPED. Do not invoke this from GitHub Actions.
import { stdin, stdout, stderr } from "node:process";
import { credentials } from "./local-api.ts";
import { publicBackend } from "./public-backend.ts";
import { enrollFirstVerifiedEmail } from "./public-email-enrollment.ts";
import { createMsmtpSender } from "./public-msmtp.ts";

const hiddenPrompt = (label: string, signal?: AbortSignal): Promise<string> => {
  if (!stdin.isTTY || !stdout.isTTY || typeof stdin.setRawMode !== "function")
    return Promise.reject(Error("Interactive provider console required"));
  stdout.write(label);
  return new Promise((resolve, reject) => {
    const previousRaw = stdin.isRaw;
    let value = "", done = false;
    const end = (error?: Error) => {
      if (done) return;
      done = true;
      stdin.off("data", onData);
      signal?.removeEventListener("abort", onAbort);
      stdin.setRawMode(Boolean(previousRaw));
      stdin.pause(); stdout.write("\n");
      if (error) reject(error); else resolve(value);
    };
    const onAbort = () => end(Error("Enrollment expired"));
    const onData = (buffer: Buffer | string) => {
      for (const key of String(buffer)) {
        if (key === "\u0003" || key === "\u0004") { end(Error("Enrollment cancelled")); return; }
        if (key === "\r" || key === "\n") { end(); return; }
        if (key === "\b" || key === "\u007f") value = value.slice(0, -1);
        else if (key >= " " && key !== "\u007f" && value.length < 1024) value += key;
      }
    };
    stdin.setRawMode(true);
    stdin.resume(); stdin.on("data", onData);
    signal?.addEventListener("abort", onAbort, { once: true });
    if (signal?.aborted) onAbort();
  });
};

async function main() {
  const args = process.argv.slice(2);
  if (args.length !== 8 || args[0] !== "--account-name" || args[2] !== "--email" ||
      args[4] !== "--catalog" || args[6] !== "--owner-ack" ||
      args[7] !== "STAGE4_EMAIL_ENROLLMENT_GO") throw Error("Invalid OWNER enrollment arguments");
  const config = credentials(process.env);
  if (!config) throw Error("Private C++ account verification unavailable");
  const backend = publicBackend(config);
  const password = await hiddenPrompt("C++ account password (not echoed): ");
  const actor = await enrollFirstVerifiedEmail(args[5]!, args[1]!, args[3]!, password, {
    verifyNamedPassword: backend.verifyNamedPassword, reauthorize: backend.reauthorize,
    sendCode: createMsmtpSender(),
    readCode: signal => hiddenPrompt("One-time email code (not echoed): ", signal),
  });
  if (!actor) throw Error("Account/mailbox enrollment refused");
  stdout.write("Mailbox verified and bound to C++ account ID " + actor + ". Restart the gateway separately.\n");
}

main().catch(() => {
  // Do not log account credentials, the mailbox, the OTP or backend tokens.
  stderr.write("Stage IV email enrollment failed. No verified account activated.\n");
  process.exitCode = 1;
});
