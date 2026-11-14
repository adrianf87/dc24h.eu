"""Shared aggregate API contract for independent transfer protocol regressions."""
FIELDS = 'received routed rejected rate_limited recipient_limited errors queued refused canceled queue_errors'.split()
NAMES = [f'{kind}_{field}' for kind in ('ctm', 'rcm') for field in FIELDS] + ['peer_protocol_unsupported', 'peer_connection_failed']


def assert_statistics(case, snapshot, **nonzero):
    expected = dict.fromkeys(NAMES, 0)
    expected.update(nonzero)
    case.assertEqual(snapshot['transfers'], {'statistics': expected})
