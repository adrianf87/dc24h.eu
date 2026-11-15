CREATE TABLE dc24h_account_credentials (
    account_id BIGINT NOT NULL,
    verifier VARBINARY(192) NOT NULL,
    created_at BIGINT NOT NULL,
    PRIMARY KEY (account_id),
    CONSTRAINT dc24h_credentials_account FOREIGN KEY (account_id)
        REFERENCES dc24h_accounts (id) ON DELETE RESTRICT ON UPDATE RESTRICT,
    CONSTRAINT dc24h_credentials_id CHECK (account_id > 0),
    CONSTRAINT dc24h_credentials_verifier CHECK (OCTET_LENGTH(verifier) BETWEEN 1 AND 192),
    CONSTRAINT dc24h_credentials_time CHECK (created_at BETWEEN 0 AND 253402300799)
) ENGINE=InnoDB
