#!/usr/bin/env python3
"""Bounded, opt-in LOOPBACK performance probe for a disposable dc24h pilot.

The report is a measurement, not a production capacity rating. Never run it
against public hosts or live user data; the operator supplies an isolated hub.
"""

import argparse
import concurrent.futures
import http.client
import json
import os
from pathlib import Path
import socket
import ssl
import stat
import time


PROFILES = ("adc", "adcs", "webadmin", "api")


def percentile(sorted_values, fraction):
    if not sorted_values:
        return None
    index = max(0, int(len(sorted_values) * fraction + 0.999999) - 1)
    return round(sorted_values[min(index, len(sorted_values) - 1)], 3)


def summarize(profile, samples, seconds, rate, concurrency, rss_before=None, rss_after=None):
    latency = sorted(round(s[1], 3) for s in samples if s[0])
    errors = {}
    for ok, _, reason in samples:
        if not ok:
            errors[reason] = errors.get(reason, 0) + 1
    total = len(samples)
    return {
        "schema": 1, "profile": profile, "target": "127.0.0.1 only",
        "requests": total, "success": len(latency), "failure": total - len(latency),
        "error_kinds": dict(sorted(errors.items())),
        "error_rate": round((total - len(latency)) / total, 6) if total else None,
        "elapsed_seconds": round(seconds, 3),
        "successful_requests_per_second": round(len(latency) / seconds, 3) if seconds > 0 else None,
        "p50_ms": percentile(latency, 0.50), "p95_ms": percentile(latency, 0.95),
        "p99_ms": percentile(latency, 0.99), "max_ms": max(latency, default=None),
        "configured_rate_per_second": rate, "concurrency": concurrency,
        "target_rss_before_kib": rss_before, "target_rss_after_kib": rss_after,
        "capacity_claim": False,
    }


def private_cookie(path):
    if path is None:
        raise ValueError("Private session cookie file required for API probe")
    file = Path(path)
    if not file.is_absolute():
        raise ValueError("Absolute cookie file required")
    # Refuse a substituted FIFO without waiting indefinitely for a writer.
    fd = os.open(file, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        info = os.fstat(fd)
        if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1 or info.st_uid != os.geteuid() or (
            info.st_mode & 0o077) or info.st_size < 77 or info.st_size > 128:
            raise ValueError("Unsafe private cookie file")
        raw = os.read(fd, 129).decode("ascii").strip()
    finally:
        os.close(fd)
    import re
    if not re.fullmatch(r"dc24h_remote=[0-9a-f]{64}", raw):
        raise ValueError("Invalid private session cookie")
    return raw


def rss_kib(pid):
    if pid is None:
        return None
    try:
        lines = Path(f"/proc/{pid}/status").read_text(encoding="ascii").splitlines()
        return int(next(row.split()[1] for row in lines if row.startswith("VmRSS:")))
    except (OSError, ValueError, StopIteration):
        return None


def adc_greeting(sock, timeout):
    """Require a complete ADC hub SUP line; an error/partial banner is not success."""
    deadline = time.monotonic() + timeout
    greeting = bytearray()
    while b"\n" not in greeting and len(greeting) < 2048:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise socket.timeout("Incomplete ADC greeting")
        sock.settimeout(remaining)
        piece = sock.recv(min(2048 - len(greeting), 512))
        if not piece:
            return False
        greeting.extend(piece)
    if b"\n" not in greeting:
        return False
    first = bytes(greeting).split(b"\n", 1)[0]
    return first.startswith(b"ISUP ") and b"ADBASE" in first.split(b" ")[1:]


def valid_http_response(profile, response, body):
    if response.status != 200 or not body or len(body) > 65536:
        return False
    content_type = response.getheader("Content-Type", "").lower()
    if profile == "webadmin":
        return content_type.split(";", 1)[0].strip() == "text/html"
    if content_type not in ("application/json", "application/json; charset=utf-8"):
        return False
    try:
        payload = json.loads(body)
    except (ValueError, UnicodeDecodeError):
        return False
    return (isinstance(payload, dict) and type(payload.get("schema")) is int and
            payload["schema"] == 1 and payload.get("mode") == "read-only" and
            isinstance(payload.get("server"), dict))


def probe(profile, port, timeout, tls, cookie):
    start = time.monotonic_ns()
    try:
        if profile in ("webadmin", "api"):
            conn = http.client.HTTPSConnection("localhost", port, timeout=timeout, context=tls)
            try:
                route = "/remote.html" if profile == "webadmin" else "/remote/v1/summary"
                headers = {"Cache-Control": "no-store", "Connection": "close"}
                if cookie:
                    headers["Cookie"] = cookie
                conn.request("GET", route, headers=headers)
                response = conn.getresponse()
                body = response.read(65537)
                if not valid_http_response(profile, response, body):
                    return False, (time.monotonic_ns() - start) / 1_000_000, "http_unexpected"
            finally:
                conn.close()
        else:
            with socket.create_connection(("127.0.0.1", port), timeout=timeout) as raw:
                if profile == "adcs":
                    raw = tls.wrap_socket(raw, server_hostname="localhost")
                with raw:
                    raw.settimeout(timeout)
                    raw.sendall(b"HSUP ADBASE ADTIGR\n")
                    if not adc_greeting(raw, timeout):
                        return False, (time.monotonic_ns() - start) / 1_000_000, "invalid_protocol"
        return True, (time.monotonic_ns() - start) / 1_000_000, "ok"
    except ssl.SSLError:
        reason = "tls_failure"
    except (socket.timeout, TimeoutError):
        reason = "timeout"
    except (OSError, http.client.HTTPException):
        reason = "transport_failure"
    return False, (time.monotonic_ns() - start) / 1_000_000, reason


def arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", choices=PROFILES, required=True)
    parser.add_argument("--port", type=int, required=True)
    parser.add_argument("--requests", type=int, default=25)
    parser.add_argument("--rate", type=int, default=2, help="Start rate, 1–20 requests/s")
    parser.add_argument("--concurrency", type=int, default=2)
    parser.add_argument("--timeout", type=float, default=4)
    parser.add_argument("--ca-file", help="Verified local CA bundle for TLS/HTTPS")
    parser.add_argument("--cookie-file", help="0600 file with only dc24h_remote=<64 hex chars>; API only")
    parser.add_argument("--target-pid", type=int, help="Optional target process /proc VmRSS snapshots")
    parser.add_argument("--ack-isolated", action="store_true", help="I verified this is an isolated loopback pilot")
    args = parser.parse_args(argv)
    if not args.ack_isolated:
        parser.error("Explicit --ack-isolated required; never benchmark production implicitly")
    if not 1 <= args.port <= 65535 or not 1 <= args.requests <= 1000 or not 1 <= args.rate <= 20 or not 1 <= args.concurrency <= 8 or not 0.2 <= args.timeout <= 5:
        parser.error("Unsafe load profile: port, requests, rate, concurrency or timeout out of bounds")
    if args.target_pid is not None and args.target_pid < 1:
        parser.error("Invalid target PID")
    if args.profile == "api" and not args.cookie_file:
        parser.error("Authenticated API probe requires an owner-provisioned private cookie file")
    if args.profile != "api" and args.cookie_file:
        parser.error("Cookie is only valid for the private API profile")
    if args.profile != "adc" and not args.ca_file:
        parser.error("Certificate-verified TLS requires --ca-file")
    return args


def main(argv=None):
    args = arguments(argv)
    cookie = private_cookie(args.cookie_file) if args.profile == "api" else None
    tls = None
    if args.profile != "adc":
        tls = ssl.create_default_context(cafile=args.ca_file)
        tls.minimum_version = ssl.TLSVersion.TLSv1_2
    before = rss_kib(args.target_pid)
    samples = []
    started = time.monotonic()
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.concurrency) as executor:
        futures = []
        for n in range(args.requests):
            target = started + n / args.rate
            pause = target - time.monotonic()
            if pause > 0:
                time.sleep(pause)
            futures.append(executor.submit(probe, args.profile, args.port, args.timeout, tls, cookie))
        for result in concurrent.futures.as_completed(futures):
            samples.append(result.result())
    seconds = time.monotonic() - started
    print(json.dumps(summarize(args.profile, samples, seconds, args.rate, args.concurrency,
                               before, rss_kib(args.target_pid)), sort_keys=True))
    return 0 if all(ok for ok, _, _ in samples) else 1


if __name__ == "__main__":
    raise SystemExit(main())
