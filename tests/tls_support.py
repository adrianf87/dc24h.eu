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


def connect_tls(case, hub, cert, version=None):
    context = ssl.create_default_context(cafile=cert)
    if version is not None:
        context.minimum_version = version
        context.maximum_version = version
    raw = socket.create_connection(tls_endpoint(hub), timeout=3)
    case.addCleanup(raw.close)
    peer = context.wrap_socket(raw, server_hostname='localhost')
    case.addCleanup(peer.close)
    return peer


def finish_tls(case, peer):
    # Peer close_notify establishes that all preceding application bytes were
    # consumed before the server closes. Server close_notify is a later milestone.
    try:
        peer.unwrap()
    except ssl.SSLEOFError:
        pass
    else:
        case.fail('server unexpectedly completed TLS shutdown; update test contract')
