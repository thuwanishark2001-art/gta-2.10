-- Driving licences and traffic tickets.

ALTER TABLE characters
    ADD COLUMN driving_licence TINYINT(1) NOT NULL DEFAULT 0 AFTER phone_number;

-- Fines written by police with /ticket. Paid with /paytickets.
CREATE TABLE tickets (
    id                   INT UNSIGNED NOT NULL AUTO_INCREMENT,
    character_id         INT UNSIGNED NOT NULL,
    officer_character_id INT UNSIGNED NULL,
    amount               INT UNSIGNED NOT NULL,
    reason               VARCHAR(96)  NOT NULL,
    created_at           DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    paid_at              DATETIME     NULL,
    PRIMARY KEY (id),
    KEY ix_tickets_character (character_id, paid_at),
    CONSTRAINT fk_tickets_character FOREIGN KEY (character_id) REFERENCES characters (id) ON DELETE CASCADE,
    CONSTRAINT fk_tickets_officer FOREIGN KEY (officer_character_id) REFERENCES characters (id) ON DELETE SET NULL,
    CONSTRAINT ck_tickets_amount CHECK (amount BETWEEN 1 AND 100000)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
