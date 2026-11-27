"""Fixed-scope checks against the approved public endpoint."""

import hashlib
import re
import socket
import ssl
import subprocess
import time

import public_stage3_external_probe as base
from public_stage3_qualification_external import FORBIDDEN_TCP, guest, public_context, require, see_quit, suffix
from stage3.evidence import Blocked, Mismatch

ENDPOINT = (base.IPV4, base.ADCS_PORT)


def forward_confirmed_ptr():
    """Verify the OWNER-provided IPv4/PTR pair using the runner's DNS resolver."""
    try:
        ptr, _, _ = socket.gethostbyaddr(base.IPV4)
    except (socket.herror, socket.gaierror, TimeoutError, OSError):
        raise Blocked("Independent PTR lookup is unavailable") from None
    if ptr.rstrip(".").lower() != base.FQDN.lower():
        raise Mismatch("Public PTR does not match the approved hostname")

    try:
        records = socket.getaddrinfo(base.FQDN, None, family=socket.AF_INET,
                                     type=socket.SOCK_STREAM)
    except (socket.gaierror, TimeoutError, OSError):
        raise Blocked("Independent forward DNS lookup is unavailable") from None
    addresses = {record[4][0] for record in records}
    if addresses != {base.IPV4}:
        raise Mismatch("The PTR hostname does not resolve exclusively to the approved IPv4")
    return f"Forward-confirmed public DNS: {base.IPV4} PTR {base.FQDN}; A {base.IPV4}"


def public_tls(suite):
    with suite.case("dns.a"):
        base.assert_dns()
    with suite.case("dns.ptr.forward-confirmed") as result:
        result.observed = forward_confirmed_ptr()
    context = public_context()
    with suite.case("tls.identity") as result:
        with base.open_tls(context) as peer:
            details = peer.getpeercert()
            der = peer.getpeercert(binary_form=True)
        require(("DNS", base.FQDN) in details["subjectAltName"], "Exact public DNS SAN was missing")
        require(("organizationName", "Let's Encrypt") in sum(details["issuer"], ()),
                "The public leaf issuer organization was unexpected")
        start = ssl.cert_time_to_seconds(details["notBefore"])
        end = ssl.cert_time_to_seconds(details["notAfter"])
        require(start <= time.time() < end, "Leaf certificate is outside its validity period")
        result.observed = (f"Verified TLS chain and SNI {base.FQDN}; exact DNS SAN; Let's Encrypt; "
                           f"validity {details['notBefore']} through {details['notAfter']}")
    with suite.case("tls.fingerprint") as result:
        fingerprint = hashlib.sha256(der).hexdigest()
        # Separate verified handshake via OpenSSL and independent DER digest calculation.
        handshake = subprocess.run(
            ["openssl", "s_client", "-connect", f"{base.IPV4}:{base.ADCS_PORT}", "-servername", base.FQDN,
             "-verify_hostname", base.FQDN, "-verify_return_error"],
            input=b"", capture_output=True, check=True, timeout=10,
        )
        leaf = re.search(rb"-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----", handshake.stdout, re.S)
        require(leaf is not None, "OpenSSL did not return a public leaf certificate")
        verified = subprocess.run(["openssl", "x509", "-noout", "-fingerprint", "-sha256"],
                                  input=leaf[0], capture_output=True, check=True, timeout=5)
        other = verified.stdout.decode("ascii").split("=", 1)[1].strip().replace(":", "").lower()
        require(fingerprint == other, "Verified handshakes returned different leaf fingerprints")
        result.observed = "Agreed public leaf SHA-256 " + fingerprint

    for identifier, version, expected in (("tls.v12", ssl.TLSVersion.TLSv1_2, "TLSv1.2"),
                                           ("tls.v13", ssl.TLSVersion.TLSv1_3, "TLSv1.3")):
        with suite.case(identifier) as result:
            context = public_context()
            context.maximum_version = version
            context.minimum_version = version
            with base.open_tls(context) as peer:
                require(peer.version() == expected, "TLS version negotiated outside the expected range")
            result.observed = expected + " negotiated with public CA and SAN validation"

    for identifier, flag, expected in (("tls.v10.denied", "-tls1", "TLS 1.0"),
                                       ("tls.v11.denied", "-tls1_1", "TLS 1.1")):
        with suite.case(identifier) as result:
            probe = subprocess.run(
                ["openssl", "s_client", "-connect", f"{base.IPV4}:{base.ADCS_PORT}", "-servername", base.FQDN,
                 flag, "-cipher", "DEFAULT:@SECLEVEL=0"],
                input=b"", capture_output=True, timeout=10,
            )
            if probe.returncode == 0:
                raise Mismatch(expected + " negotiated on the public listener")
            if b"alert protocol version" not in probe.stderr.lower():
                raise Blocked("No conclusive server protocol-version alert for " + expected)
            result.observed = "Server sent protocol-version rejection for " + expected
    with suite.case("tls.plaintext.denied") as result:
        with socket.create_connection(ENDPOINT, timeout=4) as raw:
            raw.sendall(b"HSUP ADBASE ADTIGR\n")
            try:
                response = raw.recv(32)
            except ConnectionResetError:
                response = b""
            except TimeoutError:
                raise Blocked("Public listener did not conclusively reject plaintext") from None
            require(not response or (len(response) == 7 and response[0] == 21 and response[1] == 3
                                     and response[3:6] == b"\x00\x02\x02"),
                    "Public ADCS returned unexpected data to a plaintext ADC greeting")
            result.observed = "Plaintext ADC greeting received only a close or fatal TLS alert"


def port_observation(port):
    try:
        with socket.create_connection((base.IPV4, port), timeout=2):
            raise Mismatch("Forbidden port is publicly accessible")
    except ConnectionRefusedError:
        return "TCP connection actively refused"
    except TimeoutError:
        return "TCP timed out from this runner; a filter could account for the timeout"
    except OSError:
        raise Blocked("Runner reported an inconclusive network error") from None


def exposure(suite):
    with suite.case("tcp.1512") as result:
        with base.open_tls(public_context()) as peer:
            result.observed = "Connection accepted on TCP 1512, with " + peer.version() + " and trusted hostname"
    for port in FORBIDDEN_TCP:
        try:
            with suite.case(f"tcp.{port}") as result:
                result.observed = port_observation(port)
        except (Mismatch, Blocked):
            continue


def extended(suite, observation_seconds=1800, cycles=20, pause=5):
    context = public_context()
    with suite.case("extended.cycles") as result:
        base.assert_dns()
        with base.open_tls(context):
            pass
        fixed_nick, fixed_identity = b"StageIIIExtended" + suffix(), base.pair(None)
        with guest(context, b"StageIIIWitness" + suffix()) as observer:
            seen = {observer.sid}
            for index in range(cycles):
                with guest(context, fixed_nick, identity=fixed_identity) as connection:
                    require(connection.sid not in seen, "Public hub assigned a previous or active SID")
                    seen.add(connection.sid)
                    observer.until(lambda line: line.startswith(b"BINF " + connection.sid + b" ") and
                                   base.guest_identity(line, fixed_nick, fixed_identity[1]), "extended guest join")
                see_quit(observer, connection.sid)
                if (index + 1) % 6 == 0:
                    heartbeat = b"BINF " + observer.sid + b" DEStageIIIWitness" + suffix()
                    observer.send(heartbeat)
                    observer.until(lambda line: line == heartbeat, "extended observer health")
                if index + 1 < cycles:
                    time.sleep(pause)
        result.observed = f"{cycles} independent join/quit observations and {cycles - 1} same-CID reconnects"

    with suite.case("extended.observation") as result:
        with guest(context, b"StageIIIHoldA" + suffix()) as one, guest(context, b"StageIIIHoldB" + suffix()) as two:
            require(one.sid != two.sid, "Two active public guests shared a SID")
            end = time.monotonic() + observation_seconds
            while time.monotonic() < end:
                time.sleep(min(25, max(0, end - time.monotonic())))
                for sender, receiver in ((one, two), (two, one)):
                    frame = b"BINF " + sender.sid + b" DEStageIIIHold" + suffix()
                    sender.send(frame)
                    receiver.until(lambda line: line == frame, "extended public session health")
        result.observed = f"Two authenticated sessions stayed active for {observation_seconds} seconds"
