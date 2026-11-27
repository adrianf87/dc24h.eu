"""Exact transfer counters, peer reports, mixed transports, restart and privacy."""
import json
import pathlib
import tempfile
import time
import unittest

from api_support import fetch_status
import transfer_limits_process_tests as limits_support
from transfer_statistics_support import NAMES
from network_process_tests import Hub



class TransferStatisticsProcessTests(unittest.TestCase):
    setup_hub = limits_support.TransferLimitsProcessTests.setup_hub
    exchange = limits_support.TransferLimitsProcessTests.exchange

    def wait_statistics(self, endpoint, secret, expected):
        deadline = time.monotonic() + 3
        while True:
            snapshot = fetch_status(endpoint, secret)
            if snapshot['transfers']['statistics'] == expected:
                return snapshot
            self.assertLess(time.monotonic(), deadline, snapshot['transfers'])
            time.sleep(.02)

    def test_exact_counters_privacy_and_restart(self):
        for ipv6 in (False, True):
            with self.subTest(ipv6=ipv6), tempfile.TemporaryDirectory(prefix='dc24h-transfer-stats-') as directory:
                hub, peers, suffix, endpoint, secret = self.setup_hub(directory, ipv6, '')
                a, b, c = peers
                initial = fetch_status(endpoint, secret)
                expected = dict.fromkeys(NAMES, 0)
                self.assertEqual(initial['transfers']['statistics'], expected)
                update = b'BINF ' + b.sid + b' SU'
                self.exchange(peers, b, [update], {p: [update] for p in peers})
                offer = b'ECTM ' + a.sid + b' ' + b.sid + b' ADCS/1.0 01515 StatisticsPrivateToken'
                self.exchange(peers, a, [offer], {a: [offer], b: [offer]})
                bad = offer.replace(b'01515', b'0')
                self.exchange(peers, a, [bad], {a: [b'ISTA 100 Invalid\\sconnection\\srequest FCECTM']})
                missing = offer.replace(b.sid, b'ZZZZ')
                self.exchange(peers, a, [missing], {a: [b'ISTA 140 CTM\\srecipient\\sunavailable FCECTM']})
                reverse = b'DRCM ' + b.sid + b' ' + a.sid + b' ADC/1.0 StatisticsPrivateToken'
                self.exchange(peers, b, [reverse], {a: [reverse]})
                incompatible = b'ERCM ' + a.sid + b' ' + b.sid + b' ADC/1.0 StatisticsPrivateToken'
                self.exchange(peers, a, [incompatible], {a: [b'ISTA 100 RCM\\srequires\\spassive\\ssender\\sand\\sactive\\srecipient FCERCM']})
                for scope, code in ((b'D', b'141'), (b'E', b'242')):
                    report = scope + b'STA ' + b.sid + b' ' + a.sid + b' ' + code + b' StatisticsPrivateFailure PRADCS/1.0 TOStatisticsPrivateToken'
                    self.exchange(peers, b, [report], {a: [report], **({b: [report]} if scope == b'E' else {})})
                missing_report = b'DSTA ' + c.sid + b' ZZZZ 141 Missing PRADC/1.0 TOStatisticsPrivateToken'
                self.exchange(peers, c, [missing_report], {c: [b'ISTA 140 Status\\srecipient\\sunavailable']})
                expected.update(ctm_received=3, ctm_routed=1, ctm_rejected=2, ctm_queued=2,
                                rcm_received=2, rcm_routed=1, rcm_rejected=1, rcm_queued=1,
                                peer_protocol_unsupported=1, peer_connection_failed=1)
                snapshot = self.wait_statistics(endpoint, secret, expected)
                self.assertEqual(snapshot['chat'], initial['chat'])
                self.assertEqual(snapshot['search'], initial['search'])
                for private in ('StatisticsPrivate', 'LimitsUser', secret, directory):
                    self.assertNotIn(private, json.dumps(snapshot))
                b.peer.close()
                for p in (a, c):
                    self.assertEqual(p.line(), b'IQUI ' + b.sid)
                self.wait_statistics(endpoint, secret, expected)
                ini = pathlib.Path(directory) / 'hub.ini'
                hub.stop()
                self.assertNotIn(b'StatisticsPrivate', hub.log)
                self.assertNotIn(secret.encode(), hub.log)
                restarted = Hub(self, '--config', str(ini))
                self.assertEqual(fetch_status(endpoint, secret)['transfers']['statistics'], dict.fromkeys(NAMES, 0))
                restarted.stop()

    def test_silent_source_and_aggregate_target_denials(self):
        for ipv6 in (False, True):
            with self.subTest(ipv6=ipv6), tempfile.TemporaryDirectory(prefix='dc24h-transfer-stat-limits-') as directory:
                settings = 'request_rate=1\nrequest_burst=1\nincoming_rate=1\nincoming_burst=1\n'
                hub, peers, suffix, endpoint, secret = self.setup_hub(directory, ipv6, settings)
                a, b, c = peers
                offer = b'DCTM ' + a.sid + b' ' + b.sid + b' ADC/1.0 1515 PrivateStatsLimit'
                self.exchange(peers, a, [offer] * 4, {b: [offer], a: [b'ISTA 100 Connection\\srequest\\srate\\slimit\\sexceeded FCDCTM']})
                incoming = offer.replace(a.sid, c.sid).replace(b'DCTM', b'ECTM')
                self.exchange(peers, c, [incoming], {c: [b'ISTA 100 Connection\\srequest\\srate\\slimit\\sexceeded FCECTM']})
                expected = dict.fromkeys(NAMES, 0)
                expected.update(ctm_received=5, ctm_routed=1, ctm_rate_limited=3, ctm_recipient_limited=1, ctm_queued=1)
                self.wait_statistics(endpoint, secret, expected)
                hub.stop()


if __name__ == '__main__':
    unittest.main()
