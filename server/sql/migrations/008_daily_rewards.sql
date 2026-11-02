-- Daily rewards (/daily): a 7-day streak that resets if a day is missed.

ALTER TABLE characters
    ADD COLUMN daily_streak     TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER driving_licence,
    ADD COLUMN daily_last_claim DATETIME         NULL     AFTER daily_streak;
