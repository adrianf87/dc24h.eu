CREATE TABLE dc24h_account_adc_credentials (
    account_id BIGINT NOT NULL,
    secret VARBINARY(1100) NOT NULL,
    created_at BIGINT NOT NULL,
    PRIMARY KEY (account_id),
    CONSTRAINT dc24h_adc_credentials_account FOREIGN KEY (account_id)
        REFERENCES dc24h_accounts(id) ON UPDATE RESTRICT ON DELETE RESTRICT,
    CONSTRAINT dc24h_adc_credentials_length CHECK (OCTET_LENGTH(secret) BETWEEN 38 AND 1061),
    CONSTRAINT dc24h_adc_credentials_time CHECK (created_at BETWEEN 0 AND 253402300799)
) ENGINE=InnoDB
