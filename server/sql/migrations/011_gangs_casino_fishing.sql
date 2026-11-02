-- Gangs and turfs, fishing.

ALTER TABLE factions
    DROP CONSTRAINT ck_factions_type,
    ADD CONSTRAINT ck_factions_type CHECK (type BETWEEN 1 AND 4);  -- 4 = gang

-- Two street gangs (faction type 4). They earn from the turfs they hold,
-- not from a salary.
INSERT INTO factions (id, name, short_name, type, color, max_rank, hq_x, hq_y, hq_z) VALUES
    (4, 'Eastside Kings',   'EK',  4, 0x8E44AD, 5, 2495.0, -1687.5, 13.5),
    (5, 'Southside Saints', 'SSS', 4, 0x27AE60, 5, 2068.0, -1731.0, 13.5);

INSERT INTO faction_ranks (faction_id, `rank`, name, salary) VALUES
    (4, 1, 'Prospect', 0), (4, 2, 'Soldier', 0), (4, 3, 'Enforcer', 0), (4, 4, 'Lieutenant', 0), (4, 5, 'Boss', 0),
    (5, 1, 'Prospect', 0), (5, 2, 'Soldier', 0), (5, 3, 'Enforcer', 0), (5, 4, 'Lieutenant', 0), (5, 5, 'Boss', 0);

-- Turfs: rectangles on the map a gang can hold (/capture).
CREATE TABLE turfs (
    id               TINYINT UNSIGNED NOT NULL,
    name             VARCHAR(32)      NOT NULL,
    min_x            FLOAT            NOT NULL,
    min_y            FLOAT            NOT NULL,
    max_x            FLOAT            NOT NULL,
    max_y            FLOAT            NOT NULL,
    owner_faction_id INT UNSIGNED     NULL,
    captured_at      DATETIME         NULL,
    PRIMARY KEY (id),
    CONSTRAINT fk_turfs_owner FOREIGN KEY (owner_faction_id) REFERENCES factions (id) ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO turfs (id, name, min_x, min_y, max_x, max_y, owner_faction_id) VALUES
    (1, 'Ganton',          2222.0, -1852.0, 2632.0, -1628.0, 4),
    (2, 'Idlewood',        1812.0, -1852.0, 2222.0, -1628.0, 5),
    (3, 'Willowfield',     2222.0, -2100.0, 2632.0, -1852.0, NULL),
    (4, 'East Los Santos', 2280.0, -1400.0, 2632.0, -1150.0, NULL),
    (5, 'Jefferson',       1990.0, -1300.0, 2280.0, -1100.0, NULL),
    (6, 'Glen Park',       1812.0, -1300.0, 1990.0, -1100.0, NULL);

-- Fish caught (/fish) and what they'll sell for (/sellfish).
ALTER TABLE characters
    ADD COLUMN fish_count SMALLINT UNSIGNED NOT NULL DEFAULT 0 AFTER wheel_last_spin,
    ADD COLUMN fish_value INT UNSIGNED      NOT NULL DEFAULT 0 AFTER fish_count;
