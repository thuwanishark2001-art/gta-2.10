// Roleplay chat: local talk, /me, /do, /b, /s, /low, /w, /pm, /o.

#define COLOR_ACTION    (0xC2A2DAFF)    // /me and /do
#define COLOR_OOC       (0xB0C4DEFF)
#define COLOR_PM        (0xF5DEB3FF)

#define CHAT_RANGE_LOW      (5.0)
#define CHAT_RANGE_SHOUT    (40.0)
#define CHAT_RANGE_ACTION   (25.0)

new bool:gOocEnabled = false;
new gLastPmFrom[MAX_PLAYERS] = { INVALID_PLAYER_ID, ... };

// Sends a line to everyone in range in the same world and interior.
stock Chat_Range(playerid, Float:range, colour, const text[])
{
    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    new world = GetPlayerVirtualWorld(playerid), interior = GetPlayerInterior(playerid);
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (Player_IsPlaying(i) && GetPlayerVirtualWorld(i) == world && GetPlayerInterior(i) == interior
            && IsPlayerInRangeOfPoint(i, range, x, y, z))
        {
            SendClientMessage(i, colour, "%s", text);
        }
    }
}

// "* Marcus Reed <action>"
stock Chat_Action(playerid, const action[])
{
    new name[MAX_PLAYER_NAME], line[160];
    Chat_DisplayName(playerid, name);
    format(line, sizeof (line), "* %s %s", name, action);
    Chat_Range(playerid, CHAT_RANGE_ACTION, COLOR_ACTION, line);
}

// Character names are stored as First_Last; show them as "First Last".
stock Chat_FixName(text[])
{
    for (new i = 0; text[i] != '\0'; i++)
    {
        if (text[i] == '_') text[i] = ' ';
    }
}

stock Chat_DisplayName(playerid, dest[], size = sizeof (dest))
{
    format(dest, size, "%s", gChar[playerid][cName]);
    Chat_FixName(dest);
}

// Mute and anti-spam check shared by chat and the chat commands.
stock bool:Chat_CanTalk(playerid, const text[])
{
    if (!Player_IsPlaying(playerid)) return false;
    if (Player_IsMuted(playerid))
    {
        new left[24];
        FormatDuration(left, gPlayer[playerid][pMutedUntil] - gettime());
        SendClientMessage(playerid, COLOR_ERROR, "You are muted for another %s.", left);
        return false;
    }
    new E_SPAM_RESULT:spam = Spam_Check(gChatSpam[playerid], GetTickCount(), text, KM_CHAT_WINDOW_MS, KM_CHAT_MAX_IN_WINDOW, KM_REPEAT_MS);
    if (spam != SPAM_OK)
    {
        Mod_SpamWarning(playerid, spam);
        return false;
    }
    return true;
}

stock Chat_Say(playerid, const text[])
{
    if (!Chat_CanTalk(playerid, text)) return;
    new name[MAX_PLAYER_NAME], line[160];
    Chat_DisplayName(playerid, name);
    if (IsPlayerInAnyVehicle(playerid)) format(line, sizeof (line), "%s says (vehicle): %s", name, text);
    else format(line, sizeof (line), "%s says: %s", name, text);
    Chat_Range(playerid, KM_CHAT_RADIUS, COLOR_WHITE, line);
}

CMD:me(playerid, params[])
{
    if (isnull(params)) return SendInfo(playerid, "Usage: /me [action]");
    if (!Chat_CanTalk(playerid, params)) return 1;
    Chat_Action(playerid, params);
    return 1;
}

CMD:do(playerid, params[])
{
    if (isnull(params)) return SendInfo(playerid, "Usage: /do [description]");
    if (!Chat_CanTalk(playerid, params)) return 1;
    new name[MAX_PLAYER_NAME], line[160];
    Chat_DisplayName(playerid, name);
    format(line, sizeof (line), "* %s (( %s ))", params, name);
    Chat_Range(playerid, CHAT_RANGE_ACTION, COLOR_ACTION, line);
    return 1;
}

CMD:b(playerid, params[])
{
    if (isnull(params)) return SendInfo(playerid, "Usage: /b [local out-of-character text]");
    if (!Chat_CanTalk(playerid, params)) return 1;
    new name[MAX_PLAYER_NAME], line[160];
    Chat_DisplayName(playerid, name);
    format(line, sizeof (line), "(( [%d] %s: %s ))", playerid, name, params);
    Chat_Range(playerid, KM_CHAT_RADIUS, COLOR_OOC, line);
    return 1;
}

CMD:s(playerid, params[])
{
    if (isnull(params)) return SendInfo(playerid, "Usage: /s [shout]");
    if (!Chat_CanTalk(playerid, params)) return 1;
    new name[MAX_PLAYER_NAME], line[160];
    Chat_DisplayName(playerid, name);
    format(line, sizeof (line), "%s shouts: %s!", name, params);
    Chat_Range(playerid, CHAT_RANGE_SHOUT, COLOR_WHITE, line);
    return 1;
}

CMD:low(playerid, params[])
{
    if (isnull(params)) return SendInfo(playerid, "Usage: /low [quiet text]");
    if (!Chat_CanTalk(playerid, params)) return 1;
    new name[MAX_PLAYER_NAME], line[160];
    Chat_DisplayName(playerid, name);
    format(line, sizeof (line), "%s says quietly: %s", name, params);
    Chat_Range(playerid, CHAT_RANGE_LOW, COLOR_GREY, line);
    return 1;
}

CMD:w(playerid, params[])
{
    new targetid, text[128];
    if (sscanf(params, "us[128]", targetid, text)) return SendInfo(playerid, "Usage: /w [player] [whisper]");
    if (!Player_IsPlaying(targetid) || targetid == playerid) return SendError(playerid, "That player isn't available.");
    if (!Player_IsNear(playerid, targetid, 2.5)) return SendError(playerid, "You need to be right next to them.");
    if (!Chat_CanTalk(playerid, text)) return 1;
    new name[MAX_PLAYER_NAME], other[MAX_PLAYER_NAME];
    Chat_DisplayName(playerid, name);
    Chat_DisplayName(targetid, other);
    SendClientMessage(targetid, COLOR_YELLOW, "%s whispers: %s", name, text);
    SendClientMessage(playerid, COLOR_YELLOW, "You whisper to %s: %s", other, text);
    new action[64];
    format(action, sizeof (action), "whispers something to %s.", other);
    Chat_Action(playerid, action);
    return 1;
}

CMD:pm(playerid, params[])
{
    new targetid, text[128];
    if (sscanf(params, "us[128]", targetid, text)) return SendInfo(playerid, "Usage: /pm [player] [message]");
    if (!Player_IsLoggedIn(targetid) || targetid == playerid) return SendError(playerid, "That player isn't available.");
    if (!Chat_CanTalk(playerid, text)) return 1;
    SendClientMessage(targetid, COLOR_PM, "(( PM from %s [%d]: %s ))", gPlayer[playerid][pUsername], playerid, text);
    SendClientMessage(playerid, COLOR_PM, "(( PM to %s [%d]: %s ))", gPlayer[targetid][pUsername], targetid, text);
    gLastPmFrom[targetid] = playerid;
    return 1;
}

// Reply to the last PM.
CMD:r(playerid, params[])
{
    if (isnull(params)) return SendInfo(playerid, "Usage: /r [reply to your last PM]");
    new from = gLastPmFrom[playerid];
    if (from == INVALID_PLAYER_ID || !Player_IsLoggedIn(from)) return SendError(playerid, "Nobody to reply to.");
    new text[140];
    format(text, sizeof (text), "%d %s", from, params);
    return cmd_pm(playerid, text);
}

// Global out-of-character chat, off unless an admin turns it on.
CMD:o(playerid, params[])
{
    if (isnull(params)) return SendInfo(playerid, "Usage: /o [global out-of-character text]");
    if (!gOocEnabled && gPlayer[playerid][pAdmin] < ADMIN_MOD) return SendError(playerid, "Global OOC chat is off.");
    if (!Chat_CanTalk(playerid, params)) return 1;
    SendClientMessageToAll(COLOR_OOC, "(( [OOC] %s: %s ))", gPlayer[playerid][pUsername], params);
    return 1;
}

CMD:togooc(playerid, params[])
{
    if (!Mod_CanAct(playerid, INVALID_PLAYER_ID, ADMIN_MOD)) return 1;
    gOocEnabled = !gOocEnabled;
    SendClientMessageToAll(COLOR_ADMIN, "Global OOC chat was turned %s by an admin.", gOocEnabled ? ("on") : ("off"));
    Log_Admin(Mod_ActorId(playerid), "toggle_ooc", 0, gOocEnabled ? ("on") : ("off"));
    return 1;
}

stock Chat_OnDisconnect(playerid)
{
    gLastPmFrom[playerid] = INVALID_PLAYER_ID;
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (gLastPmFrom[i] == playerid) gLastPmFrom[i] = INVALID_PLAYER_ID;
    }
}
