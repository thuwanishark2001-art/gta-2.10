// Admin spectate: /spec [player] watches them, /specoff puts the admin back
// where they were (position, interior, world, health and armour).

enum E_SPEC
{
    bool:spOn,
    bool:spRestore,     // the next spawn comes back from spectating
    spTarget,
    Float:spX, Float:spY, Float:spZ, Float:spA,
    spInterior, spWorld,
    Float:spHealth, Float:spArmour
}
static gSpec[MAX_PLAYERS][E_SPEC];

stock Spec_Reset(playerid)
{
    static const empty[E_SPEC];
    gSpec[playerid] = empty;
    gSpec[playerid][spTarget] = INVALID_PLAYER_ID;
}

stock bool:Spec_IsSpectating(playerid)
{
    return gSpec[playerid][spOn];
}

// Where a spectating admin really is, for saving the character.
stock Spec_SavedPlace(playerid, &Float:x, &Float:y, &Float:z, &Float:a, &interior, &world)
{
    x = gSpec[playerid][spX];
    y = gSpec[playerid][spY];
    z = gSpec[playerid][spZ];
    a = gSpec[playerid][spA];
    interior = gSpec[playerid][spInterior];
    world = gSpec[playerid][spWorld];
}

// Points the spectator's camera at the target, or the target's vehicle.
static Spec_Follow(playerid)
{
    new targetid = gSpec[playerid][spTarget];
    SetPlayerInterior(playerid, GetPlayerInterior(targetid));
    SetPlayerVirtualWorld(playerid, GetPlayerVirtualWorld(targetid));
    new vehicleid = GetPlayerVehicleID(targetid);
    if (vehicleid) PlayerSpectateVehicle(playerid, vehicleid);
    else PlayerSpectatePlayer(playerid, targetid);
}

static Spec_Stop(playerid, const reason[] = "")
{
    if (!gSpec[playerid][spOn]) return;
    gSpec[playerid][spOn] = false;
    gSpec[playerid][spTarget] = INVALID_PLAYER_ID;
    gSpec[playerid][spRestore] = true;
    // Spectating off respawns the player: spawn where they were.
    SetSpawnInfo(playerid, NO_TEAM, gChar[playerid][cSkin], gSpec[playerid][spX], gSpec[playerid][spY], gSpec[playerid][spZ], gSpec[playerid][spA]);
    TogglePlayerSpectating(playerid, false);
    if (reason[0] != '\0') SendClientMessage(playerid, COLOR_ADMIN, "Stopped spectating: %s", reason);
    else SendClientMessage(playerid, COLOR_ADMIN, "Stopped spectating.");
}

// From OnPlayerSpawn, after Char_OnSpawn.
stock Spec_OnSpawn(playerid)
{
    if (!gSpec[playerid][spRestore]) return;
    gSpec[playerid][spRestore] = false;
    SetPlayerInterior(playerid, gSpec[playerid][spInterior]);
    SetPlayerVirtualWorld(playerid, gSpec[playerid][spWorld]);
    SetPlayerHealth(playerid, gSpec[playerid][spHealth]);
    SetPlayerArmour(playerid, gSpec[playerid][spArmour]);
}

// From OnPlayerStateChange: spectators follow their target into and out of vehicles.
stock Spec_OnStateChange(playerid)
{
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (gSpec[i][spOn] && gSpec[i][spTarget] == playerid && IsPlayerConnected(i)) Spec_Follow(i);
    }
}

// From OnPlayerDisconnect: anyone watching this player stops.
stock Spec_OnDisconnect(playerid)
{
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (i != playerid && gSpec[i][spOn] && gSpec[i][spTarget] == playerid && IsPlayerConnected(i)) Spec_Stop(i, "they left the server.");
    }
}

CMD:spec(playerid, params[])
{
    new targetid;
    if (sscanf(params, "u", targetid)) return SendInfo(playerid, "Usage: /spec [player]. /specoff stops.");
    if (!Mod_CanAct(playerid, INVALID_PLAYER_ID, ADMIN_MOD)) return 1;
    if (targetid == INVALID_PLAYER_ID || !Player_IsPlaying(targetid)) return SendError(playerid, "That player isn't playing.");
    if (targetid == playerid) return SendError(playerid, "You can't spectate yourself.");
    if (!Player_IsPlaying(playerid)) return SendError(playerid, "Pick a character first.");
    if (gJob[playerid][jsWorking]) return SendError(playerid, "Finish your shift first (/stopwork).");
    if (gSpec[targetid][spOn]) return SendError(playerid, "They're spectating someone.");

    if (!gSpec[playerid][spOn])
    {
        GetPlayerPos(playerid, gSpec[playerid][spX], gSpec[playerid][spY], gSpec[playerid][spZ]);
        GetPlayerFacingAngle(playerid, gSpec[playerid][spA]);
        gSpec[playerid][spInterior] = GetPlayerInterior(playerid);
        gSpec[playerid][spWorld] = GetPlayerVirtualWorld(playerid);
        GetPlayerHealth(playerid, gSpec[playerid][spHealth]);
        GetPlayerArmour(playerid, gSpec[playerid][spArmour]);
        if (gSpec[playerid][spHealth] < 1.0) gSpec[playerid][spHealth] = 100.0;
        TogglePlayerSpectating(playerid, true);
        gSpec[playerid][spOn] = true;
    }
    gSpec[playerid][spTarget] = targetid;
    Spec_Follow(playerid);
    SendClientMessage(playerid, COLOR_ADMIN, "Spectating %s [%d]. /specoff stops.", gChar[targetid][cName], targetid);
    new details[48];
    format(details, sizeof (details), "%s", gChar[targetid][cName]);
    Log_Admin(Mod_ActorId(playerid), "spectate", gPlayer[targetid][pAccountId], details);
    return 1;
}

CMD:specoff(playerid, params[])
{
    if (!gSpec[playerid][spOn]) return SendError(playerid, "You're not spectating.");
    Spec_Stop(playerid);
    return 1;
}
