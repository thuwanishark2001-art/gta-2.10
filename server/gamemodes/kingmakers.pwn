// Kingmakers RP gamemode (open.mp).
// Modules live in gamemodes/km/. Database schema: server/sql/migrations.

#define MAX_PLAYERS (100)

#include <open.mp>
#include <a_mysql>
#include <samp_bcrypt>
#include <sscanf2>

#include "km/config.inc"
#include "km/antispam.inc"
#include "km/players.inc"
#include "km/util.inc"
#include "km/validate.inc"
#include "km/db.inc"
#include "km/cmd.inc"
#include "km/accounts.inc"
#include "km/characters.inc"
#include "km/moderation.inc"

main()
{
    print("Kingmakers RP " KM_VERSION);
}

public OnGameModeInit()
{
    if (!DB_Connect())
    {
        print("[km] no database, shutting down");
        SendRconCommand("exit");
        return 1;
    }

    SetGameModeText(KM_MODE_TEXT);
    // Mobile clients need a class to reach OnPlayerRequestClass.
    AddPlayerClass(0, KM_SPAWN_X, KM_SPAWN_Y, KM_SPAWN_Z, KM_SPAWN_A);
    bcrypt_set_thread_limit(2);
    SetTimer("Char_AutoSaveAll", KM_AUTOSAVE_MS, true);
    return 1;
}

public OnGameModeExit()
{
    Char_AutoSaveAll();
    mysql_close(gDB);
    return 1;
}

public OnPlayerConnect(playerid)
{
    if (IsPlayerNPC(playerid)) return 1;
    gPlayerRace[playerid]++;
    Player_Reset(playerid);
    Char_Reset(playerid);
    Acc_OnConnect(playerid);
    return 1;
}

public OnPlayerDisconnect(playerid, reason)
{
    if (Player_IsPlaying(playerid)) Char_Save(playerid);
    Acc_OnDisconnect(playerid, reason);
    gPlayerRace[playerid]++;
    Player_Reset(playerid);
    Char_Reset(playerid);
    return 1;
}

public OnPlayerRequestClass(playerid, classid)
{
    if (gPlayer[playerid][pStage] != STAGE_PLAYING)
    {
        // Hide the class picker while logging in; a character spawn ends this.
        TogglePlayerSpectating(playerid, true);
        SetPlayerCameraPos(playerid, 1460.0, -1600.0, 120.0);
        SetPlayerCameraLookAt(playerid, 1540.0, -1675.0, 40.0);
    }
    return 1;
}

public OnPlayerRequestSpawn(playerid)
{
    return Player_IsPlaying(playerid) ? 1 : 0;
}

public OnPlayerSpawn(playerid)
{
    if (!Player_IsPlaying(playerid))
    {
        TogglePlayerSpectating(playerid, true);
        return 1;
    }
    Char_OnSpawn(playerid);
    return 1;
}

public OnDialogResponse(playerid, dialogid, response, listitem, inputtext[])
{
    if (Acc_OnDialog(playerid, dialogid, response, inputtext)) return 1;
    if (Char_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
    return 0;
}

// Local in-character chat. Phase 2 adds /me, /do, /b, /shout and the rest.
public OnPlayerText(playerid, text[])
{
    if (!Player_IsPlaying(playerid)) return 0;
    if (Player_IsMuted(playerid))
    {
        new left[24];
        FormatDuration(left, gPlayer[playerid][pMutedUntil] - gettime());
        SendClientMessage(playerid, COLOR_ERROR, "You are muted for another %s.", left);
        return 0;
    }
    new E_SPAM_RESULT:spam = Spam_Check(gChatSpam[playerid], GetTickCount(), text, KM_CHAT_WINDOW_MS, KM_CHAT_MAX_IN_WINDOW, KM_REPEAT_MS);
    if (spam != SPAM_OK)
    {
        Mod_SpamWarning(playerid, spam);
        return 0;
    }

    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    new world = GetPlayerVirtualWorld(playerid);
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (Player_IsPlaying(i) && GetPlayerVirtualWorld(i) == world && IsPlayerInRangeOfPoint(i, KM_CHAT_RADIUS, x, y, z))
        {
            SendClientMessage(i, COLOR_WHITE, "%s says: %s", gChar[playerid][cName], text);
        }
    }
    return 0;
}

public OnPlayerCommandText(playerid, cmdtext[])
{
    if (!Player_IsLoggedIn(playerid))
    {
        SendError(playerid, "Log in first.");
        return 1;
    }
    new E_SPAM_RESULT:spam = Spam_Check(gCmdSpam[playerid], GetTickCount(), cmdtext, KM_CMD_WINDOW_MS, KM_CMD_MAX_IN_WINDOW, 0);
    if (spam == SPAM_FLOOD)
    {
        Mod_SpamWarning(playerid, spam);
        return 1;
    }
    if (!Cmd_Dispatch(playerid, cmdtext))
    {
        SendError(playerid, "Unknown command. Type /help.");
    }
    return 1;
}

CMD:help(playerid, params[])
{
    SendClientMessage(playerid, COLOR_BRAND, "Kingmakers RP commands");
    SendClientMessage(playerid, COLOR_WHITE, "/stats - your character  /help - this list");
    if (gPlayer[playerid][pAdmin] >= ADMIN_MOD)
    {
        SendClientMessage(playerid, COLOR_ADMIN, "Admin: /kick /mute /unmute");
    }
    if (gPlayer[playerid][pAdmin] >= ADMIN_ADMIN)
    {
        SendClientMessage(playerid, COLOR_ADMIN, "Admin: /ban /unban");
    }
    return 1;
}

CMD:stats(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return SendError(playerid, "Pick a character first.");
    new played[24];
    FormatDuration(played, gettime() - gChar[playerid][cSessionStart]);
    SendClientMessage(playerid, COLOR_BRAND, "%s", gChar[playerid][cName]);
    SendClientMessage(playerid, COLOR_WHITE, "Level %d | Cash $%d | Account %s | This session %s",
        gChar[playerid][cLevel], gChar[playerid][cCash], gPlayer[playerid][pUsername], played);
    return 1;
}
