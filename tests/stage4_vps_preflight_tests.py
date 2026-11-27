import unittest

from scripts.stage4_vps_preflight import evaluate_listeners, listening_port


class Stage4VpsPreflightTests(unittest.TestCase):
    def test_fixed_expected_tcp_listeners(self):
        output = """State Recv-Q Send-Q Local Address:Port Peer Address:Port
LISTEN 0 511 0.0.0.0:1512 0.0.0.0:*
LISTEN 0 128 127.0.0.1:4278 0.0.0.0:*
LISTEN 0 128 [::1]:7278 [::]:*
"""
        checks = evaluate_listeners(output)
        self.assertEqual(checks["hub_adcs_1512"], "PASS")
        self.assertEqual(checks["no_adc_1511"], "PASS")
        self.assertEqual(checks["node_4278_loopback_only"], "PASS")
        self.assertEqual(checks["apache_7278"], "PRESENT")

    def test_public_node_and_forbidden_adc_are_rejected(self):
        output = """State Recv-Q Send-Q Local Address:Port Peer Address:Port
LISTEN 0 128 0.0.0.0:4278 0.0.0.0:*
LISTEN 0 128 [::]:1511 [::]:*
"""
        checks = evaluate_listeners(output)
        self.assertEqual(checks["node_4278_loopback_only"], "FAIL")
        self.assertEqual(checks["no_adc_1511"], "FAIL")
        self.assertEqual(checks["hub_adcs_1512"], "FAIL")
        self.assertEqual(listening_port(output, 1512), [])


if __name__ == "__main__":
    unittest.main()
