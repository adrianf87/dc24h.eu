CREATE TABLE dc24h_account_identities (
    account_id BIGINT NOT NULL,
    nickname VARBINARY(64) NOT NULL,
    cid VARBINARY(39) NOT NULL,
    created_at BIGINT NOT NULL,
    PRIMARY KEY (account_id),
    UNIQUE KEY dc24h_identities_nickname (nickname),
    UNIQUE KEY dc24h_identities_cid (cid),
    CONSTRAINT dc24h_identities_account FOREIGN KEY (account_id)
        REFERENCES dc24h_accounts(id) ON UPDATE RESTRICT ON DELETE RESTRICT,
    CONSTRAINT dc24h_identities_account_positive CHECK (account_id > 0),
    CONSTRAINT dc24h_identities_nickname_length CHECK (OCTET_LENGTH(nickname) BETWEEN 1 AND 64),
    CONSTRAINT dc24h_identities_cid_shape CHECK (OCTET_LENGTH(cid) = 39 AND cid REGEXP '^[A-Z2-7]{38}[AIQY]$'),
    CONSTRAINT dc24h_identities_time CHECK (created_at BETWEEN 0 AND 253402300799)
) ENGINE=InnoDB
