import { spawn } from "node:child_process";
import { lstatSync } from "node:fs";

const mailDirectory = "/var/lib/dc24h-stage4";
const mailConfiguration = mailDirectory + "/.msmtprc";

// Never fall back to root's ~/.msmtprc, a system-wide account or SMTP
// configuration supplied by an HTTP request. Secrets belong to this UID only.
export function dedicatedMsmtpConfigReady(): boolean {
  try {
    const uid = process.geteuid?.();
    const parent = lstatSync(mailDirectory), file = lstatSync(mailConfiguration);
    return uid !== undefined && uid !== 0 && parent.isDirectory() &&
      parent.uid === uid && (parent.mode & 0o777) === 0o700 &&
      file.isFile() && file.uid === uid && file.nlink === 1 &&
      (file.mode & 0o777) === 0o600 && file.size > 0 && file.size <= 16384;
  } catch { return false; }
}

const mailboxPattern = /^[A-Za-z0-9.!#$%&'*+\/=?^_`{|}~-]+@[A-Za-z0-9](?:[A-Za-z0-9.-]{0,251}[A-Za-z0-9])?$/;
export const isSafeMailbox = (value: unknown): value is string =>
  typeof value === "string" && value.length <= 254 && mailboxPattern.test(value) &&
  !value.includes("..") && !value.includes("\r") && !value.includes("\n");

export function createLoginMessage(recipient: string, code: string): string {
  if (!isSafeMailbox(recipient) || !/^[0-9]{8}$/.test(code)) throw Error("Unsafe email message");
  return "From: admin@dc24h.eu\nTo: " + recipient +
    "\nSubject: dc24h.eu WebAdmin login code\n" +
    "MIME-Version: 1.0\nContent-Type: text/plain; charset=UTF-8\n" +
    "\nYour dc24h.eu WebAdmin sign-in code is: " + code +
    "\nThis code expires after 5 minutes. Do not share it.\n";
}

/** Matches `printf 'From: ...\nTo: ...\n...' | msmtp -t` without a shell.
 * The `To` address is supplied only from an operator-verified account roster.
 */
export function createMsmtpSender(binary = "/usr/bin/msmtp") {
  if (binary !== "/usr/bin/msmtp") throw Error("Unexpected msmtp executable");
  return (recipient: string, code: string): Promise<boolean> => {
    if (!isSafeMailbox(recipient) || !/^[0-9]{8}$/.test(code) ||
        !dedicatedMsmtpConfigReady()) return Promise.resolve(false);
    return new Promise(resolve => {
      let finished = false, timedOut = false;
      const complete = (ok: boolean) => {
        if (finished) return;
        finished = true; clearTimeout(timer); resolve(ok);
      };
      const child = spawn(binary, ["--file=" + mailConfiguration, "-t"], {
        shell: false, stdio: ["pipe", "ignore", "ignore"],
        env: { HOME: mailDirectory, PATH: "/usr/bin:/bin", LANG: "C.UTF-8" },
      });
      const timer = setTimeout(() => { timedOut = true; child.kill("SIGKILL"); }, 10_000);
      child.once("error", () => complete(false));
      child.once("close", exitCode => complete(!timedOut && exitCode === 0));
      child.stdin.on("error", () => complete(false));
      child.stdin.end(createLoginMessage(recipient, code));
    });
  };
}
