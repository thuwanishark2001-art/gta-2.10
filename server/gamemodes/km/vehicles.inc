// Vehicles: every vehicle the gamemode creates is registered here with a
// type (owned, job, faction, admin), so access checks know who may drive it.
// Owned vehicles are bought at the dealership, spawn while their owner plays,
// use fuel and are locked with /lock. Engines are manual (/engine).

enum E_VEH_TYPE
{
    VEH_NONE,       // not created by the gamemode
    VEH_OWNED,
    VEH_JOB,
    VEH_FACTION,
    VEH_ADMIN
}

enum E_VEH
{
    E_VEH_TYPE:vType,
    vDbId,          // vehicles.id for owned vehicles
    vOwner,         // player id of the owner (owned) or the worker (job)
    vFaction,
    Float:vFuel,
    bool:vLocked,
    vPrice
}

new gVeh[MAX_VEHICLES][E_VEH];
new bool:gVehBuying[MAX_PLAYERS];   // a purchase is waiting for the database

enum E_DEALER_ITEM { diModel, diName[16], diPrice }

stock const gDealerStock[][E_DEALER_ITEM] = {
    { 462, "Faggio", 3500 },
    { 404, "Perennial", 9000 },
    { 410, "Manana", 10000 },
    { 436, "Previon", 12000 },
    { 401, "Bravura", 14000 },
    { 426, "Premier", 22000 },
    { 445, "Admiral", 26000 },
    { 405, "Sentinel", 30000 },
    { 400, "Landstalker", 32000 },
    { 579, "Huntley", 45000 },
    { 560, "Sultan", 65000 },
    { 522, "NRG-500", 80000 },
    { 541, "Bullet", 120000 },
    { 411, "Infernus", 150000 }
};

#define VEH_SELECT_SQL \
    "SELECT id, model, color1, color2, pos_x, pos_y, pos_z, pos_a, fuel, health, locked, price FROM vehicles WHERE owner_character_id = %d"

forward Veh_OnOwnedLoaded(playerid, race);
forward Veh_OnBought(playerid, race, stockIndex, colour1, colour2);
forward Veh_FuelTick();

stock Veh_Create(E_VEH_TYPE:type, model, Float:x, Float:y, Float:z, Float:a, colour1, colour2, respawnDelay, bool:siren = false)
{
    new vehicleid = CreateVehicle(model, x, y, z, a, colour1, colour2, respawnDelay, siren);
    if (vehicleid == INVALID_VEHICLE_ID || vehicleid <= 0 || vehicleid >= MAX_VEHICLES) return INVALID_VEHICLE_ID;
    static const empty[E_VEH];
    gVeh[vehicleid] = empty;
    gVeh[vehicleid][vType] = type;
    gVeh[vehicleid][vOwner] = INVALID_PLAYER_ID;
    gVeh[vehicleid][vFuel] = 100.0;
    Radio_OnVehicleSpawn(vehicleid); // a reused id starts with the radio off
    SetVehicleParamsEx(vehicleid, 0, 0, 0, 0, 0, 0, 0);
    return vehicleid;
}

stock Veh_Destroy(vehicleid)
{
    if (!(0 < vehicleid < MAX_VEHICLES)) return;
    gVeh[vehicleid][vType] = VEH_NONE;
    Radio_OnVehicleSpawn(vehicleid);
    DestroyVehicle(vehicleid);
}

stock Veh_Init()
{
    for (new i = 0; i < sizeof (gFactionCars); i++)
    {
        new colour = gFactionCars[i][fcFaction] == 1 ? 0 : 1;
        new vehicleid = Veh_Create(VEH_FACTION, gFactionCars[i][fcModel], gFactionCars[i][fcX], gFactionCars[i][fcY], gFactionCars[i][fcZ], gFactionCars[i][fcA], colour, 1, 300, true);
        if (vehicleid != INVALID_VEHICLE_ID) gVeh[vehicleid][vFaction] = gFactionCars[i][fcFaction];
    }
    Create3DTextLabel("Coutt and Schutz\n" EMBED_WHITE "/buyvehicle", COLOR_BRAND, gDealer[ptX], gDealer[ptY], gDealer[ptZ] + 0.6, 15.0, 0, true);
    CreatePickup(1239, 1, gDealer[ptX], gDealer[ptY], gDealer[ptZ], 0);
    for (new i = 0; i < sizeof (gGasStations); i++)
    {
        Create3DTextLabel("Gas station\n" EMBED_WHITE "/refuel", COLOR_BRAND, gGasStations[i][ptX], gGasStations[i][ptY], gGasStations[i][ptZ] + 0.6, 20.0, 0, true);
    }
    SetTimer("Veh_FuelTick", KM_FUEL_TICK_MS, true);
}

stock bool:Veh_UsesFuel(vehicleid)
{
    return gVeh[vehicleid][vType] == VEH_OWNED;
}

stock Veh_SetFuel(vehicleid, Float:fuel)
{
    gVeh[vehicleid][vFuel] = fuel;
}

stock bool:Veh_EngineOn(vehicleid)
{
    new engine, lights, alarm, doors, bonnet, boot, objective;
    GetVehicleParamsEx(vehicleid, engine, lights, alarm, doors, bonnet, boot, objective);
    return engine == 1;
}

stock Veh_SetEngine(vehicleid, bool:on)
{
    new engine, lights, alarm, doors, bonnet, boot, objective;
    GetVehicleParamsEx(vehicleid, engine, lights, alarm, doors, bonnet, boot, objective);
    SetVehicleParamsEx(vehicleid, on ? 1 : 0, lights, alarm, doors, bonnet, boot, objective);
}

stock Veh_SetLocked(vehicleid, bool:locked)
{
    gVeh[vehicleid][vLocked] = locked;
    new engine, lights, alarm, doors, bonnet, boot, objective;
    GetVehicleParamsEx(vehicleid, engine, lights, alarm, doors, bonnet, boot, objective);
    SetVehicleParamsEx(vehicleid, engine, lights, alarm, locked ? 1 : 0, bonnet, boot, objective);
}

// Who may start the engine.
stock bool:Veh_CanDrive(playerid, vehicleid)
{
    switch (gVeh[vehicleid][vType])
    {
        case VEH_OWNED: return gVeh[vehicleid][vOwner] == playerid;
        case VEH_JOB: return gVeh[vehicleid][vOwner] == playerid;
        case VEH_FACTION: return gChar[playerid][cFaction] == gVeh[vehicleid][vFaction];
        default: return true;
    }
    return true;
}

stock Veh_CountOwned(playerid)
{
    new count = 0;
    for (new v = 1; v < MAX_VEHICLES; v++)
    {
        if (gVeh[v][vType] == VEH_OWNED && gVeh[v][vOwner] == playerid) count++;
    }
    return count;
}

// Nearest vehicle owned by the player within range, or INVALID_VEHICLE_ID.
stock Veh_NearestOwned(playerid, Float:range)
{
    new best = INVALID_VEHICLE_ID, Float:bestDist = range, Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    for (new v = 1; v < MAX_VEHICLES; v++)
    {
        if (gVeh[v][vType] != VEH_OWNED || gVeh[v][vOwner] != playerid) continue;
        new Float:d = GetVehicleDistanceFromPoint(v, x, y, z);
        if (d <= bestDist)
        {
            bestDist = d;
            best = v;
        }
    }
    return best;
}

// ---- owned vehicles: load, save, unload ----------------------------------

stock Veh_LoadOwned(playerid)
{
    new query[256];
    mysql_format(gDB, query, sizeof (query), VEH_SELECT_SQL, gChar[playerid][cId]);
    mysql_tquery(gDB, query, "Veh_OnOwnedLoaded", "dd", playerid, gPlayerRace[playerid]);
}

public Veh_OnOwnedLoaded(playerid, race)
{
    if (gPlayerRace[playerid] != race) return 1;
    for (new row = 0, rows = cache_num_rows(); row < rows; row++)
    {
        new id, model, c1, c2, locked, price, Float:x, Float:y, Float:z, Float:a, Float:fuel, Float:health;
        cache_get_value_name_int(row, "id", id);
        cache_get_value_name_int(row, "model", model);
        cache_get_value_name_int(row, "color1", c1);
        cache_get_value_name_int(row, "color2", c2);
        cache_get_value_name_float(row, "pos_x", x);
        cache_get_value_name_float(row, "pos_y", y);
        cache_get_value_name_float(row, "pos_z", z);
        cache_get_value_name_float(row, "pos_a", a);
        cache_get_value_name_float(row, "fuel", fuel);
        cache_get_value_name_float(row, "health", health);
        cache_get_value_name_int(row, "locked", locked);
        cache_get_value_name_int(row, "price", price);
        Veh_SpawnOwned(playerid, id, model, x, y, z, a, c1, c2, fuel, health, locked != 0, price);
    }
    return 1;
}

stock Veh_SpawnOwned(playerid, dbId, model, Float:x, Float:y, Float:z, Float:a, c1, c2, Float:fuel, Float:health, bool:locked, price)
{
    // Unoccupied owned vehicles return to their parking spot after 30 minutes.
    new vehicleid = Veh_Create(VEH_OWNED, model, x, y, z, a, c1, c2, 1800);
    if (vehicleid == INVALID_VEHICLE_ID) return INVALID_VEHICLE_ID;
    gVeh[vehicleid][vDbId] = dbId;
    gVeh[vehicleid][vOwner] = playerid;
    gVeh[vehicleid][vFuel] = fuel;
    gVeh[vehicleid][vPrice] = price;
    new plate[12];
    Plate_Format(plate, dbId);
    SetVehicleNumberPlate(vehicleid, plate);
    // The plate only shows after a respawn.
    SetVehicleToRespawn(vehicleid);
    SetVehicleHealth(vehicleid, health < 300.0 ? 300.0 : health);
    Veh_SetLocked(vehicleid, locked);
    return vehicleid;
}

stock Veh_SaveOwned(vehicleid)
{
    new Float:health;
    GetVehicleHealth(vehicleid, health);
    new query[200];
    mysql_format(gDB, query, sizeof (query), "UPDATE vehicles SET fuel = %f, health = %f, locked = %d WHERE id = %d",
        gVeh[vehicleid][vFuel], health, gVeh[vehicleid][vLocked] ? 1 : 0, gVeh[vehicleid][vDbId]);
    mysql_tquery(gDB, query);
}

stock Veh_OnOwnerLeave(playerid)
{
    for (new v = 1; v < MAX_VEHICLES; v++)
    {
        if (gVeh[v][vType] == VEH_OWNED && gVeh[v][vOwner] == playerid)
        {
            Veh_SaveOwned(v);
            Veh_Destroy(v);
        }
    }
}

stock Veh_SaveAll()
{
    for (new v = 1; v < MAX_VEHICLES; v++)
    {
        if (gVeh[v][vType] == VEH_OWNED) Veh_SaveOwned(v);
    }
}

// ---- callbacks -----------------------------------------------------------

stock Veh_OnStateChange(playerid, PLAYER_STATE:newstate)
{
    if (newstate != PLAYER_STATE_DRIVER) return;
    new vehicleid = GetPlayerVehicleID(playerid);
    if (!(0 < vehicleid < MAX_VEHICLES)) return;

    if (gChar[playerid][cCuffed])
    {
        RemovePlayerFromVehicle(playerid);
        return;
    }
    switch (gVeh[vehicleid][vType])
    {
        case VEH_JOB:
        {
            if (gVeh[vehicleid][vOwner] != playerid)
            {
                RemovePlayerFromVehicle(playerid);
                SendError(playerid, "That's someone else's work vehicle.");
                return;
            }
        }
        case VEH_FACTION:
        {
            if (gChar[playerid][cFaction] != gVeh[vehicleid][vFaction])
            {
                RemovePlayerFromVehicle(playerid);
                SendError(playerid, "Only members of that faction can drive this vehicle.");
                return;
            }
        }
    }
    if (!Veh_EngineOn(vehicleid))
    {
        if (Veh_UsesFuel(vehicleid)) SendClientMessage(playerid, COLOR_INFO, "Type /engine to start the engine. Fuel: %d%%", floatround(gVeh[vehicleid][vFuel]));
        else SendInfo(playerid, "Type /engine to start the engine.");
    }
}

stock Veh_OnSpawn(vehicleid)
{
    if (!(0 < vehicleid < MAX_VEHICLES) || gVeh[vehicleid][vType] == VEH_NONE) return;
    // Respawning resets params: engine off, keep the lock.
    SetVehicleParamsEx(vehicleid, 0, 0, 0, gVeh[vehicleid][vLocked] ? 1 : 0, 0, 0, 0);
}

public Veh_FuelTick()
{
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
    {
        if (!Player_IsPlaying(playerid) || GetPlayerState(playerid) != PLAYER_STATE_DRIVER) continue;
        new vehicleid = GetPlayerVehicleID(playerid);
        if (!(0 < vehicleid < MAX_VEHICLES) || !Veh_UsesFuel(vehicleid) || !Veh_EngineOn(vehicleid)) continue;

        new Float:vx, Float:vy, Float:vz;
        GetVehicleVelocity(vehicleid, vx, vy, vz);
        gVeh[vehicleid][vFuel] -= Fuel_Use(Speed_Kmh(vx, vy, vz));
        if (gVeh[vehicleid][vFuel] <= 0.0)
        {
            gVeh[vehicleid][vFuel] = 0.0;
            Veh_SetEngine(vehicleid, false);
            SendError(playerid, "You ran out of fuel. Buy a fuel can or call a mechanic.");
        }
        else if (gVeh[vehicleid][vFuel] < 10.0)
        {
            GameTextForPlayer(playerid, "~r~Low fuel", 1500, 4);
        }
    }
    return 1;
}

// ---- commands ----------------------------------------------------------

CMD:engine(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    new vehicleid = GetPlayerVehicleID(playerid);
    if (vehicleid == 0 || GetPlayerState(playerid) != PLAYER_STATE_DRIVER) return SendError(playerid, "You need to be driving.");
    if (Veh_EngineOn(vehicleid))
    {
        Veh_SetEngine(vehicleid, false);
        Chat_Action(playerid, "turns the engine off.");
        return 1;
    }
    if (!Veh_CanDrive(playerid, vehicleid)) return SendError(playerid, "You don't have the keys to this vehicle.");
    if (Veh_UsesFuel(vehicleid) && gVeh[vehicleid][vFuel] <= 0.0) return SendError(playerid, "The tank is empty.");
    Veh_SetEngine(vehicleid, true);
    Chat_Action(playerid, "starts the engine.");
    if (gVeh[vehicleid][vType] == VEH_OWNED && !gChar[playerid][cDrivingLicence])
    {
        SendInfo(playerid, "You don't have a driving licence. Police can fine you; take the test with /drivingtest.");
    }
    return 1;
}

CMD:lights(playerid, params[])
{
    new vehicleid = GetPlayerVehicleID(playerid);
    if (vehicleid == 0 || GetPlayerState(playerid) != PLAYER_STATE_DRIVER) return SendError(playerid, "You need to be driving.");
    new engine, lights, alarm, doors, bonnet, boot, objective;
    GetVehicleParamsEx(vehicleid, engine, lights, alarm, doors, bonnet, boot, objective);
    SetVehicleParamsEx(vehicleid, engine, lights == 1 ? 0 : 1, alarm, doors, bonnet, boot, objective);
    return 1;
}

// Locks your property if you're at its door, otherwise your nearest vehicle.
CMD:lock(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    if (Prop_TryLock(playerid)) return 1;
    new vehicleid = IsPlayerInAnyVehicle(playerid) ? GetPlayerVehicleID(playerid) : Veh_NearestOwned(playerid, 6.0);
    if (vehicleid == INVALID_VEHICLE_ID || vehicleid == 0 || gVeh[vehicleid][vType] != VEH_OWNED || gVeh[vehicleid][vOwner] != playerid)
    {
        return SendError(playerid, "You're not near anything of yours to lock.");
    }
    new bool:locked = !gVeh[vehicleid][vLocked];
    Veh_SetLocked(vehicleid, locked);
    Veh_SaveOwned(vehicleid);
    GameTextForPlayer(playerid, locked ? ("~r~Locked") : ("~g~Unlocked"), 1500, 4);
    Chat_Action(playerid, locked ? ("locks their vehicle.") : ("unlocks their vehicle."));
    return 1;
}

CMD:park(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    new vehicleid = GetPlayerVehicleID(playerid);
    if (vehicleid == 0 || GetPlayerState(playerid) != PLAYER_STATE_DRIVER || gVeh[vehicleid][vType] != VEH_OWNED || gVeh[vehicleid][vOwner] != playerid)
    {
        return SendError(playerid, "You need to be driving your own vehicle.");
    }
    if (GetPlayerInterior(playerid) != 0 || GetPlayerVirtualWorld(playerid) != 0) return SendError(playerid, "You can't park here.");
    new Float:x, Float:y, Float:z, Float:a;
    GetVehiclePos(vehicleid, x, y, z);
    GetVehicleZAngle(vehicleid, a);
    new query[200];
    mysql_format(gDB, query, sizeof (query), "UPDATE vehicles SET pos_x = %f, pos_y = %f, pos_z = %f, pos_a = %f WHERE id = %d",
        x, y, z, a, gVeh[vehicleid][vDbId]);
    mysql_tquery(gDB, query);
    Veh_SaveOwned(vehicleid);
    SendClientMessage(playerid, COLOR_SUCCESS, "Parked. Your vehicle will be here the next time you play.");
    return 1;
}

CMD:myvehicles(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    new body[512], line[96], plate[12], count = 0;
    body = "Vehicle\tPlate\tFuel\tStatus\n";
    for (new v = 1; v < MAX_VEHICLES; v++)
    {
        if (gVeh[v][vType] != VEH_OWNED || gVeh[v][vOwner] != playerid) continue;
        Plate_Format(plate, gVeh[v][vDbId]);
        format(line, sizeof (line), "%s\t%s\t%d%%\t%s\n", Veh_ModelName(GetVehicleModel(v)), plate, floatround(gVeh[v][vFuel]), gVeh[v][vLocked] ? ("Locked") : ("Unlocked"));
        strcat(body, line);
        count++;
    }
    if (count == 0) return SendInfo(playerid, "You don't own any vehicles. Buy one at Coutt and Schutz with /buyvehicle.");
    ShowPlayerDialog(playerid, DIALOG_INFO, DIALOG_STYLE_TABLIST_HEADERS, "Your vehicles", body, "Close", "");
    return 1;
}

stock Veh_ModelName(model)
{
    new name[16] = "Vehicle";
    for (new i = 0; i < sizeof (gDealerStock); i++)
    {
        if (gDealerStock[i][diModel] == model)
        {
            format(name, sizeof (name), "%s", gDealerStock[i][diName]);
            break;
        }
    }
    return name;
}

CMD:buyvehicle(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    if (!Place_Near(playerid, gDealer, 5.0)) return SendError(playerid, "Buy vehicles at Coutt and Schutz in Jefferson.");
    if (Veh_CountOwned(playerid) >= KM_MAX_OWNED_VEHICLES) return SendClientMessage(playerid, COLOR_ERROR, "[!] You can own at most %d vehicles.", KM_MAX_OWNED_VEHICLES);
    new body[640], line[48];
    body = "Model\tPrice\n";
    for (new i = 0; i < sizeof (gDealerStock); i++)
    {
        format(line, sizeof (line), "%s\t$%d\n", gDealerStock[i][diName], gDealerStock[i][diPrice]);
        strcat(body, line);
    }
    ShowPlayerDialog(playerid, DIALOG_DEALER, DIALOG_STYLE_TABLIST_HEADERS, "Coutt and Schutz", body, "Buy", "Cancel");
    return 1;
}

stock Veh_OnDialog(playerid, dialogid, response, listitem)
{
    if (dialogid != DIALOG_DEALER) return 0;
    if (!response || !Player_IsPlaying(playerid)) return 1;
    if (!(0 <= listitem < sizeof (gDealerStock))) return 1;
    if (!Place_Near(playerid, gDealer, 5.0)) return SendError(playerid, "You walked away from the dealership."), 1;
    if (Veh_CountOwned(playerid) >= KM_MAX_OWNED_VEHICLES) return SendError(playerid, "You already own the maximum number of vehicles."), 1;

    if (gVehBuying[playerid]) return SendError(playerid, "Your last purchase is still going through."), 1;

    // The row is created first and paid for in Veh_OnBought, so a failed
    // insert never costs money; a failed payment deletes the row again.
    new price = gDealerStock[listitem][diPrice];
    if (gChar[playerid][cBank] + gChar[playerid][cCash] < price)
    {
        SendClientMessage(playerid, COLOR_ERROR, "[!] You need $%d (bank and cash) for a %s.", price, gDealerStock[listitem][diName]);
        return 1;
    }
    gVehBuying[playerid] = true;
    new query[400], c1 = random(127), c2 = random(127);
    mysql_format(gDB, query, sizeof (query),
        "INSERT INTO vehicles (owner_character_id, model, color1, color2, pos_x, pos_y, pos_z, pos_a, price) VALUES (%d, %d, %d, %d, %f, %f, %f, %f, %d)",
        gChar[playerid][cId], gDealerStock[listitem][diModel], c1, c2,
        gDealerSpawn[ptX], gDealerSpawn[ptY], gDealerSpawn[ptZ], gDealerSpawn[ptA], price);
    mysql_tquery(gDB, query, "Veh_OnBought", "ddddd", playerid, gPlayerRace[playerid], listitem, c1, c2);
    return 1;
}

public Veh_OnBought(playerid, race, stockIndex, colour1, colour2)
{
    new dbId = cache_insert_id();
    new plate[12], query[96];
    Plate_Format(plate, dbId);
    mysql_format(gDB, query, sizeof (query), "DELETE FROM vehicles WHERE id = %d", dbId);
    if (gPlayerRace[playerid] != race)
    {
        // Left before paying.
        if (dbId > 0) mysql_tquery(gDB, query);
        return 1;
    }
    gVehBuying[playerid] = false;
    if (dbId <= 0) return SendError(playerid, "The purchase failed. You weren't charged."), 1;
    if (!Money_Charge(playerid, gDealerStock[stockIndex][diPrice], "vehicle_buy", plate))
    {
        mysql_tquery(gDB, query);
        return SendError(playerid, "You no longer have enough money. You weren't charged."), 1;
    }
    new vehicleid = Veh_SpawnOwned(playerid, dbId, gDealerStock[stockIndex][diModel], gDealerSpawn[ptX], gDealerSpawn[ptY], gDealerSpawn[ptZ], gDealerSpawn[ptA],
        colour1, colour2, 100.0, 1000.0, false, gDealerStock[stockIndex][diPrice]);
    SendClientMessage(playerid, COLOR_SUCCESS, "You bought a %s (plate %s). It's parked outside. /park sets where it waits for you.", gDealerStock[stockIndex][diName], plate);
    if (vehicleid != INVALID_VEHICLE_ID) PutPlayerInVehicle(playerid, vehicleid, 0);
    return 1;
}

CMD:sellvehicle(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    new vehicleid = GetPlayerVehicleID(playerid);
    if (vehicleid == 0 || gVeh[vehicleid][vType] != VEH_OWNED || gVeh[vehicleid][vOwner] != playerid) return SendError(playerid, "You need to be in your own vehicle.");
    if (!Place_Near(playerid, gDealer, 25.0)) return SendError(playerid, "Bring it to Coutt and Schutz to sell it.");
    new refund = gVeh[vehicleid][vPrice] * KM_VEHICLE_SELL_PERCENT / 100;
    if (strcmp(params, "confirm", true) != 0)
    {
        SendClientMessage(playerid, COLOR_INFO, "The dealer will pay $%d for it. Type /sellvehicle confirm to sell.", refund);
        return 1;
    }
    new query[96], plate[12];
    Plate_Format(plate, gVeh[vehicleid][vDbId]);
    mysql_format(gDB, query, sizeof (query), "DELETE FROM vehicles WHERE id = %d", gVeh[vehicleid][vDbId]);
    mysql_tquery(gDB, query);
    Veh_Destroy(vehicleid);
    Money_Give(playerid, 0, refund, "vehicle_sell", INVALID_PLAYER_ID, plate);
    SendClientMessage(playerid, COLOR_SUCCESS, "Sold. $%d was paid into your bank account.", refund);
    return 1;
}

CMD:refuel(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    new vehicleid = GetPlayerVehicleID(playerid);
    if (vehicleid == 0 || GetPlayerState(playerid) != PLAYER_STATE_DRIVER) return SendError(playerid, "You need to be driving.");
    if (!Veh_UsesFuel(vehicleid)) return SendInfo(playerid, "This vehicle is fuelled by its owner's company.");
    new bool:atStation = false;
    for (new i = 0; i < sizeof (gGasStations); i++)
    {
        if (Place_Near(playerid, gGasStations[i], 12.0)) atStation = true;
    }
    if (!atStation) return SendError(playerid, "You need to be at a gas station.");
    if (Veh_EngineOn(vehicleid)) return SendError(playerid, "Turn the engine off first (/engine).");
    new litres, cost = Fuel_RefillCost(gVeh[vehicleid][vFuel], litres);
    if (litres <= 0) return SendInfo(playerid, "The tank is already full.");
    if (!Money_Give(playerid, -cost, 0, "fuel")) return SendClientMessage(playerid, COLOR_ERROR, "[!] Filling up costs $%d in cash.", cost);
    gVeh[vehicleid][vFuel] = 100.0;
    SendClientMessage(playerid, COLOR_SUCCESS, "Filled up %d litres for $%d.", litres, cost);
    return 1;
}

// ---- admin ---------------------------------------------------------------

CMD:veh(playerid, params[])
{
    new model;
    if (sscanf(params, "d", model)) return SendInfo(playerid, "Usage: /veh [model 400-611]");
    if (!Mod_CanAct(playerid, INVALID_PLAYER_ID, ADMIN_ADMIN)) return 1;
    if (!(400 <= model <= 611)) return SendError(playerid, "Model must be 400-611.");
    new Float:x, Float:y, Float:z, Float:a;
    GetPlayerPos(playerid, x, y, z);
    GetPlayerFacingAngle(playerid, a);
    new vehicleid = Veh_Create(VEH_ADMIN, model, x, y, z + 1.0, a, 1, 1, -1);
    if (vehicleid == INVALID_VEHICLE_ID) return SendError(playerid, "Couldn't create the vehicle.");
    LinkVehicleToInterior(vehicleid, GetPlayerInterior(playerid));
    SetVehicleVirtualWorld(vehicleid, GetPlayerVirtualWorld(playerid));
    PutPlayerInVehicle(playerid, vehicleid, 0);
    new details[24];
    format(details, sizeof (details), "model %d", model);
    Log_Admin(Mod_ActorId(playerid), "spawn_vehicle", 0, details);
    return 1;
}

CMD:dveh(playerid, params[])
{
    if (!Mod_CanAct(playerid, INVALID_PLAYER_ID, ADMIN_ADMIN)) return 1;
    new vehicleid = GetPlayerVehicleID(playerid);
    if (vehicleid == 0 || gVeh[vehicleid][vType] != VEH_ADMIN) return SendError(playerid, "Get in an admin-spawned vehicle first.");
    Veh_Destroy(vehicleid);
    return 1;
}
