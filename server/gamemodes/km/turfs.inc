// Gangs and turfs. Turfs (table turfs) are rectangles shown on everyone's
// radar in the owning gang's colour. A gang member inside a turf their gang
// doesn't hold can /capture it: a KM_TURF_WAR_SECONDS war in which the zone
// flashes for everyone. When it ends, the side with more of its members
// alive inside the zone wins (the attackers need more than the defenders).
// Each payday, gang members earn KM_TURF_INCOME per turf their gang holds.

enum E_TURF
{
    bool:tUsed,
    tId,
    tName[32],
    Float:tMinX, Float:tMinY, Float:tMaxX, Float:tMaxY,
    tOwner,             // faction id, 0 = nobody
    tZone,              // gang zone id
    tAttacker,          // faction attacking, 0 = no war
    tWarEnds,           // gettime()
    tCooldownUntil      // gettime()
}

new gTurf[KM_MAX_TURFS][E_TURF];

forward Turf_OnLoaded();

stock bool:Turf_Contains(Float:minX, Float:minY, Float:maxX, Float:maxY, Float:x, Float:y)
{
    return minX <= x <= maxX && minY <= y <= maxY;
}

// Who holds the turf after a war: the attackers if they outnumber the
// defenders inside the zone, else the owner.
stock Turf_Winner(owner, attacker, attackersIn, defendersIn)
{
    return (attackersIn > 0 && attackersIn > defendersIn) ? attacker : owner;
}

stock bool:Turf_IsGang(factionId)
{
    return factionId > 0 && Fac_Type(factionId) == FACTION_GANG;
}

// Radar colour: the gang's colour, translucent; grey for nobody's.
stock Turf_Colour(owner)
{
    if (owner <= 0 || !Fac_Valid(owner)) return 0xFFFFFF33;
    return (gFac[owner][fColour] & ~0xFF) | 0x66;
}

stock Turf_Load()
{
    mysql_tquery(gDB, "SELECT id, name, min_x, min_y, max_x, max_y, IFNULL(owner_faction_id, 0) AS owner FROM turfs ORDER BY id", "Turf_OnLoaded");
}

public Turf_OnLoaded()
{
    new rows = cache_num_rows();
    for (new i = 0; i < rows && i < KM_MAX_TURFS; i++)
    {
        gTurf[i][tUsed] = true;
        cache_get_value_name_int(i, "id", gTurf[i][tId]);
        cache_get_value_name(i, "name", gTurf[i][tName], 32);
        cache_get_value_name_float(i, "min_x", gTurf[i][tMinX]);
        cache_get_value_name_float(i, "min_y", gTurf[i][tMinY]);
        cache_get_value_name_float(i, "max_x", gTurf[i][tMaxX]);
        cache_get_value_name_float(i, "max_y", gTurf[i][tMaxY]);
        cache_get_value_name_int(i, "owner", gTurf[i][tOwner]);
        gTurf[i][tZone] = GangZoneCreate(gTurf[i][tMinX], gTurf[i][tMinY], gTurf[i][tMaxX], gTurf[i][tMaxY]);
        gTurf[i][tAttacker] = 0;
    }
    printf("[turf] loaded %d turfs", rows);
    return 1;
}

// From the first spawn: puts every turf on the player's radar.
stock Turf_ShowAll(playerid)
{
    for (new i = 0; i < KM_MAX_TURFS; i++)
    {
        if (!gTurf[i][tUsed]) continue;
        GangZoneShowForPlayer(playerid, gTurf[i][tZone], Turf_Colour(gTurf[i][tOwner]));
        if (gTurf[i][tAttacker]) GangZoneFlashForPlayer(playerid, gTurf[i][tZone], Turf_Colour(gTurf[i][tAttacker]));
    }
}

static Turf_Redraw(slot)
{
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (!Player_IsPlaying(i)) continue;
        GangZoneShowForPlayer(i, gTurf[slot][tZone], Turf_Colour(gTurf[slot][tOwner]));
        if (gTurf[slot][tAttacker]) GangZoneFlashForPlayer(i, gTurf[slot][tZone], Turf_Colour(gTurf[slot][tAttacker]));
        else GangZoneStopFlashForPlayer(i, gTurf[slot][tZone]);
    }
}

// The turf the player is standing in, or -1.
stock Turf_At(playerid)
{
    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    if (GetPlayerInterior(playerid) != 0 || GetPlayerVirtualWorld(playerid) != 0) return -1;
    for (new i = 0; i < KM_MAX_TURFS; i++)
    {
        if (gTurf[i][tUsed] && Turf_Contains(gTurf[i][tMinX], gTurf[i][tMinY], gTurf[i][tMaxX], gTurf[i][tMaxY], x, y)) return i;
    }
    return -1;
}

stock Turf_CountOwned(factionId)
{
    if (factionId <= 0) return 0;
    new n = 0;
    for (new i = 0; i < KM_MAX_TURFS; i++)
    {
        if (gTurf[i][tUsed] && gTurf[i][tOwner] == factionId) n++;
    }
    return n;
}

// Payday bonus for gang members.
stock Turf_Income(playerid)
{
    new fac = gChar[playerid][cFaction];
    return Turf_IsGang(fac) ? KM_TURF_INCOME * Turf_CountOwned(fac) : 0;
}

static Turf_CountIn(slot, factionId)
{
    new n = 0;
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (!Player_IsPlaying(i) || gChar[i][cFaction] != factionId || GetPlayerState(i) == PLAYER_STATE_WASTED) continue;
        if (Turf_At(i) == slot) n++;
    }
    return n;
}

static Turf_EndWar(slot)
{
    new attacker = gTurf[slot][tAttacker];
    if (!attacker) return;
    new owner = gTurf[slot][tOwner];
    new attackersIn = Turf_CountIn(slot, attacker);
    new defendersIn = owner ? Turf_CountIn(slot, owner) : 0;
    new winner = Turf_Winner(owner, attacker, attackersIn, defendersIn);
    gTurf[slot][tAttacker] = 0;
    gTurf[slot][tCooldownUntil] = gettime() + KM_TURF_COOLDOWN_SECONDS;
    new text[144];
    if (winner == attacker)
    {
        gTurf[slot][tOwner] = attacker;
        new query[160];
        mysql_format(gDB, query, sizeof (query), "UPDATE turfs SET owner_faction_id = %d, captured_at = UTC_TIMESTAMP() WHERE id = %d", attacker, gTurf[slot][tId]);
        mysql_tquery(gDB, query);
        format(text, sizeof (text), "TURF WAR: %s took %s (%d against %d).", gFac[attacker][fName], gTurf[slot][tName], attackersIn, defendersIn);
    }
    else if (owner)
    {
        format(text, sizeof (text), "TURF WAR: %s held %s (%d against %d).", gFac[owner][fName], gTurf[slot][tName], defendersIn, attackersIn);
    }
    else
    {
        format(text, sizeof (text), "TURF WAR: %s failed to take %s.", gFac[attacker][fName], gTurf[slot][tName]);
    }
    SendClientMessageToAll(COLOR_BRAND, text);
    Turf_Redraw(slot);
}

// From KM_Tick, once a second.
stock Turf_Tick()
{
    new now = gettime();
    for (new i = 0; i < KM_MAX_TURFS; i++)
    {
        if (gTurf[i][tUsed] && gTurf[i][tAttacker] && gTurf[i][tWarEnds] <= now) Turf_EndWar(i);
    }
}

CMD:capture(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    new fac = gChar[playerid][cFaction];
    if (!Turf_IsGang(fac)) return SendError(playerid, "Only gang members can capture turfs.");
    new slot = Turf_At(playerid);
    if (slot == -1) return SendError(playerid, "You need to be inside a turf (see /turfs).");
    if (gTurf[slot][tOwner] == fac) return SendError(playerid, "Your gang already holds this turf.");
    if (gTurf[slot][tAttacker]) return SendError(playerid, "There's already a war for this turf.");
    for (new i = 0; i < KM_MAX_TURFS; i++)
    {
        if (gTurf[i][tUsed] && gTurf[i][tAttacker] == fac) return SendError(playerid, "Your gang is already fighting for a turf.");
    }
    new wait = gTurf[slot][tCooldownUntil] - gettime();
    if (wait > 0) return SendClientMessage(playerid, COLOR_ERROR, "[!] This turf can be attacked again in %d min.", (wait + 59) / 60), 1;
    gTurf[slot][tAttacker] = fac;
    gTurf[slot][tWarEnds] = gettime() + KM_TURF_WAR_SECONDS;
    new text[144];
    if (gTurf[slot][tOwner])
        format(text, sizeof (text), "TURF WAR: %s is attacking %s, held by %s! %d minutes.", gFac[fac][fName], gTurf[slot][tName], gFac[gTurf[slot][tOwner]][fName], KM_TURF_WAR_SECONDS / 60);
    else
        format(text, sizeof (text), "TURF WAR: %s is taking %s! %d minutes.", gFac[fac][fName], gTurf[slot][tName], KM_TURF_WAR_SECONDS / 60);
    SendClientMessageToAll(COLOR_BRAND, text);
    Turf_Redraw(slot);
    return 1;
}

CMD:turfs(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    SendClientMessage(playerid, COLOR_BRAND, "Turfs:");
    for (new i = 0; i < KM_MAX_TURFS; i++)
    {
        if (!gTurf[i][tUsed]) continue;
        new owner = gTurf[i][tOwner], holder[48] = "nobody";
        if (owner) format(holder, sizeof (holder), "%s", gFac[owner][fName]);
        if (gTurf[i][tAttacker])
            SendClientMessage(playerid, COLOR_WHITE, "  %s: %s (under attack by %s)", gTurf[i][tName], holder, gFac[gTurf[i][tAttacker]][fName]);
        else
            SendClientMessage(playerid, COLOR_WHITE, "  %s: %s", gTurf[i][tName], holder);
    }
    return 1;
}

// Lead admins: end every running war now (events, testing).
CMD:endwars(playerid, params[])
{
    if (!Mod_CanAct(playerid, INVALID_PLAYER_ID, ADMIN_LEAD)) return 1;
    new ended = 0;
    for (new i = 0; i < KM_MAX_TURFS; i++)
    {
        if (gTurf[i][tUsed] && gTurf[i][tAttacker]) Turf_EndWar(i), ended++;
    }
    if (ended == 0) return SendInfo(playerid, "No turf wars are running.");
    Log_Admin(Mod_ActorId(playerid), "end_turf_wars", 0, "");
    return 1;
}
