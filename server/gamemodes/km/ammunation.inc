// Ammu-Nation (gAmmunation): /buygun opens the counter. Melee weapons and
// armour are for anyone; guns need a gun licence (/buylicence gun at the
// licensing office, level KM_GUN_LICENCE_LEVEL+). Weapons last until you die
// or log out.

enum E_GUN { gnWeapon, gnAmmo, gnPrice, bool:gnLicence, gnName[24] }

static const gGunStock[][E_GUN] =
{
    { _:WEAPON_BAT,      1,    100, false, "Baseball bat" },
    { _:WEAPON_KNIFE,    1,    150, false, "Knife" },
    { -1,                0,    750, false, "Body armour" },
    { _:WEAPON_COLT45,   60,  1500, true,  "9mm pistol + 60" },
    { _:WEAPON_DEAGLE,   35,  4000, true,  "Desert Eagle + 35" },
    { _:WEAPON_SHOTGUN,  30,  3500, true,  "Shotgun + 30" },
    { _:WEAPON_MP5,      120, 6000, true,  "MP5 + 120" }
};

static bool:gGunLicence[MAX_PLAYERS];

forward Gun_OnLoaded(playerid, race);

stock Gun_Reset(playerid) { gGunLicence[playerid] = false; }
stock bool:Gun_HasLicence(playerid) { return gGunLicence[playerid]; }
stock Gun_StockCount() { return sizeof (gGunStock); }
stock bool:Gun_NeedsLicence(item) { return gGunStock[item][gnLicence]; }

// From the first spawn.
stock Gun_Load(playerid)
{
    new query[80];
    mysql_format(gDB, query, sizeof (query), "SELECT gun_licence FROM characters WHERE id = %d", gChar[playerid][cId]);
    mysql_tquery(gDB, query, "Gun_OnLoaded", "dd", playerid, gPlayerRace[playerid]);
}

public Gun_OnLoaded(playerid, race)
{
    if (gPlayerRace[playerid] != race || !cache_num_rows()) return 1;
    new licence;
    cache_get_value_name_int(0, "gun_licence", licence);
    gGunLicence[playerid] = licence != 0;
    return 1;
}

stock Gun_SetLicence(playerid, bool:has)
{
    gGunLicence[playerid] = has;
    new query[96];
    mysql_format(gDB, query, sizeof (query), "UPDATE characters SET gun_licence = %d WHERE id = %d", has ? 1 : 0, gChar[playerid][cId]);
    mysql_tquery(gDB, query);
}

CMD:buylicence(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    if (strcmp(params, "gun", true) != 0) return SendInfo(playerid, "Usage: /buylicence gun. Driving licences come from /drivingtest.");
    if (!Place_Near(playerid, gLicenceOffice, 5.0)) return SendError(playerid, "Buy it at the licensing office by City Hall (/gps).");
    if (gGunLicence[playerid]) return SendInfo(playerid, "You already have a gun licence.");
    if (gChar[playerid][cLevel] < KM_GUN_LICENCE_LEVEL) return SendClientMessage(playerid, COLOR_ERROR, "[!] You need to be level %d.", KM_GUN_LICENCE_LEVEL), 1;
    if (gChar[playerid][cWanted] > 0) return SendError(playerid, "Not while you're wanted by the police.");
    if (!Money_Charge(playerid, KM_GUN_LICENCE_PRICE, "gun_licence")) return SendClientMessage(playerid, COLOR_ERROR, "[!] A gun licence costs $%d.", KM_GUN_LICENCE_PRICE), 1;
    Gun_SetLicence(playerid, true);
    SendClientMessage(playerid, COLOR_SUCCESS, "You have a gun licence now ($%d). Ammu-Nation is on /gps.", KM_GUN_LICENCE_PRICE);
    return 1;
}

// Police rank 3+: take a gun licence away.
CMD:revokegun(playerid, params[])
{
    new targetid;
    if (!Fac_PoliceCheck(playerid)) return 1;
    if (gChar[playerid][cFactionRank] < 3) return SendError(playerid, "You need rank 3 or higher.");
    if (sscanf(params, "u", targetid)) return SendInfo(playerid, "Usage: /revokegun [player]");
    if (!Player_IsPlaying(targetid)) return SendError(playerid, "That player isn't playing.");
    if (!gGunLicence[targetid]) return SendError(playerid, "They don't have a gun licence.");
    Gun_SetLicence(targetid, false);
    SendClientMessage(playerid, COLOR_DEPARTMENT, "You revoked %s's gun licence.", gChar[targetid][cName]);
    SendClientMessage(targetid, COLOR_ERROR, "[!] %s revoked your gun licence.", gChar[playerid][cName]);
    return 1;
}

CMD:buygun(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    if (!Place_Near(playerid, gAmmunation, 6.0)) return SendError(playerid, "You need to be at Ammu-Nation in Market (/gps).");
    new body[512] = "Item\tPrice\tLicence\n", line[64];
    for (new i = 0; i < sizeof (gGunStock); i++)
    {
        format(line, sizeof (line), "%s\t$%d\t%s\n", gGunStock[i][gnName], gGunStock[i][gnPrice], gGunStock[i][gnLicence] ? ("gun licence") : ("-"));
        strcat(body, line);
    }
    ShowPlayerDialog(playerid, DIALOG_AMMU, DIALOG_STYLE_TABLIST_HEADERS, "Ammu-Nation", body, "Buy", "Close");
    return 1;
}

stock Gun_OnDialog(playerid, dialogid, response, listitem)
{
    if (dialogid != DIALOG_AMMU) return 0;
    if (!response || !Player_IsPlaying(playerid) || !(0 <= listitem < sizeof (gGunStock))) return 1;
    if (!Place_Near(playerid, gAmmunation, 6.0)) return 1;
    if (gGunStock[listitem][gnLicence] && !gGunLicence[playerid])
        return SendError(playerid, "You need a gun licence for that (/buylicence gun at the licensing office)."), 1;
    new price = gGunStock[listitem][gnPrice];
    if (!Money_Charge(playerid, price, "ammunation", gGunStock[listitem][gnName]))
        return SendClientMessage(playerid, COLOR_ERROR, "[!] That costs $%d.", price), 1;
    if (gGunStock[listitem][gnWeapon] == -1) SetPlayerArmour(playerid, 100.0);
    else GivePlayerWeapon(playerid, WEAPON:gGunStock[listitem][gnWeapon], gGunStock[listitem][gnAmmo]);
    SendClientMessage(playerid, COLOR_SUCCESS, "You bought: %s ($%d). Weapons are lost if you die or log out.", gGunStock[listitem][gnName], price);
    return 1;
}
