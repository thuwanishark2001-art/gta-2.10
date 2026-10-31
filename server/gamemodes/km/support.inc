// Player support and staff tools.
//   Players: /report (to admins), /helpme (to helpers).
//   Staff: /reports, /ar, /cr (admins, all requests); helpers the same for
//   help requests. /a admin chat, /h helper chat, /sethelper, and admin
//   tools: /goto, /gethere, /freeze, /unfreeze, /check, /givemoney.
// Requests are stored in the reports table; every staff action in admin_log.

#define REPORT_KIND_REPORT  (1)
#define REPORT_KIND_HELP    (2)

#define REPORT_OPEN         (0)
#define REPORT_ACCEPTED     (1)
#define REPORT_CLOSED       (2)

#define HELPER_LEVEL_MAX    (3)

new gLastReport[MAX_PLAYERS];

#define REPORT_LIST_SQL \
    "SELECT r.id, r.kind, r.status, a.username, r.text, TIMESTAMPDIFF(MINUTE, r.created_at, UTC_TIMESTAMP()) AS age \
     FROM reports r JOIN accounts a ON a.id = r.reporter_account_id \
     WHERE r.status <> %d AND r.kind >= %d ORDER BY r.id LIMIT 15"

// Helpers (minKind = help requests) can only take help requests.
#define REPORT_UPDATE_SQL \
    "UPDATE reports SET status = %d, handled_by = %d, closed_at = IF(%d = %d, UTC_TIMESTAMP(), NULL) \
     WHERE id = %d AND status <> %d AND kind >= %d"

forward Sup_OnReportCreated(playerid, race, kind, const text[]);
forward Sup_OnReportList(playerid, race);
forward Sup_OnReportUpdated(playerid, race, reportId, newStatus);
forward Sup_OnReporterFound(playerid, race, reportId, newStatus);

stock bool:Sup_IsAdmin(playerid)
{
    return gPlayer[playerid][pAdmin] >= ADMIN_MOD || IsPlayerAdmin(playerid);
}

stock bool:Sup_IsStaff(playerid)
{
    return Sup_IsAdmin(playerid) || gPlayer[playerid][pHelper] >= 1;
}

// Sends to online admins (kind 1) or to admins and helpers (kind 2).
stock Sup_NotifyStaff(kind, colour, const text[])
{
    new count = 0;
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (!Player_IsLoggedIn(i)) continue;
        if (kind == REPORT_KIND_REPORT ? Sup_IsAdmin(i) : Sup_IsStaff(i))
        {
            SendClientMessage(i, colour, "%s", text);
            count++;
        }
    }
    return count;
}

stock Sup_Create(playerid, kind, const text[])
{
    if (!Player_IsLoggedIn(playerid)) return 1;
    if (gLastReport[playerid] != 0 && gettime() - gLastReport[playerid] < KM_REPORT_COOLDOWN)
    {
        return SendClientMessage(playerid, COLOR_ERROR, "[!] Wait %d seconds before sending another request.", KM_REPORT_COOLDOWN - (gettime() - gLastReport[playerid]));
    }
    if (strlen(text) < 4) return SendError(playerid, "Describe the problem in a few words.");
    gLastReport[playerid] = gettime();
    new query[320];
    mysql_format(gDB, query, sizeof (query), "INSERT INTO reports (kind, reporter_account_id, text) VALUES (%d, %d, '%e')",
        kind, gPlayer[playerid][pAccountId], text);
    mysql_tquery(gDB, query, "Sup_OnReportCreated", "ddds", playerid, gPlayerRace[playerid], kind, text);
    return 1;
}

public Sup_OnReportCreated(playerid, race, kind, const text[])
{
    new id = cache_insert_id();
    if (gPlayerRace[playerid] != race || id <= 0) return 1;
    new line[200];
    format(line, sizeof (line), "[%s #%d] %s [%d]: %s", kind == REPORT_KIND_REPORT ? ("Report") : ("Help"), id, gPlayer[playerid][pUsername], playerid, text);
    new staff = Sup_NotifyStaff(kind, kind == REPORT_KIND_REPORT ? COLOR_ADMIN : COLOR_INFO, line);
    if (staff > 0) SendClientMessage(playerid, COLOR_SUCCESS, "Request #%d sent to %d staff member(s) online.", id, staff);
    else SendClientMessage(playerid, COLOR_SUCCESS, "Request #%d saved. No staff are online right now; it'll be picked up later.", id);
    return 1;
}

CMD:report(playerid, params[])
{
    if (isnull(params)) return SendInfo(playerid, "Usage: /report [who and what happened]");
    return Sup_Create(playerid, REPORT_KIND_REPORT, params);
}

CMD:helpme(playerid, params[])
{
    if (isnull(params)) return SendInfo(playerid, "Usage: /helpme [your question]");
    return Sup_Create(playerid, REPORT_KIND_HELP, params);
}

CMD:reports(playerid, params[])
{
    if (!Sup_IsStaff(playerid)) return SendError(playerid, "Staff only.");
    new query[400];
    mysql_format(gDB, query, sizeof (query), REPORT_LIST_SQL,
        REPORT_CLOSED, Sup_IsAdmin(playerid) ? REPORT_KIND_REPORT : REPORT_KIND_HELP);
    mysql_tquery(gDB, query, "Sup_OnReportList", "dd", playerid, gPlayerRace[playerid]);
    return 1;
}

public Sup_OnReportList(playerid, race)
{
    if (gPlayerRace[playerid] != race) return 1;
    new rows = cache_num_rows();
    if (rows == 0) return SendInfo(playerid, "No open requests."), 1;
    SendClientMessage(playerid, COLOR_BRAND, "Open requests (/ar [id] to accept, /cr [id] [note] to close):");
    for (new i = 0; i < rows; i++)
    {
        new id, kind, status, age, name[MAX_PLAYER_NAME], text[128];
        cache_get_value_name_int(i, "id", id);
        cache_get_value_name_int(i, "kind", kind);
        cache_get_value_name_int(i, "status", status);
        cache_get_value_name_int(i, "age", age);
        cache_get_value_name(i, "username", name);
        cache_get_value_name(i, "text", text);
        SendClientMessage(playerid, status == REPORT_ACCEPTED ? COLOR_GREY : COLOR_WHITE, "#%d %s %s, %d min ago%s: %s",
            id, kind == REPORT_KIND_REPORT ? ("report") : ("help"), name, age, status == REPORT_ACCEPTED ? (" (taken)") : (""), text);
    }
    return 1;
}

stock Sup_Update(playerid, reportId, newStatus)
{
    new query[320];
    mysql_format(gDB, query, sizeof (query), REPORT_UPDATE_SQL,
        newStatus, gPlayer[playerid][pAccountId], newStatus, REPORT_CLOSED, reportId, REPORT_CLOSED,
        Sup_IsAdmin(playerid) ? REPORT_KIND_REPORT : REPORT_KIND_HELP);
    mysql_tquery(gDB, query, "Sup_OnReportUpdated", "dddd", playerid, gPlayerRace[playerid], reportId, newStatus);
}

public Sup_OnReportUpdated(playerid, race, reportId, newStatus)
{
    if (gPlayerRace[playerid] != race) return 1;
    if (cache_affected_rows() == 0) return SendError(playerid, "No open request with that number that you can handle."), 1;
    new details[32];
    format(details, sizeof (details), "report #%d", reportId);
    Log_Admin(gPlayer[playerid][pAccountId], newStatus == REPORT_CLOSED ? ("close_report") : ("accept_report"), 0, details);
    new query[128];
    mysql_format(gDB, query, sizeof (query), "SELECT reporter_account_id FROM reports WHERE id = %d", reportId);
    mysql_tquery(gDB, query, "Sup_OnReporterFound", "dddd", playerid, race, reportId, newStatus);
    return 1;
}

public Sup_OnReporterFound(playerid, race, reportId, newStatus)
{
    if (gPlayerRace[playerid] != race || cache_num_rows() == 0) return 1;
    new accountId;
    cache_get_value_index_int(0, 0, accountId);
    new line[128];
    format(line, sizeof (line), "%s %s request #%d.", gPlayer[playerid][pUsername], newStatus == REPORT_CLOSED ? ("closed") : ("took"), reportId);
    Sup_NotifyStaff(REPORT_KIND_HELP, COLOR_GREY, line);
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (Player_IsLoggedIn(i) && gPlayer[i][pAccountId] == accountId)
        {
            if (newStatus == REPORT_CLOSED) SendClientMessage(i, COLOR_SUCCESS, "Your request #%d was closed by %s.", reportId, gPlayer[playerid][pUsername]);
            else SendClientMessage(i, COLOR_SUCCESS, "%s is handling your request #%d. Answer them with /pm %d.", gPlayer[playerid][pUsername], reportId, playerid);
        }
    }
    return 1;
}

CMD:ar(playerid, params[])
{
    new id;
    if (!Sup_IsStaff(playerid)) return SendError(playerid, "Staff only.");
    if (sscanf(params, "d", id)) return SendInfo(playerid, "Usage: /ar [request id]");
    Sup_Update(playerid, id, REPORT_ACCEPTED);
    return 1;
}

CMD:cr(playerid, params[])
{
    new id;
    if (!Sup_IsStaff(playerid)) return SendError(playerid, "Staff only.");
    if (sscanf(params, "d", id)) return SendInfo(playerid, "Usage: /cr [request id]");
    Sup_Update(playerid, id, REPORT_CLOSED);
    return 1;
}

// ---- staff chat ----------------------------------------------------------

CMD:a(playerid, params[])
{
    if (!Sup_IsAdmin(playerid)) return SendError(playerid, "Admins only.");
    if (isnull(params)) return SendInfo(playerid, "Usage: /a [admin chat]");
    new line[160];
    format(line, sizeof (line), "[Admin] %s (%d): %s", gPlayer[playerid][pUsername], gPlayer[playerid][pAdmin], params);
    Sup_NotifyStaff(REPORT_KIND_REPORT, COLOR_ADMIN, line);
    return 1;
}

CMD:h(playerid, params[])
{
    if (!Sup_IsStaff(playerid)) return SendError(playerid, "Staff only.");
    if (isnull(params)) return SendInfo(playerid, "Usage: /h [staff chat]");
    new line[160];
    format(line, sizeof (line), "[Staff] %s: %s", gPlayer[playerid][pUsername], params);
    Sup_NotifyStaff(REPORT_KIND_HELP, COLOR_INFO, line);
    return 1;
}

CMD:sethelper(playerid, params[])
{
    new targetid, level;
    if (sscanf(params, "ud", targetid, level)) return SendInfo(playerid, "Usage: /sethelper [player] [level 0-3]");
    if (!Mod_CanAct(playerid, INVALID_PLAYER_ID, ADMIN_LEAD)) return 1;
    if (!Player_IsLoggedIn(targetid)) return SendError(playerid, "That player isn't logged in.");
    if (!(0 <= level <= HELPER_LEVEL_MAX)) return SendError(playerid, "Level must be 0-3.");
    gPlayer[targetid][pHelper] = level;
    new query[128], details[16];
    mysql_format(gDB, query, sizeof (query), "UPDATE accounts SET helper_level = %d WHERE id = %d", level, gPlayer[targetid][pAccountId]);
    mysql_tquery(gDB, query);
    format(details, sizeof (details), "level %d", level);
    Log_Admin(Mod_ActorId(playerid), "set_helper", gPlayer[targetid][pAccountId], details);
    SendClientMessage(targetid, COLOR_ADMIN, "Your helper level is now %d.", level);
    SendClientMessage(playerid, COLOR_ADMIN, "Helper level set.");
    return 1;
}

CMD:staff(playerid, params[])
{
    SendClientMessage(playerid, COLOR_BRAND, "Staff online:");
    new count = 0;
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (!Player_IsLoggedIn(i) || (gPlayer[i][pAdmin] == 0 && gPlayer[i][pHelper] == 0)) continue;
        if (gPlayer[i][pAdmin] > 0) SendClientMessage(playerid, COLOR_ADMIN, "  %s [%d] - admin level %d", gPlayer[i][pUsername], i, gPlayer[i][pAdmin]);
        else SendClientMessage(playerid, COLOR_INFO, "  %s [%d] - helper", gPlayer[i][pUsername], i);
        count++;
    }
    if (count == 0) SendInfo(playerid, "  Nobody right now. /report and /helpme are saved for later.");
    return 1;
}

// ---- admin tools -----------------------------------------------------------

stock Sup_Teleport(playerid, targetid)
{
    new Float:x, Float:y, Float:z;
    GetPlayerPos(targetid, x, y, z);
    SetPlayerInterior(playerid, GetPlayerInterior(targetid));
    SetPlayerVirtualWorld(playerid, GetPlayerVirtualWorld(targetid));
    if (IsPlayerInAnyVehicle(playerid) && GetPlayerState(playerid) == PLAYER_STATE_DRIVER)
    {
        new vehicleid = GetPlayerVehicleID(playerid);
        SetVehiclePos(vehicleid, x + 2.0, y, z + 0.5);
        LinkVehicleToInterior(vehicleid, GetPlayerInterior(targetid));
        SetVehicleVirtualWorld(vehicleid, GetPlayerVirtualWorld(targetid));
    }
    else
    {
        SetPlayerPos(playerid, x + 1.0, y, z);
    }
}

CMD:goto(playerid, params[])
{
    new targetid;
    if (sscanf(params, "u", targetid)) return SendInfo(playerid, "Usage: /goto [player]");
    if (!Mod_CanAct(playerid, INVALID_PLAYER_ID, ADMIN_MOD)) return 1;
    if (!Player_IsPlaying(targetid) || targetid == playerid) return SendError(playerid, "That player isn't playing.");
    Sup_Teleport(playerid, targetid);
    Log_Admin(Mod_ActorId(playerid), "goto", gPlayer[targetid][pAccountId], "");
    return 1;
}

CMD:gethere(playerid, params[])
{
    new targetid;
    if (sscanf(params, "u", targetid)) return SendInfo(playerid, "Usage: /gethere [player]");
    if (!Mod_CanAct(playerid, targetid, ADMIN_MOD)) return 1;
    if (!Player_IsPlaying(targetid) || targetid == playerid) return SendError(playerid, "That player isn't playing.");
    if (gChar[targetid][cJailSeconds] > 0) return SendError(playerid, "They're in jail. Use /ajail [player] 0 first.");
    Sup_Teleport(targetid, playerid);
    SendClientMessage(targetid, COLOR_ADMIN, "An admin teleported you.");
    Log_Admin(Mod_ActorId(playerid), "gethere", gPlayer[targetid][pAccountId], "");
    return 1;
}

CMD:freeze(playerid, params[])
{
    new targetid;
    if (sscanf(params, "u", targetid)) return SendInfo(playerid, "Usage: /freeze [player]");
    if (!Mod_CanAct(playerid, targetid, ADMIN_MOD)) return 1;
    TogglePlayerControllable(targetid, false);
    SendClientMessage(targetid, COLOR_ADMIN, "An admin froze you.");
    Log_Admin(Mod_ActorId(playerid), "freeze", gPlayer[targetid][pAccountId], "");
    return 1;
}

CMD:unfreeze(playerid, params[])
{
    new targetid;
    if (sscanf(params, "u", targetid)) return SendInfo(playerid, "Usage: /unfreeze [player]");
    if (!Mod_CanAct(playerid, targetid, ADMIN_MOD)) return 1;
    TogglePlayerControllable(targetid, true);
    SendClientMessage(targetid, COLOR_ADMIN, "An admin unfroze you.");
    Log_Admin(Mod_ActorId(playerid), "unfreeze", gPlayer[targetid][pAccountId], "");
    return 1;
}

CMD:check(playerid, params[])
{
    new targetid;
    if (sscanf(params, "u", targetid)) return SendInfo(playerid, "Usage: /check [player]");
    if (!Mod_CanAct(playerid, INVALID_PLAYER_ID, ADMIN_MOD)) return 1;
    if (!Player_IsLoggedIn(targetid)) return SendError(playerid, "That player isn't logged in.");
    SendClientMessage(playerid, COLOR_ADMIN, "Account %s (#%d) | IP %s | admin %d helper %d",
        gPlayer[targetid][pUsername], gPlayer[targetid][pAccountId], gPlayer[targetid][pIp], gPlayer[targetid][pAdmin], gPlayer[targetid][pHelper]);
    if (Player_IsPlaying(targetid))
    {
        new Float:health, Float:armour;
        GetPlayerHealth(targetid, health);
        GetPlayerArmour(targetid, armour);
        SendClientMessage(playerid, COLOR_ADMIN, "Character %s (#%d) | level %d | cash $%d bank $%d | job %s | faction %d rank %d",
            gChar[targetid][cName], gChar[targetid][cId], gChar[targetid][cLevel], gChar[targetid][cCash], gChar[targetid][cBank],
            gJobNames[gChar[targetid][cJob]], gChar[targetid][cFaction], gChar[targetid][cFactionRank]);
        SendClientMessage(playerid, COLOR_ADMIN, "Health %.0f armour %.0f | wanted %d | jail %ds | vehicles %d",
            health, armour, gChar[targetid][cWanted], gChar[targetid][cJailSeconds], Veh_CountOwned(targetid));
    }
    Log_Admin(Mod_ActorId(playerid), "check", gPlayer[targetid][pAccountId], "");
    return 1;
}

// Negative amounts take money. Bank only, so it shows up in money_log clearly.
CMD:givemoney(playerid, params[])
{
    new targetid, amount, reason[64];
    if (sscanf(params, "uds[64]", targetid, amount, reason)) return SendInfo(playerid, "Usage: /givemoney [player] [amount, negative to take] [reason]");
    if (!Mod_CanAct(playerid, INVALID_PLAYER_ID, ADMIN_LEAD)) return 1;
    if (!Player_IsPlaying(targetid)) return SendError(playerid, "That player isn't playing.");
    if (amount == 0 || !(-10000000 <= amount <= 10000000)) return SendError(playerid, "Amount must be non-zero and within $10,000,000.");
    if (!Money_Give(targetid, 0, amount, "admin", playerid, reason)) return SendError(playerid, "They don't have that much in the bank.");
    new details[96];
    format(details, sizeof (details), "$%d: %s", amount, reason);
    Log_Admin(Mod_ActorId(playerid), "give_money", gPlayer[targetid][pAccountId], details);
    SendClientMessage(targetid, COLOR_ADMIN, "An admin changed your bank balance by $%d. Reason: %s", amount, reason);
    SendClientMessage(playerid, COLOR_ADMIN, "Done. Their bank balance is $%d.", gChar[targetid][cBank]);
    return 1;
}

stock Sup_OnDisconnect(playerid)
{
    gLastReport[playerid] = 0;
}
