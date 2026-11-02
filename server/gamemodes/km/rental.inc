// Bike rental at gRental (by the spawn): /rent gives a Faggio for
// KM_RENT_PRICE until /unrent or logging out. Only the renter can ride it.

static gRented[MAX_PLAYERS] = { INVALID_VEHICLE_ID, ... };

stock Rent_Reset(playerid)
{
    if (gRented[playerid] != INVALID_VEHICLE_ID && gVeh[gRented[playerid]][vType] == VEH_RENTAL && gVeh[gRented[playerid]][vOwner] == playerid)
        Veh_Destroy(gRented[playerid]);
    gRented[playerid] = INVALID_VEHICLE_ID;
}

CMD:rent(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    if (!Place_Near(playerid, gRental, 6.0)) return SendError(playerid, "Bikes are rented by Unity Station, where you first arrived (/gps).");
    if (gRented[playerid] != INVALID_VEHICLE_ID) return SendError(playerid, "You already have a rented bike. /unrent returns it.");
    if (IsPlayerInAnyVehicle(playerid)) return SendError(playerid, "Get out of your vehicle first.");
    if (!Money_Charge(playerid, KM_RENT_PRICE, "rental")) return SendClientMessage(playerid, COLOR_ERROR, "[!] A rental costs $%d.", KM_RENT_PRICE), 1;
    new Float:x, Float:y, Float:z, Float:a;
    GetPlayerPos(playerid, x, y, z);
    GetPlayerFacingAngle(playerid, a);
    new vehicleid = Veh_Create(VEH_RENTAL, 462, x + 2.0, y, z, a, 3, 1, -1);
    if (vehicleid == INVALID_VEHICLE_ID)
    {
        Money_Give(playerid, 0, KM_RENT_PRICE, "rental_refund");
        return SendError(playerid, "No bike is available. You were refunded.");
    }
    gVeh[vehicleid][vOwner] = playerid;
    gRented[playerid] = vehicleid;
    PutPlayerInVehicle(playerid, vehicleid, 0);
    Veh_SetEngine(vehicleid, true);
    SendClientMessage(playerid, COLOR_SUCCESS, "You rented a Faggio ($%d). It's yours until /unrent or you log out.", KM_RENT_PRICE);
    return 1;
}

CMD:unrent(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    if (gRented[playerid] == INVALID_VEHICLE_ID) return SendError(playerid, "You haven't rented anything.");
    Rent_Reset(playerid);
    SendInfo(playerid, "You returned your rented bike.");
    return 1;
}

// A wrecked rental is gone; the renter can /rent again.
stock Rent_OnVehicleDeath(vehicleid)
{
    if (gVeh[vehicleid][vType] != VEH_RENTAL) return;
    new owner = gVeh[vehicleid][vOwner];
    if (0 <= owner < MAX_PLAYERS && gRented[owner] == vehicleid) gRented[owner] = INVALID_VEHICLE_ID;
    Veh_Destroy(vehicleid);
}

// Labels at the places from this batch and the last (casino, pier, Binco,
// Ammu-Nation, rental). From OnGameModeInit.
stock Places_InitLabels()
{
    Create3DTextLabel("Casino\n" EMBED_WHITE "/roulette", COLOR_BRAND, gCasino[ptX], gCasino[ptY], gCasino[ptZ] + 0.6, 15.0, 0, true);
    Create3DTextLabel("Fishing pier\n" EMBED_WHITE "/fish /sellfish", COLOR_BRAND, gFishPier[ptX], gFishPier[ptY], gFishPier[ptZ] + 0.6, 20.0, 0, true);
    Create3DTextLabel("Binco\n" EMBED_WHITE "/clothes [skin id]", COLOR_BRAND, gClothesShop[ptX], gClothesShop[ptY], gClothesShop[ptZ] + 0.6, 15.0, 0, true);
    Create3DTextLabel("Ammu-Nation\n" EMBED_WHITE "/buygun", COLOR_BRAND, gAmmunation[ptX], gAmmunation[ptY], gAmmunation[ptZ] + 0.6, 15.0, 0, true);
    Create3DTextLabel("Bike rental\n" EMBED_WHITE "/rent ($50)", COLOR_BRAND, gRental[ptX], gRental[ptY], gRental[ptZ] + 0.6, 15.0, 0, true);
    CreatePickup(1275, 1, gClothesShop[ptX], gClothesShop[ptY], gClothesShop[ptZ], 0);
    CreatePickup(1239, 1, gRental[ptX], gRental[ptY], gRental[ptZ], 0);
}
