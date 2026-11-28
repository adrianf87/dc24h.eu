ALTER TABLE stage4_emails
    DROP CONSTRAINT IF EXISTS CONSTRAINT_1,
    DROP CONSTRAINT IF EXISTS stage4_initial_owner,
    ADD CONSTRAINT stage4_initial_owner CHECK (initial_owner IS NULL OR initial_owner = 1)
@@
CREATE OR REPLACE PROCEDURE stage4_confirm_owner(IN aid BIGINT, IN expected BIGINT, IN now_ms BIGINT)
SQL SECURITY DEFINER
MODIFIES SQL DATA
BEGIN
    UPDATE dc24h_accounts a JOIN stage4_emails e ON e.account_id=a.id
       SET a.status='active', a.revision=a.revision+1,
           a.updated_at=GREATEST(a.updated_at,now_ms DIV 1000), e.verified_at=now_ms
     WHERE a.id=aid AND a.name='Administrator' AND a.role='owner' AND a.status='disabled'
       AND a.revision=expected AND e.initial_owner=1 AND e.verified_at IS NULL;
    IF ROW_COUNT() <> 2 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Owner enrollment refused';
    END IF;
END
