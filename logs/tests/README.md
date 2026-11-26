# Test logs / Logi testów

Place **reviewed and redacted** test evidence here. This folder is ready for future test results; no test result is claimed by this README.

Recommended file names:

- `2026-11-26-debian13-ncdc-1f2ce42.redacted.log` — **example name only**, not an existing report.
- `YYYY-MM-DD-<component>-<commit>.summary.md` — short human-readable verdict.
- `YYYY-MM-DD-<component>-<commit>.redacted.json` — sanitized structured evidence if needed.

Each report should identify:

1. Exact Git commit and branch, OS/kernel, C++/OpenSSL/ncdc versions.
2. Test command/prompt, environment constraints (e.g. IPv4-only vs IPv6), timestamp and exit code.
3. PASS/FAIL/SKIP per test, negative test evidence, and unresolved failures.
4. Source of the evidence (CI artifact or local run) and SHA-256 of the original report, when available.
5. The related [public deployment Issue #10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10) / PR.

Do not upload full raw logs if they contain secrets or personal data. Prefer `*.redacted.log` after review; the root `.gitignore` continues to ignore other `*.log` files.

## PL

Dodawaj tu **wyłącznie zweryfikowane i oczyszczone** logi z testów. Każdy raport powinien zawierać SHA kodu, wersję systemu i klienta, polecenia, wyniki pozytywne i negatywne oraz powiązanie z Issue/PR. Nie oznaczaj niezaliczonych testów jako PASS. Nie publikuj haseł, kluczy TLS, tokenów ani danych prywatnych.
