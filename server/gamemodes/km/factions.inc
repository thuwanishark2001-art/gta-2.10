// Factions (loaded from the factions and faction_ranks tables): membership,
// ranks, salaries, duty, radio. Police get cuffs, charges, arrests and the
// MDT; medics treat players for a fee. Jail time is saved on the character.

#define KM_MAX_FACTIONS     (10)
#define KM_MAX_RANKS        (10)

#define COLOR_RADIO         (0x8D8DFFFF)
#define COLOR_DEPARTMENT    (0xFF8282FF)

enum E_FACTION
{
    bool:fUsed,
    fName[48],
    fShort[8],
    fType,
    fColour,
    fMaxRank,
    Float:fHqX,
    Float:fHqY,
    Float:fHqZ
}

new gFac[KM_MAX_FACTIONS + 1][E_FACTION];             // indexed by faction id
new gFacRankName[KM_MAX_FACTIONS + 1][KM_MAX_RANKS + 1][32];
new gFacRankSalary[KM_MAX_FACTIONS + 1][KM_MAX_RANKS + 1];
new gFacInvite[MAX_PLAYERS];                          // pending invite: faction id

// MDT lookups: by character id, or by name with id 0. MDT_QUERY_SIZE is
// checked against the longest possible query in tests/km_tests.pwn.
#define MDT_QUERY_SIZE (1024)
#define MDT_PERSON_SQL \
    "SELECT c.id, c.name, c.wanted, c.jail_seconds, IFNULL(c.phone_number, 0) AS phone, c.driving_licence, \
     (SELECT GROUP_CONCAT(v.id ORDER BY v.id) FROM vehicles v WHERE v.owner_character_id = c.id) AS vehicles, \
     (SELECT COUNT(*) FROM tickets t WHERE t.character_id = c.id AND t.paid_at IS NULL) AS tickets, \
     (SELECT IFNULL(SUM(t.amount), 0) FROM tickets t WHERE t.character_id = c.id AND t.paid_at IS NULL) AS owed \
     FROM characters c WHERE c.id = %d OR (%d = 0 AND c.name = '%e') LIMIT 1"
#define MDT_RECORDS_SQL \
    "SELECT r.charge, DATE_FORMAT(r.created_at, '%%Y-%%m-%%d') AS day, IFNULL(o.name, '-') AS officer \
     FROM criminal_records r LEFT JOIN characters o ON o.id = r.officer_character_id \
     WHERE r.character_id = %d ORDER BY r.id DESC LIMIT 10"

forward Fac_OnLoaded();
forward Fac_OnRanksLoaded();
// Offline members: removal by name (only lower ranks) and the roster.
#define FAC_REMOVE_OFFLINE_SQL \
    "UPDATE characters SET faction_id = NULL, faction_rank = 0 WHERE name = '%e' AND faction_id = %d AND faction_rank < %d"
#define FAC_ROSTER_SQL \
    "SELECT name, faction_rank, IFNULL(DATE_FORMAT(last_played_at, '%%Y-%%m-%%d'), '-') AS seen \
     FROM characters WHERE faction_id = %d ORDER BY faction_rank DESC, name LIMIT 60"

// Logging out while cuffed counts as fleeing custody: a charge, and jail
// time when they come back.
#define FLED_CUSTODY_CHARGE     "Fled custody (left while cuffed)"

forward Fac_OnMdt(playerid, race);
forward Fac_OnRemovedOffline(playerid, race, const name[]);
forward Fac_OnRoster(playerid, race);
forward Fac_OnMdtRecords(playerid, race, characterId);

stock Fac_Load()
{
    mysql_tquery(gDB, "SELECT id, name, short_name, type, color, max_rank, hq_x, hq_y, hq_z FROM factions", "Fac_OnLoaded");
}

public Fac_OnLoaded()
{
    for (new row = 0, rows = cache_num_rows(); row < rows; row++)
    {
        new id;
        cache_get_value_name_int(row, "id", id);
        if (!(1 <= id <= KM_MAX_FACTIONS))
        {
            printf("[faction] skipping faction #%d: ids must be 1-%d", id, KM_MAX_FACTIONS);
            continue;
        }
        new colour;
        gFac[id][fUsed] = true;
        cache_get_value_name(row, "name", gFac[id][fName], 48);
        cache_get_value_name(row, "short_name", gFac[id][fShort], 8);
        cache_get_value_name_int(row, "type", gFac[id][fType]);
        cache_get_value_name_int(row, "color", colour);
        gFac[id][fColour] = (colour << 8) | 0xFF;
        cache_get_value_name_int(row, "max_rank", gFac[id][fMaxRank]);
        cache_get_value_name_float(row, "hq_x", gFac[id][fHqX]);
        cache_get_value_name_float(row, "hq_y", gFac[id][fHqY]);
        cache_get_value_name_float(row, "hq_z", gFac[id][fHqZ]);
        gFac[id][fMaxRank] = Clamp(gFac[id][fMaxRank], 1, KM_MAX_RANKS);

        new label[96];
        format(label, sizeof (label), "%s\n" EMBED_WHITE "/duty", gFac[id][fName]);
        Create3DTextLabel(label, gFac[id][fColour], gFac[id][fHqX], gFac[id][fHqY], gFac[id][fHqZ] + 0.6, 15.0, 0, true);
    }
    mysql_tquery(gDB, "SELECT faction_id, `rank`, name, salary FROM faction_ranks", "Fac_OnRanksLoaded");
    return 1;
}

public Fac_OnRanksLoaded()
{
    for (new row = 0, rows = cache_num_rows(); row < rows; row++)
    {
        new id, rank;
        cache_get_value_name_int(row, "faction_id", id);
        cache_get_value_name_int(row, "rank", rank);
        if (!(1 <= id <= KM_MAX_FACTIONS) || !(1 <= rank <= KM_MAX_RANKS)) continue;
        cache_get_value_name(row, "name", gFacRankName[id][rank], 32);
        cache_get_value_name_int(row, "salary", gFacRankSalary[id][rank]);
    }
    print("[faction] factions loaded");
    return 1;
}

stock bool:Fac_Valid(id)
{
    return 1 <= id <= KM_MAX_FACTIONS && gFac[id][fUsed];
}

stock Fac_MaxRank(id)
{
    return Fac_Valid(id) ? gFac[id][fMaxRank] : 0;
}

stock Fac_Salary(id, rank)
{
    if (!Fac_Valid(id) || !(1 <= rank <= KM_MAX_RANKS)) return 0;
    return gFacRankSalary[id][rank];
}

stock Fac_Type(id)
{
    return Fac_Valid(id) ? gFac[id][fType] : 0;
}

stock Fac_RankName(id, rank, dest[], size = sizeof (dest))
{
    if (Fac_Valid(id) && 1 <= rank <= KM_MAX_RANKS && gFacRankName[id][rank][0] != '\0') format(dest, size, "%s", gFacRankName[id][rank]);
    else format(dest, size, "Rank %d", rank);
}

stock Fac_DutySkin(id, gender)
{
    switch (Fac_Type(id))
    {
        case FACTION_POLICE: return gender == 1 ? 306 : 280;
        case FACTION_MEDICAL: return gender == 1 ? 308 : 274;
        case FACTION_GOVERNMENT: return gender == 1 ? 141 : 147;
    }
    return 0;
}

stock bool:Fac_IsOnDutyType(playerid, type)
{
    return Player_IsPlaying(playerid) && gChar[playerid][cOnDuty] && Fac_Type(gChar[playerid][cFaction]) == type;
}

stock bool:Fac_IsLeader(playerid)
{
    new id = gChar[playerid][cFaction];
    return Fac_Valid(id) && gChar[playerid][cFactionRank] >= gFac[id][fMaxRank] - 1;
}

stock Fac_SaveMember(playerid)
{
    new query[160];
    mysql_format(gDB, query, sizeof (query), "UPDATE characters SET faction_id = NULLIF(%d, 0), faction_rank = %d WHERE id = %d",
        gChar[playerid][cFaction], gChar[playerid][cFactionRank], gChar[playerid][cId]);
    mysql_tquery(gDB, query);
}

stock Fac_SetDuty(playerid, bool:onDuty)
{
    gChar[playerid][cOnDuty] = onDuty;
    ResetPlayerWeapons(playerid);
    if (onDuty)
    {
        SetPlayerColor(playerid, gFac[gChar[playerid][cFaction]][fColour]);
        switch (Fac_Type(gChar[playerid][cFaction]))
        {
            case FACTION_POLICE:
            {
                GivePlayerWeapon(playerid, WEAPON_NITESTICK, 1);
                GivePlayerWeapon(playerid, WEAPON_DEAGLE, 50);
                SetPlayerArmour(playerid, 100.0);
            }
            case FACTION_MEDICAL: GivePlayerWeapon(playerid, WEAPON_FIREEXTINGUISHER, 500);
        }
    }
    else
    {
        SetPlayerColor(playerid, COLOR_WHITE);
    }
    Char_ApplySkin(playerid);
}

// Faction radio. Returns how many players received it.
stock Fac_Broadcast(id, colour, const text[])
{
    new count = 0;
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (Player_IsPlaying(i) && gChar[i][cFaction] == id)
        {
            SendClientMessage(i, colour, "%s", text);
            count++;
        }
    }
    return count;
}

// /911 calls go to on-duty police and medics.
stock Fac_Dispatch(callerid, const text[])
{
    new Float:x, Float:y, Float:z;
    GetPlayerPos(callerid, x, y, z);
    new count = 0;
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (Fac_IsOnDutyType(i, FACTION_POLICE) || Fac_IsOnDutyType(i, FACTION_MEDICAL))
        {
            SendClientMessage(i, COLOR_DEPARTMENT, "[911] Caller %s (phone %d): %s", gChar[callerid][cName], gChar[callerid][cPhone], text);
            SendClientMessage(i, COLOR_DEPARTMENT, "[911] Location marked on your map.");
            SetPlayerCheckpoint(i, x, y, z, 5.0);
            gJob[i][jsGps] = true;
            count++;
        }
    }
    return count;
}

// ---- faction commands ------------------------------------------------------

CMD:f(playerid, params[])
{
    new id = gChar[playerid][cFaction];
    if (!Fac_Valid(id)) return SendError(playerid, "You're not in a faction.");
    if (isnull(params)) return SendInfo(playerid, "Usage: /f [faction radio]");
    if (!Chat_CanTalk(playerid, params)) return 1;
    new rank[32], name[MAX_PLAYER_NAME], line[160];
    Fac_RankName(id, gChar[playerid][cFactionRank], rank);
    Chat_DisplayName(playerid, name);
    format(line, sizeof (line), "** [%s] %s %s: %s **", gFac[id][fShort], rank, name, params);
    Fac_Broadcast(id, COLOR_RADIO, line);
    return 1;
}

// Department radio between on-duty police and medics.
CMD:d(playerid, params[])
{
    new type = Fac_Type(gChar[playerid][cFaction]);
    if ((type != FACTION_POLICE && type != FACTION_MEDICAL) || !gChar[playerid][cOnDuty]) return SendError(playerid, "Only on-duty police and medics use the department radio.");
    if (isnull(params)) return SendInfo(playerid, "Usage: /d [department radio]");
    if (!Chat_CanTalk(playerid, params)) return 1;
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (Fac_IsOnDutyType(i, FACTION_POLICE) || Fac_IsOnDutyType(i, FACTION_MEDICAL))
        {
            SendClientMessage(i, COLOR_DEPARTMENT, "** [%s] %s: %s **", gFac[gChar[playerid][cFaction]][fShort], gChar[playerid][cName], params);
        }
    }
    return 1;
}

CMD:duty(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    new id = gChar[playerid][cFaction];
    if (!Fac_Valid(id)) return SendError(playerid, "You're not in a faction.");
    if (!gChar[playerid][cOnDuty] && !IsPlayerInRangeOfPoint(playerid, 5.0, gFac[id][fHqX], gFac[id][fHqY], gFac[id][fHqZ]))
    {
        if (gJob[playerid][jsWorking]) return SendError(playerid, "Go on duty at your headquarters.");
        SendError(playerid, "Go on duty at your headquarters. It's marked on your map.");
        SetPlayerCheckpoint(playerid, gFac[id][fHqX], gFac[id][fHqY], gFac[id][fHqZ], 3.0);
        gJob[playerid][jsGps] = true;
        return 1;
    }
    if (gJob[playerid][jsWorking]) return SendError(playerid, "End your job shift first (/stopwork).");
    Fac_SetDuty(playerid, !gChar[playerid][cOnDuty]);
    new line[96];
    format(line, sizeof (line), "%s is now %s duty.", gChar[playerid][cName], gChar[playerid][cOnDuty] ? ("on") : ("off"));
    Chat_FixName(line);
    Fac_Broadcast(id, COLOR_RADIO, line);
    return 1;
}

CMD:members(playerid, params[])
{
    new id = gChar[playerid][cFaction];
    if (!Fac_Valid(id)) return SendError(playerid, "You're not in a faction.");
    new body[1024], line[96], rank[32];
    body = "Name\tRank\tDuty\n";
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (!Player_IsPlaying(i) || gChar[i][cFaction] != id) continue;
        Fac_RankName(id, gChar[i][cFactionRank], rank);
        format(line, sizeof (line), "%s [%d]\t%s\t%s\n", gChar[i][cName], i, rank, gChar[i][cOnDuty] ? ("On duty") : ("Off duty"));
        strcat(body, line);
    }
    ShowPlayerDialog(playerid, DIALOG_INFO, DIALOG_STYLE_TABLIST_HEADERS, gFac[id][fName], body, "Close", "");
    return 1;
}

CMD:invite(playerid, params[])
{
    new id = gChar[playerid][cFaction], targetid;
    if (!Fac_Valid(id) || !Fac_IsLeader(playerid)) return SendError(playerid, "Only faction leaders can invite.");
    if (sscanf(params, "u", targetid)) return SendInfo(playerid, "Usage: /invite [player]");
    if (!Player_IsPlaying(targetid) || targetid == playerid) return SendError(playerid, "That player isn't available.");
    if (gChar[targetid][cFaction] != 0) return SendError(playerid, "They're already in a faction.");
    gFacInvite[targetid] = id;
    SendClientMessage(targetid, COLOR_INFO, "%s invited you to join %s. Type /joinfaction to accept.", gChar[playerid][cName], gFac[id][fName]);
    SendClientMessage(playerid, COLOR_SUCCESS, "Invitation sent to %s.", gChar[targetid][cName]);
    return 1;
}

CMD:joinfaction(playerid, params[])
{
    new id = gFacInvite[playerid];
    if (!Fac_Valid(id) || !Player_IsPlaying(playerid)) return SendError(playerid, "You don't have a faction invitation.");
    if (gChar[playerid][cFaction] != 0) return SendError(playerid, "Leave your current faction first.");
    gFacInvite[playerid] = 0;
    gChar[playerid][cFaction] = id;
    gChar[playerid][cFactionRank] = 1;
    Fac_SaveMember(playerid);
    new line[96];
    format(line, sizeof (line), "%s joined the faction.", gChar[playerid][cName]);
    Chat_FixName(line);
    Fac_Broadcast(id, COLOR_RADIO, line);
    return 1;
}

stock Fac_Remove(playerid)
{
    if (gChar[playerid][cOnDuty]) Fac_SetDuty(playerid, false);
    gChar[playerid][cFaction] = 0;
    gChar[playerid][cFactionRank] = 0;
    Fac_SaveMember(playerid);
}

CMD:uninvite(playerid, params[])
{
    new id = gChar[playerid][cFaction], targetid;
    if (!Fac_Valid(id) || !Fac_IsLeader(playerid)) return SendError(playerid, "Only faction leaders can remove members.");
    if (isnull(params)) return SendInfo(playerid, "Usage: /uninvite [player, or First_Last for someone offline]");
    if (sscanf(params, "u", targetid) || !Player_IsPlaying(targetid))
    {
        // Not online: remove by character name.
        if (!IsValidCharacterName(params)) return SendError(playerid, "Nobody online by that name. For an offline member, use their First_Last name.");
        new query[256];
        mysql_format(gDB, query, sizeof (query), FAC_REMOVE_OFFLINE_SQL, params, id, gChar[playerid][cFactionRank]);
        mysql_tquery(gDB, query, "Fac_OnRemovedOffline", "dds", playerid, gPlayerRace[playerid], params);
        return 1;
    }
    if (gChar[targetid][cFaction] != id) return SendError(playerid, "They're not in your faction.");
    if (targetid != playerid && gChar[targetid][cFactionRank] >= gChar[playerid][cFactionRank]) return SendError(playerid, "You can only remove lower ranks.");
    Fac_Remove(targetid);
    SendClientMessage(targetid, COLOR_INFO, "You were removed from %s.", gFac[id][fName]);
    if (targetid != playerid) SendClientMessage(playerid, COLOR_SUCCESS, "%s was removed from the faction.", gChar[targetid][cName]);
    return 1;
}

public Fac_OnRemovedOffline(playerid, race, const name[])
{
    if (gPlayerRace[playerid] != race) return 1;
    if (cache_affected_rows() == 0) return SendError(playerid, "No lower-ranked member of your faction has that name."), 1;
    SendClientMessage(playerid, COLOR_SUCCESS, "%s was removed from the faction.", name);
    return 1;
}

// Every member, online or not.
CMD:roster(playerid, params[])
{
    new id = gChar[playerid][cFaction];
    if (!Fac_Valid(id) || !Fac_IsLeader(playerid)) return SendError(playerid, "Only faction leaders can see the full roster.");
    new query[256];
    mysql_format(gDB, query, sizeof (query), FAC_ROSTER_SQL, id);
    mysql_tquery(gDB, query, "Fac_OnRoster", "dd", playerid, gPlayerRace[playerid]);
    return 1;
}

public Fac_OnRoster(playerid, race)
{
    if (gPlayerRace[playerid] != race) return 1;
    new id = gChar[playerid][cFaction];
    if (!Fac_Valid(id)) return 1;
    new body[2048], line[80], name[MAX_PLAYER_NAME], rank, rankName[32], seen[12];
    body = "Name\tRank\tLast played\n";
    for (new i = 0, rows = cache_num_rows(); i < rows; i++)
    {
        cache_get_value_name(i, "name", name);
        cache_get_value_name_int(i, "faction_rank", rank);
        cache_get_value_name(i, "seen", seen);
        Fac_RankName(id, rank, rankName);
        format(line, sizeof (line), "%s\t%s\t%s\n", name, rankName, seen);
        strcat(body, line);
    }
    ShowPlayerDialog(playerid, DIALOG_INFO, DIALOG_STYLE_TABLIST_HEADERS, gFac[id][fName], body, "Close", "");
    return 1;
}

// Called on disconnect, before the character is saved.
stock Fac_OnLeaveWhileCuffed(playerid)
{
    if (!gChar[playerid][cCuffed]) return;
    gChar[playerid][cCuffed] = false;
    new minutes = Clamp(5 * (gChar[playerid][cWanted] + 1), 5, KM_JAIL_MAX_MIN);
    if (gChar[playerid][cJailSeconds] < minutes * 60) gChar[playerid][cJailSeconds] = minutes * 60;
    gChar[playerid][cWanted] = 0;
    new query[256];
    mysql_format(gDB, query, sizeof (query), "INSERT INTO criminal_records (character_id, officer_character_id, charge) VALUES (%d, NULL, '%e')",
        gChar[playerid][cId], FLED_CUSTODY_CHARGE);
    mysql_tquery(gDB, query);
    new line[128];
    format(line, sizeof (line), "[Dispatch] %s left while cuffed and will serve %d minutes when they return.", gChar[playerid][cName], minutes);
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (i != playerid && Fac_IsOnDutyType(i, FACTION_POLICE)) SendClientMessage(i, COLOR_DEPARTMENT, "%s", line);
    }
}

CMD:quitfaction(playerid, params[])
{
    new id = gChar[playerid][cFaction];
    if (!Fac_Valid(id)) return SendError(playerid, "You're not in a faction.");
    Fac_Remove(playerid);
    SendClientMessage(playerid, COLOR_INFO, "You left %s.", gFac[id][fName]);
    return 1;
}

CMD:setrank(playerid, params[])
{
    new id = gChar[playerid][cFaction], targetid, rank;
    if (!Fac_Valid(id) || !Fac_IsLeader(playerid)) return SendError(playerid, "Only faction leaders can change ranks.");
    if (sscanf(params, "ud", targetid, rank)) return SendInfo(playerid, "Usage: /setrank [player] [rank]");
    if (!Player_IsPlaying(targetid) || gChar[targetid][cFaction] != id || targetid == playerid) return SendError(playerid, "They're not an online member of your faction.");
    if (!(1 <= rank < gChar[playerid][cFactionRank])) return SendError(playerid, "You can only set ranks below your own.");
    if (gChar[targetid][cFactionRank] >= gChar[playerid][cFactionRank]) return SendError(playerid, "You can't change the rank of someone at or above your rank.");
    gChar[targetid][cFactionRank] = rank;
    Fac_SaveMember(targetid);
    new name[32];
    Fac_RankName(id, rank, name);
    SendClientMessage(targetid, COLOR_INFO, "Your rank is now %s.", name);
    SendClientMessage(playerid, COLOR_SUCCESS, "%s is now %s.", gChar[targetid][cName], name);
    return 1;
}

CMD:setfaction(playerid, params[])
{
    new targetid, id, rank;
    if (sscanf(params, "udd", targetid, id, rank)) return SendInfo(playerid, "Usage: /setfaction [player] [faction id, 0 = none] [rank]");
    if (!Mod_CanAct(playerid, INVALID_PLAYER_ID, ADMIN_LEAD)) return 1;
    if (!Player_IsPlaying(targetid)) return SendError(playerid, "That player isn't playing.");
    if (id != 0 && !Fac_Valid(id)) return SendError(playerid, "Unknown faction id.");
    if (id != 0 && !(1 <= rank <= gFac[id][fMaxRank])) return SendClientMessage(playerid, COLOR_ERROR, "[!] Rank must be 1-%d.", gFac[id][fMaxRank]);
    if (gChar[targetid][cOnDuty]) Fac_SetDuty(targetid, false);
    gChar[targetid][cFaction] = id;
    gChar[targetid][cFactionRank] = id == 0 ? 0 : rank;
    Fac_SaveMember(targetid);
    new details[48];
    format(details, sizeof (details), "faction %d rank %d", id, gChar[targetid][cFactionRank]);
    Log_Admin(Mod_ActorId(playerid), "set_faction", gPlayer[targetid][pAccountId], details);
    if (id == 0) SendClientMessage(targetid, COLOR_ADMIN, "An admin removed you from your faction.");
    else SendClientMessage(targetid, COLOR_ADMIN, "An admin put you in %s at rank %d.", gFac[id][fName], rank);
    SendClientMessage(playerid, COLOR_ADMIN, "Done: %s.", details);
    return 1;
}

// ---- police ----------------------------------------------------------------

stock bool:Fac_PoliceCheck(playerid)
{
    if (!Fac_IsOnDutyType(playerid, FACTION_POLICE))
    {
        SendError(playerid, "You need to be an on-duty police officer.");
        return false;
    }
    return true;
}

CMD:cuff(playerid, params[])
{
    new targetid;
    if (!Fac_PoliceCheck(playerid)) return 1;
    if (sscanf(params, "u", targetid)) return SendInfo(playerid, "Usage: /cuff [player]");
    if (!Player_IsPlaying(targetid) || targetid == playerid || !Player_IsNear(playerid, targetid, 2.5)) return SendError(playerid, "They need to be right next to you.");
    if (IsPlayerInAnyVehicle(targetid)) return SendError(playerid, "Get them out of the vehicle first.");
    if (gChar[targetid][cCuffed]) return SendError(playerid, "They're already cuffed.");
    gChar[targetid][cCuffed] = true;
    SetPlayerSpecialAction(targetid, SPECIAL_ACTION_CUFFED);
    new action[64];
    format(action, sizeof (action), "puts handcuffs on %s.", gChar[targetid][cName]);
    Chat_Action(playerid, action);
    return 1;
}

CMD:uncuff(playerid, params[])
{
    new targetid;
    if (!Fac_PoliceCheck(playerid)) return 1;
    if (sscanf(params, "u", targetid)) return SendInfo(playerid, "Usage: /uncuff [player]");
    if (!Player_IsPlaying(targetid) || !gChar[targetid][cCuffed] || !Player_IsNear(playerid, targetid, 2.5)) return SendError(playerid, "They need to be cuffed and right next to you.");
    gChar[targetid][cCuffed] = false;
    SetPlayerSpecialAction(targetid, SPECIAL_ACTION_NONE);
    new action[64];
    format(action, sizeof (action), "removes the handcuffs from %s.", gChar[targetid][cName]);
    Chat_Action(playerid, action);
    return 1;
}

stock Fac_AddRecord(officerid, targetid, const charge[])
{
    new query[320];
    mysql_format(gDB, query, sizeof (query), "INSERT INTO criminal_records (character_id, officer_character_id, charge) VALUES (%d, %d, '%e')",
        gChar[targetid][cId], gChar[officerid][cId], charge);
    mysql_tquery(gDB, query);
}

stock Fac_SetWanted(playerid, level)
{
    gChar[playerid][cWanted] = Clamp(level, 0, KM_MAX_WANTED);
    SetPlayerWantedLevel(playerid, gChar[playerid][cWanted]);
    new query[96];
    mysql_format(gDB, query, sizeof (query), "UPDATE characters SET wanted = %d WHERE id = %d", gChar[playerid][cWanted], gChar[playerid][cId]);
    mysql_tquery(gDB, query);
}

// Suspect: records a charge and raises the wanted level by one.
CMD:su(playerid, params[])
{
    new targetid, charge[96];
    if (!Fac_PoliceCheck(playerid)) return 1;
    if (sscanf(params, "us[96]", targetid, charge)) return SendInfo(playerid, "Usage: /su [player] [charge]");
    if (!Player_IsPlaying(targetid) || targetid == playerid) return SendError(playerid, "That player isn't available.");
    Fac_AddRecord(playerid, targetid, charge);
    Fac_SetWanted(targetid, gChar[targetid][cWanted] + 1);
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (Fac_IsOnDutyType(i, FACTION_POLICE))
        {
            SendClientMessage(i, COLOR_DEPARTMENT, "[Dispatch] %s charged %s: %s (wanted level %d)", gChar[playerid][cName], gChar[targetid][cName], charge, gChar[targetid][cWanted]);
        }
    }
    return 1;
}

CMD:clearwanted(playerid, params[])
{
    new targetid;
    if (!Fac_PoliceCheck(playerid)) return 1;
    if (gChar[playerid][cFactionRank] < 3) return SendError(playerid, "You need rank 3 or higher.");
    if (sscanf(params, "u", targetid)) return SendInfo(playerid, "Usage: /clearwanted [player]");
    if (!Player_IsPlaying(targetid)) return SendError(playerid, "That player isn't playing.");
    Fac_SetWanted(targetid, 0);
    SendClientMessage(playerid, COLOR_SUCCESS, "Cleared the wanted level of %s.", gChar[targetid][cName]);
    return 1;
}

CMD:arrest(playerid, params[])
{
    new targetid, minutes;
    if (!Fac_PoliceCheck(playerid)) return 1;
    if (sscanf(params, "ud", targetid, minutes)) return SendInfo(playerid, "Usage: /arrest [player] [minutes]");
    if (!Place_Near(playerid, gArrestPoint, 8.0)) return SendError(playerid, "Bring them to the LSPD garage to book them.");
    if (!Player_IsPlaying(targetid) || targetid == playerid || !Player_IsNear(playerid, targetid, 5.0)) return SendError(playerid, "They need to be with you.");
    if (!gChar[targetid][cCuffed]) return SendError(playerid, "Cuff them first.");
    if (gChar[targetid][cWanted] == 0) return SendError(playerid, "They have no charges. Use /su first.");
    if (!(1 <= minutes <= KM_JAIL_MAX_MIN)) return SendClientMessage(playerid, COLOR_ERROR, "[!] Jail time is 1-%d minutes.", KM_JAIL_MAX_MIN);
    gChar[targetid][cCuffed] = false;
    SetPlayerSpecialAction(targetid, SPECIAL_ACTION_NONE);
    Fac_SetWanted(targetid, 0);
    Jail_Start(targetid, minutes * 60);
    SendClientMessageToAll(COLOR_DEPARTMENT, "[LSPD] %s was arrested by %s and jailed for %d minute(s).", gChar[targetid][cName], gChar[playerid][cName], minutes);
    return 1;
}

// MDT lookup by player id or character name (online or not).
CMD:mdt(playerid, params[])
{
    if (!Fac_PoliceCheck(playerid)) return 1;
    if (isnull(params)) return SendInfo(playerid, "Usage: /mdt [player id or First_Last name]");
    new query[MDT_QUERY_SIZE], targetid, characterId = 0;
    if (!sscanf(params, "u", targetid) && Player_IsPlaying(targetid)) characterId = gChar[targetid][cId];
    mysql_format(gDB, query, sizeof (query), MDT_PERSON_SQL, characterId, characterId, params);
    mysql_tquery(gDB, query, "Fac_OnMdt", "dd", playerid, gPlayerRace[playerid]);
    return 1;
}

new gMdtBody[MAX_PLAYERS][1200];

public Fac_OnMdt(playerid, race)
{
    if (gPlayerRace[playerid] != race) return 1;
    if (cache_num_rows() == 0) return SendError(playerid, "No record found. Names are First_Last."), 1;
    new id, name[MAX_PLAYER_NAME], wanted, jail, phone, licence, tickets, owed, vehicles[96], bool:noVehicles;
    cache_get_value_name_int(0, "id", id);
    cache_get_value_name(0, "name", name);
    cache_get_value_name_int(0, "wanted", wanted);
    cache_get_value_name_int(0, "jail_seconds", jail);
    cache_get_value_name_int(0, "phone", phone);
    cache_get_value_name_int(0, "driving_licence", licence);
    cache_get_value_name_int(0, "tickets", tickets);
    cache_get_value_name_int(0, "owed", owed);
    cache_is_value_name_null(0, "vehicles", noVehicles);
    if (!noVehicles) cache_get_value_name(0, "vehicles", vehicles);

    new plates[160] = "none";
    if (!noVehicles)
    {
        // GROUP_CONCAT of ids, shown as plates.
        plates[0] = '\0';
        new pos = 0, vid, plate[12];
        while (vehicles[pos] != '\0')
        {
            vid = strval(vehicles[pos]);
            Plate_Format(plate, vid);
            if (plates[0] != '\0') strcat(plates, ", ");
            strcat(plates, plate);
            while (vehicles[pos] != '\0' && vehicles[pos] != ',') pos++;
            if (vehicles[pos] == ',') pos++;
        }
    }
    format(gMdtBody[playerid], sizeof (gMdtBody[]), EMBED_BRAND "%s\n" EMBED_WHITE "Wanted level: %d\nIn jail: %s\nDriving licence: %s\nUnpaid tickets: %d ($%d)\nPhone: %d\nVehicles: %s\n\n" EMBED_BRAND "Records\n" EMBED_WHITE,
        name, wanted, jail > 0 ? ("yes") : ("no"), licence ? ("valid") : ("none"), tickets, owed, phone, plates);
    new query[320];
    mysql_format(gDB, query, sizeof (query), MDT_RECORDS_SQL, id);
    mysql_tquery(gDB, query, "Fac_OnMdtRecords", "ddd", playerid, race, id);
    return 1;
}

public Fac_OnMdtRecords(playerid, race, characterId)
{
    if (gPlayerRace[playerid] != race) return 1;
    new rows = cache_num_rows(), charge[96], day[12], officer[MAX_PLAYER_NAME], line[160];
    if (rows == 0) strcat(gMdtBody[playerid], "No records.", sizeof (gMdtBody[]));
    for (new i = 0; i < rows; i++)
    {
        cache_get_value_name(i, "charge", charge);
        cache_get_value_name(i, "day", day);
        cache_get_value_name(i, "officer", officer);
        format(line, sizeof (line), "%s  %s  (%s)\n", day, charge, officer);
        strcat(gMdtBody[playerid], line, sizeof (gMdtBody[]));
    }
    ShowPlayerDialog(playerid, DIALOG_INFO, DIALOG_STYLE_MSGBOX, "LSPD Mobile Data Terminal", gMdtBody[playerid], "Close", "");
    return 1;
}

// ---- medics ----------------------------------------------------------------

CMD:heal(playerid, params[])
{
    new targetid, price;
    if (!Fac_IsOnDutyType(playerid, FACTION_MEDICAL)) return SendError(playerid, "You need to be an on-duty medic.");
    if (sscanf(params, "ud", targetid, price)) return SendInfo(playerid, "Usage: /heal [player] [price]");
    if (!Player_IsPlaying(targetid) || targetid == playerid || !Player_IsNear(playerid, targetid, 4.0)) return SendError(playerid, "They need to be next to you.");
    if (!(0 <= price <= KM_SERVICE_PRICE_MAX)) return SendClientMessage(playerid, COLOR_ERROR, "[!] Price must be between $0 and $%d.", KM_SERVICE_PRICE_MAX);
    Offer_Make(playerid, targetid, OFFER_HEAL, price);
    return 1;
}

// ---- jail ------------------------------------------------------------------

stock Jail_Start(playerid, seconds)
{
    gChar[playerid][cJailSeconds] = seconds;
    if (gChar[playerid][cOnDuty]) Fac_SetDuty(playerid, false);
    Job_Stop(playerid);
    if (IsPlayerInAnyVehicle(playerid)) RemovePlayerFromVehicle(playerid, true);
    Jail_Place(playerid);
    Char_Save(playerid);
}

stock Jail_Place(playerid)
{
    ResetPlayerWeapons(playerid);
    SetPlayerInterior(playerid, KM_JAIL_INTERIOR);
    SetPlayerVirtualWorld(playerid, 0);
    SetPlayerPos(playerid, gJailCell[ptX], gJailCell[ptY], gJailCell[ptZ]);
    SetPlayerFacingAngle(playerid, gJailCell[ptA]);
    SetCameraBehindPlayer(playerid);
    new left[24];
    FormatDuration(left, gChar[playerid][cJailSeconds]);
    SendClientMessage(playerid, COLOR_ERROR, "You're in jail for another %s.", left);
}

stock Jail_Release(playerid)
{
    gChar[playerid][cJailSeconds] = 0;
    SetPlayerInterior(playerid, 0);
    SetPlayerVirtualWorld(playerid, 0);
    SetPlayerPos(playerid, gJailRelease[ptX], gJailRelease[ptY], gJailRelease[ptZ]);
    SetPlayerFacingAngle(playerid, gJailRelease[ptA]);
    SetCameraBehindPlayer(playerid);
    SendClientMessage(playerid, COLOR_SUCCESS, "You served your time and were released.");
    Char_Save(playerid);
}

// Once per second while playing.
stock Jail_Tick(playerid)
{
    if (gChar[playerid][cJailSeconds] <= 0) return;
    if (GetPlayerState(playerid) == PLAYER_STATE_WASTED) return;
    if (--gChar[playerid][cJailSeconds] <= 0)
    {
        Jail_Release(playerid);
        return;
    }
    // Keep them in the cell.
    if (GetPlayerInterior(playerid) != KM_JAIL_INTERIOR || !Place_Near(playerid, gJailCell, 12.0)) Jail_Place(playerid);
}

CMD:ajail(playerid, params[])
{
    new targetid, minutes, reason[64];
    if (sscanf(params, "uds[64]", targetid, minutes, reason)) return SendInfo(playerid, "Usage: /ajail [player] [minutes, 0 = release] [reason]");
    if (!Player_IsPlaying(targetid)) return SendError(playerid, "That player isn't playing.");
    if (!Mod_CanAct(playerid, targetid, ADMIN_MOD)) return 1;
    if (!(0 <= minutes <= 600)) return SendError(playerid, "Minutes must be 0-600.");
    new details[96];
    format(details, sizeof (details), "%d min: %s", minutes, reason);
    Log_Admin(Mod_ActorId(playerid), "admin_jail", gPlayer[targetid][pAccountId], details);
    if (minutes == 0)
    {
        if (gChar[targetid][cJailSeconds] > 0) Jail_Release(targetid);
        SendClientMessage(playerid, COLOR_ADMIN, "Released.");
        return 1;
    }
    SendClientMessageToAll(COLOR_ADMIN, "%s was jailed by an admin for %d minute(s). Reason: %s", gPlayer[targetid][pUsername], minutes, reason);
    Jail_Start(targetid, minutes * 60);
    return 1;
}
