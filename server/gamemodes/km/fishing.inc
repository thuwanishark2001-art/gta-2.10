// Fishing at Santa Maria pier (gFishPier): /fish takes KM_FISH_SECONDS and
// may catch something; catches are kept (characters.fish_count and
// fish_value, up to KM_FISH_MAX) and sold at the pier with /sellfish.

enum E_FISH { fsWeight, fsValue, fsName[24] }

// Weights add up to 100. A value of 0 is "nothing".
static const gFishTable[][E_FISH] =
{
    { 25,    0, "nothing" },
    { 40,   40, "a small mackerel" },
    { 25,   90, "a sea bass" },
    {  9,  200, "a big tuna" },
    {  1, 1000, "a golden marlin" }
};

static gFishing[MAX_PLAYERS];           // timer id, or 0
static gFishCount[MAX_PLAYERS];
static gFishValue[MAX_PLAYERS];

forward Fish_OnCatch(playerid, race);
forward Fish_OnLoaded(playerid, race);

stock Fish_TotalWeight()
{
    new total = 0;
    for (new i = 0; i < sizeof (gFishTable); i++) total += gFishTable[i][fsWeight];
    return total;
}

// The catch for a roll of 0..99.
stock Fish_CatchFor(roll)
{
    new upTo = 0;
    for (new i = 0; i < sizeof (gFishTable); i++)
    {
        upTo += gFishTable[i][fsWeight];
        if (roll < upTo) return i;
    }
    return 0;
}

stock Fish_Value(fish) { return gFishTable[fish][fsValue]; }

stock Fish_Reset(playerid)
{
    if (gFishing[playerid]) KillTimer(gFishing[playerid]);
    gFishing[playerid] = 0;
    gFishCount[playerid] = 0;
    gFishValue[playerid] = 0;
}

// From the first spawn.
stock Fish_Load(playerid)
{
    new query[96];
    mysql_format(gDB, query, sizeof (query), "SELECT fish_count, fish_value FROM characters WHERE id = %d", gChar[playerid][cId]);
    mysql_tquery(gDB, query, "Fish_OnLoaded", "dd", playerid, gPlayerRace[playerid]);
}

public Fish_OnLoaded(playerid, race)
{
    if (gPlayerRace[playerid] != race || !cache_num_rows()) return 1;
    cache_get_value_name_int(0, "fish_count", gFishCount[playerid]);
    cache_get_value_name_int(0, "fish_value", gFishValue[playerid]);
    return 1;
}

static Fish_Save(playerid)
{
    new query[128];
    mysql_format(gDB, query, sizeof (query), "UPDATE characters SET fish_count = %d, fish_value = %d WHERE id = %d",
        gFishCount[playerid], gFishValue[playerid], gChar[playerid][cId]);
    mysql_tquery(gDB, query);
}

static bool:Fish_AtPier(playerid)
{
    return IsPlayerInRangeOfPoint(playerid, 30.0, gFishPier[ptX], gFishPier[ptY], gFishPier[ptZ]);
}

CMD:fish(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    if (!Fish_AtPier(playerid)) return SendError(playerid, "You need to be on Santa Maria pier (/gps).");
    if (GetPlayerState(playerid) != PLAYER_STATE_ONFOOT) return SendError(playerid, "You can't fish from a vehicle.");
    if (gFishing[playerid]) return SendError(playerid, "You're already fishing.");
    if (gFishCount[playerid] >= KM_FISH_MAX) return SendClientMessage(playerid, COLOR_ERROR, "[!] You can't carry more than %d fish. /sellfish first.", KM_FISH_MAX), 1;
    gFishing[playerid] = SetTimerEx("Fish_OnCatch", KM_FISH_SECONDS * 1000, false, "dd", playerid, gPlayerRace[playerid]);
    Chat_Action(playerid, "casts a fishing line.");
    SendClientMessage(playerid, COLOR_INFO, "You cast your line... wait for a bite.");
    return 1;
}

public Fish_OnCatch(playerid, race)
{
    if (gPlayerRace[playerid] != race) return 1;
    gFishing[playerid] = 0;
    if (!Player_IsPlaying(playerid) || !Fish_AtPier(playerid)) return SendInfo(playerid, "You left the pier, and the fish got away."), 1;
    new fish = Fish_CatchFor(random(Fish_TotalWeight()));
    new value = gFishTable[fish][fsValue];
    if (value == 0) return SendClientMessage(playerid, COLOR_INFO, "Nothing bit this time. Try again with /fish."), 1;
    gFishCount[playerid]++;
    gFishValue[playerid] += value;
    Fish_Save(playerid);
    Pass_AddPoints(playerid, 1);
    SendClientMessage(playerid, COLOR_SUCCESS, "You caught %s (worth $%d). You have %d fish, worth $%d. Sell them with /sellfish.",
        gFishTable[fish][fsName], value, gFishCount[playerid], gFishValue[playerid]);
    return 1;
}

CMD:sellfish(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    if (!Fish_AtPier(playerid)) return SendError(playerid, "The fish market is on Santa Maria pier (/gps).");
    if (gFishCount[playerid] == 0) return SendError(playerid, "You don't have any fish.");
    new count = gFishCount[playerid], value = gFishValue[playerid];
    new details[32];
    format(details, sizeof (details), "%d fish", count);
    if (!Money_Give(playerid, value, 0, "fishing", INVALID_PLAYER_ID, details)) return 1;
    gFishCount[playerid] = 0;
    gFishValue[playerid] = 0;
    Fish_Save(playerid);
    SendClientMessage(playerid, COLOR_SUCCESS, "You sold %d fish for $%d.", count, value);
    return 1;
}
