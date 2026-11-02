// In-car radio and admin audio streams.
//
// The mobile client can't load GTA's own sound (2.11 keeps it where other
// apps can't read it), so the radio uses the client's audio stream player:
// PlayAudioStreamForPlayer with an internet radio URL. Everyone in a vehicle
// hears the station its driver picked with /radio. An admin stream
// (/playstream) takes over a player's audio until /stopstream.

#define RADIO_OFF           (-1)
#define RADIO_URL_MAX       (128)

enum E_RADIO_STATION
{
    rsName[32],
    rsUrl[RADIO_URL_MAX]
}

// Public listener streams. Edit freely: name shown in /radio, MP3/AAC URL.
static const gRadioStations[][E_RADIO_STATION] =
{
    { "Radio Paradise (mix)",        "http://stream.radioparadise.com/mp3-128" },
    { "Groove Salad (chill)",        "http://ice1.somafm.com/groovesalad-128-mp3" },
    { "Indie Pop Rocks",             "http://ice1.somafm.com/indiepop-128-mp3" },
    { "Underground 80s",             "http://ice1.somafm.com/u80s-128-mp3" },
    { "Beat Blender (house)",        "http://ice1.somafm.com/beatblender-128-mp3" },
    { "Metal Detector",              "http://ice1.somafm.com/metal-128-mp3" }
};

new gVehRadio[MAX_VEHICLES] = { RADIO_OFF, ... };
static bool:gRadioOn[MAX_PLAYERS];       // hearing a vehicle's radio
static bool:gAdminStream[MAX_PLAYERS];   // an admin stream has the player's audio

stock Radio_StationCount()
{
    return sizeof (gRadioStations);
}

stock bool:Radio_IsValidStation(station)
{
    return 0 <= station < sizeof (gRadioStations);
}

// http(s) URL without spaces or quotes that fits the client's buffer.
stock bool:Radio_IsValidUrl(const url[])
{
    new len = strlen(url);
    if (len < 12 || len >= RADIO_URL_MAX) return false;
    if (strcmp(url, "http://", true, 7) != 0 && strcmp(url, "https://", true, 8) != 0) return false;
    for (new i = 0; i < len; i++)
    {
        if (url[i] <= ' ' || url[i] == '"' || url[i] == '\'' || url[i] > '~') return false;
    }
    return true;
}

stock Radio_Reset(playerid)
{
    gRadioOn[playerid] = false;
    gAdminStream[playerid] = false;
}

static bool:Radio_InVehicle(playerid, vehicleid)
{
    if (GetPlayerVehicleID(playerid) != vehicleid) return false;
    new PLAYER_STATE:pstate = GetPlayerState(playerid);
    return pstate == PLAYER_STATE_DRIVER || pstate == PLAYER_STATE_PASSENGER;
}

// Plays the player's vehicle station, or stops the radio if there's none.
static Radio_Update(playerid)
{
    if (gAdminStream[playerid]) return;
    new vehicleid = GetPlayerVehicleID(playerid);
    new station = (0 < vehicleid < MAX_VEHICLES && Radio_InVehicle(playerid, vehicleid)) ? gVehRadio[vehicleid] : RADIO_OFF;
    if (Radio_IsValidStation(station))
    {
        PlayAudioStreamForPlayer(playerid, gRadioStations[station][rsUrl]);
        gRadioOn[playerid] = true;
    }
    else if (gRadioOn[playerid])
    {
        StopAudioStreamForPlayer(playerid);
        gRadioOn[playerid] = false;
    }
}

stock Radio_SetStation(vehicleid, station)
{
    if (!(0 < vehicleid < MAX_VEHICLES)) return;
    gVehRadio[vehicleid] = Radio_IsValidStation(station) ? station : RADIO_OFF;
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (IsPlayerConnected(i) && Radio_InVehicle(i, vehicleid)) Radio_Update(i);
    }
}

stock Radio_OnStateChange(playerid, PLAYER_STATE:newstate)
{
    if (newstate == PLAYER_STATE_DRIVER || newstate == PLAYER_STATE_PASSENGER || gRadioOn[playerid]) Radio_Update(playerid);
}

stock Radio_OnVehicleSpawn(vehicleid)
{
    if (0 < vehicleid < MAX_VEHICLES) gVehRadio[vehicleid] = RADIO_OFF;
}

stock Radio_ShowMenu(playerid)
{
    new list[512] = "Off";
    for (new i = 0; i < sizeof (gRadioStations); i++)
    {
        strcat(list, "\n");
        strcat(list, gRadioStations[i][rsName]);
    }
    ShowPlayerDialog(playerid, DIALOG_RADIO, DIALOG_STYLE_LIST, "Radio", list, "Tune", "Close");
}

stock Radio_OnDialog(playerid, dialogid, response, listitem)
{
    if (dialogid != DIALOG_RADIO) return 0;
    if (!response || !Player_IsPlaying(playerid)) return 1;
    new vehicleid = GetPlayerVehicleID(playerid);
    if (vehicleid == 0 || GetPlayerState(playerid) != PLAYER_STATE_DRIVER) return SendError(playerid, "Only the driver can change the radio."), 1;
    new station = listitem - 1;
    Radio_SetStation(vehicleid, station);
    if (Radio_IsValidStation(station))
    {
        new action[64];
        format(action, sizeof (action), "tunes the radio to %s.", gRadioStations[station][rsName]);
        Chat_Action(playerid, action);
    }
    else
    {
        Chat_Action(playerid, "turns the radio off.");
    }
    return 1;
}

CMD:radio(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    new vehicleid = GetPlayerVehicleID(playerid);
    if (vehicleid == 0 || GetPlayerState(playerid) != PLAYER_STATE_DRIVER) return SendError(playerid, "You need to be driving.");
    if (!isnull(params) && strcmp(params, "off", true) == 0)
    {
        Radio_SetStation(vehicleid, RADIO_OFF);
        Chat_Action(playerid, "turns the radio off.");
        return 1;
    }
    Radio_ShowMenu(playerid);
    return 1;
}

// Admin streams: "/playstream all <url>" or "/playstream <player> <url>".
stock Radio_AdminPlay(targetid, const url[])
{
    gAdminStream[targetid] = true;
    gRadioOn[targetid] = false;
    PlayAudioStreamForPlayer(targetid, url);
}

static Radio_AdminStop(targetid)
{
    if (!gAdminStream[targetid]) return;
    gAdminStream[targetid] = false;
    StopAudioStreamForPlayer(targetid);
    Radio_Update(targetid);
}

CMD:playstream(playerid, params[])
{
    if (gPlayer[playerid][pAdmin] < ADMIN_ADMIN) return SendError(playerid, "You don't have permission to use this command.");
    new target[24], url[RADIO_URL_MAX + 8];
    if (sscanf(params, "s[24]s[136]", target, url)) return SendInfo(playerid, "Usage: /playstream [player/all] [http(s) url]");
    if (!Radio_IsValidUrl(url)) return SendError(playerid, "That isn't a usable http(s) stream URL.");
    if (strcmp(target, "all", true) == 0)
    {
        new count;
        for (new i = 0; i < MAX_PLAYERS; i++)
        {
            if (Player_IsPlaying(i)) Radio_AdminPlay(i, url), count++;
        }
        SendClientMessage(playerid, COLOR_ADMIN, "Playing the stream for %d players. Stop it with /stopstream all.", count);
        return 1;
    }
    new targetid;
    if (sscanf(target, "u", targetid) || targetid == INVALID_PLAYER_ID || !Player_IsPlaying(targetid)) return SendError(playerid, "That player isn't playing.");
    Radio_AdminPlay(targetid, url);
    SendClientMessage(playerid, COLOR_ADMIN, "Playing the stream for %s.", gChar[targetid][cName]);
    return 1;
}

CMD:stopstream(playerid, params[])
{
    if (gPlayer[playerid][pAdmin] < ADMIN_ADMIN) return SendError(playerid, "You don't have permission to use this command.");
    if (isnull(params)) return SendInfo(playerid, "Usage: /stopstream [player/all]");
    if (strcmp(params, "all", true) == 0)
    {
        for (new i = 0; i < MAX_PLAYERS; i++)
        {
            if (IsPlayerConnected(i)) Radio_AdminStop(i);
        }
        SendClientMessage(playerid, COLOR_ADMIN, "Stopped the stream for everyone.");
        return 1;
    }
    new targetid;
    if (sscanf(params, "u", targetid) || targetid == INVALID_PLAYER_ID) return SendError(playerid, "That player isn't connected.");
    Radio_AdminStop(targetid);
    SendClientMessage(playerid, COLOR_ADMIN, "Stopped the stream for %s.", gChar[targetid][cName]);
    return 1;
}
