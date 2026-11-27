"""Incremental INF fanout and full-list consistency on actual ADC/ADCS."""
import pathlib
import tempfile
import unittest

from adc_login_process_tests import Peer, pair
from network_process_tests import Hub, drain_until_eof
from tls_support import credentials, connect_tls
from user_list_process_tests import apply


class InfUpdatesProcessTests(unittest.TestCase):
    def test_incremental_changes_cancellations_unknown_fields_and_new_list(self):
        for encrypted in (False, True):
            with self.subTest(encrypted=encrypted), tempfile.TemporaryDirectory(prefix='dc24h-inf-update-') as directory:
                cert, _ = credentials(directory)
                ini = pathlib.Path(directory) / 'hub.ini'
                ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                               '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n')
                hub = Hub(self, '--config', str(ini), '--log-connections', 'on')

                def peer(tls):
                    return Peer(self, connect_tls(self, hub, cert) if tls else hub.connect())

                owner, observer = peer(encrypted), peer(not encrypted)
                owner_initial = owner.login(b'Owner', 1)
                observer_initial = observer.login(b'Observer', 2)
                owner.line()
                view = {}
                for line in owner_initial + observer_initial:
                    apply(view, line)
                update = (' NIŻółw DEZażółć\\sgęślą SS0 SF3 SL2 SUTCP4,ADC0 I40.0.0.0 US4096 EMx@example.test APClient LOpl_PL XXprivate').encode()
                owner.send(b'BINF ' + owner.sid + update)
                delta = owner.line()
                self.assertEqual(observer.line(), delta)
                self.assertEqual(delta, b'BINF ' + owner.sid + update.replace(b'I40.0.0.0', b'I4127.0.0.1').replace(b' XXprivate', b''))
                self.assertNotIn(b' ID', delta)
                apply(view, delta)
                owner.send(b'BINF ' + owner.sid + b' DE EM SU US RF')
                canceled = owner.line()
                self.assertEqual(observer.line(), canceled)
                self.assertEqual(canceled, b'BINF ' + owner.sid + b' DE EM SU US')
                apply(view, canceled)
                # Unknown-only input causes no public output before this known
                # unchanged field is echoed. It cannot leak or reset the profile.
                owner.send(b'BINF ' + owner.sid + b' XXprivate YY')
                owner.send(b'BINF ' + owner.sid + b' SS0')
                repeated = owner.line()
                self.assertEqual(repeated, b'BINF ' + owner.sid + b' SS0')
                self.assertEqual(observer.line(), repeated)
                newcomer = peer(encrypted)
                listing = newcomer.login(b'Newcomer', 3)
                newcomer_view = {}
                for line in listing:
                    apply(newcomer_view, line)
                self.assertEqual(newcomer_view[owner.sid], view[owner.sid])
                owner.line(); observer.line()
                # A bad multi-field update must not be relayed before rejection.
                owner.send(b'BINF ' + owner.sid + b' NIUncommitted DEBad SS-1')
                self.assertTrue(owner.line().startswith(b'ISTA 243 '))
                self.assertEqual(owner.line().split(b' ')[:2], [b'IQUI', owner.sid])
                self.assertEqual(drain_until_eof(owner.peer), b'')
                self.assertEqual(observer.line(), b'IQUI ' + owner.sid)
                self.assertEqual(newcomer.line(), b'IQUI ' + owner.sid)
                replacement = peer(encrypted)
                replacement.login('Żółw'.encode(), 1)
                self.assertIn(b' ID' + pair(1)[1], observer.line())
                newcomer.line()
                hub.stop()
                self.assertEqual(hub.log.count(b'event=login_ready'), 4)
                for private in (b'Uncommitted', b'XXprivate', 'Zażółć'.encode(), pair(1)[0]):
                    self.assertNotIn(private, hub.log)


if __name__ == '__main__':
    unittest.main()
