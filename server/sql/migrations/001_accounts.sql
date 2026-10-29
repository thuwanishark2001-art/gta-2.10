-- Accounts, login sessions and the login audit trail.
-- All DATETIME columns are UTC (MySQL runs with default-time-zone=+00:00).

CREATE TABLE accounts (
    id              INT UNSIGNED     NOT NULL AUTO_INCREMENT,
    username        VARCHAR(24)      NOT NULL,
    password_hash   CHAR(60)         NOT NULL,          -- bcrypt, cost from the gamemode
    admin_level     TINYINT UNSIGNED NOT NULL DEFAULT 0,
    helper_level    TINYINT UNSIGNED NOT NULL DEFAULT 0,
    muted_until     DATETIME         NULL,
    mute_reason     VARCHAR(128)     NULL,
    register_ip     VARCHAR(16)      NOT NULL,
    last_ip         VARCHAR(16)      NULL,
    created_at      DATETIME         NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_login_at   DATETIME         NULL,
    PRIMARY KEY (id),
    UNIQUE KEY uq_accounts_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE account_sessions (
    id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    account_id  INT UNSIGNED    NOT NULL,
    ip          VARCHAR(16)     NOT NULL,
    started_at  DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    ended_at    DATETIME        NULL,
    end_reason  VARCHAR(16)     NULL,                    -- quit, timeout, kick
    PRIMARY KEY (id),
    KEY ix_sessions_account (account_id),
    CONSTRAINT fk_sessions_account FOREIGN KEY (account_id) REFERENCES accounts (id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE login_attempts (
    id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    account_id  INT UNSIGNED    NULL,
    ip          VARCHAR(16)     NOT NULL,
    success     TINYINT(1)      NOT NULL,
    created_at  DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    KEY ix_login_attempts_ip_time (ip, created_at),
    CONSTRAINT fk_login_attempts_account FOREIGN KEY (account_id) REFERENCES accounts (id) ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
