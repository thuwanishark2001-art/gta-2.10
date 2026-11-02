-- Season pass (/pass): points earned by playing, tiers claimed in order.
-- A new season (KM_PASS_SEASON in gamemodes/km/pass.inc) starts everyone at 0.

CREATE TABLE pass_progress (
    character_id INT UNSIGNED     NOT NULL,
    season       SMALLINT UNSIGNED NOT NULL,
    points       INT UNSIGNED     NOT NULL DEFAULT 0,
    claimed_tier TINYINT UNSIGNED NOT NULL DEFAULT 0,
    updated_at   DATETIME         NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (character_id, season),
    CONSTRAINT fk_pass_character FOREIGN KEY (character_id) REFERENCES characters (id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
