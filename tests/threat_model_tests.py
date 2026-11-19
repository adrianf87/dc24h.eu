#!/usr/bin/env python3
"""Lock the alpha-0.9.1 threat model to the current boundaries."""

from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
IDENTIFIERS = [f"T{number:02d}" for number in range(1, 17)]
LINKS = (
    "SECURITY_SESSIONS.md",
    "APACHE2_PROXY.md",
    "APACHE2_HTTPS.md",
    "ANTI_BRUTEFORCE.md",
    "AUDIT_LOGS.md",
    "ADC_AUTHENTICATION.md",
    "PERMISSIONS.md",
    "SEARCH_SECURITY.md",
    "TRANSFER_LIMITS.md",
    "API.md",
    "DATABASE.md",
)


def headings(text):
    return re.findall(r"^### (T\d{2})\b", text, re.MULTILINE)


def require(condition, message, errors):
    if not condition:
        errors.append(message)


def check(path, phrases):
    errors = []
    text = path.read_text(encoding="utf-8")
    if "\ufffd" in text or text.startswith("\ufeff"):
        errors.append(f"{path.name}: replacement character or UTF-8 BOM")
    found = headings(text)
    require(found == IDENTIFIERS, f"{path.name}: threat headings are {found}", errors)
    for phrase in phrases:
        require(text.count(phrase) >= 1, f"{path.name}: missing {phrase!r}", errors)
    for link in LINKS:
        require(f"]({link})" in text, f"{path.name}: missing link {link}", errors)
    require("dc24h-account-admin" in text, f"{path.name}: maintenance process is unnamed", errors)
    require("Require local" in text, f"{path.name}: local access is unnamed", errors)
    require("X-Forwarded-For" in text, f"{path.name}: proxy identity is unnamed", errors)
    require("7872471" in text, f"{path.name}: base commit is unnamed", errors)
    for forbidden in ("HTTP/2 is implemented", "OCSP is implemented", "public gateway is implemented"):
        require(forbidden not in text, f"{path.name}: forbidden claim {forbidden!r}", errors)
    return errors


def main():
    errors = []
    errors.extend(check(ROOT / "doc/us/THREAT_MODEL.md", (
        "# Threat model — alpha-0.9.1",
        "Schema v9 is unchanged",
        "`configuration.manage` and `hub.manage` stay unimplemented",
        "This is not a public authenticated gateway",
        "HTTP/2, OCSP and automatic certificate renewal are not implemented",
        "The analysis does not add a protocol check, a schema migration, an Apache setting or a WebAdmin control.",
        "This document does not mark any of them done.",
    )))
    errors.extend(check(ROOT / "doc/pl/THREAT_MODEL.md", (
        "# Model zagrożeń — alpha-0.9.1",
        "Schemat v9 pozostaje bez zmian",
        "`configuration.manage` i `hub.manage` pozostają niezaimplementowane",
        "To nie jest publiczna uwierzytelniona brama",
        "HTTP/2, OCSP i automatyczne odnawianie certyfikatu nie są zaimplementowane",
        "Analiza nie dodaje sprawdzenia protokołu, migracji schematu, ustawienia Apache ani mechanizmu WebAdmin.",
        "Ten dokument nie oznacza żadnego z nich jako ukończonego.",
    )))
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print("Threat model boundaries, residual risks and language parity passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
