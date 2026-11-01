// Driving licences and traffic tickets.
//   /drivingtest at the licensing office: a timed route in a test car.
//   /licence, /showlicence [player]
//   Police: /ticket [player] [amount] [reason]; the MDT shows licence and
//   unpaid tickets. Players: /tickets, /paytickets (bank, at an ATM or by phone).

#define TICKETS_UNPAID_SQL \
    "SELECT id, amount, reason FROM tickets WHERE character_id = %d AND paid_at IS NULL ORDER BY id LIMIT 20"
// Marks the listed tickets paid. Only unpaid ones up to the newest seen, so a
// ticket written meanwhile stays open.
#define TICKETS_PAY_SQL \
    "UPDATE tickets SET paid_at = UTC_TIMESTAMP() WHERE character_id = %d AND paid_at IS NULL AND id <= %d"

enum E_LICENCE_TEST
{
    bool:ltActive,
    ltVehicle,
    ltStep,
    ltStarted
}

new gTest[MAX_PLAYERS][E_LICENCE_TEST];
new bool:gPayingTickets[MAX_PLAYERS];

forward Lic_OnTicketList(playerid, race, bool:pay);
forward Lic_OnTicketsPaid(playerid, race, total, count);

stock Lic_Reset(playerid)
{
    gTest[playerid][ltActive] = false;
    gTest[playerid][ltVehicle] = INVALID_VEHICLE_ID;
    gPayingTickets[playerid] = false;
}

stock Lic_Init()
{
    Create3DTextLabel("Licensing office\n" EMBED_WHITE "/drivingtest", COLOR_BRAND, gLicenceOffice[ptX], gLicenceOffice[ptY], gLicenceOffice[ptZ] + 0.6, 15.0, 0, true);
    CreatePickup(1239, 1, gLicenceOffice[ptX], gLicenceOffice[ptY], gLicenceOffice[ptZ], 0);
}

stock Lic_End(playerid, bool:passed, const reason[] = "")
{
    if (!gTest[playerid][ltActive]) return;
    gTest[playerid][ltActive] = false;
    if (gTest[playerid][ltVehicle] != INVALID_VEHICLE_ID)
    {
        Veh_Destroy(gTest[playerid][ltVehicle]);
        gTest[playerid][ltVehicle] = INVALID_VEHICLE_ID;
    }
    DisablePlayerCheckpoint(playerid);
    if (!IsPlayerConnected(playerid)) return;
    if (passed)
    {
        gChar[playerid][cDrivingLicence] = true;
        new query[96];
        mysql_format(gDB, query, sizeof (query), "UPDATE characters SET driving_licence = 1 WHERE id = %d", gChar[playerid][cId]);
        mysql_tquery(gDB, query);
        SendClientMessage(playerid, COLOR_SUCCESS, "You passed! You now have a driving licence.");
        KMUI_Toast(playerid, "Driving licence earned");
    }
    else
    {
        SendClientMessage(playerid, COLOR_ERROR, "You failed the driving test: %s Try again with /drivingtest.", reason);
    }
}

// Returns true if the checkpoint belonged to a driving test.
stock bool:Lic_OnCheckpoint(playerid)
{
    if (!gTest[playerid][ltActive]) return false;
    if (GetPlayerVehicleID(playerid) != gTest[playerid][ltVehicle])
    {
        SendError(playerid, "Get back in the test car.");
        return true;
    }
    gTest[playerid][ltStep]++;
    if (gTest[playerid][ltStep] < sizeof (gLicenceRoute))
    {
        Job_SetCheckpoint(playerid, gLicenceRoute[gTest[playerid][ltStep]], 5.0);
        return true;
    }
    new Float:health;
    GetVehicleHealth(gTest[playerid][ltVehicle], health);
    if (health < KM_LICENCE_MIN_HEALTH) Lic_End(playerid, false, "the car is too damaged.");
    else Lic_End(playerid, true);
    return true;
}

// Once a second.
stock Lic_Tick(playerid)
{
    if (gTest[playerid][ltActive] && gettime() - gTest[playerid][ltStarted] > KM_LICENCE_TEST_SECONDS)
    {
        Lic_End(playerid, false, "you ran out of time.");
    }
}

stock Lic_OnVehicleDeath(vehicleid)
{
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (gTest[i][ltActive] && gTest[i][ltVehicle] == vehicleid) Lic_End(i, false, "the car was wrecked.");
    }
}

CMD:drivingtest(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    if (gChar[playerid][cDrivingLicence]) return SendInfo(playerid, "You already have a driving licence.");
    if (gTest[playerid][ltActive]) return SendError(playerid, "You're already taking the test.");
    if (!Place_Near(playerid, gLicenceOffice, 5.0)) return SendError(playerid, "Take the test at the licensing office by City Hall.");
    if (gJob[playerid][jsWorking] || gChar[playerid][cOnDuty]) return SendError(playerid, "Finish your shift or duty first.");
    if (IsPlayerInAnyVehicle(playerid)) return SendError(playerid, "Get out of your vehicle first.");
    if (!Money_Charge(playerid, KM_LICENCE_FEE, "driving_test")) return SendClientMessage(playerid, COLOR_ERROR, "[!] The test costs $%d.", KM_LICENCE_FEE);

    new vehicleid = Veh_Create(VEH_JOB, 410, gLicenceCarSpawn[ptX], gLicenceCarSpawn[ptY], gLicenceCarSpawn[ptZ], gLicenceCarSpawn[ptA], 1, 1, -1);
    if (vehicleid == INVALID_VEHICLE_ID)
    {
        Money_Give(playerid, 0, KM_LICENCE_FEE, "driving_test_refund");
        return SendError(playerid, "No test car is available. You were refunded.");
    }
    gVeh[vehicleid][vOwner] = playerid;
    gTest[playerid][ltActive] = true;
    gTest[playerid][ltVehicle] = vehicleid;
    gTest[playerid][ltStep] = 0;
    gTest[playerid][ltStarted] = gettime();
    PutPlayerInVehicle(playerid, vehicleid, 0);
    Veh_SetEngine(vehicleid, true);
    Job_SetCheckpoint(playerid, gLicenceRoute[0], 5.0);
    SendClientMessage(playerid, COLOR_INFO, "Drive through the %d markers within %d minutes without damaging the car.",
        sizeof (gLicenceRoute), KM_LICENCE_TEST_SECONDS / 60);
    return 1;
}

CMD:licence(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    if (gChar[playerid][cDrivingLicence]) SendInfo(playerid, "You have a driving licence.");
    else SendInfo(playerid, "You don't have a driving licence. Take the test at the licensing office (/drivingtest).");
    return 1;
}

CMD:showlicence(playerid, params[])
{
    new targetid;
    if (sscanf(params, "u", targetid)) return SendInfo(playerid, "Usage: /showlicence [player]");
    if (!Player_IsPlaying(targetid) || targetid == playerid || !Player_IsNear(playerid, targetid, 3.0)) return SendError(playerid, "They need to be next to you.");
    if (!gChar[playerid][cDrivingLicence]) return SendError(playerid, "You don't have a driving licence to show.");
    SendClientMessage(targetid, COLOR_INFO, "%s shows you a valid San Andreas driving licence.", gChar[playerid][cName]);
    new action[64];
    format(action, sizeof (action), "shows their driving licence to %s.", Chat_Name(targetid));
    Chat_Action(playerid, action);
    return 1;
}

// ---- tickets ---------------------------------------------------------------

CMD:ticket(playerid, params[])
{
    new targetid, amount, reason[96];
    if (!Fac_PoliceCheck(playerid)) return 1;
    if (sscanf(params, "uds[96]", targetid, amount, reason)) return SendInfo(playerid, "Usage: /ticket [player] [amount] [reason]");
    if (!Player_IsPlaying(targetid) || targetid == playerid || !Player_IsNear(playerid, targetid, 8.0)) return SendError(playerid, "They need to be near you.");
    if (!(1 <= amount <= KM_TICKET_MAX)) return SendClientMessage(playerid, COLOR_ERROR, "[!] Tickets are $1-$%d.", KM_TICKET_MAX);
    new query[320];
    mysql_format(gDB, query, sizeof (query), "INSERT INTO tickets (character_id, officer_character_id, amount, reason) VALUES (%d, %d, %d, '%e')",
        gChar[targetid][cId], gChar[playerid][cId], amount, reason);
    mysql_tquery(gDB, query);
    SendClientMessage(targetid, COLOR_YELLOW, "Officer %s gave you a $%d ticket: %s. Pay it with /paytickets.", gChar[playerid][cName], amount, reason);
    SendClientMessage(playerid, COLOR_SUCCESS, "Ticket written: $%d to %s.", amount, gChar[targetid][cName]);
    new action[64];
    format(action, sizeof (action), "writes a ticket and hands it to %s.", Chat_Name(targetid));
    Chat_Action(playerid, action);
    return 1;
}

CMD:tickets(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    new query[160];
    mysql_format(gDB, query, sizeof (query), TICKETS_UNPAID_SQL, gChar[playerid][cId]);
    mysql_tquery(gDB, query, "Lic_OnTicketList", "ddd", playerid, gPlayerRace[playerid], false);
    return 1;
}

CMD:paytickets(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    if (!Eco_NearAtm(playerid) && !Inv_Has(playerid, ITEM_PHONE)) return SendError(playerid, "Pay at an ATM, or with a phone.");
    if (gPayingTickets[playerid]) return 1;
    gPayingTickets[playerid] = true;
    new query[160];
    mysql_format(gDB, query, sizeof (query), TICKETS_UNPAID_SQL, gChar[playerid][cId]);
    mysql_tquery(gDB, query, "Lic_OnTicketList", "ddd", playerid, gPlayerRace[playerid], true);
    return 1;
}

public Lic_OnTicketList(playerid, race, bool:pay)
{
    if (gPlayerRace[playerid] != race) return 1;
    new rows = cache_num_rows(), total = 0, newest = 0;
    if (rows == 0)
    {
        gPayingTickets[playerid] = false;
        return SendInfo(playerid, "You have no unpaid tickets."), 1;
    }
    if (!pay) SendClientMessage(playerid, COLOR_BRAND, "Unpaid tickets:");
    for (new i = 0; i < rows; i++)
    {
        new id, amount, reason[96];
        cache_get_value_name_int(i, "id", id);
        cache_get_value_name_int(i, "amount", amount);
        cache_get_value_name(i, "reason", reason);
        total += amount;
        if (id > newest) newest = id;
        if (!pay) SendClientMessage(playerid, COLOR_WHITE, "  #%d $%d - %s", id, amount, reason);
    }
    if (!pay)
    {
        SendClientMessage(playerid, COLOR_INFO, "Total $%d. Pay with /paytickets.", total);
        return 1;
    }
    new details[32];
    format(details, sizeof (details), "%d ticket(s)", rows);
    if (!Money_Give(playerid, 0, -total, "tickets", INVALID_PLAYER_ID, details))
    {
        gPayingTickets[playerid] = false;
        return SendClientMessage(playerid, COLOR_ERROR, "[!] You owe $%d but don't have that much in the bank.", total), 1;
    }
    new query[160];
    mysql_format(gDB, query, sizeof (query), TICKETS_PAY_SQL, gChar[playerid][cId], newest);
    mysql_tquery(gDB, query, "Lic_OnTicketsPaid", "dddd", playerid, race, total, rows);
    return 1;
}

public Lic_OnTicketsPaid(playerid, race, total, count)
{
    if (gPlayerRace[playerid] != race) return 1;
    gPayingTickets[playerid] = false;
    SendClientMessage(playerid, COLOR_SUCCESS, "Paid %d ticket(s), $%d in total.", count, total);
    return 1;
}
