-- Bans and the admin action log.

CREATE TABLE bans (
    id                  INT UNSIGNED NOT NULL AUTO_INCREMENT,
    account_id          INT UNSIGNED NULL,
    ip                  VARCHAR(16)  NULL,
    reason              VARCHAR(128) NOT NULL,
    banned_by           INT UNSIGNED NULL,               -- admin account, NULL = system
    created_at          DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    expires_at          DATETIME     NULL,               -- NULL = permanent
    lifted_at           DATETIME     NULL,
    lifted_by           INT UNSIGNED NULL,
    PRIMARY KEY (id),
    KEY ix_bans_account (account_id),
    KEY ix_bans_ip (ip),
    CONSTRAINT fk_bans_account FOREIGN KEY (account_id) REFERENCES accounts (id) ON DELETE CASCADE,
    CONSTRAINT fk_bans_banned_by FOREIGN KEY (banned_by) REFERENCES accounts (id) ON DELETE SET NULL,
    CONSTRAINT fk_bans_lifted_by FOREIGN KEY (lifted_by) REFERENCES accounts (id) ON DELETE SET NULL,
    CONSTRAINT ck_bans_target CHECK (account_id IS NOT NULL OR ip IS NOT NULL)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Every moderation action. Phase 2 extends this to all admin actions.
CREATE TABLE admin_log (
    id                  BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    admin_account_id    INT UNSIGNED    NULL,            -- NULL = system / console
    action              VARCHAR(32)     NOT NULL,
    target_account_id   INT UNSIGNED    NULL,
    details             VARCHAR(255)    NOT NULL DEFAULT '',
    created_at          DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    KEY ix_admin_log_admin (admin_account_id),
    KEY ix_admin_log_target (target_account_id),
    KEY ix_admin_log_time (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
