// Kick, mute and ban, for admins and for the automatic anti-spam.
// Every action is written to admin_log.

forward Mod_OnUnban(playerid, race, const target[]);
forward Mod_OnRenameAccount(playerid, race, const oldName[], const newName[]);

// Admin account id for logging, 0 for console / system.
stock Mod_ActorId(playerid)
{
    return (playerid == INVALID_PLAYER_ID) ? 0 : gPlayer[playerid][pAccountId];
}

stock Mod_ActorName(playerid, dest[], size = sizeof (dest))
{
    if (playerid == INVALID_PLAYER_ID) format(dest, size, "System");
    else format(dest, size, "%s", gPlayer[playerid][pUsername]);
}

// True if `playerid` has at least `level` and outranks `targetid`.
stock bool:Mod_CanAct(playerid, targetid, level)
{
    if (gPlayer[playerid][pAdmin] < level && !IsPlayerAdmin(playerid))
    {
        SendError(playerid, "You don't have permission to use this command.");
        return false;
    }
    if (targetid != INVALID_PLAYER_ID)
    {
        if (!IsPlayerConnected(targetid))
        {
            SendError(playerid, "That player isn't connected.");
            return false;
        }
        if (targetid != playerid && gPlayer[targetid][pAdmin] >= gPlayer[playerid][pAdmin] && !IsPlayerAdmin(playerid))
        {
            SendError(playerid, "You can't do that to an admin of the same or higher level.");
            return false;
        }
    }
    return true;
}

stock Mod_Kick(adminid, targetid, const reason[])
{
    new actor[MAX_PLAYER_NAME], name[MAX_PLAYER_NAME];
    Mod_ActorName(adminid, actor);
    GetPlayerName(targetid, name, sizeof (name));
    SendClientMessageToAll(COLOR_ADMIN, "%s was kicked by %s. Reason: %s", name, actor, reason);
    Log_Admin(Mod_ActorId(adminid), "kick", gPlayer[targetid][pAccountId], reason);
    KickDelayed(targetid);
}

stock Mod_Mute(adminid, targetid, minutes, const reason[])
{
    new actor[MAX_PLAYER_NAME], name[MAX_PLAYER_NAME], details[160];
    Mod_ActorName(adminid, actor);
    GetPlayerName(targetid, name, sizeof (name));

    gPlayer[targetid][pMutedUntil] = gettime() + minutes * 60;
    format(gPlayer[targetid][pMuteReason], 128, "%s", reason);

    if (gPlayer[targetid][pAccountId] != 0)
    {
        new query[320];
        mysql_format(gDB, query, sizeof (query),
            "UPDATE accounts SET muted_until = UTC_TIMESTAMP() + INTERVAL %d MINUTE, mute_reason = '%e' WHERE id = %d",
            minutes, reason, gPlayer[targetid][pAccountId]);
        mysql_tquery(gDB, query);
    }
    SendClientMessageToAll(COLOR_ADMIN, "%s was muted for %d minute(s) by %s. Reason: %s", name, minutes, actor, reason);
    format(details, sizeof (details), "%d min: %s", minutes, reason);
    Log_Admin(Mod_ActorId(adminid), "mute", gPlayer[targetid][pAccountId], details);
}

stock Mod_Unmute(adminid, targetid)
{
    gPlayer[targetid][pMutedUntil] = 0;
    gPlayer[targetid][pMuteReason][0] = '\0';
    new query[128];
    mysql_format(gDB, query, sizeof (query), "UPDATE accounts SET muted_until = NULL, mute_reason = NULL WHERE id = %d", gPlayer[targetid][pAccountId]);
    mysql_tquery(gDB, query);
    SendClientMessage(targetid, COLOR_ADMIN, "You were unmuted.");
    Log_Admin(Mod_ActorId(adminid), "unmute", gPlayer[targetid][pAccountId], "");
}

// days = 0 bans permanently. Bans both the account and the IP.
stock Mod_Ban(adminid, targetid, days, const reason[])
{
    new actor[MAX_PLAYER_NAME], name[MAX_PLAYER_NAME], details[160], query[400];
    Mod_ActorName(adminid, actor);
    GetPlayerName(targetid, name, sizeof (name));

    if (days > 0)
    {
        mysql_format(gDB, query, sizeof (query),
            "INSERT INTO bans (account_id, ip, reason, banned_by, expires_at) VALUES (NULLIF(%d, 0), '%e', '%e', NULLIF(%d, 0), UTC_TIMESTAMP() + INTERVAL %d DAY)",
            gPlayer[targetid][pAccountId], gPlayer[targetid][pIp], reason, Mod_ActorId(adminid), days);
        SendClientMessageToAll(COLOR_ADMIN, "%s was banned for %d day(s) by %s. Reason: %s", name, days, actor, reason);
        format(details, sizeof (details), "%d days: %s", days, reason);
    }
    else
    {
        mysql_format(gDB, query, sizeof (query),
            "INSERT INTO bans (account_id, ip, reason, banned_by) VALUES (NULLIF(%d, 0), '%e', '%e', NULLIF(%d, 0))",
            gPlayer[targetid][pAccountId], gPlayer[targetid][pIp], reason, Mod_ActorId(adminid));
        SendClientMessageToAll(COLOR_ADMIN, "%s was permanently banned by %s. Reason: %s", name, actor, reason);
        format(details, sizeof (details), "permanent: %s", reason);
    }
    mysql_tquery(gDB, query);
    Log_Admin(Mod_ActorId(adminid), "ban", gPlayer[targetid][pAccountId], details);
    KickDelayed(targetid);
}

// Records a spam warning and mutes automatically after KM_SPAM_WARNINGS.
stock Mod_SpamWarning(playerid, E_SPAM_RESULT:result)
{
    gPlayer[playerid][pSpamWarnings]++;
    if (gPlayer[playerid][pSpamWarnings] >= KM_SPAM_WARNINGS)
    {
        gPlayer[playerid][pSpamWarnings] = 0;
        Mod_Mute(INVALID_PLAYER_ID, playerid, KM_SPAM_MUTE_MIN, "Spam");
        return;
    }
    SendError(playerid, result == SPAM_REPEAT ? ("Don't repeat the same message.") : ("Slow down, you're sending messages too fast."));
}

// ---- commands ----------------------------------------------------------

CMD:kick(playerid, params[])
{
    new targetid, reason[96];
    if (sscanf(params, "us[96]", targetid, reason)) return SendInfo(playerid, "Usage: /kick [player] [reason]");
    if (targetid == INVALID_PLAYER_ID) return SendError(playerid, "That player isn't connected.");
    if (!Mod_CanAct(playerid, targetid, ADMIN_MOD)) return 1;
    Mod_Kick(playerid, targetid, reason);
    return 1;
}

CMD:mute(playerid, params[])
{
    new targetid, minutes, reason[96];
    if (sscanf(params, "uds[96]", targetid, minutes, reason)) return SendInfo(playerid, "Usage: /mute [player] [minutes] [reason]");
    if (targetid == INVALID_PLAYER_ID) return SendError(playerid, "That player isn't connected.");
    if (!(1 <= minutes <= 10080)) return SendError(playerid, "Minutes must be between 1 and 10080 (one week).");
    if (!Mod_CanAct(playerid, targetid, ADMIN_MOD)) return 1;
    Mod_Mute(playerid, targetid, minutes, reason);
    return 1;
}

CMD:unmute(playerid, params[])
{
    new targetid;
    if (sscanf(params, "u", targetid)) return SendInfo(playerid, "Usage: /unmute [player]");
    if (targetid == INVALID_PLAYER_ID) return SendError(playerid, "That player isn't connected.");
    if (!Mod_CanAct(playerid, targetid, ADMIN_MOD)) return 1;
    Mod_Unmute(playerid, targetid);
    SendClientMessage(playerid, COLOR_ADMIN, "Player unmuted.");
    return 1;
}

CMD:ban(playerid, params[])
{
    new targetid, days, reason[96];
    if (sscanf(params, "uds[96]", targetid, days, reason)) return SendInfo(playerid, "Usage: /ban [player] [days, 0 = permanent] [reason]");
    if (targetid == INVALID_PLAYER_ID) return SendError(playerid, "That player isn't connected.");
    if (!(0 <= days <= 3650)) return SendError(playerid, "Days must be between 0 (permanent) and 3650.");
    if (!Mod_CanAct(playerid, targetid, ADMIN_ADMIN)) return 1;
    Mod_Ban(playerid, targetid, days, reason);
    return 1;
}

CMD:unban(playerid, params[])
{
    new target[MAX_PLAYER_NAME];
    if (sscanf(params, "s[24]", target)) return SendInfo(playerid, "Usage: /unban [account name]");
    if (!Mod_CanAct(playerid, INVALID_PLAYER_ID, ADMIN_ADMIN)) return 1;

    new query[320];
    mysql_format(gDB, query, sizeof (query),
        "UPDATE bans SET lifted_at = UTC_TIMESTAMP(), lifted_by = NULLIF(%d, 0) \
         WHERE lifted_at IS NULL AND account_id = (SELECT id FROM accounts WHERE username = '%e')",
        Mod_ActorId(playerid), target);
    mysql_tquery(gDB, query, "Mod_OnUnban", "dds", playerid, gPlayerRace[playerid], target);
    return 1;
}

public Mod_OnUnban(playerid, race, const target[])
{
    new lifted = cache_affected_rows();
    if (gPlayerRace[playerid] == race)
    {
        if (lifted > 0) SendClientMessage(playerid, COLOR_ADMIN, "Lifted %d ban(s) on %s.", lifted, target);
        else SendError(playerid, "No active ban found for that account.");
    }
    if (lifted > 0)
    {
        new details[64];
        format(details, sizeof (details), "account %s", target);
        Log_Admin(gPlayerRace[playerid] == race ? Mod_ActorId(playerid) : 0, "unban", 0, details);
    }
    return 1;
}

CMD:setadmin(playerid, params[])
{
    new targetid, level;
    if (sscanf(params, "ud", targetid, level)) return SendInfo(playerid, "Usage: /setadmin [player] [level 0-5]");
    if (targetid == INVALID_PLAYER_ID) return SendError(playerid, "That player isn't connected.");
    if (!(0 <= level <= ADMIN_OWNER)) return SendError(playerid, "Level must be 0-5.");
    // Owners, or anyone logged into RCON, can grant levels below their own.
    if (!IsPlayerAdmin(playerid))
    {
        if (gPlayer[playerid][pAdmin] < ADMIN_OWNER) return SendError(playerid, "You don't have permission to use this command.");
        if (level >= gPlayer[playerid][pAdmin] && targetid != playerid) return SendError(playerid, "You can only grant levels below your own.");
    }
    if (gPlayer[targetid][pAccountId] == 0) return SendError(playerid, "That player isn't logged in.");

    gPlayer[targetid][pAdmin] = level;
    new query[128], details[32];
    mysql_format(gDB, query, sizeof (query), "UPDATE accounts SET admin_level = %d WHERE id = %d", level, gPlayer[targetid][pAccountId]);
    mysql_tquery(gDB, query);
    format(details, sizeof (details), "level %d", level);
    Log_Admin(Mod_ActorId(playerid), "set_admin", gPlayer[targetid][pAccountId], details);
    SendClientMessage(targetid, COLOR_ADMIN, "Your admin level is now %d.", level);
    if (targetid != playerid) SendClientMessage(playerid, COLOR_ADMIN, "Admin level set to %d.", level);
    return 1;
}

// Owner tool: renames an account (its characters, money and admin level stay).
// The player has to be offline; they log in with the new name and the same password.
CMD:renameaccount(playerid, params[])
{
    new oldName[MAX_PLAYER_NAME], newName[MAX_PLAYER_NAME];
    if (sscanf(params, "s[24]s[24]", oldName, newName)) return SendInfo(playerid, "Usage: /renameaccount [old account name] [new account name]");
    if (!Mod_CanAct(playerid, INVALID_PLAYER_ID, ADMIN_OWNER)) return 1;
    if (!IsValidAccountName(newName)) return SendError(playerid, "The new name must be 3-20 letters, digits or underscores.");
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (IsPlayerConnected(i) && strcmp(gPlayer[i][pUsername], oldName, true) == 0)
            return SendError(playerid, "That account is in use. It can only be renamed while its player is offline.");
    }
    new query[256];
    // The derived table lets MariaDB check the new name in the same table.
    mysql_format(gDB, query, sizeof (query),
        "UPDATE accounts SET username = '%e' WHERE username = '%e' \
         AND NOT EXISTS (SELECT 1 FROM (SELECT id FROM accounts WHERE username = '%e') taken)",
        newName, oldName, newName);
    mysql_tquery(gDB, query, "Mod_OnRenameAccount", "ddss", playerid, gPlayerRace[playerid], oldName, newName);
    return 1;
}

public Mod_OnRenameAccount(playerid, race, const oldName[], const newName[])
{
    new renamed = cache_affected_rows();
    if (gPlayerRace[playerid] == race)
    {
        if (renamed > 0) SendClientMessage(playerid, COLOR_ADMIN, "Renamed account %s to %s.", oldName, newName);
        else SendError(playerid, "No account has the old name, or the new name is already taken.");
    }
    if (renamed > 0)
    {
        new details[64];
        format(details, sizeof (details), "%s -> %s", oldName, newName);
        Log_Admin(gPlayerRace[playerid] == race ? Mod_ActorId(playerid) : 0, "rename_account", 0, details);
    }
    return 1;
}

// Owner tool: puts test map icons and a checkpoint around you, to check how
// the client draws radar blips (coloured squares and sprite icons).
//   /testblips      show them        /testblips off   remove them
CMD:testblips(playerid, params[])
{
    if (gPlayer[playerid][pAdmin] < ADMIN_OWNER) return SendError(playerid, "You don't have permission to use this command.");
    if (!isnull(params) && strcmp(params, "off", true) == 0)
    {
        for (new i = 90; i <= 95; i++) RemovePlayerMapIcon(playerid, i);
        DisablePlayerCheckpoint(playerid);
        SendClientMessage(playerid, COLOR_ADMIN, "Test blips removed.");
        return 1;
    }
    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    // Coloured squares: the colour is sent as RGBA, like player markers.
    SetPlayerMapIcon(playerid, 90, x + 60.0, y, z, 0, 0xFF0000FF, MAPICON_GLOBAL);   // red
    SetPlayerMapIcon(playerid, 91, x, y + 60.0, z, 0, 0x00FF00FF, MAPICON_GLOBAL);   // green
    SetPlayerMapIcon(playerid, 92, x - 60.0, y, z, 0, 0x3399FFFF, MAPICON_GLOBAL);   // blue
    // Sprite icons: should keep their own colours.
    SetPlayerMapIcon(playerid, 93, x, y - 60.0, z, 6, 0, MAPICON_GLOBAL);            // Ammu-Nation
    SetPlayerMapIcon(playerid, 94, x + 45.0, y + 45.0, z, 10, 0, MAPICON_GLOBAL);    // Burger Shot
    SetPlayerMapIcon(playerid, 95, x - 45.0, y - 45.0, z, 52, 0, MAPICON_GLOBAL);    // cash
    SetPlayerCheckpoint(playerid, x + 20.0, y + 20.0, z, 3.0);
    SendClientMessage(playerid, COLOR_ADMIN, "Test blips: red east, green north, blue west; gun shop south; burger NE; cash SW; checkpoint nearby.");
    SendClientMessage(playerid, COLOR_ADMIN, "Screenshot the radar and the pause-menu map. /testblips off removes them.");
    return 1;
}
