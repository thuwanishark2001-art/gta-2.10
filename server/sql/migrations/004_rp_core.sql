-- Phase 2: economy, jobs, vehicles, property, inventory, factions, MDT,
-- reports. All DATETIME columns are UTC.

CREATE TABLE factions (
    id          INT UNSIGNED     NOT NULL AUTO_INCREMENT,
    name        VARCHAR(48)      NOT NULL,
    short_name  VARCHAR(8)       NOT NULL,
    type        TINYINT UNSIGNED NOT NULL,          -- 1 police, 2 medical, 3 government
    color       INT UNSIGNED     NOT NULL,          -- 0xRRGGBB (alpha is added in game)
    max_rank    TINYINT UNSIGNED NOT NULL DEFAULT 6,
    hq_x        FLOAT            NOT NULL,          -- /duty point
    hq_y        FLOAT            NOT NULL,
    hq_z        FLOAT            NOT NULL,
    PRIMARY KEY (id),
    UNIQUE KEY uq_factions_short (short_name),
    CONSTRAINT ck_factions_type CHECK (type BETWEEN 1 AND 3),
    CONSTRAINT ck_factions_rank CHECK (max_rank BETWEEN 1 AND 10)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE faction_ranks (
    faction_id  INT UNSIGNED     NOT NULL,
    `rank`      TINYINT UNSIGNED NOT NULL,          -- 1 = lowest, max_rank = leader
    name        VARCHAR(32)      NOT NULL,
    salary      INT UNSIGNED     NOT NULL,          -- paid every payday on duty or not
    PRIMARY KEY (faction_id, `rank`),
    CONSTRAINT fk_faction_ranks_faction FOREIGN KEY (faction_id) REFERENCES factions (id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO factions (id, name, short_name, type, color, max_rank, hq_x, hq_y, hq_z) VALUES
    (1, 'Los Santos Police Department', 'LSPD', 1, 0x3A6FD8, 6, 1553.6, -1675.6, 16.2),
    (2, 'Los Santos Fire & Medical',    'LSFMD', 2, 0xE86A6A, 6, 1172.9, -1323.4, 15.4),
    (3, 'San Andreas Government',       'GOV',  3, 0x9CC77A, 6, 1481.0, -1772.3, 18.8);

INSERT INTO faction_ranks (faction_id, `rank`, name, salary) VALUES
    (1, 1, 'Cadet', 400), (1, 2, 'Officer', 550), (1, 3, 'Senior Officer', 700),
    (1, 4, 'Sergeant', 850), (1, 5, 'Lieutenant', 1000), (1, 6, 'Chief of Police', 1250),
    (2, 1, 'Trainee', 400), (2, 2, 'EMT', 550), (2, 3, 'Paramedic', 700),
    (2, 4, 'Captain', 850), (2, 5, 'Deputy Chief', 1000), (2, 6, 'Fire Chief', 1250),
    (3, 1, 'Clerk', 400), (3, 2, 'Secretary', 550), (3, 3, 'Advisor', 700),
    (3, 4, 'Director', 850), (3, 5, 'Deputy Mayor', 1000), (3, 6, 'Mayor', 1250);

ALTER TABLE characters
    ADD COLUMN bank            INT              NOT NULL DEFAULT 0 AFTER cash,
    ADD COLUMN xp              INT UNSIGNED     NOT NULL DEFAULT 0 AFTER level,
    ADD COLUMN job             TINYINT UNSIGNED NOT NULL DEFAULT 0,          -- see JOB_* in jobs.inc
    ADD COLUMN faction_id      INT UNSIGNED     NULL,
    ADD COLUMN faction_rank    TINYINT UNSIGNED NOT NULL DEFAULT 0,
    ADD COLUMN payday_seconds  INT UNSIGNED     NOT NULL DEFAULT 0,          -- played time towards the next payday
    ADD COLUMN wanted          TINYINT UNSIGNED NOT NULL DEFAULT 0,
    ADD COLUMN jail_seconds    INT UNSIGNED     NOT NULL DEFAULT 0,          -- time left in jail
    ADD COLUMN phone_number    INT UNSIGNED     NULL,
    ADD UNIQUE KEY uq_characters_phone (phone_number),
    ADD KEY ix_characters_faction (faction_id),
    ADD CONSTRAINT fk_characters_faction FOREIGN KEY (faction_id) REFERENCES factions (id) ON DELETE SET NULL,
    ADD CONSTRAINT ck_characters_money CHECK (cash >= 0 AND bank >= 0),
    ADD CONSTRAINT ck_characters_wanted CHECK (wanted <= 6);

-- Every change to a character's cash or bank, with the reason.
CREATE TABLE money_log (
    id              BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    character_id    INT UNSIGNED    NOT NULL,
    cash_delta      INT             NOT NULL,
    bank_delta      INT             NOT NULL,
    cash_after      INT             NOT NULL,
    bank_after      INT             NOT NULL,
    reason          VARCHAR(32)     NOT NULL,
    other_character_id INT UNSIGNED NULL,
    details         VARCHAR(128)    NOT NULL DEFAULT '',
    created_at      DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    KEY ix_money_log_character (character_id, created_at),
    KEY ix_money_log_reason (reason),
    CONSTRAINT fk_money_log_character FOREIGN KEY (character_id) REFERENCES characters (id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Player-owned vehicles. Spawned while the owner is playing. The plate is
-- derived from the id (KM-0001), so it isn't stored.
CREATE TABLE vehicles (
    id              INT UNSIGNED      NOT NULL AUTO_INCREMENT,
    owner_character_id INT UNSIGNED   NOT NULL,
    model           SMALLINT UNSIGNED NOT NULL,
    color1          SMALLINT UNSIGNED NOT NULL DEFAULT 1,
    color2          SMALLINT UNSIGNED NOT NULL DEFAULT 1,
    pos_x           FLOAT             NOT NULL,
    pos_y           FLOAT             NOT NULL,
    pos_z           FLOAT             NOT NULL,
    pos_a           FLOAT             NOT NULL,
    fuel            FLOAT             NOT NULL DEFAULT 100,
    health          FLOAT             NOT NULL DEFAULT 1000,
    locked          TINYINT(1)        NOT NULL DEFAULT 1,
    price           INT UNSIGNED      NOT NULL,                   -- paid at the dealership
    created_at      DATETIME          NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    KEY ix_vehicles_owner (owner_character_id),
    CONSTRAINT fk_vehicles_owner FOREIGN KEY (owner_character_id) REFERENCES characters (id) ON DELETE CASCADE,
    CONSTRAINT ck_vehicles_model CHECK (model BETWEEN 400 AND 611),
    CONSTRAINT ck_vehicles_fuel CHECK (fuel BETWEEN 0 AND 100)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Houses and businesses. Created by admins in game (/createproperty).
CREATE TABLE properties (
    id              INT UNSIGNED     NOT NULL AUTO_INCREMENT,
    type            TINYINT UNSIGNED NOT NULL,                    -- 1 house, 2 business (24/7 shop)
    name            VARCHAR(48)      NOT NULL,
    price           INT UNSIGNED     NOT NULL,
    owner_character_id INT UNSIGNED  NULL,                        -- NULL = for sale
    ent_x           FLOAT            NOT NULL,
    ent_y           FLOAT            NOT NULL,
    ent_z           FLOAT            NOT NULL,
    ent_a           FLOAT            NOT NULL DEFAULT 0,
    interior_preset TINYINT UNSIGNED NOT NULL,                    -- index into gInteriors in property.inc
    locked          TINYINT(1)       NOT NULL DEFAULT 0,
    till            INT UNSIGNED     NOT NULL DEFAULT 0,          -- business takings
    created_at      DATETIME         NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    KEY ix_properties_owner (owner_character_id),
    CONSTRAINT fk_properties_owner FOREIGN KEY (owner_character_id) REFERENCES characters (id) ON DELETE SET NULL,
    CONSTRAINT ck_properties_type CHECK (type IN (1, 2))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Inventory: item ids are defined in inventory.inc.
CREATE TABLE character_items (
    character_id    INT UNSIGNED      NOT NULL,
    item_id         SMALLINT UNSIGNED NOT NULL,
    quantity        INT UNSIGNED      NOT NULL,
    PRIMARY KEY (character_id, item_id),
    CONSTRAINT fk_character_items_character FOREIGN KEY (character_id) REFERENCES characters (id) ON DELETE CASCADE,
    CONSTRAINT ck_character_items_quantity CHECK (quantity > 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- MDT: charges recorded by police officers.
CREATE TABLE criminal_records (
    id              INT UNSIGNED  NOT NULL AUTO_INCREMENT,
    character_id    INT UNSIGNED  NOT NULL,
    officer_character_id INT UNSIGNED NULL,
    charge          VARCHAR(96)   NOT NULL,
    created_at      DATETIME      NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    KEY ix_records_character (character_id, created_at),
    CONSTRAINT fk_records_character FOREIGN KEY (character_id) REFERENCES characters (id) ON DELETE CASCADE,
    CONSTRAINT fk_records_officer FOREIGN KEY (officer_character_id) REFERENCES characters (id) ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Player reports (/report, for admins) and help requests (/helpme, for helpers).
CREATE TABLE reports (
    id              INT UNSIGNED     NOT NULL AUTO_INCREMENT,
    kind            TINYINT UNSIGNED NOT NULL,                    -- 1 report, 2 help request
    reporter_account_id INT UNSIGNED NOT NULL,
    text            VARCHAR(128)     NOT NULL,
    status          TINYINT UNSIGNED NOT NULL DEFAULT 0,          -- 0 open, 1 accepted, 2 closed
    handled_by      INT UNSIGNED     NULL,
    created_at      DATETIME         NOT NULL DEFAULT CURRENT_TIMESTAMP,
    closed_at       DATETIME         NULL,
    PRIMARY KEY (id),
    KEY ix_reports_status (status, created_at),
    CONSTRAINT fk_reports_reporter FOREIGN KEY (reporter_account_id) REFERENCES accounts (id) ON DELETE CASCADE,
    CONSTRAINT fk_reports_handler FOREIGN KEY (handled_by) REFERENCES accounts (id) ON DELETE SET NULL,
    CONSTRAINT ck_reports_kind CHECK (kind IN (1, 2)),
    CONSTRAINT ck_reports_status CHECK (status IN (0, 1, 2))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
