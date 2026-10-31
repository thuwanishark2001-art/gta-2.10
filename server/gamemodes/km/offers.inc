// Paid service offers between players: a mechanic's repair or refill, a
// medic's treatment. The customer answers with /accept or /decline.

enum E_OFFER_TYPE
{
    OFFER_NONE,
    OFFER_REPAIR,
    OFFER_REFILL,
    OFFER_HEAL
}

enum E_OFFER
{
    E_OFFER_TYPE:oType,
    oFrom,
    oFromRace,
    oPrice,
    oExpires
}

#define KM_OFFER_SECONDS    (60)

new gOffer[MAX_PLAYERS][E_OFFER];

stock Offer_Reset(playerid)
{
    gOffer[playerid][oType] = OFFER_NONE;
}

stock Offer_Name(E_OFFER_TYPE:type, dest[], size = sizeof (dest))
{
    switch (type)
    {
        case OFFER_REPAIR: format(dest, size, "a vehicle repair");
        case OFFER_REFILL: format(dest, size, "a fuel refill");
        case OFFER_HEAL: format(dest, size, "medical treatment");
        default: format(dest, size, "nothing");
    }
}

stock Offer_Make(fromid, targetid, E_OFFER_TYPE:type, price)
{
    gOffer[targetid][oType] = type;
    gOffer[targetid][oFrom] = fromid;
    gOffer[targetid][oFromRace] = gPlayerRace[fromid];
    gOffer[targetid][oPrice] = price;
    gOffer[targetid][oExpires] = gettime() + KM_OFFER_SECONDS;

    new what[32];
    Offer_Name(type, what);
    SendClientMessage(targetid, COLOR_INFO, "%s offers you %s for $%d. Type /accept or /decline.", gChar[fromid][cName], what, price);
    SendClientMessage(fromid, COLOR_INFO, "You offered %s %s for $%d.", gChar[targetid][cName], what, price);
}

CMD:decline(playerid, params[])
{
    if (gOffer[playerid][oType] == OFFER_NONE) return SendError(playerid, "You have no offer.");
    new fromid = gOffer[playerid][oFrom];
    if (IsPlayerConnected(fromid) && gPlayerRace[fromid] == gOffer[playerid][oFromRace])
    {
        SendClientMessage(fromid, COLOR_INFO, "%s declined your offer.", gChar[playerid][cName]);
    }
    Offer_Reset(playerid);
    SendInfo(playerid, "Offer declined.");
    return 1;
}

CMD:accept(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    new E_OFFER_TYPE:type = gOffer[playerid][oType];
    if (type == OFFER_NONE) return SendError(playerid, "You have no offer to accept.");
    new fromid = gOffer[playerid][oFrom], price = gOffer[playerid][oPrice];
    new expires = gOffer[playerid][oExpires], fromRace = gOffer[playerid][oFromRace];
    Offer_Reset(playerid);

    if (gettime() > expires) return SendError(playerid, "That offer expired.");
    if (!Player_IsPlaying(fromid) || gPlayerRace[fromid] != fromRace) return SendError(playerid, "That player left.");
    if (!Player_IsNear(playerid, fromid, 8.0)) return SendError(playerid, "They need to be near you.");

    new vehicleid = GetPlayerVehicleID(playerid);
    switch (type)
    {
        case OFFER_REPAIR, OFFER_REFILL:
        {
            if (vehicleid == 0 || GetPlayerState(playerid) != PLAYER_STATE_DRIVER) return SendError(playerid, "You need to be driving the vehicle.");
            if (!Job_IsWorking(fromid, JOB_MECHANIC)) return SendError(playerid, "They are no longer working as a mechanic.");
        }
        case OFFER_HEAL:
        {
            if (!Fac_IsOnDutyType(fromid, FACTION_MEDICAL)) return SendError(playerid, "They are no longer on duty.");
        }
        default: return 1;
    }

    new what[32];
    Offer_Name(type, what);
    if (!Money_Give(playerid, -price, 0, "service_paid", fromid, what)) return SendError(playerid, "You don't have enough cash.");
    Money_Give(fromid, price, 0, "service_earned", playerid, what);

    new action[96];
    switch (type)
    {
        case OFFER_REPAIR:
        {
            RepairVehicle(vehicleid);
            format(action, sizeof (action), "repairs the vehicle of %s.", gChar[playerid][cName]);
        }
        case OFFER_REFILL:
        {
            Veh_SetFuel(vehicleid, 100.0);
            format(action, sizeof (action), "refills the fuel tank of %s's vehicle.", gChar[playerid][cName]);
        }
        case OFFER_HEAL:
        {
            SetPlayerHealth(playerid, 100.0);
            format(action, sizeof (action), "treats the injuries of %s.", gChar[playerid][cName]);
        }
        default: {}
    }
    Chat_Action(fromid, action);
    SendClientMessage(playerid, COLOR_SUCCESS, "You paid $%d for %s.", price, what);
    SendClientMessage(fromid, COLOR_SUCCESS, "%s paid you $%d.", gChar[playerid][cName], price);
    return 1;
}
