ALTER TABLE dc24h_accounts
    DROP CONSTRAINT dc24h_accounts_role,
    ADD CONSTRAINT dc24h_accounts_role CHECK (role IN ('registered','vip','moderator','operator','administrator','owner'))
