-- Gun licences (/buylicence at the licensing office, needed at Ammu-Nation).
ALTER TABLE characters
    ADD COLUMN gun_licence TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER driving_licence;

-- When each 24/7 was last robbed (/robstore), so a shop can't be robbed
-- again straight away (kept for staff to check).
ALTER TABLE properties
    ADD COLUMN last_robbed_at DATETIME NULL;
