"""Convergent user lists across concurrent joins, updates, quits and reconnect."""
import pathlib
import tempfile
import unittest

from adc_login_process_tests import Peer, pair
from network_process_tests import Hub
from tls_support import credentials, connect_tls


def apply(view, line):
    fields = line.split(b' ')
    if fields[0] == b'IQUI':
        view.pop(fields[1], None)
    elif fields[0] == b'BINF':
        current = view.setdefault(fields[1], {})
        for token in fields[2:]:
            if len(token) == 2:
                current.pop(token, None)
            else:
                current[token[:2]] = token[2:]


class UserListProcessTests(unittest.TestCase):
    def test_concurrent_join_update_quit_and_reconnect_converge(self):
        for encrypted in (False, True):
            with self.subTest(encrypted=encrypted), tempfile.TemporaryDirectory(prefix='dc24h-user-list-') as directory:
                cert, _ = credentials(directory)
                ini = pathlib.Path(directory) / 'hub.ini'
                ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                               '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n')
                hub = Hub(self, '--config', str(ini), '--log-connections', 'on')

                def peer(tls):
                    return Peer(self, connect_tls(self, hub, cert) if tls else hub.connect())

                watcher = peer(encrypted)
                watcher_initial = watcher.login(b'Watcher', 1)
                first, second = peer(not encrypted), peer(encrypted)
                first.handshake(); second.handshake()
                # Neither client's welcome has been consumed before both identities.
                first.send(first.identity('Żółw'.encode(), 2))
                second.send(second.identity(b'Second', 3))
                views = [{}, {}, {}]
                for line in watcher_initial:
                    apply(views[0], line)
                for client, view in zip((first, second), views[1:]):
                    self.assertTrue(client.line().startswith(b'IINF '))
                    own_count = 0
                    while not own_count:
                        line = client.line(); apply(view, line)
                        self.assertTrue(line.startswith(b'BINF '), line)
                        self.assertNotIn(b' PD', line)
                        self.assertNotIn(b' XX', line)
                        own_count += line.startswith(b'BINF ' + client.sid + b' ')
                    self.assertTrue(client.line().startswith(b'IMSG Welcome\\sto\\sdc24h.eu-twilight.'))
                for _ in range(2):
                    apply(views[0], watcher.line())
                peers = (watcher, first, second)

                def fence(marker):
                    watcher.send(b'BINF ' + watcher.sid + b' DE' + marker)
                    for client, view in zip(peers, views):
                        for _ in range(20):
                            line = client.line(); apply(view, line)
                            if line.startswith(b'BINF ' + watcher.sid + b' ') and b' DE' + marker in line:
                                break
                        else:
                            self.fail('Synchronization fence not reached')
                    self.assertEqual(views[0], views[1])
                    self.assertEqual(views[1], views[2])

                fence(b'FirstFence')
                self.assertEqual(set(views[0]), {client.sid for client in peers})
                first.send(b'BINF ' + first.sid + b' NI' + 'Łoś'.encode() + b' DE')
                for client, view in zip(peers, views):
                    update = client.line(); apply(view, update)
                    self.assertIn(b' NI' + 'Łoś'.encode(), update)
                newcomer = peer(not encrypted)
                initial = newcomer.login(b'Newcomer', 4)
                initial_view = {}
                for line in initial:
                    apply(initial_view, line)
                self.assertEqual(initial_view[first.sid][b'NI'], 'Łoś'.encode())
                self.assertNotIn(b'DE', initial_view[first.sid])
                for client, view in zip(peers, views):
                    apply(view, client.line())
                self.assertEqual(initial_view, views[0])
                old_sid = second.sid
                second.peer.close()
                for client, view in ((watcher, views[0]), (first, views[1]), (newcomer, initial_view)):
                    line = client.line(); self.assertEqual(line, b'IQUI ' + old_sid); apply(view, line)
                replacement = peer(encrypted)
                listing = replacement.login(b'Second', 3)
                replacement_view = {}
                for line in listing:
                    apply(replacement_view, line)
                self.assertNotEqual(replacement.sid, old_sid)
                self.assertNotIn(old_sid, replacement_view)
                for client, view in ((watcher, views[0]), (first, views[1]), (newcomer, initial_view)):
                    apply(view, client.line()); self.assertEqual(view, replacement_view)
                self.assertEqual(sum(fields.get(b'ID') == pair(3)[1] for fields in replacement_view.values()), 1)
                hub.stop()
                self.assertEqual(hub.log.count(b'event=login_ready'), 5)
                for secret in (b'FirstFence', pair(3)[0], 'Łoś'.encode()):
                    self.assertNotIn(secret, hub.log)


if __name__ == '__main__':
    unittest.main()
