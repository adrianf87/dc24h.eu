"""Real SCH parsing over ADC/ADCS: semantic refusals, recovery and privacy."""
import json
import pathlib
import tempfile
import unittest

from adc_login_process_tests import Peer
from api_support import enable_api, fetch_status
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls


class SchProcessTests(unittest.TestCase):
    def fixture(self, encrypted):
        directory = tempfile.TemporaryDirectory(prefix='dc24h-sch-')
        self.addCleanup(directory.cleanup)
        cert, _ = credentials(directory.name)
        ini = pathlib.Path(directory.name) / 'hub.ini'
        ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                       '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n'
                       '[security]\nmax_frames_per_second=10000\n')
        endpoint, token = enable_api(ini, directory.name)
        hub = Hub(self, '--config', str(ini), '--log-connections', 'on', '--log-chat', 'on')
        source = Peer(self, connect_tls(self, hub, cert) if encrypted else hub.connect())
        source.login(b'SchSource', 1)
        observer = Peer(self, hub.connect()); observer.login(b'SchObserver', 2)
        self.assertTrue(source.line().startswith(b'BINF ' + observer.sid + b' '))
        return hub, cert, endpoint, token, source, observer

    def fence(self, source, observer):
        frame = b'BINF ' + source.sid + b' DESchFence'
        source.send(frame)
        for peer in (source, observer): self.assertEqual(peer.line(), frame)

    def test_fields_limits_ignored_queries_recovery_and_source_priority(self):
        for encrypted in (False, True):
            hub, cert, endpoint, token, source, observer = self.fixture(encrypted)
            before = fetch_status(endpoint, token)['chat']
            valid = [b'ANSchPrivateSecret ANlinux NOgentoo EXISO EXIMG GE0 LE9223372036854775807 EQ0 TY01 TOtoken XXfuture XX',
                     'ANSchPrivateSecret\\sŻółw\\s😀 TOtoken\\sŻółw'.encode(),
                     b'EXiso GE0 TY2', b'EXiso', b'NOtmp', b'GE0', b'LE0', b'EQ0', b'TY1', b'TY2',
                     'EXŻÓŁW GE0 LE1 TOtoken\\sŻółw'.encode(), b'TR' + b'A' * 39,
                     b'ANSchPrivateSecret TR' + b'A' * 39]
            for kind in (b'BSCH', b'FSCH', b'DSCH', b'ESCH'):
                header = kind + b' ' + source.sid
                if kind in (b'DSCH', b'ESCH'): header += b' ' + observer.sid
                if kind == b'FSCH': header += b' +TCP4-UDP4'
                for fields in valid:
                    frame = header + b' ' + fields + b' TOPrivateSchToken' if b' TO' not in fields else header + b' ' + fields
                    # Fragmented transport must produce one semantic response.
                    wire = frame + b'\n'
                    source.peer.sendall(wire[:7]); source.peer.sendall(wire[7:])
                    self.assertEqual(source.line(), b'ISTA 125 Search\\srouting\\snot\\simplemented FC' + kind)
                    self.fence(source, observer)
            invalid = [b'AN', b'AN\\s', b'ANbad\\ntext', b'ANx NO', b'ANx EX.iso', b'ANx EXa/b',
                       b'ANx GE-1', b'ANx LE+1', b'ANx EQ9223372036854775808', b'ANx GE2 LE1',
                       b'ANx TY0', b'ANx TY3', b'ANx TY1 TY1', b'ANx TO', b'ANx TOa TOb',
                       b'ANx TRinvalid', b'AN' + b'x' * 257,
                       b'ANx ' + b'NOx ' * 32, b'ANx ' + b'EXiso ' * 17,
                       b'ANx XX' + b'x' * 4092, b'ANx TO' + b'x' * 129]
            for fields in invalid:
                source.send(b'BSCH ' + source.sid + b' ' + fields.rstrip(b' '))
                self.assertEqual(source.line(), b'ISTA 100 Invalid\\ssearch\\squery FCBSCH')
                self.assertFalse(source.pending)
                self.fence(source, observer)
            source.send(b'FSCH ' + source.sid + b' +TCP4-TCP4 ANx')
            self.assertEqual(source.line(), b'ISTA 100 Invalid\\ssearch\\squery FCFSCH')
            self.fence(source, observer)
            for fields in (b'', b' XXSchPrivateSecret', b' TOPrivateSchToken XXopaque'):
                source.send(b'BSCH ' + source.sid + fields)
                self.fence(source, observer)  # No criterion: no response or fanout.
            # Raw parser errors remain isolated and never reach the SCH handler.
            for fields in (b'ANbad\\q', b'ANe\xcc\x81', b'AN\xff'):
                source.send(b'BSCH ' + source.sid + b' ' + fields)
                self.fence(source, observer)
            self.assertEqual(fetch_status(endpoint, token)['chat'], before)
            waiting = Peer(self, hub.connect()); waiting.handshake()
            waiting.send(b'BSCH ' + waiting.sid + b' GEbad')
            self.assertTrue(drain_until_eof(waiting.peer).startswith(b'ISTA 244 '))
            source.send(b'BSCH ' + observer.sid + b' GEbad')
            terminal = drain_until_eof(source.peer)
            self.assertTrue(terminal.startswith(b'ISTA 240 ')); self.assertNotIn(b'GEbad', terminal)
            self.assertEqual(observer.line(), b'IQUI ' + source.sid)
            snapshot = json.dumps(fetch_status(endpoint, token))
            self.assertNotIn('SchPrivateSecret', snapshot); self.assertNotIn('PrivateSchToken', snapshot)
            hub.stop()
            for private in (b'SchPrivateSecret', b'PrivateSchToken', b'GEbad', token.encode()):
                self.assertNotIn(private, hub.log)


if __name__ == '__main__':
    unittest.main()
