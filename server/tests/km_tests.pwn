// Kingmakers RP server tests. Runs as a gamemode (KM_SCRIPT=km_tests):
// unit tests for the pure modules, then integration tests against the
// migrated MySQL schema and the bcrypt plugin. Prints one summary line,
// "KM-TESTS DONE pass=N fail=N", and shuts the server down.
// Run with: server/scripts/test.sh

#define MAX_PLAYERS (100)

#include <open.mp>
#include <a_mysql>
#include <samp_bcrypt>
#include <sscanf2>

#pragma dynamic 16384

// The whole gamemode minus its callbacks, so the tests use the real code.
#include "km/config.inc"
#include "km/ids.inc"
#include "km/proto.inc"
#include "km/antispam.inc"
#include "km/players.inc"
#include "km/util.inc"
#include "km/achooks.inc"
#include "km/validate.inc"
#include "km/rpcalc.inc"
#include "km/places.inc"
#include "km/db.inc"
#include "km/cmd.inc"
#include "km/accounts.inc"
#include "km/characters.inc"
#include "km/moderation.inc"
#include "km/chat.inc"
#include "km/economy.inc"
#include "km/offers.inc"
#include "km/vehicles.inc"
#include "km/jobs.inc"
#include "km/property.inc"
#include "km/inventory.inc"
#include "km/factions.inc"
#include "km/support.inc"
#include "km/kmui.inc"
#include "km/anticheat.inc"
#include "km/licence.inc"
#include "km/radio.inc"
#include "km/content.inc"
#include "km/mapicons.inc"
#include "km/admintools.inc"
#include "km/spectate.inc"
#include "km/gps.inc"
#include "km/guide.inc"
#include "km/owner.inc"

new gPass, gFail;

stock Expect(bool:ok, const name[], line = 0)
{
    if (ok)
    {
        gPass++;
    }
    else
    {
        gFail++;
        printf("KM-TEST FAIL km_tests.pwn:%d %s", line, name);
    }
}

// Failures are reported with their line in this file.
#define EXPECT(%0) Expect(bool:(%0), "", __line)

main() {}

public OnGameModeInit()
{
    Test_Validation();
    Test_AntiSpam();
    Test_Commands();
    Test_Util();
    Test_Rules();
    Test_Kmui();
    Test_AntiCheat();

    if (!DB_Connect())
    {
        Expect(false, "database connection");
        Test_Finish();
        return 1;
    }
    Test_Schema();
    Test_Phase2Schema();
    Test_SharedQueries();
    // Asynchronous checks run next, then bcrypt; the chain ends in Test_Finish.
    Test_StartAsync();
    return 1;
}

Test_Validation()
{
    EXPECT(IsValidAccountName("Marcus"));
    EXPECT(IsValidAccountName("king_maker_01"));
    EXPECT(!IsValidAccountName("ab"));
    EXPECT(!IsValidAccountName("waytoolongaccountname1"));
    EXPECT(!IsValidAccountName("bad name"));
    EXPECT(!IsValidAccountName("bad[tag]"));

    EXPECT(Radio_IsValidUrl("http://stream.radioparadise.com/mp3-128"));
    EXPECT(Radio_IsValidUrl("https://ice1.somafm.com/groovesalad-128-mp3"));
    EXPECT(!Radio_IsValidUrl("ftp://example.com/stream"));
    EXPECT(Content_IsValidSoundPath("sounds/welcome.mp3"));
    EXPECT(Content_IsValidSoundPath("sounds/Bell_2.ogg"));
    EXPECT(Content_IsValidSoundPath("horn.wav"));
    EXPECT(!Content_IsValidSoundPath("../secret.mp3"));
    EXPECT(!Content_IsValidSoundPath("sounds/../../x.mp3"));
    EXPECT(!Content_IsValidSoundPath("/etc/x.mp3"));
    EXPECT(!Content_IsValidSoundPath("sounds//x.mp3"));
    EXPECT(!Content_IsValidSoundPath("sounds/x y.mp3"));
    EXPECT(!Content_IsValidSoundPath("manifest.json"));
    EXPECT(!Content_IsValidSoundPath("sounds/x.mp3?a=1"));
    EXPECT(!Radio_IsValidUrl("http://bad url"));
    EXPECT(!Radio_IsValidUrl("http://example.com/a\"b"));
    EXPECT(!Radio_IsValidUrl("http://x"));
    EXPECT(Radio_StationCount() > 0);
    EXPECT(Radio_IsValidStation(0));
    EXPECT(!Radio_IsValidStation(RADIO_OFF));
    EXPECT(!Radio_IsValidStation(Radio_StationCount()));

    EXPECT(IsStockSkin(0));
    EXPECT(IsStockSkin(7));
    EXPECT(IsStockSkin(21));
    EXPECT(IsStockSkin(280));
    EXPECT(!IsStockSkin(2));
    EXPECT(!IsStockSkin(74));
    EXPECT(!IsStockSkin(306));
    EXPECT(!IsStockSkin(-1));
    EXPECT(!IsStockSkin(312));

    EXPECT(IsValidCharacterName("Marcus_Reed"));
    EXPECT(IsValidCharacterName("Al_Li"));
    EXPECT(!IsValidCharacterName("marcus_Reed"));
    EXPECT(!IsValidCharacterName("Marcus_reed"));
    EXPECT(!IsValidCharacterName("MarcusReed"));
    EXPECT(!IsValidCharacterName("Marcus_Reed_Jr"));
    EXPECT(!IsValidCharacterName("M_Reed"));
    EXPECT(!IsValidCharacterName("Marcus_R"));
    EXPECT(!IsValidCharacterName("MArcus_Reed"));
    EXPECT(!IsValidCharacterName("Marcus1_Reed"));
    EXPECT(!IsValidCharacterName("Abcdefghijkl_Reed"));
    EXPECT(!IsValidCharacterName("_Reed"));
    EXPECT(!IsValidCharacterName("Marcus_"));

    EXPECT(CheckPassword("hunter22", "Marcus") == PASSWORD_OK);
    EXPECT(CheckPassword("short", "Marcus") == PASSWORD_TOO_SHORT);
    EXPECT(CheckPassword("marcus", "Marcus") == PASSWORD_SAME_AS_NAME);
    EXPECT(CheckPassword("tab\tinside", "Marcus") == PASSWORD_BAD_CHAR);
    EXPECT(CheckPassword("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "Marcus") == PASSWORD_TOO_LONG);
}

Test_AntiSpam()
{
    new s[E_SPAM];
    Spam_Reset(s);
    // 3 per 3000 ms allowed
    EXPECT(Spam_Check(s, 1000, "a", 3000, 3, 5000) == SPAM_OK);
    EXPECT(Spam_Check(s, 1100, "b", 3000, 3, 5000) == SPAM_OK);
    EXPECT(Spam_Check(s, 1200, "c", 3000, 3, 5000) == SPAM_OK);
    EXPECT(Spam_Check(s, 1300, "d", 3000, 3, 5000) == SPAM_FLOOD);
    // new window
    EXPECT(Spam_Check(s, 4500, "e", 3000, 3, 5000) == SPAM_OK);
    // same text again, case-insensitive, inside the repeat window
    EXPECT(Spam_Check(s, 5000, "E", 3000, 3, 5000) == SPAM_REPEAT);
    // same text after the repeat window is fine
    EXPECT(Spam_Check(s, 12000, "e", 3000, 3, 5000) == SPAM_OK);
    // repeat check off (repeatMs = 0), as used for commands
    Spam_Reset(s);
    EXPECT(Spam_Check(s, 100, "/stats", 3000, 5, 0) == SPAM_OK);
    EXPECT(Spam_Check(s, 200, "/stats", 3000, 5, 0) == SPAM_OK);
}

Test_Commands()
{
    new name[28], params[128];
    EXPECT(Cmd_Parse("/Kick 5 bad words", name, sizeof (name), params, sizeof (params)));
    EXPECT(strcmp(name, "kick") == 0);
    EXPECT(strcmp(params, "5 bad words") == 0);

    EXPECT(Cmd_Parse("/help", name, sizeof (name), params, sizeof (params)));
    EXPECT(strcmp(name, "help") == 0 && isnull(params));

    EXPECT(Cmd_Parse("/stats    ", name, sizeof (name), params, sizeof (params)) && isnull(params));
    EXPECT(!Cmd_Parse("hello", name, sizeof (name), params, sizeof (params)));
    EXPECT(!Cmd_Parse("/", name, sizeof (name), params, sizeof (params)));
    EXPECT(!Cmd_Parse("/k!ck 1", name, sizeof (name), params, sizeof (params)));
    EXPECT(!Cmd_Parse("/averyveryveryverylongcommandname", name, sizeof (name), params, sizeof (params)));

    new id, minutes, reason[64];
    EXPECT(!sscanf("12 30 Spamming chat", "dds[64]", id, minutes, reason) && id == 12 && minutes == 30 && strcmp(reason, "Spamming chat") == 0);
    EXPECT(sscanf("\1", "dds[64]", id, minutes, reason) != 0);
}

Test_Util()
{
    new s[24];
    FormatDuration(s, 45);    EXPECT(strcmp(s, "45s") == 0);
    FormatDuration(s, 750);   EXPECT(strcmp(s, "12m 30s") == 0);
    FormatDuration(s, 3900);  EXPECT(strcmp(s, "1h 05m") == 0);
    FormatDuration(s, -5);    EXPECT(strcmp(s, "0s") == 0);
    EXPECT(StrHashCI("Hello") == StrHashCI("hELLO"));
    EXPECT(StrHashCI("Hello") != StrHashCI("Hellp"));
}

// Runs a statement and returns its error number (0 = success).
DB_Exec(const query[])
{
    new Cache:c = mysql_query(gDB, query, false);
    if (c) cache_delete(c);
    return mysql_errno(gDB);
}

DB_Int(const query[])
{
    new value = -1;
    new Cache:c = mysql_query(gDB, query);
    if (cache_num_rows() > 0) cache_get_value_index_int(0, 0, value);
    cache_delete(c);
    return value;
}

Test_Schema()
{
    // Clean slate for the test accounts.
    DB_Exec("DELETE FROM bans WHERE reason LIKE 'test:%'");
    DB_Exec("DELETE FROM accounts WHERE username LIKE 'kmtest_%'");

    EXPECT(DB_Int("SELECT COUNT(*) FROM schema_migrations") >= 4);

    EXPECT(DB_Exec("INSERT INTO accounts (username, password_hash, register_ip) VALUES ('kmtest_a', REPEAT('x', 60), '127.0.0.1')") == 0);
    new accountId = DB_Int("SELECT id FROM accounts WHERE username = 'kmtest_a'");
    EXPECT(accountId > 0);

    // usernames are unique, case-insensitively
    EXPECT(DB_Exec("INSERT INTO accounts (username, password_hash, register_ip) VALUES ('KMTEST_A', REPEAT('x', 60), '127.0.0.1')") == 1062);
    // the gamemode registers with INSERT IGNORE and treats 0 rows as "taken"
    EXPECT(DB_Exec("INSERT IGNORE INTO accounts (username, password_hash, register_ip) VALUES ('kmtest_a', REPEAT('x', 60), '127.0.0.1')") == 0);

    new q[640];
    // the gamemode's capped character insert: three succeed, the fourth inserts nothing
    for (new i = 0; i < 4; i++)
    {
        format(q, sizeof (q),
            "INSERT IGNORE INTO characters (account_id, name, gender, skin, cash, pos_x, pos_y, pos_z, pos_a) \
             SELECT %d, 'Test_Char%c', 0, 2, 500, 0, 0, 0, 0 FROM DUAL \
             WHERE (SELECT COUNT(*) FROM characters WHERE account_id = %d) < %d",
            accountId, 'a' + i, accountId, KM_MAX_CHARACTERS);
        DB_Exec(q);
    }
    format(q, sizeof (q), "SELECT COUNT(*) FROM characters WHERE account_id = %d", accountId);
    EXPECT(DB_Int(q) == KM_MAX_CHARACTERS);

    // character names are globally unique
    format(q, sizeof (q), "INSERT INTO characters (account_id, name, gender, skin, pos_x, pos_y, pos_z) VALUES (%d, 'Test_Chara', 0, 2, 0, 0, 0)", accountId);
    EXPECT(DB_Exec(q) == 1062);
    // gender is constrained
    format(q, sizeof (q), "INSERT INTO characters (account_id, name, gender, skin, pos_x, pos_y, pos_z) VALUES (%d, 'Test_Gender', 7, 2, 0, 0, 0)", accountId);
    EXPECT(DB_Exec(q) != 0);

    // ban lookup: same filter as Acc_OnConnect
    format(q, sizeof (q), "INSERT INTO bans (account_id, ip, reason, expires_at) VALUES (%d, NULL, 'test:expired', UTC_TIMESTAMP() - INTERVAL 1 DAY)", accountId);
    DB_Exec(q);
    EXPECT(Test_ActiveBans("10.9.9.9", "kmtest_a") == 0);

    format(q, sizeof (q), "INSERT INTO bans (account_id, ip, reason, expires_at) VALUES (%d, NULL, 'test:active', UTC_TIMESTAMP() + INTERVAL 1 DAY)", accountId);
    DB_Exec(q);
    EXPECT(Test_ActiveBans("10.9.9.9", "kmtest_a") == 1);
    EXPECT(Test_ActiveBans("10.9.9.9", "kmtest_nobody") == 0);

    DB_Exec("INSERT INTO bans (ip, reason) VALUES ('10.1.2.3', 'test:ipban')");
    EXPECT(Test_ActiveBans("10.1.2.3", "kmtest_nobody") == 1);

    DB_Exec("UPDATE bans SET lifted_at = UTC_TIMESTAMP() WHERE reason LIKE 'test:%'");
    EXPECT(Test_ActiveBans("10.1.2.3", "kmtest_a") == 0);

    // a ban must target something
    EXPECT(DB_Exec("INSERT INTO bans (reason) VALUES ('test:nothing')") != 0);

    // mute expiry round-trips as unix time in UTC
    format(q, sizeof (q), "UPDATE accounts SET muted_until = UTC_TIMESTAMP() + INTERVAL 10 MINUTE WHERE id = %d", accountId);
    DB_Exec(q);
    format(q, sizeof (q), "SELECT UNIX_TIMESTAMP(muted_until) FROM accounts WHERE id = %d", accountId);
    new mutedUntil = DB_Int(q), expected = gettime() + 600;
    EXPECT(mutedUntil >= expected - 5 && mutedUntil <= expected + 5);

    // admin_log accepts system actions (NULL admin)
    EXPECT(DB_Exec("INSERT INTO admin_log (admin_account_id, action, details) VALUES (NULL, 'test', 'from km_tests')") == 0);
    DB_Exec("DELETE FROM admin_log WHERE action = 'test'");

    // deleting an account removes its characters and sessions
    format(q, sizeof (q), "INSERT INTO account_sessions (account_id, ip) VALUES (%d, '127.0.0.1')", accountId);
    DB_Exec(q);
    format(q, sizeof (q), "DELETE FROM accounts WHERE id = %d", accountId);
    DB_Exec(q);
    format(q, sizeof (q), "SELECT (SELECT COUNT(*) FROM characters WHERE account_id = %d) + (SELECT COUNT(*) FROM account_sessions WHERE account_id = %d)", accountId, accountId);
    EXPECT(DB_Int(q) == 0);
}

Test_Rules()
{
    EXPECT(Clamp(5, 1, 3) == 3 && Clamp(-2, 0, 9) == 0 && Clamp(4, 1, 9) == 4);

    EXPECT(Level_XpNeeded(1) == 4);
    EXPECT(Level_XpNeeded(0) == 4);
    EXPECT(Level_XpNeeded(5) == 20);

    EXPECT(Payday_Interest(0) == 0);
    EXPECT(Payday_Interest(-50) == 0);
    EXPECT(Payday_Interest(999) == 0);
    EXPECT(Payday_Interest(1000) == 1);
    EXPECT(Payday_Interest(250000) == 250);
    EXPECT(Payday_Interest(10000000) == KM_INTEREST_MAX);

    EXPECT(Job_DistancePay(0.0, 100, 250, 2000) == 100);
    EXPECT(Job_DistancePay(4000.0, 100, 250, 2000) == 1100);
    EXPECT(Job_DistancePay(100000.0, 100, 250, 2000) == 2000);
    EXPECT(Job_DistancePay(-30.0, 60, 300, 400) == 60);

    EXPECT(floatabs(Fuel_Use(0.0) - 0.05) < 0.0001);
    EXPECT(floatabs(Fuel_Use(100.0) - 0.25) < 0.0001);
    EXPECT(floatabs(Fuel_Use(-10.0) - 0.05) < 0.0001);

    new litres;
    EXPECT(Fuel_RefillCost(100.0, litres) == 0 && litres == 0);
    EXPECT(Fuel_RefillCost(37.5, litres) == 62 * KM_FUEL_PRICE && litres == 62);
    EXPECT(Fuel_RefillCost(0.0, litres) == 100 * KM_FUEL_PRICE && litres == 100);

    new plate[12];
    Plate_Format(plate, 1);     EXPECT(strcmp(plate, "KM-0001") == 0);
    Plate_Format(plate, 12345); EXPECT(strcmp(plate, "KM-12345") == 0);

    EXPECT(floatabs(Speed_Kmh(0.5, 0.0, 0.0) - 90.0) < 0.001);
    EXPECT(floatabs(Dist3D(0.0, 0.0, 0.0, 3.0, 4.0, 0.0) - 5.0) < 0.001);

    new bool:ok = true;
    for (new i = 0; i < 300; i++)
    {
        new r = RandomOther(5, 2);
        if (r == 2 || r < 0 || r >= 5) ok = false;
        r = RandomOther(5, -1);
        if (r < 0 || r >= 5) ok = false;
    }
    EXPECT(ok);
    EXPECT(RandomOther(1, 0) == 0);

    // Item and job tables line up with the ids stored in the database.
    EXPECT(sizeof (gItems) == ITEM_COUNT && sizeof (gJobNames) == JOB_COUNT);
    EXPECT(strcmp(gItems[ITEM_PHONE][itName], "Phone") == 0 && gItems[ITEM_PHONE][itMax] == 1);
    EXPECT(strcmp(gJobNames[JOB_MECHANIC], "Mechanic") == 0);
    EXPECT(strcmp(gJobNames[JOB_BUS], "Bus driver") == 0 && sizeof (gBusRoute) == 8);
    // Every dealership model is a real vehicle model.
    ok = true;
    for (new i = 0; i < sizeof (gDealerStock); i++)
    {
        if (!(400 <= gDealerStock[i][diModel] <= 611) || gDealerStock[i][diPrice] <= 0) ok = false;
    }
    EXPECT(ok);
    // Property interiors are all houses or businesses.
    ok = true;
    for (new i = 0; i < sizeof (gInteriors); i++)
    {
        if (gInteriors[i][intType] != PROP_HOUSE && gInteriors[i][intType] != PROP_BUSINESS) ok = false;
    }
    EXPECT(ok);
}

Test_Kmui()
{
    new text[64] = "a|b;c=d\te";
    KMUI_Escape(text);
    EXPECT(strcmp(text, "a b c d e") == 0);

    new msg[KMUI_MAX_TEXT + 16];
    KMUI_Build(msg, "toast", "text=hi");
    EXPECT(strcmp(msg, "KM1|toast|text=hi") == 0);
    // never longer than a client message can be
    new longText[300];
    for (new i = 0; i < 250; i++) longText[i] = 'x';
    KMUI_Build(msg, "toast", longText);
    EXPECT(strlen(msg) == KMUI_MAX_TEXT - 1);

    new hud[128];
    KMUI_HudPayload(hud, 120, 5000, 3, 2, 12, "Taxi driver", 1, -1);
    EXPECT(strcmp(hud, "cash=120;bank=5000;lvl=3;xp=2;xpmax=12;job=Taxi driver;wanted=1;fuel=-1") == 0);
    KMUI_HudPayload(hud, 0, 0, 1, 0, 4, "Bad;job=x", 0, 55);
    EXPECT(strcmp(hud, "cash=0;bank=0;lvl=1;xp=0;xpmax=4;job=Bad job x;wanted=0;fuel=55") == 0);
    // the full HUD message fits in one client message
    KMUI_HudPayload(hud, 2000000000, 2000000000, 999, 999, 3996, "Pizza delivery", 6, 100);
    KMUI_Build(msg, "hud", hud);
    EXPECT(strfind(msg, "fuel=100") != -1);

    // standard clients get nothing
    gKmClient[5] = 0;
    EXPECT(KMUI_Send(5, "toast", "text=x") == 0);
}

Test_AntiCheat()
{
    EXPECT(AC_WeaponAllowed(WEAPON:0, WEAPON:0));
    EXPECT(AC_WeaponAllowed(WEAPON_DEAGLE, WEAPON_DEAGLE));
    EXPECT(AC_WeaponAllowed(WEAPON:0, WEAPON_PARACHUTE));
    EXPECT(!AC_WeaponAllowed(WEAPON:0, WEAPON_DEAGLE));
    EXPECT(!AC_WeaponAllowed(WEAPON_NITESTICK, WEAPON_SHOTGUN));

    EXPECT(!AC_IsTeleport(0.0, 0.0, 60.0, 60.0, 100.0));
    EXPECT(AC_IsTeleport(0.0, 0.0, 80.0, 80.0, 100.0));
    EXPECT(AC_IsTeleport(500.0, 500.0, 300.0, 500.0, 100.0));

    // the hooks record what the server hands out (player 7 isn't connected,
    // the natives just fail)
    AC_Reset(7);
    GivePlayerWeapon(7, WEAPON_DEAGLE, 50);
    GivePlayerWeapon(7, WEAPON_NITESTICK, 1);
    EXPECT(gAcWeapon[7][_:GetWeaponSlot(WEAPON_DEAGLE)] == WEAPON_DEAGLE);
    EXPECT(gAcWeapon[7][_:GetWeaponSlot(WEAPON_NITESTICK)] == WEAPON_NITESTICK);
    ResetPlayerWeapons(7);
    EXPECT(gAcWeapon[7][_:GetWeaponSlot(WEAPON_DEAGLE)] == WEAPON:0);
    EXPECT(gAcGraceUntil[7] == 0);
    SetPlayerPos(7, 0.0, 0.0, 0.0);
    EXPECT(gAcGraceUntil[7] >= gettime() + AC_GRACE_SECONDS - 1);

    EXPECT(AC_IsAircraft(487) && AC_IsAircraft(520) && !AC_IsAircraft(411) && !AC_IsAircraft(522));
    // the driving test route has a few stops and the test car fits the clock
    EXPECT(sizeof (gLicenceRoute) >= 3 && KM_LICENCE_TEST_SECONDS >= 60);
}

// Affected rows of a statement, or -1 on error.
DB_Affected(const query[])
{
    new Cache:c = mysql_query(gDB, query);
    new rows = mysql_errno(gDB) == 0 ? cache_affected_rows() : -1;
    if (c) cache_delete(c);
    return rows;
}

new gTestAccount, gTestChar, gTestChar2, gTestProperty;

Test_Phase2Schema()
{
    DB_Exec("DELETE FROM properties WHERE name LIKE 'kmtest%'");
    DB_Exec("DELETE FROM accounts WHERE username LIKE 'kmtest_%'");

    // seeded factions and a rank row for every rank
    EXPECT(DB_Int("SELECT COUNT(*) FROM factions") >= 3);
    EXPECT(DB_Int("SELECT COUNT(*) FROM factions f WHERE (SELECT COUNT(*) FROM faction_ranks r WHERE r.faction_id = f.id AND r.`rank` BETWEEN 1 AND f.max_rank) <> f.max_rank") == 0);
    EXPECT(DB_Int("SELECT type FROM factions WHERE short_name = 'LSPD'") == FACTION_POLICE);

    DB_Exec("INSERT INTO accounts (username, password_hash, register_ip) VALUES ('kmtest_p2', REPEAT('x', 60), '127.0.0.1')");
    gTestAccount = DB_Int("SELECT id FROM accounts WHERE username = 'kmtest_p2'");
    EXPECT(gTestAccount > 0);
    new q[512];
    format(q, sizeof (q), "INSERT INTO characters (account_id, name, gender, skin, cash, pos_x, pos_y, pos_z) VALUES (%d, 'Test_Money', 0, 2, 500, 0, 0, 0), (%d, 'Test_Other', 1, 12, 0, 0, 0, 0)", gTestAccount, gTestAccount);
    EXPECT(DB_Exec(q) == 0);
    gTestChar = DB_Int("SELECT id FROM characters WHERE name = 'Test_Money'");
    gTestChar2 = DB_Int("SELECT id FROM characters WHERE name = 'Test_Other'");
    EXPECT(gTestChar > 0 && gTestChar2 > 0);

    // new character defaults
    format(q, sizeof (q), "SELECT bank + xp + job + faction_rank + payday_seconds + wanted + jail_seconds + IFNULL(faction_id, 0) + IFNULL(phone_number, 0) FROM characters WHERE id = %d", gTestChar);
    EXPECT(DB_Int(q) == 0);

    // money can't go negative, wanted level is capped
    format(q, sizeof (q), "UPDATE characters SET cash = -1 WHERE id = %d", gTestChar);
    EXPECT(DB_Exec(q) != 0);
    format(q, sizeof (q), "UPDATE characters SET bank = -1 WHERE id = %d", gTestChar);
    EXPECT(DB_Exec(q) != 0);
    format(q, sizeof (q), "UPDATE characters SET wanted = 7 WHERE id = %d", gTestChar);
    EXPECT(DB_Exec(q) != 0);
    // unknown faction is rejected
    format(q, sizeof (q), "UPDATE characters SET faction_id = 999 WHERE id = %d", gTestChar);
    EXPECT(DB_Exec(q) != 0);

    // phone numbers are unique; the gamemode's UPDATE IGNORE reports 0 rows when taken
    format(q, sizeof (q), "UPDATE IGNORE characters SET phone_number = 5551234 WHERE id = %d", gTestChar);
    EXPECT(DB_Affected(q) == 1);
    format(q, sizeof (q), "UPDATE IGNORE characters SET phone_number = 5551234 WHERE id = %d", gTestChar2);
    EXPECT(DB_Affected(q) == 0);

    // inventory upsert, as Inv_Save does it
    format(q, sizeof (q), "INSERT INTO character_items (character_id, item_id, quantity) VALUES (%d, 2, 1) ON DUPLICATE KEY UPDATE quantity = VALUES(quantity)", gTestChar);
    DB_Exec(q);
    format(q, sizeof (q), "INSERT INTO character_items (character_id, item_id, quantity) VALUES (%d, 2, 3) ON DUPLICATE KEY UPDATE quantity = VALUES(quantity)", gTestChar);
    DB_Exec(q);
    format(q, sizeof (q), "SELECT quantity FROM character_items WHERE character_id = %d AND item_id = 2", gTestChar);
    EXPECT(DB_Int(q) == 3);
    format(q, sizeof (q), "INSERT INTO character_items (character_id, item_id, quantity) VALUES (%d, 3, 0)", gTestChar);
    EXPECT(DB_Exec(q) != 0);

    // vehicles: model range and fuel range
    format(q, sizeof (q), "INSERT INTO vehicles (owner_character_id, model, pos_x, pos_y, pos_z, pos_a, price) VALUES (%d, 399, 0, 0, 0, 0, 1)", gTestChar);
    EXPECT(DB_Exec(q) != 0);
    format(q, sizeof (q), "INSERT INTO vehicles (owner_character_id, model, pos_x, pos_y, pos_z, pos_a, fuel, price) VALUES (%d, 411, 0, 0, 0, 0, 101, 1)", gTestChar);
    EXPECT(DB_Exec(q) != 0);
    format(q, sizeof (q), "INSERT INTO vehicles (owner_character_id, model, pos_x, pos_y, pos_z, pos_a, price) VALUES (%d, 411, 1, 2, 3, 90, 150000), (%d, 462, 1, 2, 3, 90, 3500)", gTestChar, gTestChar);
    EXPECT(DB_Exec(q) == 0);

    // property purchase: the claim only succeeds once
    EXPECT(DB_Exec("INSERT INTO properties (type, name, price, ent_x, ent_y, ent_z, interior_preset) VALUES (1, 'kmtest house', 25000, 1.0, 2.0, 3.0, 0)") == 0);
    gTestProperty = DB_Int("SELECT id FROM properties WHERE name = 'kmtest house'");
    format(q, sizeof (q), "UPDATE properties SET owner_character_id = %d, locked = 1 WHERE id = %d AND owner_character_id IS NULL", gTestChar, gTestProperty);
    EXPECT(DB_Affected(q) == 1);
    format(q, sizeof (q), "UPDATE properties SET owner_character_id = %d, locked = 1 WHERE id = %d AND owner_character_id IS NULL", gTestChar2, gTestProperty);
    EXPECT(DB_Affected(q) == 0);
    EXPECT(DB_Exec("INSERT INTO properties (type, name, price, ent_x, ent_y, ent_z, interior_preset) VALUES (3, 'kmtest bad', 1, 0, 0, 0, 0)") != 0);

    // reports: kind and status are constrained
    format(q, sizeof (q), "INSERT INTO reports (kind, reporter_account_id, text) VALUES (3, %d, 'bad kind')", gTestAccount);
    EXPECT(DB_Exec(q) != 0);
    format(q, sizeof (q), "INSERT INTO reports (kind, reporter_account_id, text) VALUES (1, %d, 'kmtest report'), (2, %d, 'kmtest help')", gTestAccount, gTestAccount);
    EXPECT(DB_Exec(q) == 0);

    // criminal records
    format(q, sizeof (q), "INSERT INTO criminal_records (character_id, officer_character_id, charge) VALUES (%d, %d, 'Speeding'), (%d, %d, 'Theft')", gTestChar, gTestChar2, gTestChar, gTestChar2);
    EXPECT(DB_Exec(q) == 0);
}

// The SQL the gamemode builds, run against the real schema.
Test_SharedQueries()
{
    new q[1024];

    mysql_format(gDB, q, sizeof (q), CHAR_SELECT_SQL, gTestChar, gTestAccount);
    new Cache:c = mysql_query(gDB, q);
    EXPECT(mysql_errno(gDB) == 0 && cache_num_rows() == 1);
    new phone, faction, licence = -1;
    cache_get_value_name_int(0, "phone_number", phone);
    cache_get_value_name_int(0, "faction_id", faction);
    cache_get_value_name_int(0, "driving_licence", licence);
    EXPECT(phone == 5551234 && faction == 0 && licence == 0);
    cache_delete(c);
    // another account can't load the character
    mysql_format(gDB, q, sizeof (q), CHAR_SELECT_SQL, gTestChar, gTestAccount + 100000);
    c = mysql_query(gDB, q);
    EXPECT(cache_num_rows() == 0);
    cache_delete(c);

    mysql_format(gDB, q, sizeof (q), CHAR_SAVE_SQL, 700, 1200, 2, 3, JOB_TAXI, 1, 2, 1800, 2, 0, 90.0, 0.0, 10.0, 20.0, 30.0, 45.0, 0, 0, 60, gTestChar);
    EXPECT(DB_Affected(q) == 1);
    format(q, sizeof (q), "SELECT cash = 700 AND bank = 1200 AND level = 2 AND xp = 3 AND job = %d AND faction_id = 1 AND faction_rank = 2 AND payday_seconds = 1800 AND wanted = 2 AND played_seconds = 60 FROM characters WHERE id = %d", JOB_TAXI, gTestChar);
    EXPECT(DB_Int(q) == 1);
    // faction 0 saves as NULL
    mysql_format(gDB, q, sizeof (q), CHAR_SAVE_SQL, 700, 1200, 2, 3, JOB_TAXI, 0, 0, 1800, 2, 0, 90.0, 0.0, 10.0, 20.0, 30.0, 45.0, 0, 0, 0, gTestChar);
    DB_Exec(q);
    format(q, sizeof (q), "SELECT faction_id IS NULL FROM characters WHERE id = %d", gTestChar);
    EXPECT(DB_Int(q) == 1);

    mysql_format(gDB, q, sizeof (q), VEH_SELECT_SQL, gTestChar);
    c = mysql_query(gDB, q);
    EXPECT(mysql_errno(gDB) == 0 && cache_num_rows() == 2);
    cache_delete(c);

    // MDT by id and by name, with plates and records
    mysql_format(gDB, q, sizeof (q), MDT_PERSON_SQL, gTestChar, gTestChar, "");
    c = mysql_query(gDB, q);
    EXPECT(mysql_errno(gDB) == 0 && cache_num_rows() == 1);
    new vehicles[32];
    cache_get_value_name(0, "vehicles", vehicles);
    EXPECT(strfind(vehicles, ",") != -1);
    cache_delete(c);
    mysql_format(gDB, q, sizeof (q), MDT_PERSON_SQL, 0, 0, "Test_Other");
    c = mysql_query(gDB, q);
    new id, bool:noVehicles;
    cache_get_value_name_int(0, "id", id);
    cache_is_value_name_null(0, "vehicles", noVehicles);
    EXPECT(cache_num_rows() == 1 && id == gTestChar2 && noVehicles);
    cache_delete(c);
    // the longest lookup (24-character name of quotes, escaped to 48) fits the game's buffer
    new mdt[MDT_QUERY_SIZE + 512];
    mysql_format(gDB, mdt, sizeof (mdt), MDT_PERSON_SQL, cellmax, cellmax, "\'\'\'\'\'\'\'\'\'\'\'\'\'\'\'\'\'\'\'\'\'\'\'\'");
    EXPECT(strlen(mdt) < MDT_QUERY_SIZE - 1);
    mysql_format(gDB, q, sizeof (q), MDT_PERSON_SQL, 0, 0, "Nobody_Here");
    c = mysql_query(gDB, q);
    EXPECT(cache_num_rows() == 0);
    cache_delete(c);
    mysql_format(gDB, q, sizeof (q), MDT_RECORDS_SQL, gTestChar);
    c = mysql_query(gDB, q);
    new officer[MAX_PLAYER_NAME], day[12];
    cache_get_value_name(0, "officer", officer);
    cache_get_value_name(0, "day", day);
    EXPECT(mysql_errno(gDB) == 0 && cache_num_rows() == 2 && strcmp(officer, "Test_Other") == 0 && strlen(day) == 10);
    cache_delete(c);

    // tickets: amount is constrained; paying only covers tickets seen
    format(q, sizeof (q), "INSERT INTO tickets (character_id, amount, reason) VALUES (%d, 0, 'free')", gTestChar);
    EXPECT(DB_Exec(q) != 0);
    format(q, sizeof (q), "INSERT INTO tickets (character_id, officer_character_id, amount, reason) VALUES (%d, %d, 150, 'Speeding'), (%d, %d, 300, 'No licence')",
        gTestChar, gTestChar2, gTestChar, gTestChar2);
    EXPECT(DB_Exec(q) == 0);
    mysql_format(gDB, q, sizeof (q), TICKETS_UNPAID_SQL, gTestChar);
    c = mysql_query(gDB, q);
    new firstTicket;
    cache_get_value_name_int(0, "id", firstTicket);
    EXPECT(mysql_errno(gDB) == 0 && cache_num_rows() == 2);
    cache_delete(c);
    mysql_format(gDB, q, sizeof (q), TICKETS_PAY_SQL, gTestChar, firstTicket);
    EXPECT(DB_Affected(q) == 1);
    mysql_format(gDB, q, sizeof (q), MDT_PERSON_SQL, gTestChar, gTestChar, "");
    c = mysql_query(gDB, q);
    new tickets, owed;
    cache_get_value_name_int(0, "tickets", tickets);
    cache_get_value_name_int(0, "owed", owed);
    EXPECT(mysql_errno(gDB) == 0 && tickets == 1 && owed == 300);
    cache_delete(c);

    // offline faction removal only works on lower ranks
    format(q, sizeof (q), "UPDATE characters SET faction_id = 1, faction_rank = 2 WHERE id = %d", gTestChar2);
    DB_Exec(q);
    mysql_format(gDB, q, sizeof (q), FAC_ROSTER_SQL, 1);
    c = mysql_query(gDB, q);
    EXPECT(mysql_errno(gDB) == 0 && cache_num_rows() >= 1);
    cache_delete(c);
    mysql_format(gDB, q, sizeof (q), FAC_REMOVE_OFFLINE_SQL, "Test_Other", 1, 2);
    EXPECT(DB_Affected(q) == 0);
    mysql_format(gDB, q, sizeof (q), FAC_REMOVE_OFFLINE_SQL, "Test_Other", 2, 6);
    EXPECT(DB_Affected(q) == 0);
    mysql_format(gDB, q, sizeof (q), FAC_REMOVE_OFFLINE_SQL, "Test_Other", 1, 3);
    EXPECT(DB_Affected(q) == 1);
    format(q, sizeof (q), "SELECT faction_id IS NULL AND faction_rank = 0 FROM characters WHERE id = %d", gTestChar2);
    EXPECT(DB_Int(q) == 1);

    // reports: helpers only see help requests, and can't close reports
    mysql_format(gDB, q, sizeof (q), REPORT_LIST_SQL, REPORT_CLOSED, REPORT_KIND_REPORT);
    c = mysql_query(gDB, q);
    new allOpen = cache_num_rows();
    cache_delete(c);
    mysql_format(gDB, q, sizeof (q), REPORT_LIST_SQL, REPORT_CLOSED, REPORT_KIND_HELP);
    c = mysql_query(gDB, q);
    EXPECT(mysql_errno(gDB) == 0 && allOpen >= 2 && cache_num_rows() == allOpen - DB_Int("SELECT COUNT(*) FROM reports WHERE kind = 1 AND status <> 2"));
    cache_delete(c);
    new reportId = DB_Int("SELECT id FROM reports WHERE text = 'kmtest report'");
    mysql_format(gDB, q, sizeof (q), REPORT_UPDATE_SQL, REPORT_CLOSED, gTestAccount, REPORT_CLOSED, REPORT_CLOSED, reportId, REPORT_CLOSED, REPORT_KIND_HELP);
    EXPECT(DB_Affected(q) == 0);
    mysql_format(gDB, q, sizeof (q), REPORT_UPDATE_SQL, REPORT_ACCEPTED, gTestAccount, REPORT_ACCEPTED, REPORT_CLOSED, reportId, REPORT_CLOSED, REPORT_KIND_REPORT);
    EXPECT(DB_Affected(q) == 1);
    mysql_format(gDB, q, sizeof (q), REPORT_UPDATE_SQL, REPORT_CLOSED, gTestAccount, REPORT_CLOSED, REPORT_CLOSED, reportId, REPORT_CLOSED, REPORT_KIND_REPORT);
    EXPECT(DB_Affected(q) == 1);
    format(q, sizeof (q), "SELECT status = 2 AND closed_at IS NOT NULL AND handled_by = %d FROM reports WHERE id = %d", gTestAccount, reportId);
    EXPECT(DB_Int(q) == 1);
    // closed requests can't be reopened
    mysql_format(gDB, q, sizeof (q), REPORT_UPDATE_SQL, REPORT_ACCEPTED, gTestAccount, REPORT_ACCEPTED, REPORT_CLOSED, reportId, REPORT_CLOSED, REPORT_KIND_REPORT);
    EXPECT(DB_Affected(q) == 0);
}

// Player slot 0 stands in for a playing character: the gamemode functions
// only need gChar/gInv filled in.
Test_StartAsync()
{
    gChar[0][cId] = gTestChar;
    gChar[0][cCash] = 700;
    gChar[0][cBank] = 1200;

    EXPECT(Money_Give(0, -200, 200, "test_deposit"));
    EXPECT(gChar[0][cCash] == 500 && gChar[0][cBank] == 1400);
    EXPECT(!Money_Give(0, -501, 0, "test_overdraw"));
    EXPECT(gChar[0][cCash] == 500);
    EXPECT(!Money_Give(0, 0, -1401, "test_overdraw"));
    // Money_Charge takes the bank first, then cash
    EXPECT(Money_Charge(0, 1600, "test_charge"));
    EXPECT(gChar[0][cBank] == 0 && gChar[0][cCash] == 300);
    EXPECT(!Money_Charge(0, 301, "test_charge"));
    // no character, no money
    gChar[1][cId] = 0;
    EXPECT(!Money_Give(1, 100, 0, "test_nochar"));

    gInvLoaded[0] = true;
    EXPECT(Inv_Add(0, ITEM_WATER, 2));
    EXPECT(!Inv_Add(0, ITEM_PHONE, 2));      // over the carry limit
    EXPECT(Inv_Add(0, ITEM_PHONE, 1));
    EXPECT(!Inv_Add(0, ITEM_PHONE, 1));
    EXPECT(Inv_Remove(0, ITEM_WATER, 2));
    EXPECT(!Inv_Remove(0, ITEM_WATER, 1));
    EXPECT(Inv_Has(0, ITEM_PHONE) && !Inv_Has(0, ITEM_WATER));

    // leaving while cuffed: jail time scaled by the wanted level, a charge
    gChar[0][cCuffed] = true;
    gChar[0][cWanted] = 2;
    gChar[0][cJailSeconds] = 0;
    Fac_OnLeaveWhileCuffed(0);
    EXPECT(!gChar[0][cCuffed] && gChar[0][cWanted] == 0 && gChar[0][cJailSeconds] == 15 * 60);
    // not cuffed: nothing happens
    gChar[0][cJailSeconds] = 0;
    Fac_OnLeaveWhileCuffed(0);
    EXPECT(gChar[0][cJailSeconds] == 0);

    // anti-cheat findings are logged against the account and character
    gPlayer[0][pAccountId] = gTestAccount;
    EXPECT(!AC_Report(0, "test", "from km_tests", false));

    // the real loaders
    Fac_Load();
    Prop_Load();
    SetTimer("T_CheckAsync", 2000, false);
}

forward T_CheckAsync();
public T_CheckAsync()
{
    new q[768];
    format(q, sizeof (q), "SELECT cash * 100000 + bank FROM characters WHERE id = %d", gTestChar);
    EXPECT(DB_Int(q) == 300 * 100000 + 0);
    format(q, sizeof (q), "SELECT COUNT(*) FROM money_log WHERE character_id = %d", gTestChar);
    EXPECT(DB_Int(q) == 2);
    format(q, sizeof (q), "SELECT COUNT(*) FROM money_log WHERE character_id = %d AND reason = 'test_charge' AND cash_delta = -200 AND bank_delta = -1400 AND cash_after = 300 AND bank_after = 0", gTestChar);
    EXPECT(DB_Int(q) == 1);
    format(q, sizeof (q), "SELECT GROUP_CONCAT(item_id, ':', quantity) IS NOT NULL AND SUM(item_id = 1 AND quantity = 1) = 1 AND COUNT(*) = 1 FROM character_items WHERE character_id = %d", gTestChar);
    EXPECT(DB_Int(q) == 1);

    EXPECT(Fac_Valid(1) && Fac_Type(1) == FACTION_POLICE && Fac_MaxRank(1) == 6);
    EXPECT(Fac_Salary(1, 1) == 400 && Fac_Salary(1, 6) == 1250 && Fac_Salary(0, 1) == 0 && Fac_Salary(1, 9) == 0);
    new rank[32];
    Fac_RankName(1, 6, rank);
    EXPECT(strcmp(rank, "Chief of Police") == 0);
    EXPECT(!Fac_Valid(0) && !Fac_Valid(99));
    EXPECT(gFac[1][fColour] == 0x3A6FD8FF);
    EXPECT(Fac_DutySkin(1, 0) == 280 && Fac_DutySkin(0, 0) == 0);

    new found = -1;
    for (new i = 0; i < KM_MAX_PROPERTIES; i++)
    {
        if (gProp[i][prUsed] && gProp[i][prId] == gTestProperty) found = i;
    }
    EXPECT(found != -1);
    if (found != -1)
    {
        EXPECT(gProp[found][prOwner] == gTestChar && strcmp(gProp[found][prOwnerName], "Test_Money") == 0 && gProp[found][prPrice] == 25000);
        EXPECT(Prop_CountOwned(gTestChar) == 1 && Prop_CountOwned(gTestChar2) == 0);
    }

    format(q, sizeof (q), "SELECT COUNT(*) FROM criminal_records WHERE character_id = %d AND officer_character_id IS NULL AND charge = '%s'", gTestChar, FLED_CUSTODY_CHARGE);
    EXPECT(DB_Int(q) == 1);
    format(q, sizeof (q), "SELECT COUNT(*) FROM anticheat_log WHERE account_id = %d AND character_id = %d AND type = 'test' AND kicked = 0", gTestAccount, gTestChar);
    EXPECT(DB_Int(q) == 1);

    // deleting the account removes everything that belongs to it
    format(q, sizeof (q), "DELETE FROM accounts WHERE id = %d", gTestAccount);
    DB_Exec(q);
    format(q, sizeof (q),
        "SELECT (SELECT COUNT(*) FROM characters WHERE account_id = %d) + (SELECT COUNT(*) FROM vehicles WHERE owner_character_id = %d) \
         + (SELECT COUNT(*) FROM money_log WHERE character_id = %d) + (SELECT COUNT(*) FROM character_items WHERE character_id = %d) \
         + (SELECT COUNT(*) FROM criminal_records WHERE character_id = %d) + (SELECT COUNT(*) FROM reports WHERE reporter_account_id = %d) \
         + (SELECT COUNT(*) FROM anticheat_log WHERE account_id = %d) + (SELECT COUNT(*) FROM tickets WHERE character_id = %d)",
        gTestAccount, gTestChar, gTestChar, gTestChar, gTestChar, gTestAccount, gTestAccount, gTestChar);
    EXPECT(DB_Int(q) == 0);
    // the property goes back on the market
    format(q, sizeof (q), "SELECT owner_character_id IS NULL FROM properties WHERE id = %d", gTestProperty);
    EXPECT(DB_Int(q) == 1);
    DB_Exec("DELETE FROM properties WHERE name LIKE 'kmtest%'");

    bcrypt_hash(0, "T_OnHash", "correct horse", 4);
    return 1;
}

Test_ActiveBans(const ip[], const username[])
{
    new q[512];
    mysql_format(gDB, q, sizeof (q),
        "SELECT COUNT(*) FROM bans \
         WHERE lifted_at IS NULL AND (expires_at IS NULL OR expires_at > UTC_TIMESTAMP()) \
         AND (ip = '%e' OR account_id = (SELECT id FROM accounts WHERE username = '%e'))",
        ip, username);
    return DB_Int(q);
}

new gHash[BCRYPT_HASH_LENGTH];

forward T_OnHash(playerid, hashid);
public T_OnHash(playerid, hashid)
{
    bcrypt_get_hash(gHash);
    EXPECT(strlen(gHash) == 60 && gHash[0] == '$');
    bcrypt_verify(0, "T_OnVerifyGood", "correct horse", gHash);
    return 1;
}

forward T_OnVerifyGood(playerid, bool:success);
public T_OnVerifyGood(playerid, bool:success)
{
    Expect(success, "bcrypt verifies the right password");
    bcrypt_verify(0, "T_OnVerifyBad", "Correct horse", gHash);
    return 1;
}

forward T_OnVerifyBad(playerid, bool:success);
public T_OnVerifyBad(playerid, bool:success)
{
    Expect(!success, "bcrypt rejects a wrong password");
    // The gamemode passes an extra argument; check where the plugin puts it.
    bcrypt_hash(3, "T_OnHashArgs", "pw", 4, "d", 4242);
    return 1;
}

forward T_OnHashArgs(playerid, extra, hashid);
public T_OnHashArgs(playerid, extra, hashid)
{
    Expect(playerid == 3 && extra == 4242, "bcrypt_hash passes custom args before hashid");
    bcrypt_get_hash(gHash);
    bcrypt_verify(3, "T_OnVerifyArgs", "pw", gHash, "d", 4343);
    return 1;
}

forward T_OnVerifyArgs(playerid, bool:success, extra);
public T_OnVerifyArgs(playerid, bool:success, extra)
{
    Expect(playerid == 3 && extra == 4343 && success, "bcrypt_verify passes custom args after success");
    Test_Finish();
    return 1;
}

Test_Finish()
{
    printf("KM-TESTS DONE pass=%d fail=%d", gPass, gFail);
    SendRconCommand("exit");
}
