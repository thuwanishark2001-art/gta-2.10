// Jobs: trucker, pizza delivery, taxi, miner, mechanic.
// /jobs shows where they are, /takejob at the site signs you up, /work starts
// a shift (with a work vehicle where needed), /stopwork ends it. Pay goes
// straight to cash and is logged in money_log with reason "job".

enum
{
    JS_IDLE,
    JS_DELIVER,     // trucker, pizza: driving to a drop-off
    JS_RETURN,      // pizza: back to the shop for more pizzas
    JS_PICKUP,      // taxi: driving to a fare
    JS_DROPOFF,     // taxi: fare on board
    JS_CALL,        // taxi: driving to a player who called
    JS_ROCK,        // miner: walking to a rock face
    JS_MINING,
    JS_CARRY        // miner: carrying ore to the drop-off
}

enum E_JOBSTATE
{
    bool:jsWorking,
    jsVehicle,
    jsStage,
    jsTarget,
    Float:jsFromX,
    Float:jsFromY,
    Float:jsFromZ,
    jsStartTime,
    jsLoad,
    jsCaller,
    bool:jsGps      // the checkpoint is only a /jobs waypoint
}

new gJob[MAX_PLAYERS][E_JOBSTATE];
new gTaxiCallTime[MAX_PLAYERS];

stock const gJobNames[JOB_COUNT][16] = { "Unemployed", "Trucker", "Pizza delivery", "Taxi driver", "Miner", "Mechanic" };

forward Job_MineDone(playerid, race);

stock Job_Site(job, point[E_POINT])
{
    switch (job)
    {
        case JOB_TRUCKER: point = gTruckDepot;
        case JOB_PIZZA: point = gPizzaShop;
        case JOB_TAXI: point = gTaxiDepot;
        case JOB_MINER: point = gQuarry;
        case JOB_MECHANIC: point = gGarage;
    }
}

stock Job_Init()
{
    new point[E_POINT], label[96];
    for (new job = 1; job < JOB_COUNT; job++)
    {
        Job_Site(job, point);
        format(label, sizeof (label), "%s job\n" EMBED_WHITE "/takejob  /work  /stopwork", gJobNames[job]);
        Create3DTextLabel(label, COLOR_BRAND, point[ptX], point[ptY], point[ptZ] + 0.6, 20.0, 0, true);
        CreatePickup(1210, 1, point[ptX], point[ptY], point[ptZ], 0);
    }
}

stock Job_Reset(playerid)
{
    static const empty[E_JOBSTATE];
    gJob[playerid] = empty;
    gJob[playerid][jsVehicle] = INVALID_VEHICLE_ID;
    gJob[playerid][jsCaller] = INVALID_PLAYER_ID;
    gTaxiCallTime[playerid] = 0;
}

stock bool:Job_IsWorking(playerid, job)
{
    return gJob[playerid][jsWorking] && gChar[playerid][cJob] == job;
}

stock Job_SetCheckpoint(playerid, const point[E_POINT], Float:size = 4.0)
{
    gJob[playerid][jsGps] = false;
    DisablePlayerCheckpoint(playerid);
    SetPlayerCheckpoint(playerid, point[ptX], point[ptY], point[ptZ], size);
}

stock Job_Pay(playerid, amount, const what[])
{
    Money_Give(playerid, amount, 0, "job", INVALID_PLAYER_ID, what);
    GameTextForPlayer(playerid, "~g~+$%d", 2500, 1, amount);
    new toast[64];
    format(toast, sizeof (toast), "+$%d for %s", amount, what);
    KMUI_Toast(playerid, toast);
}

stock Job_SpawnVehicle(playerid, model, const point[E_POINT], colour1, colour2)
{
    new vehicleid = Veh_Create(VEH_JOB, model, point[ptX], point[ptY], point[ptZ], point[ptA], colour1, colour2, -1);
    if (vehicleid == INVALID_VEHICLE_ID) return INVALID_VEHICLE_ID;
    gVeh[vehicleid][vOwner] = playerid;
    gJob[playerid][jsVehicle] = vehicleid;
    PutPlayerInVehicle(playerid, vehicleid, 0);
    Veh_SetEngine(vehicleid, true);
    return vehicleid;
}

stock Job_Stop(playerid, const reason[] = "")
{
    if (!gJob[playerid][jsWorking]) return;
    if (gJob[playerid][jsVehicle] != INVALID_VEHICLE_ID) Veh_Destroy(gJob[playerid][jsVehicle]);
    if (gJob[playerid][jsStage] == JS_MINING || gJob[playerid][jsStage] == JS_CARRY)
    {
        TogglePlayerControllable(playerid, true);
        ClearAnimations(playerid);
        RemovePlayerAttachedObject(playerid, 9);
        SetPlayerSpecialAction(playerid, SPECIAL_ACTION_NONE);
    }
    DisablePlayerCheckpoint(playerid);
    gJob[playerid][jsWorking] = false;
    gJob[playerid][jsVehicle] = INVALID_VEHICLE_ID;
    gJob[playerid][jsStage] = JS_IDLE;
    gJob[playerid][jsCaller] = INVALID_PLAYER_ID;
    if (reason[0] != '\0' && IsPlayerConnected(playerid)) SendClientMessage(playerid, COLOR_INFO, "Shift ended: %s", reason);
}

stock Job_OnVehicleDeath(vehicleid)
{
    if (!(0 < vehicleid < MAX_VEHICLES) || gVeh[vehicleid][vType] != VEH_JOB) return;
    new playerid = gVeh[vehicleid][vOwner];
    if (IsPlayerConnected(playerid) && gJob[playerid][jsVehicle] == vehicleid)
    {
        gJob[playerid][jsVehicle] = INVALID_VEHICLE_ID;
        Job_Stop(playerid, "your work vehicle was destroyed.");
    }
    Veh_Destroy(vehicleid);
}

stock bool:Job_InOwnVehicle(playerid)
{
    return gJob[playerid][jsVehicle] != INVALID_VEHICLE_ID && GetPlayerVehicleID(playerid) == gJob[playerid][jsVehicle];
}

stock Job_RememberPos(playerid)
{
    GetPlayerPos(playerid, gJob[playerid][jsFromX], gJob[playerid][jsFromY], gJob[playerid][jsFromZ]);
    gJob[playerid][jsStartTime] = gettime();
}

stock Float:Job_DistanceFromStart(const point[E_POINT], playerid)
{
    return Dist3D(gJob[playerid][jsFromX], gJob[playerid][jsFromY], gJob[playerid][jsFromZ], point[ptX], point[ptY], point[ptZ]);
}

// ---- next task for each job ---------------------------------------------

stock Job_NextTruck(playerid)
{
    new target = RandomOther(sizeof (gTruckDrops), gJob[playerid][jsStage] == JS_DELIVER ? gJob[playerid][jsTarget] : -1);
    gJob[playerid][jsStage] = JS_DELIVER;
    gJob[playerid][jsTarget] = target;
    Job_RememberPos(playerid);
    Job_SetCheckpoint(playerid, gTruckDrops[target], 6.0);
    SendInfo(playerid, "Cargo loaded. Deliver it to the marker on your map.");
}

stock Job_NextPizza(playerid)
{
    if (gJob[playerid][jsLoad] <= 0)
    {
        gJob[playerid][jsStage] = JS_RETURN;
        Job_SetCheckpoint(playerid, gPizzaShop);
        SendInfo(playerid, "Out of pizzas. Head back to the Pizza Stack for more.");
        return;
    }
    new target = RandomOther(sizeof (gPizzaHouses), gJob[playerid][jsStage] == JS_DELIVER ? gJob[playerid][jsTarget] : -1);
    gJob[playerid][jsStage] = JS_DELIVER;
    gJob[playerid][jsTarget] = target;
    Job_RememberPos(playerid);
    Job_SetCheckpoint(playerid, gPizzaHouses[target], 3.0);
    SendClientMessage(playerid, COLOR_INFO, "Deliver a pizza to the marker (%d left). Be quick for a tip.", gJob[playerid][jsLoad]);
}

stock Job_NextFare(playerid)
{
    new target = random(sizeof (gTaxiStops));
    gJob[playerid][jsStage] = JS_PICKUP;
    gJob[playerid][jsTarget] = target;
    Job_SetCheckpoint(playerid, gTaxiStops[target]);
    SendInfo(playerid, "A fare is waiting at the marker.");
}

stock Job_NextRock(playerid)
{
    new target = RandomOther(sizeof (gQuarryRocks), gJob[playerid][jsTarget]);
    gJob[playerid][jsStage] = JS_ROCK;
    gJob[playerid][jsTarget] = target;
    Job_SetCheckpoint(playerid, gQuarryRocks[target], 2.0);
    SendInfo(playerid, "Walk to the marked rock face to mine it.");
}

// ---- checkpoint progress -----------------------------------------------

stock Job_OnCheckpoint(playerid)
{
    if (gJob[playerid][jsGps])
    {
        gJob[playerid][jsGps] = false;
        DisablePlayerCheckpoint(playerid);
        return 1;
    }
    if (!gJob[playerid][jsWorking]) return 0;

    switch (gJob[playerid][jsStage])
    {
        case JS_DELIVER:
        {
            if (!Job_InOwnVehicle(playerid)) return SendError(playerid, "You need your work vehicle."), 1;
            if (gChar[playerid][cJob] == JOB_TRUCKER)
            {
                new pay = Job_DistancePay(Job_DistanceFromStart(gTruckDrops[gJob[playerid][jsTarget]], playerid), 100, 250, 2000);
                Job_Pay(playerid, pay, "truck delivery");
                Job_NextTruck(playerid);
            }
            else
            {
                new Float:dist = Job_DistanceFromStart(gPizzaHouses[gJob[playerid][jsTarget]], playerid);
                new pay = Job_DistancePay(dist, 60, 300, 400);
                new limit = floatround(dist / 7.0) + 20;
                if (gettime() - gJob[playerid][jsStartTime] <= limit)
                {
                    pay += pay / 2;
                    SendInfo(playerid, "Fast delivery! The customer tipped you.");
                }
                gJob[playerid][jsLoad]--;
                Job_Pay(playerid, pay, "pizza delivery");
                Job_NextPizza(playerid);
            }
        }
        case JS_RETURN:
        {
            if (!Job_InOwnVehicle(playerid)) return SendError(playerid, "You need your work vehicle."), 1;
            gJob[playerid][jsLoad] = KM_PIZZAS_PER_LOAD;
            Job_NextPizza(playerid);
        }
        case JS_PICKUP:
        {
            if (!Job_InOwnVehicle(playerid)) return SendError(playerid, "You need your taxi."), 1;
            new from = gJob[playerid][jsTarget];
            new to = RandomOther(sizeof (gTaxiStops), from);
            gJob[playerid][jsStage] = JS_DROPOFF;
            gJob[playerid][jsTarget] = to;
            Job_RememberPos(playerid);
            Job_SetCheckpoint(playerid, gTaxiStops[to]);
            SendInfo(playerid, "The passenger got in. Drive them to the marker.");
        }
        case JS_DROPOFF:
        {
            if (!Job_InOwnVehicle(playerid)) return SendError(playerid, "You need your taxi."), 1;
            new pay = Job_DistancePay(Job_DistanceFromStart(gTaxiStops[gJob[playerid][jsTarget]], playerid), 50, 400, 900);
            Job_Pay(playerid, pay, "taxi fare");
            Job_NextFare(playerid);
        }
        case JS_CALL:
        {
            DisablePlayerCheckpoint(playerid);
            gJob[playerid][jsStage] = JS_IDLE;
            SendInfo(playerid, "You reached the caller. Agree a fare in character; they can /pay you. /work gets a new fare.");
        }
        case JS_ROCK:
        {
            if (IsPlayerInAnyVehicle(playerid)) return SendError(playerid, "Get out of the vehicle to mine."), 1;
            gJob[playerid][jsStage] = JS_MINING;
            DisablePlayerCheckpoint(playerid);
            TogglePlayerControllable(playerid, false);
            ApplyAnimation(playerid, "BASEBALL", "Bat_4", 4.1, true, false, false, false, 0, SYNC_ALL);
            SetTimerEx("Job_MineDone", KM_MINE_MS, false, "dd", playerid, gPlayerRace[playerid]);
            Chat_Action(playerid, "swings a pickaxe at the rock.");
        }
        case JS_CARRY:
        {
            RemovePlayerAttachedObject(playerid, 9);
            SetPlayerSpecialAction(playerid, SPECIAL_ACTION_NONE);
            Job_Pay(playerid, KM_ORE_PAY, "ore");
            Job_NextRock(playerid);
        }
    }
    return 1;
}

public Job_MineDone(playerid, race)
{
    if (gPlayerRace[playerid] != race || !gJob[playerid][jsWorking] || gJob[playerid][jsStage] != JS_MINING) return 1;
    ClearAnimations(playerid, SYNC_ALL);
    TogglePlayerControllable(playerid, true);
    SetPlayerSpecialAction(playerid, SPECIAL_ACTION_CARRY);
    SetPlayerAttachedObject(playerid, 9, 2936, 5, 0.1, 0.1, 0.2, 0.0, 0.0, 0.0, 0.3, 0.3, 0.3);
    gJob[playerid][jsStage] = JS_CARRY;
    Job_SetCheckpoint(playerid, gQuarryDrop, 2.5);
    SendInfo(playerid, "Carry the ore to the drop-off marker.");
    return 1;
}

// ---- commands ----------------------------------------------------------

CMD:jobs(playerid, params[])
{
    new body[400], line[64];
    body = "Job\tWhere\n";
    for (new job = 1; job < JOB_COUNT; job++)
    {
        format(line, sizeof (line), "%s\t%s\n", gJobNames[job],
            job == JOB_TRUCKER ? ("Ocean Docks") : job == JOB_PIZZA ? ("Idlewood Pizza Stack") : job == JOB_TAXI ? ("Unity Station") :
            job == JOB_MINER ? ("Hunter Quarry") : ("Idlewood garage"));
        strcat(body, line);
    }
    ShowPlayerDialog(playerid, DIALOG_JOBS, DIALOG_STYLE_TABLIST_HEADERS, "Jobs | pick one to mark it on your map", body, "Mark", "Close");
    return 1;
}

stock Job_OnDialog(playerid, dialogid, response, listitem)
{
    if (dialogid != DIALOG_JOBS) return 0;
    if (!response || !Player_IsPlaying(playerid)) return 1;
    if (gJob[playerid][jsWorking]) return SendError(playerid, "Finish your shift first (/stopwork)."), 1;
    new job = listitem + 1;
    if (!(1 <= job < JOB_COUNT)) return 1;
    new point[E_POINT];
    Job_Site(job, point);
    Job_SetCheckpoint(playerid, point);
    gJob[playerid][jsGps] = true;
    SendClientMessage(playerid, COLOR_INFO, "The %s job is marked on your map.", gJobNames[job]);
    return 1;
}

stock Job_AtSite(playerid)
{
    new point[E_POINT];
    for (new job = 1; job < JOB_COUNT; job++)
    {
        Job_Site(job, point);
        if (GetPlayerInterior(playerid) == 0 && Place_Near(playerid, point, 6.0)) return job;
    }
    return JOB_NONE;
}

CMD:takejob(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    new job = Job_AtSite(playerid);
    if (job == JOB_NONE) return SendError(playerid, "Go to a job site first. /jobs shows where they are.");
    if (gChar[playerid][cJob] == job) return SendError(playerid, "You already have this job.");
    if (gJob[playerid][jsWorking]) return SendError(playerid, "Finish your shift first (/stopwork).");
    gChar[playerid][cJob] = job;
    new query[96];
    mysql_format(gDB, query, sizeof (query), "UPDATE characters SET job = %d WHERE id = %d", job, gChar[playerid][cId]);
    mysql_tquery(gDB, query);
    SendClientMessage(playerid, COLOR_SUCCESS, "You're now working as: %s. Type /work here to start a shift.", gJobNames[job]);
    return 1;
}

CMD:quitjob(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    if (gChar[playerid][cJob] == JOB_NONE) return SendError(playerid, "You don't have a job.");
    Job_Stop(playerid);
    gChar[playerid][cJob] = JOB_NONE;
    new query[96];
    mysql_format(gDB, query, sizeof (query), "UPDATE characters SET job = 0 WHERE id = %d", gChar[playerid][cId]);
    mysql_tquery(gDB, query);
    SendInfo(playerid, "You quit your job.");
    return 1;
}

CMD:work(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    new job = gChar[playerid][cJob];
    if (job == JOB_NONE) return SendError(playerid, "You don't have a job. /jobs shows where to find one.");
    if (gChar[playerid][cJailSeconds] > 0 || gChar[playerid][cCuffed]) return SendError(playerid, "Not now.");
    if (gChar[playerid][cOnDuty]) return SendError(playerid, "Go off faction duty first (/duty).");

    // Taxi drivers can ask for a new fare during a shift.
    if (gJob[playerid][jsWorking])
    {
        if (job == JOB_TAXI && (gJob[playerid][jsStage] == JS_IDLE || gJob[playerid][jsStage] == JS_PICKUP)) {
            Job_NextFare(playerid);
            return 1;
        }
        return SendError(playerid, "You're already on shift. /stopwork ends it.");
    }
    new point[E_POINT];
    Job_Site(job, point);
    if (!Place_Near(playerid, point, 8.0)) return SendError(playerid, "Start your shift at the job site. /jobs marks it on your map.");
    if (IsPlayerInAnyVehicle(playerid) && job != JOB_MINER) return SendError(playerid, "Get out of your vehicle first.");

    gJob[playerid][jsWorking] = true;
    gJob[playerid][jsTarget] = -1;
    switch (job)
    {
        case JOB_TRUCKER:
        {
            if (Job_SpawnVehicle(playerid, 456, gTruckSpawn, 1, 1) == INVALID_VEHICLE_ID) {
                Job_Stop(playerid, "no work vehicle available.");
                return 1;
            }
            Job_NextTruck(playerid);
        }
        case JOB_PIZZA:
        {
            if (Job_SpawnVehicle(playerid, 448, gPizzaSpawn, 3, 1) == INVALID_VEHICLE_ID) {
                Job_Stop(playerid, "no work vehicle available.");
                return 1;
            }
            gJob[playerid][jsLoad] = KM_PIZZAS_PER_LOAD;
            Job_NextPizza(playerid);
        }
        case JOB_TAXI:
        {
            if (Job_SpawnVehicle(playerid, 420, gTaxiSpawn, 6, 1) == INVALID_VEHICLE_ID) {
                Job_Stop(playerid, "no work vehicle available.");
                return 1;
            }
            Job_NextFare(playerid);
            SendInfo(playerid, "Players can call you with /calltaxi. Accept with /taxiaccept [player].");
        }
        case JOB_MINER:
        {
            Job_NextRock(playerid);
        }
        case JOB_MECHANIC:
        {
            if (Job_SpawnVehicle(playerid, 525, gGarageSpawn, 1, 1) == INVALID_VEHICLE_ID) {
                Job_Stop(playerid, "no work vehicle available.");
                return 1;
            }
            SendInfo(playerid, "Offer services with /repair [player] [price] and /refill [player] [price].");
        }
    }
    SendClientMessage(playerid, COLOR_SUCCESS, "Shift started as %s. /stopwork ends it.", gJobNames[job]);
    return 1;
}

CMD:stopwork(playerid, params[])
{
    if (!gJob[playerid][jsWorking]) return SendError(playerid, "You're not on shift.");
    Job_Stop(playerid, "you clocked out.");
    return 1;
}

CMD:calltaxi(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    if (gettime() - gTaxiCallTime[playerid] < 60) return SendError(playerid, "You called a taxi recently. Wait a minute.");
    new drivers = 0;
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (Player_IsPlaying(i) && Job_IsWorking(i, JOB_TAXI))
        {
            SendClientMessage(i, COLOR_YELLOW, "[Taxi] %s [%d] needs a ride. /taxiaccept %d", gChar[playerid][cName], playerid, playerid);
            drivers++;
        }
    }
    if (drivers == 0) return SendError(playerid, "No taxi drivers are working right now.");
    gTaxiCallTime[playerid] = gettime();
    SendClientMessage(playerid, COLOR_SUCCESS, "Your call went out to %d taxi driver(s).", drivers);
    return 1;
}

CMD:taxiaccept(playerid, params[])
{
    if (!Job_IsWorking(playerid, JOB_TAXI)) return SendError(playerid, "You need to be on a taxi shift.");
    new targetid;
    if (sscanf(params, "u", targetid)) return SendInfo(playerid, "Usage: /taxiaccept [player]");
    if (!Player_IsPlaying(targetid) || targetid == playerid) return SendError(playerid, "That player isn't available.");
    if (gTaxiCallTime[targetid] == 0 || gettime() - gTaxiCallTime[targetid] > 300) return SendError(playerid, "That player hasn't called a taxi.");
    if (GetPlayerInterior(targetid) != 0) return SendError(playerid, "They're inside a building. Ask them to come outside.");
    gTaxiCallTime[targetid] = 0;
    new point[E_POINT];
    GetPlayerPos(targetid, point[ptX], point[ptY], point[ptZ]);
    gJob[playerid][jsStage] = JS_CALL;
    gJob[playerid][jsCaller] = targetid;
    Job_SetCheckpoint(playerid, point);
    SendClientMessage(targetid, COLOR_SUCCESS, "Taxi driver %s accepted your call and is on the way.", gChar[playerid][cName]);
    SendInfo(playerid, "The caller is marked on your map.");
    return 1;
}

stock Job_ServiceCheck(playerid, targetid, price)
{
    if (!Job_IsWorking(playerid, JOB_MECHANIC)) return SendError(playerid, "You need to be on a mechanic shift."), 0;
    if (!Player_IsPlaying(targetid) || targetid == playerid) return SendError(playerid, "That player isn't available."), 0;
    if (!(1 <= price <= KM_SERVICE_PRICE_MAX)) return SendClientMessage(playerid, COLOR_ERROR, "[!] Price must be between $1 and $%d.", KM_SERVICE_PRICE_MAX), 0;
    if (!Player_IsNear(playerid, targetid, 8.0)) return SendError(playerid, "You need to be next to their vehicle."), 0;
    if (GetPlayerState(targetid) != PLAYER_STATE_DRIVER) return SendError(playerid, "They need to be in the driver's seat."), 0;
    return 1;
}

CMD:repair(playerid, params[])
{
    new targetid, price;
    if (sscanf(params, "ud", targetid, price)) return SendInfo(playerid, "Usage: /repair [player] [price]");
    if (!Job_ServiceCheck(playerid, targetid, price)) return 1;
    Offer_Make(playerid, targetid, OFFER_REPAIR, price);
    return 1;
}

CMD:refill(playerid, params[])
{
    new targetid, price;
    if (sscanf(params, "ud", targetid, price)) return SendInfo(playerid, "Usage: /refill [player] [price]");
    if (!Job_ServiceCheck(playerid, targetid, price)) return 1;
    Offer_Make(playerid, targetid, OFFER_REFILL, price);
    return 1;
}
