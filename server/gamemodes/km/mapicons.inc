// Map icons for the fixed places in places.inc, on the radar and the
// pause-menu map. Player map icon ids 0-89 are ours (/testblips uses 90-95).

#define MAPICON_COLOUR_GAS      (0xFF8C00FF)    // orange square
#define MAPICON_COLOUR_SHOP     (0x2ECC71FF)    // green square: 24/7 shops (business properties)
#define MAPICON_SHOP_FIRST      (20)
#define MAPICON_SHOP_LAST       (89)

#define MAPICON_COLOUR_TAXI     (0xFFD700FF)    // yellow square

static gShopIconCount[MAX_PLAYERS];

static MapIcon_Set(playerid, &id, const point[E_POINT], type, colour = 0)
{
    SetPlayerMapIcon(playerid, id, point[ptX], point[ptY], point[ptZ], type, colour, MAPICON_GLOBAL);
    id++;
}

// Shows every place icon; returns how many were set.
stock MapIcons_Show(playerid)
{
    new id = 0;
    for (new i = 0; i < sizeof (gAtms); i++) MapIcon_Set(playerid, id, gAtms[i], 52);                          // $
    for (new i = 0; i < sizeof (gGasStations); i++) MapIcon_Set(playerid, id, gGasStations[i], 0, MAPICON_COLOUR_GAS);
    MapIcon_Set(playerid, id, gDealer, 55);             // car: dealership
    MapIcon_Set(playerid, id, gHospital, 22);           // hospital
    MapIcon_Set(playerid, id, gJailRelease, 30);        // LSPD front door
    MapIcon_Set(playerid, id, gLicenceOffice, 36);      // school: licensing office
    MapIcon_Set(playerid, id, gTruckDepot, 51);         // truck: trucker job
    MapIcon_Set(playerid, id, gPizzaShop, 29);          // pizza: pizza job
    MapIcon_Set(playerid, id, gTaxiDepot, 0, MAPICON_COLOUR_TAXI);
    MapIcon_Set(playerid, id, gQuarry, 11);             // bulldozer: miner job
    MapIcon_Set(playerid, id, gGarage, 27);             // wrench: mechanic job
    MapIcons_ShowShops(playerid);
    return id;
}

// Business properties (24/7 shops), which admins create in game.
stock MapIcons_ShowShops(playerid)
{
    new id = MAPICON_SHOP_FIRST;
    for (new i = 0; i < KM_MAX_PROPERTIES && id <= MAPICON_SHOP_LAST; i++)
    {
        if (!gProp[i][prUsed] || gProp[i][prId] <= 0 || gProp[i][prType] != PROP_BUSINESS) continue;
        SetPlayerMapIcon(playerid, id, gProp[i][prX], gProp[i][prY], gProp[i][prZ], 0, MAPICON_COLOUR_SHOP, MAPICON_GLOBAL);
        id++;
    }
    new shown = id - MAPICON_SHOP_FIRST;
    for (new k = shown; k < gShopIconCount[playerid]; k++) RemovePlayerMapIcon(playerid, MAPICON_SHOP_FIRST + k);
    gShopIconCount[playerid] = shown;
}

// After a shop is created or deleted.
stock MapIcons_RefreshShops()
{
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (Player_IsPlaying(i)) MapIcons_ShowShops(i);
    }
}

stock MapIcons_Reset(playerid)
{
    gShopIconCount[playerid] = 0;
}
