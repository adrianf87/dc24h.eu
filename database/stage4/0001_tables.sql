CREATE TABLE IF NOT EXISTS stage4_emails (
    account_id BIGINT NOT NULL PRIMARY KEY,
    email VARCHAR(254) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    initial_owner TINYINT NULL UNIQUE,
    verified_at BIGINT NULL,
    FOREIGN KEY (account_id) REFERENCES dc24h_accounts(id),
    CHECK (initial_owner IS NULL OR (initial_owner = 1 AND email = 'admin@dc24h.eu'))
) ENGINE=InnoDB
@@
CREATE TABLE IF NOT EXISTS stage4_challenges (
    ticket BINARY(32) NOT NULL PRIMARY KEY,
    kind ENUM('login','owner','register') NOT NULL,
    account_id BIGINT NULL,
    revision BIGINT NULL,
    code_mac BINARY(32) NOT NULL,
    created_at BIGINT NOT NULL,
    expires_at BIGINT NOT NULL,
    attempts TINYINT NOT NULL DEFAULT 0,
    delivered BOOLEAN NOT NULL DEFAULT FALSE,
    KEY (expires_at),
    FOREIGN KEY (account_id) REFERENCES dc24h_accounts(id),
    CHECK (attempts <= 5 AND expires_at > created_at AND expires_at - created_at <= 300000),
    CHECK ((kind = 'register' AND account_id IS NULL AND revision IS NULL)
        OR (kind <> 'register' AND account_id IS NOT NULL AND revision IS NOT NULL))
) ENGINE=InnoDB
@@
CREATE TABLE IF NOT EXISTS stage4_registrations (
    ticket BINARY(32) NOT NULL PRIMARY KEY,
    username VARCHAR(64) CHARACTER SET ascii COLLATE ascii_general_ci NOT NULL UNIQUE,
    email VARCHAR(254) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
    verifier VARBINARY(192) NOT NULL,
    FOREIGN KEY (ticket) REFERENCES stage4_challenges(ticket) ON DELETE CASCADE,
    CHECK (email <> 'admin@dc24h.eu' AND LOWER(username) <> 'administrator')
) ENGINE=InnoDB
@@
CREATE TABLE IF NOT EXISTS stage4_attempts (
    bucket BINARY(32) NOT NULL PRIMARY KEY,
    expires_at BIGINT NOT NULL,
    hits INT UNSIGNED NOT NULL,
    KEY (expires_at)
) ENGINE=InnoDB
@@
CREATE TABLE IF NOT EXISTS stage4_sessions (
    token BINARY(32) NOT NULL PRIMARY KEY,
    account_id BIGINT NOT NULL,
    revision BIGINT NOT NULL,
    issued_at BIGINT NOT NULL,
    seen_at BIGINT NOT NULL,
    expires_at BIGINT NOT NULL,
    KEY (account_id),
    KEY (expires_at),
    FOREIGN KEY (account_id) REFERENCES dc24h_accounts(id)
) ENGINE=InnoDB
@@
CREATE TABLE IF NOT EXISTS stage4_audit (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    at_time BIGINT NOT NULL,
    account_id BIGINT NULL,
    event ENUM('register','verify_email','login','logout','revoke_sessions','password_change','recovery') NOT NULL,
    FOREIGN KEY (account_id) REFERENCES dc24h_accounts(id),
    KEY (account_id,at_time),
    KEY (at_time)
) ENGINE=InnoDB
@@
CREATE TABLE IF NOT EXISTS stage4_recovery (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    account_id BIGINT NOT NULL,
    opened_at BIGINT NOT NULL,
    closed_at BIGINT NULL,
    result ENUM('restored','denied') NULL,
    FOREIGN KEY (account_id) REFERENCES dc24h_accounts(id),
    CHECK ((closed_at IS NULL AND result IS NULL) OR (closed_at >= opened_at AND result IS NOT NULL))
) ENGINE=InnoDB
