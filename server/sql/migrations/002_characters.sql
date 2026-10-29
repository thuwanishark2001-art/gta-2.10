-- Characters: several per account, named First_Last.

CREATE TABLE characters (
    id              INT UNSIGNED     NOT NULL AUTO_INCREMENT,
    account_id      INT UNSIGNED     NOT NULL,
    name            VARCHAR(24)      NOT NULL,          -- First_Last
    gender          TINYINT UNSIGNED NOT NULL,          -- 0 male, 1 female
    skin            SMALLINT UNSIGNED NOT NULL,
    level           SMALLINT UNSIGNED NOT NULL DEFAULT 1,
    cash            INT              NOT NULL DEFAULT 0,
    health          FLOAT            NOT NULL DEFAULT 100,
    armour          FLOAT            NOT NULL DEFAULT 0,
    pos_x           FLOAT            NOT NULL,
    pos_y           FLOAT            NOT NULL,
    pos_z           FLOAT            NOT NULL,
    pos_a           FLOAT            NOT NULL DEFAULT 0,
    interior        INT UNSIGNED     NOT NULL DEFAULT 0,
    virtual_world   INT UNSIGNED     NOT NULL DEFAULT 0,
    played_seconds  INT UNSIGNED     NOT NULL DEFAULT 0,
    created_at      DATETIME         NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_played_at  DATETIME         NULL,
    PRIMARY KEY (id),
    UNIQUE KEY uq_characters_name (name),
    KEY ix_characters_account (account_id),
    CONSTRAINT fk_characters_account FOREIGN KEY (account_id) REFERENCES accounts (id) ON DELETE CASCADE,
    CONSTRAINT ck_characters_gender CHECK (gender IN (0, 1))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
