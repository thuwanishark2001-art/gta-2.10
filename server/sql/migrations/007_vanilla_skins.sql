-- Skin 2 (Maccer) only exists with SA-MP's data pack, which the mobile client
-- doesn't ship; players with it showed up as an invisible model. Move them to
-- skin 21, which every copy of GTA: San Andreas has.
UPDATE characters SET skin = 21 WHERE skin = 2;
