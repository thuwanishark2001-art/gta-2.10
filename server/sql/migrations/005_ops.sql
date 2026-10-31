-- Phase 4: anti-cheat findings.

CREATE TABLE anticheat_log (
    id              BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    account_id      INT UNSIGNED    NULL,
    character_id    INT UNSIGNED    NULL,
    type            VARCHAR(16)     NOT NULL,           -- weapon, jetpack, health, teleport
    details         VARCHAR(128)    NOT NULL DEFAULT '',
    kicked          TINYINT(1)      NOT NULL DEFAULT 0,
    created_at      DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    KEY ix_anticheat_account (account_id, created_at),
    KEY ix_anticheat_type (type, created_at),
    CONSTRAINT fk_anticheat_account FOREIGN KEY (account_id) REFERENCES accounts (id) ON DELETE CASCADE,
    CONSTRAINT fk_anticheat_character FOREIGN KEY (character_id) REFERENCES characters (id) ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
