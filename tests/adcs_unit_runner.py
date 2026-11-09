"""Generate disposable TLS credentials and execute the OpenSSL transport unit test."""
import pathlib
import subprocess
import sys
import tempfile

def main():
    binary = sys.argv[1]
    with tempfile.TemporaryDirectory(prefix='dc24h-adcs-unit-') as directory:
        cert = pathlib.Path(directory) / 'cert.pem'
        key = pathlib.Path(directory) / 'key.pem'
        subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes',
                        '-days', '1', '-subj', '/CN=localhost',
                        '-keyout', str(key), '-out', str(cert)],
                       check=True, capture_output=True, timeout=15)
        subprocess.run([binary, str(cert), str(key)], check=True, timeout=10)

if __name__ == '__main__':
    main()
