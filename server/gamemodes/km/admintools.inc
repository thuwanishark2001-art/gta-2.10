// Everyday admin tools: announcements, health, skins, time and weather.

// Skins every copy of GTA: San Andreas has. The mobile client has no SA-MP
// data pack, so SA-MP-only skins would show up as a stand-in pedestrian.
stock bool:IsStockSkin(skin)
{
    if (!(0 <= skin <= 311)) return false;
    if (1 <= skin <= 6 || skin >= 300) return false;
    switch (skin)
    {
        case 8, 42, 65, 74, 86, 119, 149, 208, 273, 289: return false;
    }
    return true;
}

// /announce [text]: big text in the middle of everyone's screen, and in chat.
CMD:announce(playerid, params[])
{
    if (!Mod_CanAct(playerid, INVALID_PLAYER_ID, ADMIN_ADMIN)) return 1;
    if (isnull(params) || strlen(params) > 100) return SendInfo(playerid, "Usage: /announce [text, up to 100 characters]");
    if (strfind(params, "~") != -1) return SendError(playerid, "Announcements can't contain ~ (game text codes).");
    new text[128];
    format(text, sizeof (text), "~w~%s", params);
    GameTextForAll(text, 6000, 3);
    SendClientMessageToAll(COLOR_ADMIN, "[Announcement] %s", params);
    Log_Admin(Mod_ActorId(playerid), "announce", 0, params);
    return 1;
}

// /sethealth [player] [0-100]
CMD:sethealth(playerid, params[])
{
    new targetid, Float:health;
    if (sscanf(params, "uf", targetid, health)) return SendInfo(playerid, "Usage: /sethealth [player] [0-100]");
    if (!Mod_CanAct(playerid, targetid, ADMIN_ADMIN)) return 1;
    if (!Player_IsPlaying(targetid)) return SendError(playerid, "That player isn't playing.");
    if (!(0.0 <= health <= 100.0)) return SendError(playerid, "Health must be 0-100.");
    SetPlayerHealth(targetid, health);
    SendClientMessage(playerid, COLOR_ADMIN, "Set %s's health to %.0f.", gChar[targetid][cName], health);
    if (targetid != playerid) SendClientMessage(targetid, COLOR_ADMIN, "An admin set your health to %.0f.", health);
    new details[48];
    format(details, sizeof (details), "health %.0f", health);
    Log_Admin(Mod_ActorId(playerid), "set_health", gPlayer[targetid][pAccountId], details);
    return 1;
}

// /setskin [player] [skin]: changes the character's everyday skin and saves it.
CMD:setskin(playerid, params[])
{
    new targetid, skin;
    if (sscanf(params, "ud", targetid, skin)) return SendInfo(playerid, "Usage: /setskin [player] [skin 0-299]");
    if (!Mod_CanAct(playerid, targetid, ADMIN_ADMIN)) return 1;
    if (!Player_IsPlaying(targetid)) return SendError(playerid, "That player isn't playing.");
    if (!IsStockSkin(skin)) return SendError(playerid, "That skin isn't in every copy of the game. Pick another (0-299).");
    gChar[targetid][cSkin] = skin;
    SetPlayerSkin(targetid, skin);
    new query[96];
    mysql_format(gDB, query, sizeof (query), "UPDATE characters SET skin = %d WHERE id = %d", skin, gChar[targetid][cId]);
    mysql_tquery(gDB, query);
    SendClientMessage(playerid, COLOR_ADMIN, "Set %s's skin to %d.", gChar[targetid][cName], skin);
    if (targetid != playerid) SendClientMessage(targetid, COLOR_ADMIN, "An admin changed your skin to %d.", skin);
    new details[24];
    format(details, sizeof (details), "skin %d", skin);
    Log_Admin(Mod_ActorId(playerid), "set_skin", gPlayer[targetid][pAccountId], details);
    return 1;
}

// /settime [0-23]: the hour for everyone.
CMD:settime(playerid, params[])
{
    new hour;
    if (sscanf(params, "d", hour)) return SendInfo(playerid, "Usage: /settime [hour 0-23]");
    if (!Mod_CanAct(playerid, INVALID_PLAYER_ID, ADMIN_LEAD)) return 1;
    if (!(0 <= hour <= 23)) return SendError(playerid, "The hour must be 0-23.");
    SetWorldTime(hour);
    SendClientMessageToAll(COLOR_ADMIN, "An admin set the time to %02d:00.", hour);
    new details[16];
    format(details, sizeof (details), "hour %d", hour);
    Log_Admin(Mod_ActorId(playerid), "set_time", 0, details);
    return 1;
}

// /setweather [0-20]: the weather for everyone (0-20 are the normal kinds).
CMD:setweather(playerid, params[])
{
    new weather;
    if (sscanf(params, "d", weather)) return SendInfo(playerid, "Usage: /setweather [0-20], e.g. 1 sunny, 8 rain, 9 fog, 19 sandstorm");
    if (!Mod_CanAct(playerid, INVALID_PLAYER_ID, ADMIN_LEAD)) return 1;
    if (!(0 <= weather <= 20)) return SendError(playerid, "Weather must be 0-20.");
    SetWeather(weather);
    SendClientMessageToAll(COLOR_ADMIN, "An admin changed the weather.");
    new details[16];
    format(details, sizeof (details), "weather %d", weather);
    Log_Admin(Mod_ActorId(playerid), "set_weather", 0, details);
    return 1;
}
