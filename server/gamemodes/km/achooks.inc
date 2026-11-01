// Anti-cheat bookkeeping. Wraps the natives that hand out weapons or move
// players, so anticheat.inc knows what the server itself did:
//   - the weapon the server last gave in each slot,
//   - a grace period after every server teleport or spawn.
// Include before any module that calls these natives.

#define AC_WEAPON_SLOTS     (13)
#define AC_GRACE_SECONDS    (3)

new WEAPON:gAcWeapon[MAX_PLAYERS][AC_WEAPON_SLOTS];
new gAcGraceUntil[MAX_PLAYERS];

stock AC_Grace(playerid)
{
    if (0 <= playerid < MAX_PLAYERS) gAcGraceUntil[playerid] = gettime() + AC_GRACE_SECONDS;
}

stock AC_ResetWeapons(playerid)
{
    for (new i = 0; i < AC_WEAPON_SLOTS; i++) gAcWeapon[playerid][i] = WEAPON:0;
}

stock bool:AC_GivePlayerWeapon(playerid, WEAPON:weaponid, ammo)
{
    new slot = _:GetWeaponSlot(weaponid);
    if (0 <= playerid < MAX_PLAYERS && 0 <= slot < AC_WEAPON_SLOTS) gAcWeapon[playerid][slot] = weaponid;
    return GivePlayerWeapon(playerid, weaponid, ammo);
}

stock bool:AC_ResetPlayerWeapons(playerid)
{
    if (0 <= playerid < MAX_PLAYERS) AC_ResetWeapons(playerid);
    return ResetPlayerWeapons(playerid);
}

stock bool:AC_SetPlayerPos(playerid, Float:x, Float:y, Float:z)
{
    AC_Grace(playerid);
    return SetPlayerPos(playerid, x, y, z);
}

stock bool:AC_SetPlayerPosFindZ(playerid, Float:x, Float:y, Float:z)
{
    AC_Grace(playerid);
    return SetPlayerPosFindZ(playerid, x, y, z);
}

stock bool:AC_PutPlayerInVehicle(playerid, vehicleid, seatid)
{
    AC_Grace(playerid);
    return PutPlayerInVehicle(playerid, vehicleid, seatid);
}

stock bool:AC_SetVehiclePos(vehicleid, Float:x, Float:y, Float:z)
{
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (IsPlayerConnected(i) && GetPlayerVehicleID(i) == vehicleid) AC_Grace(i);
    }
    return SetVehiclePos(vehicleid, x, y, z);
}

stock bool:AC_TogglePlayerSpectating(playerid, bool:toggle)
{
    AC_Grace(playerid);
    return TogglePlayerSpectating(playerid, toggle);
}

#define GivePlayerWeapon AC_GivePlayerWeapon
#define ResetPlayerWeapons AC_ResetPlayerWeapons
#define SetPlayerPos AC_SetPlayerPos
#define SetPlayerPosFindZ AC_SetPlayerPosFindZ
#define PutPlayerInVehicle AC_PutPlayerInVehicle
#define SetVehiclePos AC_SetVehiclePos
#define TogglePlayerSpectating AC_TogglePlayerSpectating
