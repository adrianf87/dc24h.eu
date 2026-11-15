CREATE TABLE dc24h_accounts (
    id BIGINT NOT NULL AUTO_INCREMENT,
    name VARBINARY(64) NOT NULL,
    role VARBINARY(16) NOT NULL DEFAULT 'registered',
    status VARBINARY(16) NOT NULL DEFAULT 'disabled',
    created_at BIGINT NOT NULL,
    updated_at BIGINT NOT NULL,
    last_login_at BIGINT NULL,
    revision BIGINT NOT NULL DEFAULT 1,
    PRIMARY KEY (id),
    UNIQUE KEY dc24h_accounts_name (name),
    CONSTRAINT dc24h_accounts_name_length CHECK (OCTET_LENGTH(name) BETWEEN 1 AND 64),
    CONSTRAINT dc24h_accounts_role CHECK (role IN ('registered','moderator','administrator')),
    CONSTRAINT dc24h_accounts_status CHECK (status IN ('disabled','active')),
    CONSTRAINT dc24h_accounts_times CHECK (
        created_at BETWEEN 0 AND 253402300799 AND
        updated_at BETWEEN created_at AND 253402300799 AND
        (last_login_at IS NULL OR last_login_at BETWEEN created_at AND updated_at)),
    CONSTRAINT dc24h_accounts_revision CHECK (revision > 0)
) ENGINE=InnoDB
