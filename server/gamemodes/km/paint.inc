// Paint shop: /paint <colour 1> <colour 2> in your own car at the
// Pay'n'Spray (gGarage) for KM_PAINT_PRICE. The colours are saved.

CMD:paint(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    new c1, c2;
    if (sscanf(params, "dd", c1, c2) || !(0 <= c1 <= 255) || !(0 <= c2 <= 255))
        return SendInfo(playerid, "Usage: /paint [colour 1] [colour 2], 0-255 (0 black, 1 white, 3 red, 79 blue, 86 green, 6 yellow).");
    new vehicleid = GetPlayerVehicleID(playerid);
    if (!vehicleid || GetPlayerState(playerid) != PLAYER_STATE_DRIVER) return SendError(playerid, "Drive your car into the Pay'n'Spray first.");
    if (gVeh[vehicleid][vType] != VEH_OWNED || gVeh[vehicleid][vOwner] != playerid) return SendError(playerid, "You can only paint your own car.");
    if (!IsPlayerInRangeOfPoint(playerid, 12.0, gGarage[ptX], gGarage[ptY], gGarage[ptZ]))
        return SendError(playerid, "You need to be at the Pay'n'Spray in Idlewood (/gps).");
    if (!Money_Charge(playerid, KM_PAINT_PRICE, "paint")) return SendClientMessage(playerid, COLOR_ERROR, "[!] A respray costs $%d.", KM_PAINT_PRICE), 1;
    ChangeVehicleColours(vehicleid, c1, c2);
    new query[128];
    mysql_format(gDB, query, sizeof (query), "UPDATE vehicles SET color1 = %d, color2 = %d WHERE id = %d", c1, c2, gVeh[vehicleid][vDbId]);
    mysql_tquery(gDB, query);
    SendClientMessage(playerid, COLOR_SUCCESS, "Your car has a new paint job ($%d).", KM_PAINT_PRICE);
    return 1;
}
