CREATE PROCEDURE IF NOT EXISTS stage4_confirm_owner(IN aid BIGINT, IN expected BIGINT, IN now_ms BIGINT)
SQL SECURITY DEFINER
MODIFIES SQL DATA
BEGIN
    UPDATE dc24h_accounts a JOIN stage4_emails e ON e.account_id=a.id
       SET a.status='active', a.revision=a.revision+1,
           a.updated_at=GREATEST(a.updated_at,now_ms DIV 1000), e.verified_at=now_ms
     WHERE a.id=aid AND a.name='Administrator' AND a.role='owner' AND a.status='disabled'
       AND a.revision=expected AND e.initial_owner=1 AND e.email='admin@dc24h.eu'
       AND e.verified_at IS NULL;
    IF ROW_COUNT() <> 2 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Owner enrollment refused';
    END IF;
END
@@
CREATE PROCEDURE IF NOT EXISTS stage4_confirm_registration(IN ticket_id BINARY(32), IN now_ms BIGINT)
SQL SECURITY DEFINER
MODIFIES SQL DATA
BEGIN
    DECLARE pending_name VARCHAR(64) CHARACTER SET ascii;
    DECLARE pending_email VARCHAR(254) CHARACTER SET ascii COLLATE ascii_bin;
    DECLARE pending_verifier VARBINARY(192);
    DECLARE created_id BIGINT;
    SELECT r.username,r.email,r.verifier INTO pending_name,pending_email,pending_verifier
      FROM stage4_registrations r JOIN stage4_challenges c ON c.ticket=r.ticket
     WHERE r.ticket=ticket_id AND c.kind='register' AND c.delivered=TRUE AND c.expires_at>now_ms
     FOR UPDATE;
    IF pending_name IS NULL OR EXISTS(SELECT 1 FROM dc24h_accounts WHERE BINARY LOWER(CONVERT(name USING utf8mb4))=BINARY LOWER(pending_name))
       OR EXISTS(SELECT 1 FROM stage4_emails WHERE email=pending_email) THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Registration unavailable';
    END IF;
    INSERT INTO dc24h_accounts(name,role,status,created_at,updated_at)
    VALUES(pending_name,'registered','active',now_ms DIV 1000,now_ms DIV 1000);
    SET created_id=LAST_INSERT_ID();
    INSERT INTO dc24h_account_credentials(account_id,verifier,created_at)
    VALUES(created_id,pending_verifier,now_ms DIV 1000);
    INSERT INTO stage4_emails(account_id,email,verified_at) VALUES(created_id,pending_email,now_ms);
    INSERT INTO stage4_audit(at_time,account_id,event) VALUES(now_ms,created_id,'register');
END
