-- Lucky wheel (/spin): one free spin every 20 hours.

ALTER TABLE characters
    ADD COLUMN wheel_last_spin DATETIME NULL AFTER daily_last_claim;
