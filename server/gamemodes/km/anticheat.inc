// Server-side anti-cheat, checked once a second for every playing
// character (the hooks are in achooks.inc):
//   weapon   a weapon the server never gave: removed, kick on the 3rd time
//   jetpack  kicked at once
//   health   health or armour above 100: put back to 100
//   teleport on foot, more than KM_AC_TELEPORT_METRES in one second without a
//            server teleport: reported to admins only (falls and lag can do it)
// Money can't be cheated: it's server-side (economy.inc) and the display is
// reset every second. Every finding goes to anticheat_log and online admins.

new gAcWeaponStrikes[MAX_PLAYERS];
new gAcLastAlert[MAX_PLAYERS];
new Float:gAcLastX[MAX_PLAYERS], Float:gAcLastY[MAX_PLAYERS];
new bool:gAcHavePos[MAX_PLAYERS];

stock AC_Reset(playerid)
{
    AC_ResetWeapons(playerid);
    gAcGraceUntil[playerid] = 0;
    gAcWeaponStrikes[playerid] = 0;
    gAcLastAlert[playerid] = 0;
    gAcHavePos[playerid] = false;
}

// A weapon the client may hold in a slot: nothing, what the server gave, or
// a parachute (the game hands those out when you jump from aircraft).
stock bool:AC_WeaponAllowed(WEAPON:allowed, WEAPON:held)
{
    return held == WEAPON:0 || held == allowed || held == WEAPON_PARACHUTE;
}

stock bool:AC_IsTeleport(Float:fromX, Float:fromY, Float:toX, Float:toY, Float:limit)
{
    new Float:dx = toX - fromX, Float:dy = toY - fromY;
    return dx * dx + dy * dy > limit * limit;
}

stock bool:AC_Report(playerid, const type[], const details[], bool:kick)
{
    new query[400];
    mysql_format(gDB, query, sizeof (query),
        "INSERT INTO anticheat_log (account_id, character_id, type, details, kicked) VALUES (NULLIF(%d, 0), NULLIF(%d, 0), '%e', '%e', %d)",
        gPlayer[playerid][pAccountId], gChar[playerid][cId], type, details, kick ? 1 : 0);
    mysql_tquery(gDB, query);
    printf("[anticheat] %s (#%d) %s: %s%s", gPlayer[playerid][pUsername], gPlayer[playerid][pAccountId], type, details, kick ? " (kicked)" : "");

    // One admin alert per player every 30 seconds, kicks always.
    if (kick || gettime() - gAcLastAlert[playerid] >= 30)
    {
        gAcLastAlert[playerid] = gettime();
        new line[160];
        format(line, sizeof (line), "[AC] %s [%d] %s: %s%s", gPlayer[playerid][pUsername], playerid, type, details, kick ? (" - kicked") : (""));
        Sup_NotifyStaff(REPORT_KIND_REPORT, COLOR_ERROR, line);
    }
    if (kick)
    {
        SendClientMessage(playerid, COLOR_ERROR, "You were kicked by the anti-cheat (%s). If this is a mistake, appeal on our community page.", type);
        KickDelayed(playerid);
    }
    return kick;
}

stock AC_Tick(playerid)
{
    if (GetPlayerSpecialAction(playerid) == SPECIAL_ACTION_USEJETPACK)
    {
        AC_Report(playerid, "jetpack", "", true);
        return;
    }

    for (new slot = 0; slot < AC_WEAPON_SLOTS; slot++)
    {
        new WEAPON:held, ammo;
        GetPlayerWeaponData(playerid, WEAPON_SLOT:slot, held, ammo);
        if (ammo <= 0 && slot != 0 && slot != 1 && slot != 10 && slot != 11) continue; // empty slots report stale ids
        if (AC_WeaponAllowed(gAcWeapon[playerid][slot], held)) continue;

        new details[48];
        format(details, sizeof (details), "weapon %d in slot %d", _:held, slot);
        gAcWeaponStrikes[playerid]++;
        if (AC_Report(playerid, "weapon", details, gAcWeaponStrikes[playerid] >= 3)) return;
        // Take everything away and give back only what the server handed out.
        new WEAPON:keep[AC_WEAPON_SLOTS];
        for (new i = 0; i < AC_WEAPON_SLOTS; i++) keep[i] = gAcWeapon[playerid][i];
        ResetPlayerWeapons(playerid);
        for (new i = 0; i < AC_WEAPON_SLOTS; i++)
        {
            if (keep[i] != WEAPON:0) GivePlayerWeapon(playerid, keep[i], i == 1 || i == 10 ? 1 : 50);
        }
        break;
    }

    new Float:health, Float:armour;
    GetPlayerHealth(playerid, health);
    GetPlayerArmour(playerid, armour);
    if (health > 100.5 || armour > 100.5)
    {
        new details[48];
        format(details, sizeof (details), "health %.0f armour %.0f", health, armour);
        AC_Report(playerid, "health", details, false);
        if (health > 100.5) SetPlayerHealth(playerid, 100.0);
        if (armour > 100.5) SetPlayerArmour(playerid, 100.0);
    }

    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    if (gAcHavePos[playerid] && GetPlayerState(playerid) == PLAYER_STATE_ONFOOT && gettime() > gAcGraceUntil[playerid]
        && AC_IsTeleport(gAcLastX[playerid], gAcLastY[playerid], x, y, KM_AC_TELEPORT_METRES))
    {
        new details[64];
        format(details, sizeof (details), "%.0f m in 1 s", floatsqroot((x - gAcLastX[playerid]) * (x - gAcLastX[playerid]) + (y - gAcLastY[playerid]) * (y - gAcLastY[playerid])));
        AC_Report(playerid, "teleport", details, false);
    }
    gAcLastX[playerid] = x;
    gAcLastY[playerid] = y;
    gAcHavePos[playerid] = true;
}
