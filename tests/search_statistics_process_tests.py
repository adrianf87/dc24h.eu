"""Authenticated effective settings, exact search counters, cleanup and privacy."""
import json
import pathlib
import tempfile
import time
import unittest

from adc_login_process_tests import Peer
from api_support import enable_api, fetch_status
from network_process_tests import Hub
from tls_support import credentials, connect_tls

NAMES = ('queries_received queries_routed queries_rejected queries_rate_limited queries_ignored query_errors '
         'query_queued query_unavailable query_refused query_canceled results_received results_routed results_rejected '
         'results_rate_limited results_recipient_limited result_errors result_queued result_unavailable result_refused tracked_searches').split()


class SearchStatisticsProcessTests(unittest.TestCase):
    def test_settings_counters_privacy_and_restart(self):
        for encrypted in (False, True):
            with self.subTest(encrypted=encrypted), tempfile.TemporaryDirectory(prefix='dc24h-search-stats-') as directory:
                cert, _ = credentials(directory)
                ini=pathlib.Path(directory)/'hub.ini'
                ini.write_text('[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n'
                               '[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n'
                               '[search]\nquery_rate=100\nquery_burst=50\nmax_active_searches=2\nmax_active_per_user=1\nrequire_result_token=on\n')
                endpoint, secret=enable_api(ini,directory)
                hub=Hub(self,'--config',str(ini),'--log-connections','on','--log-chat','on')
                initial=fetch_status(endpoint,secret)['search']
                self.assertEqual(set(initial['statistics']),set(NAMES))
                self.assertTrue(all(value==0 for value in initial['statistics'].values()))
                expected_config={'queries_enabled':True,'results_enabled':True,'guest_queries':True,'guest_results':True,
                                 'require_result_token':True,'max_active_searches':2,'max_active_per_user':1,'search_lifetime_seconds':60,
                                 'query_rate':100,'query_burst':50,'query_byte_rate':32768,'query_byte_burst':131072,
                                 'result_rate':64,'result_burst':128,'result_byte_rate':262144,'result_byte_burst':1048576}
                self.assertEqual(initial['configuration'],expected_config)
                source=Peer(self,connect_tls(self,hub,cert) if encrypted else hub.connect());source.login(b'StatsSource',1)
                target=Peer(self,hub.connect());target.login(b'StatsTarget',2)
                self.assertTrue(source.line().startswith(b'BINF '+target.sid+b' '))
                chat_before=fetch_status(endpoint,secret)['chat']
                query=b'DSCH '+source.sid+b' '+target.sid+b' ANStatsPrivate TOStatsPrivateToken'
                source.send(query);self.assertEqual(target.line(),query)
                result=b'DRES '+target.sid+b' '+source.sid+b' FN/StatsPrivate SI0 TR'+b'A'*39+b' TOStatsPrivateToken'
                target.send(result);self.assertEqual(source.line(),result)
                source.send(query);self.assertTrue(source.line().startswith(b'ISTA 100 Search\\stoken'))
                source.send(b'BSCH '+source.sid+b' TOignored')
                fence=b'BSTA '+source.sid+b' 000 StatsFence';source.send(fence)
                for peer in (source,target):self.assertEqual(peer.line(),fence)
                target.send(b'DRES '+target.sid+b' '+source.sid+b' FNbad SI-1 TOStatsPrivateToken')
                self.assertEqual(target.line(),b'ISTA 100 Invalid\\ssearch\\sresult FCDRES')
                expected={name:0 for name in NAMES}
                expected.update(queries_received=3,queries_routed=1,queries_rejected=1,queries_ignored=1,query_queued=1,
                                results_received=2,results_routed=1,results_rejected=1,result_queued=1,tracked_searches=1)
                deadline=time.monotonic()+3
                while True:
                    snapshot=fetch_status(endpoint,secret)
                    if snapshot['search']['statistics']==expected:break
                    self.assertLess(time.monotonic(),deadline,snapshot['search']);time.sleep(.02)
                self.assertEqual(snapshot['search']['configuration'],expected_config)
                self.assertEqual(snapshot['chat'],chat_before)
                text=json.dumps(snapshot)
                for private in ('StatsPrivate','StatsSource','StatsTarget',secret,str(ini)):self.assertNotIn(private,text)
                source.peer.close();self.assertEqual(target.line(),b'IQUI '+source.sid)
                deadline=time.monotonic()+3
                while fetch_status(endpoint,secret)['search']['statistics']['tracked_searches']:
                    self.assertLess(time.monotonic(),deadline);time.sleep(.02)
                expected['tracked_searches']=0
                self.assertEqual(fetch_status(endpoint,secret)['search']['statistics'],expected)
                hub.stop()
                self.assertNotIn(b'StatsPrivate',hub.log);self.assertNotIn(secret.encode(),hub.log)
                restarted=Hub(self,'--config',str(ini))
                fresh=fetch_status(endpoint,secret)['search']
                self.assertTrue(all(value==0 for value in fresh['statistics'].values()))
                self.assertEqual(fresh['configuration'],expected_config);restarted.stop()


if __name__=='__main__':unittest.main()
