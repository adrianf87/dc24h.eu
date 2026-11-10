"""MSG codec validation on real mixed ADC/ADCS connections; chat routing remains deferred."""
import pathlib
import tempfile
import unittest

from adc_login_process_tests import Peer
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls


class MsgProcessTests(unittest.TestCase):
    def hub(self):
        directory = tempfile.TemporaryDirectory(prefix='dc24h-msg-')
        self.addCleanup(directory.cleanup)
        cert, _ = credentials(directory.name)
        ini = pathlib.Path(directory.name) / 'hub.ini'
        ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                       '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n')
        return Hub(self, '--config', str(ini), '--log-connections', 'on'), cert

    def peer(self, hub, cert, encrypted):
        return Peer(self, connect_tls(self, hub, cert) if encrypted else hub.connect())

    def test_valid_frames_and_syntax_recovery(self):
        hub, cert = self.hub()
        for encrypted in (False, True):
            peer = self.peer(hub, cert, encrypted)
            peer.login(b'CodecUser'+str(int(encrypted)).encode(), int(encrypted)+1)
            for prefix, tail in ((b'BMSG '+peer.sid, b''), (b'HMSG', b' ME0'),
                                 (b'DMSG '+peer.sid+b' ZZZZ', b' PM'+peer.sid),
                                 (b'EMSG '+peer.sid+b' ZZZZ', b' PMDDDD ME1'),
                                 (b'FMSG '+peer.sid+b' +BASE-TCP4', b' XXfuture XXagain')):
                message = prefix+b' '+r'PrivateŻółw\s😀\nline\\path'.encode()+tail+b'\n'
                # Exercise UTF-8 and escape boundaries in stream input.
                for byte in message:
                    peer.peer.sendall(bytes([byte]))
                response = peer.line()
                if prefix.startswith(b'BMSG '):
                    self.assertEqual(response, prefix+b' '+r'PrivateŻółw\s😀\nline\\path'.encode())
                else:
                    self.assertTrue(response.startswith(b'ISTA 125 '), response)
                    self.assertIn(b' FC'+prefix.split()[0], response)
            for malformed in (b'BMSG '+peer.sid, b'BMSG '+peer.sid+b' Bad\\q',
                              b'BMSG '+peer.sid+b' hi bad', b'x'*16385):
                peer.send(malformed)
                peer.send(b'BMSG '+peer.sid+b' ValidAfterSyntax')
                self.assertEqual(peer.line(), b'BMSG '+peer.sid+b' ValidAfterSyntax')
            peer.peer.close()
        hub.stop()
        self.assertNotIn(b'Private', hub.log)
        self.assertNotIn(b'ValidAfterSyntax', hub.log)

    def test_invalid_flags_source_and_readiness(self):
        hub, cert = self.hub()
        watcher = self.peer(hub, cert, False);watcher.login(b'Watcher', 99)
        for encrypted in (False, True):
            for suffix in (b' PM', b' PMabc0', b' ME', b' ME2', b' ME0 ME1', b' PMBBBB PMCCCC'):
                peer = self.peer(hub, cert, encrypted);peer.login(b'Bad', 1)
                self.assertTrue(watcher.line().startswith(b'BINF '+peer.sid))
                peer.send(b'BMSG '+peer.sid+b' PrivateBad'+suffix)
                self.assertTrue(peer.line().startswith(b'ISTA 240 '))
                self.assertTrue(peer.line().startswith(b'IQUI '+peer.sid+b' '))
                self.assertEqual(drain_until_eof(peer.peer), b'')
                self.assertEqual(watcher.line(), b'IQUI '+peer.sid)
            spoof = self.peer(hub, cert, encrypted);spoof.login(b'Bad', 1);watcher.line()
            spoof.send(b'BMSG '+watcher.sid+b' SpoofPrivate ME1')
            self.assertTrue(spoof.line().startswith(b'ISTA 240 '));spoof.line()
            self.assertEqual(drain_until_eof(spoof.peer), b'');self.assertEqual(watcher.line(), b'IQUI '+spoof.sid)
            early = self.peer(hub, cert, encrypted);early.handshake()
            early.send(b'BMSG '+early.sid+b' PrematurePrivate')
            self.assertTrue(early.line().startswith(b'ISTA 244 '));early.line()
            self.assertEqual(drain_until_eof(early.peer), b'')
        # No MSG has been relayed to the observer; INF acts as an ordered fence.
        watcher.send(b'BINF '+watcher.sid+b' DEAlive')
        self.assertEqual(watcher.line(), b'BINF '+watcher.sid+b' DEAlive')
        hub.stop()
        self.assertNotIn(b'Private', hub.log)


if __name__ == '__main__':
    unittest.main()
