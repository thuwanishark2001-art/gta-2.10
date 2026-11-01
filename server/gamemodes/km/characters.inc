// Characters: list, create (name, gender, skin), select, spawn, save.

enum E_CHAR
{
    cId,
    cName[MAX_PLAYER_NAME],
    cGender,
    cSkin,
    cLevel,
    cXp,
    cCash,
    cBank,
    cJob,
    cFaction,           // faction id, 0 = none
    cFactionRank,
    cPaydaySeconds,
    cWanted,
    cJailSeconds,
    cPhone,             // phone number, 0 = none
    Float:cHealth,
    Float:cArmour,
    Float:cX,
    Float:cY,
    Float:cZ,
    Float:cA,
    cInterior,
    cWorld,
    cPlayedMark,        // unix time of the last played_seconds update
    cSessionStart,
    bool:cSpawned,
    bool:cDied,         // respawn at the hospital
    bool:cOnDuty,
    bool:cCuffed,
    bool:cDrivingLicence
}

new gChar[MAX_PLAYERS][E_CHAR];

// Shared with tests/km_tests.pwn, which runs them against the real schema.
#define CHAR_SELECT_SQL \
    "SELECT id, name, gender, skin, level, xp, cash, bank, job, IFNULL(faction_id, 0) AS faction_id, faction_rank, \
     payday_seconds, wanted, jail_seconds, IFNULL(phone_number, 0) AS phone_number, driving_licence, \
     health, armour, pos_x, pos_y, pos_z, pos_a, interior, virtual_world \
     FROM characters WHERE id = %d AND account_id = %d"

#define CHAR_SAVE_SQL \
    "UPDATE characters SET cash = %d, bank = %d, level = %d, xp = %d, job = %d, faction_id = NULLIF(%d, 0), faction_rank = %d, \
     payday_seconds = %d, wanted = %d, jail_seconds = %d, health = %f, armour = %f, pos_x = %f, pos_y = %f, pos_z = %f, pos_a = %f, \
     interior = %d, virtual_world = %d, played_seconds = played_seconds + %d, last_played_at = UTC_TIMESTAMP() WHERE id = %d"

// The character list shown in the select dialog.
new gCharListCount[MAX_PLAYERS];
new gCharListId[MAX_PLAYERS][KM_MAX_CHARACTERS];

// Character being created.
new gNewCharName[MAX_PLAYERS][MAX_PLAYER_NAME];
new gNewCharGender[MAX_PLAYERS];

// Only skins every copy of GTA: San Andreas has: the mobile client has no SA-MP
// data pack, so SA-MP-only skins (1-6, 8, 42, 65, 86, 119, 149, 208, 273, 289,
// 300-311) don't exist there.
static const gSkinsMale[] = { 21, 7, 14, 15, 20, 22, 23, 29, 46, 47, 48, 59, 60, 67, 72, 98, 101, 170, 185, 188, 250 };
static const gSkinsFemale[] = { 12, 13, 31, 40, 41, 55, 56, 69, 76, 91, 93, 141, 148, 150, 169, 190, 192, 193, 211, 216, 233 };

forward Char_OnListLoaded(playerid, race);
forward Char_OnNameChecked(playerid, race);
forward Char_OnCreated(playerid, race);
forward Char_OnLoaded(playerid, race);
forward Char_AutoSaveAll();

stock Char_Reset(playerid)
{
    static const empty[E_CHAR];
    gChar[playerid] = empty;
    gCharListCount[playerid] = 0;
    gNewCharName[playerid][0] = '\0';
}

stock Char_LoadList(playerid)
{
    gPlayer[playerid][pStage] = STAGE_CHAR_SELECT;
    new query[256];
    mysql_format(gDB, query, sizeof (query),
        "SELECT id, name, level, played_seconds FROM characters WHERE account_id = %d ORDER BY id LIMIT %d",
        gPlayer[playerid][pAccountId], KM_MAX_CHARACTERS);
    mysql_tquery(gDB, query, "Char_OnListLoaded", "dd", playerid, gPlayerRace[playerid]);
}

public Char_OnListLoaded(playerid, race)
{
    if (gPlayerRace[playerid] != race) return 1;

    new rows = cache_num_rows();
    gCharListCount[playerid] = rows;
    if (rows == 0)
    {
        Char_StartCreate(playerid, "");
        return 1;
    }

    new body[512], line[96], name[MAX_PLAYER_NAME], level, played, playedText[24];
    body = "Character\tLevel\tPlayed\n";
    for (new i = 0; i < rows; i++)
    {
        cache_get_value_name_int(i, "id", gCharListId[playerid][i]);
        cache_get_value_name(i, "name", name);
        cache_get_value_name_int(i, "level", level);
        cache_get_value_name_int(i, "played_seconds", played);
        FormatDuration(playedText, played);
        format(line, sizeof (line), EMBED_WHITE "%s\t%d\t%s\n", name, level, playedText);
        strcat(body, line);
    }
    if (rows < KM_MAX_CHARACTERS)
    {
        strcat(body, EMBED_BRAND "+ Create a new character\t\t\n");
    }
    ShowPlayerDialog(playerid, DIALOG_CHAR_SELECT, DIALOG_STYLE_TABLIST_HEADERS, "Kingmakers RP | Characters", body, "Play", "Quit");
    return 1;
}

stock Char_StartCreate(playerid, const error[])
{
    gPlayer[playerid][pStage] = STAGE_CHAR_CREATE;
    new body[400];
    format(body, sizeof (body),
        EMBED_WHITE "Enter your character's name as " EMBED_BRAND "First_Last" EMBED_WHITE ", for example " EMBED_BRAND "Marcus_Reed" EMBED_WHITE ".\n" \
        EMBED_GREY "Each part 2-11 letters, starting with a capital. Use a realistic name.\nNo celebrities or famous characters.");
    Dialog_AppendError(body, error);
    ShowPlayerDialog(playerid, DIALOG_CHAR_NAME, DIALOG_STYLE_INPUT, "Kingmakers RP | New character", body, "Next", "Back");
    return 1;
}

stock Char_ShowGender(playerid)
{
    ShowPlayerDialog(playerid, DIALOG_CHAR_GENDER, DIALOG_STYLE_LIST, "Kingmakers RP | Gender", "Male\nFemale", "Next", "Back");
    return 1;
}

stock Char_ShowSkins(playerid)
{
    new body[512], line[24];
    if (gNewCharGender[playerid] == 0)
    {
        for (new i = 0; i < sizeof (gSkinsMale); i++)
        {
            format(line, sizeof (line), "Outfit %d (skin %d)\n", i + 1, gSkinsMale[i]);
            strcat(body, line);
        }
    }
    else
    {
        for (new i = 0; i < sizeof (gSkinsFemale); i++)
        {
            format(line, sizeof (line), "Outfit %d (skin %d)\n", i + 1, gSkinsFemale[i]);
            strcat(body, line);
        }
    }
    ShowPlayerDialog(playerid, DIALOG_CHAR_SKIN, DIALOG_STYLE_LIST, "Kingmakers RP | Outfit", body, "Create", "Back");
    return 1;
}

stock Char_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
    switch (dialogid)
    {
        case DIALOG_CHAR_SELECT:
        {
            if (gPlayer[playerid][pStage] != STAGE_CHAR_SELECT) return 1;
            if (!response) return Kick(playerid), 1;
            if (listitem >= 0 && listitem < gCharListCount[playerid])
            {
                Char_Load(playerid, gCharListId[playerid][listitem]);
            }
            else if (gCharListCount[playerid] < KM_MAX_CHARACTERS)
            {
                Char_StartCreate(playerid, "");
            }
            else
            {
                Char_LoadList(playerid);
            }
            return 1;
        }
        case DIALOG_CHAR_NAME:
        {
            if (gPlayer[playerid][pStage] != STAGE_CHAR_CREATE) return 1;
            if (!response)
            {
                if (gCharListCount[playerid] > 0) Char_LoadList(playerid);
                else Kick(playerid);
                return 1;
            }
            if (!IsValidCharacterName(inputtext))
            {
                Char_StartCreate(playerid, "That isn't a valid First_Last name.");
                return 1;
            }
            strmid(gNewCharName[playerid], inputtext, 0, strlen(inputtext), MAX_PLAYER_NAME);

            new query[160];
            mysql_format(gDB, query, sizeof (query), "SELECT id FROM characters WHERE name = '%e'", gNewCharName[playerid]);
            mysql_tquery(gDB, query, "Char_OnNameChecked", "dd", playerid, gPlayerRace[playerid]);
            return 1;
        }
        case DIALOG_CHAR_GENDER:
        {
            if (gPlayer[playerid][pStage] != STAGE_CHAR_CREATE) return 1;
            if (!response) return Char_StartCreate(playerid, "");
            gNewCharGender[playerid] = (listitem == 1) ? 1 : 0;
            Char_ShowSkins(playerid);
            return 1;
        }
        case DIALOG_CHAR_SKIN:
        {
            if (gPlayer[playerid][pStage] != STAGE_CHAR_CREATE) return 1;
            if (!response) return Char_ShowGender(playerid);

            new skin;
            if (gNewCharGender[playerid] == 0)
            {
                if (!(0 <= listitem < sizeof (gSkinsMale))) return Char_ShowSkins(playerid);
                skin = gSkinsMale[listitem];
            }
            else
            {
                if (!(0 <= listitem < sizeof (gSkinsFemale))) return Char_ShowSkins(playerid);
                skin = gSkinsFemale[listitem];
            }

            // The account's character limit is enforced in the INSERT itself,
            // so two quick creates can't both get through.
            new query[640];
            mysql_format(gDB, query, sizeof (query),
                "INSERT IGNORE INTO characters (account_id, name, gender, skin, cash, pos_x, pos_y, pos_z, pos_a) \
                 SELECT %d, '%e', %d, %d, %d, %f, %f, %f, %f FROM DUAL \
                 WHERE (SELECT COUNT(*) FROM characters WHERE account_id = %d) < %d",
                gPlayer[playerid][pAccountId], gNewCharName[playerid], gNewCharGender[playerid], skin, KM_START_CASH,
                KM_SPAWN_X, KM_SPAWN_Y, KM_SPAWN_Z, KM_SPAWN_A,
                gPlayer[playerid][pAccountId], KM_MAX_CHARACTERS);
            mysql_tquery(gDB, query, "Char_OnCreated", "dd", playerid, gPlayerRace[playerid]);
            return 1;
        }
    }
    return 0;
}

public Char_OnNameChecked(playerid, race)
{
    if (gPlayerRace[playerid] != race) return 1;
    if (cache_num_rows() > 0)
    {
        Char_StartCreate(playerid, "That name is taken. Pick another one.");
        return 1;
    }
    Char_ShowGender(playerid);
    return 1;
}

public Char_OnCreated(playerid, race)
{
    if (gPlayerRace[playerid] != race) return 1;
    new id = cache_insert_id();
    if (id <= 0)
    {
        // Name taken in the meantime, or the account is at its limit.
        Char_StartCreate(playerid, "That character couldn't be created. The name may have just been taken.");
        return 1;
    }
    printf("[character] account #%d created #%d %s", gPlayer[playerid][pAccountId], id, gNewCharName[playerid]);
    new details[64];
    format(details, sizeof (details), "character #%d %s", id, gNewCharName[playerid]);
    Log_Admin(0, "character_create", gPlayer[playerid][pAccountId], details);
    Char_Load(playerid, id);
    return 1;
}

stock Char_Load(playerid, characterId)
{
    new query[512];
    mysql_format(gDB, query, sizeof (query),
CHAR_SELECT_SQL,
        characterId, gPlayer[playerid][pAccountId]);
    mysql_tquery(gDB, query, "Char_OnLoaded", "dd", playerid, gPlayerRace[playerid]);
}

public Char_OnLoaded(playerid, race)
{
    if (gPlayerRace[playerid] != race) return 1;
    if (cache_num_rows() == 0)
    {
        Char_LoadList(playerid);
        return 1;
    }

    cache_get_value_name_int(0, "id", gChar[playerid][cId]);
    cache_get_value_name(0, "name", gChar[playerid][cName], MAX_PLAYER_NAME);
    cache_get_value_name_int(0, "gender", gChar[playerid][cGender]);
    cache_get_value_name_int(0, "skin", gChar[playerid][cSkin]);
    cache_get_value_name_int(0, "level", gChar[playerid][cLevel]);
    cache_get_value_name_int(0, "xp", gChar[playerid][cXp]);
    cache_get_value_name_int(0, "cash", gChar[playerid][cCash]);
    cache_get_value_name_int(0, "bank", gChar[playerid][cBank]);
    cache_get_value_name_int(0, "job", gChar[playerid][cJob]);
    cache_get_value_name_int(0, "faction_id", gChar[playerid][cFaction]);
    cache_get_value_name_int(0, "faction_rank", gChar[playerid][cFactionRank]);
    cache_get_value_name_int(0, "payday_seconds", gChar[playerid][cPaydaySeconds]);
    cache_get_value_name_int(0, "wanted", gChar[playerid][cWanted]);
    cache_get_value_name_int(0, "jail_seconds", gChar[playerid][cJailSeconds]);
    cache_get_value_name_int(0, "phone_number", gChar[playerid][cPhone]);
    new licence;
    cache_get_value_name_int(0, "driving_licence", licence);
    gChar[playerid][cDrivingLicence] = licence != 0;
    cache_get_value_name_float(0, "health", gChar[playerid][cHealth]);
    cache_get_value_name_float(0, "armour", gChar[playerid][cArmour]);
    cache_get_value_name_float(0, "pos_x", gChar[playerid][cX]);
    cache_get_value_name_float(0, "pos_y", gChar[playerid][cY]);
    cache_get_value_name_float(0, "pos_z", gChar[playerid][cZ]);
    cache_get_value_name_float(0, "pos_a", gChar[playerid][cA]);
    cache_get_value_name_int(0, "interior", gChar[playerid][cInterior]);
    cache_get_value_name_int(0, "virtual_world", gChar[playerid][cWorld]);

    if (!(JOB_NONE <= gChar[playerid][cJob] < JOB_COUNT)) gChar[playerid][cJob] = JOB_NONE;
    // A faction that no longer exists, or a rank above its new maximum.
    if (!Fac_Valid(gChar[playerid][cFaction])) gChar[playerid][cFaction] = 0;
    if (gChar[playerid][cFaction] == 0) gChar[playerid][cFactionRank] = 0;
    else gChar[playerid][cFactionRank] = Clamp(gChar[playerid][cFactionRank], 1, Fac_MaxRank(gChar[playerid][cFaction]));

    if (SetPlayerName(playerid, gChar[playerid][cName]) == -1)
    {
        // Someone connected with this exact nickname.
        SendError(playerid, "Another player is using that name as a nickname right now. Pick another character or try later.");
        gChar[playerid][cId] = 0;
        Char_LoadList(playerid);
        return 1;
    }

    gPlayer[playerid][pStage] = STAGE_PLAYING;
    gChar[playerid][cPlayedMark] = gettime();
    gChar[playerid][cSessionStart] = gChar[playerid][cPlayedMark];
    gChar[playerid][cSpawned] = false;

    SetSpawnInfo(playerid, NO_TEAM, gChar[playerid][cSkin],
        gChar[playerid][cX], gChar[playerid][cY], gChar[playerid][cZ], gChar[playerid][cA]);
    TogglePlayerSpectating(playerid, false); // spawns the player
    Inv_Load(playerid);
    Veh_LoadOwned(playerid);

    new query[128];
    mysql_format(gDB, query, sizeof (query), "UPDATE characters SET last_played_at = UTC_TIMESTAMP() WHERE id = %d", gChar[playerid][cId]);
    mysql_tquery(gDB, query);

    printf("[character] %s (#%d) playing #%d %s", gPlayer[playerid][pUsername], gPlayer[playerid][pAccountId], gChar[playerid][cId], gChar[playerid][cName]);
    return 1;
}

// Called from OnPlayerSpawn while playing.
stock Char_OnSpawn(playerid)
{
    if (!gChar[playerid][cSpawned])
    {
        gChar[playerid][cSpawned] = true;
        SetPlayerInterior(playerid, gChar[playerid][cInterior]);
        SetPlayerVirtualWorld(playerid, gChar[playerid][cWorld]);
        SetPlayerHealth(playerid, gChar[playerid][cHealth] < 1.0 ? 100.0 : gChar[playerid][cHealth]);
        SetPlayerArmour(playerid, gChar[playerid][cArmour]);
        SendClientMessage(playerid, COLOR_BRAND, "You are now playing as %s. Type /help for commands.", gChar[playerid][cName]);
    }
    else
    {
        SetPlayerInterior(playerid, 0);
        SetPlayerVirtualWorld(playerid, 0);
    }
    if (gChar[playerid][cDied])
    {
        gChar[playerid][cDied] = false;
        gChar[playerid][cCuffed] = false;
        SetPlayerHealth(playerid, 100.0);
        if (gChar[playerid][cJailSeconds] == 0) Char_ChargeHospital(playerid);
    }
    Char_ApplySkin(playerid);
    SetPlayerScore(playerid, gChar[playerid][cLevel]);
    SetPlayerWantedLevel(playerid, gChar[playerid][cWanted]);
    Char_SyncCash(playerid);
    SetCameraBehindPlayer(playerid);
    if (gChar[playerid][cJailSeconds] > 0) Jail_Place(playerid);
}

// Faction uniform while on duty, otherwise the character's own outfit.
stock Char_ApplySkin(playerid)
{
    if (gChar[playerid][cOnDuty]) SetPlayerSkin(playerid, Fac_DutySkin(gChar[playerid][cFaction], gChar[playerid][cGender]));
    else SetPlayerSkin(playerid, gChar[playerid][cSkin]);
}

// Called from OnPlayerDeath: the next spawn is at the hospital (or the cell).
stock Char_OnDeath(playerid)
{
    if (!Player_IsPlaying(playerid)) return;
    gChar[playerid][cDied] = true;
    if (gChar[playerid][cJailSeconds] > 0)
    {
        SetSpawnInfo(playerid, NO_TEAM, gChar[playerid][cSkin], gJailRelease[ptX], gJailRelease[ptY], gJailRelease[ptZ], gJailRelease[ptA]);
    }
    else
    {
        SetSpawnInfo(playerid, NO_TEAM, gChar[playerid][cSkin], gHospital[ptX], gHospital[ptY], gHospital[ptZ], gHospital[ptA]);
    }
}

stock Char_ChargeHospital(playerid)
{
    new fee = KM_HOSPITAL_FEE;
    if (gChar[playerid][cCash] >= fee) Money_Give(playerid, -fee, 0, "hospital");
    else if (gChar[playerid][cBank] >= fee) Money_Give(playerid, 0, -fee, "hospital");
    else fee = 0;
    if (fee > 0) SendClientMessage(playerid, COLOR_INFO, "You were treated at All Saints General. Hospital bill: $%d.", fee);
    else SendClientMessage(playerid, COLOR_INFO, "You were treated at All Saints General. You couldn't pay, so the bill was waived.");
}

// Money is server authoritative: the client's value is only a display.
// Change it with Money_Give (economy.inc).
stock Char_SyncCash(playerid)
{
    ResetPlayerMoney(playerid);
    GivePlayerMoney(playerid, gChar[playerid][cCash]);
}

stock Char_Save(playerid)
{
    if (gChar[playerid][cId] == 0 || !gChar[playerid][cSpawned]) return;

    new Float:health, Float:armour;
    GetPlayerHealth(playerid, health);
    GetPlayerArmour(playerid, armour);
    if (health < 1.0 || gChar[playerid][cJailSeconds] > 0)
    {
        // Dead (respawns at the hospital) or in jail: keep the last position.
        if (health < 1.0) health = 100.0;
    }
    else
    {
        GetPlayerPos(playerid, gChar[playerid][cX], gChar[playerid][cY], gChar[playerid][cZ]);
        GetPlayerFacingAngle(playerid, gChar[playerid][cA]);
        gChar[playerid][cInterior] = GetPlayerInterior(playerid);
        gChar[playerid][cWorld] = GetPlayerVirtualWorld(playerid);
    }
    gChar[playerid][cHealth] = health;
    gChar[playerid][cArmour] = armour;

    new now = gettime();
    new played = now - gChar[playerid][cPlayedMark];
    gChar[playerid][cPlayedMark] = now;

    new query[768];
    mysql_format(gDB, query, sizeof (query),
CHAR_SAVE_SQL,
        gChar[playerid][cCash], gChar[playerid][cBank], gChar[playerid][cLevel], gChar[playerid][cXp], gChar[playerid][cJob],
        gChar[playerid][cFaction], gChar[playerid][cFactionRank], gChar[playerid][cPaydaySeconds], gChar[playerid][cWanted],
        gChar[playerid][cJailSeconds], health, armour, gChar[playerid][cX], gChar[playerid][cY], gChar[playerid][cZ], gChar[playerid][cA],
        gChar[playerid][cInterior], gChar[playerid][cWorld], played, gChar[playerid][cId]);
    mysql_tquery(gDB, query);
}

public Char_AutoSaveAll()
{
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (Player_IsPlaying(i)) Char_Save(i);
    }
    return 1;
}
