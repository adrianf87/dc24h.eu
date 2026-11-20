ALTER TABLE dc24h_audit_events
    DROP CONSTRAINT dc24h_audit_events_action,
    DROP CONSTRAINT dc24h_audit_events_target,
    DROP CONSTRAINT dc24h_audit_events_outcome,
    ADD CONSTRAINT dc24h_audit_events_action CHECK (action IN ('accounts.create','accounts.activate','accounts.disable','accounts.password','accounts.reset','accounts.role','ownership.bootstrap','ownership.transfer','bans.create','bans.revoke','configuration.write','configuration.restore')),
    ADD CONSTRAINT dc24h_audit_events_target CHECK ((target_kind IN ('account','ban') AND target_id > 0) OR (target_kind = 'configuration' AND target_id = 1)),
    ADD CONSTRAINT dc24h_audit_events_outcome CHECK ((target_kind = 'configuration' AND action IN ('configuration.write','configuration.restore') AND outcome IN ('started','succeeded','failed')) OR (target_kind IN ('account','ban') AND action NOT IN ('configuration.write','configuration.restore') AND outcome = 'succeeded'))
