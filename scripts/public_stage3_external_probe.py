#!/usr/bin/env python3
"""Bounded external TLS/guest ADC check for one OWNER-approved public endpoint.

Run only on a separate GitHub-hosted Ubuntu runner after approved activation.
No broad port scan, credentials, user data, or private network probes.
"""

import base64
import ctypes
import socket
import ssl
import sys


FQDN = "server-747585786.dc24h.eu"
IPV4 = "62.171.156.32"
ADCS_PORT = 1512
PLAINTEXT_ADC_PORT = 1511


def pair(seed):
    pid = bytes((number + seed) % 256 for number in range(24))
    gcrypt = ctypes.CDLL("libgcrypt.so.20")
    gcrypt.gcry_md_hash_buffer.argtypes = [ctypes.c_int, ctypes.c_void_p,
                                           ctypes.c_void_p, ctypes.c_size_t]
    digest = ctypes.create_string_buffer(24)
    gcrypt.gcry_md_hash_buffer(306, digest, pid, len(pid))  # TIGER1
    encode = lambda value: base64.b32encode(value).rstrip(b"=")
    return encode(pid), encode(digest.raw)


def recv_line(peer, buffer):
    while b"\n" not in buffer:
        block = peer.recv(4096)
        if not block or len(buffer) + len(block) > 65536:
            raise RuntimeError("Guest ADC session ended or exceeded its bound")
        buffer += block
    return buffer.split(b"\n", 1)


def connect_guest(context, nick, seed):
    raw = socket.create_connection((IPV4, ADCS_PORT), timeout=6)
    tls = context.wrap_socket(raw, server_hostname=FQDN)
    tls.settimeout(6)
    pending = b""
    try:
        tls.sendall(b"HSUP ADBASE ADTIGR\n")
        first, pending = recv_line(tls, pending)
        if first != b"ISUP ADBASE ADTIGR":
            raise RuntimeError("Unexpected ADCS SUP negotiation")
        second, pending = recv_line(tls, pending)
        if not second.startswith(b"ISID ") or len(second) != 9:
            raise RuntimeError("ADCS hub did not assign a valid SID")
        sid = second[5:]
        pid, cid = pair(seed)
        tls.sendall(b"BINF " + sid + b" NI" + nick + b" ID" + cid +
                    b" PD" + pid + b" I40.0.0.0 DEStageIIIProbe\n")
        for _ in range(32):
            line, pending = recv_line(tls, pending)
            if line.startswith((b"HPAS", b"ISTA 1", b"ISTA 2", b"IQUI ")):
                raise RuntimeError("Guest login was rejected or requested a password")
            if line.startswith(b"BINF " + sid + b" "):
                return tls, sid
        raise RuntimeError("Guest login did not reach NORMAL in bounded frames")
    except Exception:
        tls.close()
        raise


def assert_dns():
    addresses = {
        item[4][0] for item in socket.getaddrinfo(FQDN, ADCS_PORT,
                                                    socket.AF_INET, socket.SOCK_STREAM)
    }
    if addresses != {IPV4}:
        raise RuntimeError("External DNS A answers do not exclusively match OWNER IPv4")


def assert_plaintext_closed():
    try:
        peer = socket.create_connection((IPV4, PLAINTEXT_ADC_PORT), timeout=3)
    except (OSError, TimeoutError):
        return
    peer.close()
    raise RuntimeError("TCP 1511 is reachable; ADCS-only exposure violated")


def main():
    assert_dns()
    assert_plaintext_closed()
    context = ssl.create_default_context(purpose=ssl.Purpose.SERVER_AUTH)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    first = second = None
    try:
        first, first_sid = connect_guest(context, b"StageIIIProbe1", 41)
        second, second_sid = connect_guest(context, b"StageIIIProbe2", 67)
        if first_sid == second_sid:
            raise RuntimeError("Two guest sessions received the same SID")
    finally:
        if second is not None:
            second.close()
        if first is not None:
            first.close()
    print("EXTERNAL_ADCS_CA_SAN_TLS_AND_TWO_GUEST_LOGINS_PASS; TCP_1511_CLOSED")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, ssl.SSLError) as error:
        print("External Stage III acceptance failed: " + type(error).__name__, file=sys.stderr)
        sys.exit(2)
