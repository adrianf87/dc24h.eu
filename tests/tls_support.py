"""Disposable credentials and real TLS connections for local transport tests."""
import pathlib
import re
import socket
import ssl
import subprocess

TLS_READY = re.compile(rb"ADCS listening on (\[[^\]]+\]|[^ :]+):([0-9]+)")


def credentials(directory):
    cert = str(pathlib.Path(directory) / 'certificate.pem')
    key = str(pathlib.Path(directory) / 'private.pem')
    subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes',
                    '-days', '1', '-subj', '/CN=localhost',
                    '-addext', 'basicConstraints=critical,CA:FALSE',
                    '-addext', 'subjectAltName=DNS:localhost,IP:127.0.0.1,IP:::1',
                    '-addext', 'keyUsage=digitalSignature,keyEncipherment',
                    '-addext', 'extendedKeyUsage=serverAuth',
                    '-keyout', key, '-out', cert],
                   check=True, timeout=15, capture_output=True)
    return cert, key


def tls_endpoint(hub):
    hub.wait_for(lambda: TLS_READY.search(hub.log) is not None)
    host, port = TLS_READY.search(hub.log).groups()
    return host.decode().strip('[]'), int(port)


def secure_client_context(cafile):
    """Trust the disposable CA without ever negotiating legacy TLS 1.0/1.1."""
    context = ssl.create_default_context(cafile=str(cafile))
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    return context


def connect_tls(case, hub, cert, version=None):
    context = secure_client_context(cert)
    if version is not None:
        if version < ssl.TLSVersion.TLSv1_2:
            raise ValueError('Legacy TLS versions are never permitted')
        context.minimum_version = version
        context.maximum_version = version
    raw = socket.create_connection(tls_endpoint(hub), timeout=3)
    case.addCleanup(raw.close)
    peer = context.wrap_socket(raw, server_hostname='localhost')
    case.addCleanup(peer.close)
    return peer


def finish_tls(case, peer):
    # Drain any ordered reply before sending close_notify. Old transport tests
    # may deliberately negotiate invalid support and receive a fatal status/EOF.
    old_timeout = peer.gettimeout()
    peer.settimeout(0.1)
    try:
        while peer.recv(65536):
            pass
        return
    except socket.timeout:
        pass
    finally:
        peer.settimeout(old_timeout)
    # The transport closes the socket without an explicit SSL_shutdown().
    # OpenSSL/Python versions can expose that as an EOF or allow unwrap() to
    # complete normally. Neither result is a protocol failure in this helper.
    # The individual process tests still verify replies, counters and logs.
    try:
        peer.unwrap()
    except ssl.SSLEOFError:
        pass


def assert_fatal_tls_alert_or_eof(case, response):
    """Invalid TLS input may yield EOF or a fatal alert before close.

    Different OpenSSL versions differ here. Reject arbitrary application data:
    a fatal alert is exactly one TLS alert record containing a two-byte alert.
    """
    if not response:
        return
    case.assertEqual(len(response), 7, response)
    case.assertEqual(response[:2], b'\x15\x03', response)
    case.assertIn(response[2], (0, 1, 2, 3, 4), response)
    case.assertEqual(response[3:6], b'\x00\x02\x02', response)
    case.assertNotEqual(response[6], 0, response)
