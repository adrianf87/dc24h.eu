#!/bin/sh
# Certbot executes this fixed hook after renewing the approved lineage.
# It records the validated candidate; the worker owns activation and rollback.
exec /usr/bin/python3 -I /usr/local/libexec/dc24h-tls/tls_renewal.py deploy-hook
