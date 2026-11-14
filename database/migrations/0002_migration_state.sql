ALTER TABLE dc24h_schema_migrations
    ADD COLUMN IF NOT EXISTS state VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'applied'
