CREATE TABLE dc24h_audit_events (
    id BIGINT NOT NULL AUTO_INCREMENT,
    occurred_at BIGINT NOT NULL,
    action VARBINARY(32) NOT NULL,
    actor_id BIGINT NOT NULL,
    target_kind VARBINARY(16) NOT NULL,
    target_id BIGINT NOT NULL,
    outcome VARBINARY(16) NOT NULL,
    PRIMARY KEY (id),
    KEY dc24h_audit_events_actor (actor_id, id),
    CONSTRAINT dc24h_audit_events_time CHECK (occurred_at BETWEEN 0 AND 253402300799),
    CONSTRAINT dc24h_audit_events_action CHECK (action IN ('accounts.create','accounts.activate','accounts.disable','accounts.password','accounts.reset','accounts.role','ownership.bootstrap','ownership.transfer','bans.create','bans.revoke')),
    CONSTRAINT dc24h_audit_events_target CHECK (target_kind IN ('account','ban') AND target_id > 0),
    CONSTRAINT dc24h_audit_events_outcome CHECK (outcome = 'succeeded'),
    CONSTRAINT dc24h_audit_events_actor FOREIGN KEY (actor_id) REFERENCES dc24h_accounts(id) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=InnoDB
