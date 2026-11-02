// /gps: marks a place on the map (the client's GPS button sends /gps).
// The marker clears when you reach it, like the /jobs marker.

enum
{
    GPS_ATM,
    GPS_GAS,
    GPS_SHOP,
    GPS_DEALER,
    GPS_HOSPITAL,
    GPS_LSPD,
    GPS_LICENCE,
    GPS_FIRST_JOB       // then one entry per job, JOB_NONE + 1 onwards
}

// Index of the point nearest to the player, or -1 if there are none.
static Gps_Nearest(playerid, const points[][E_POINT], count)
{
    new Float:x, Float:y, Float:z, best = -1, Float:bestDist = 1.0e9;
    GetPlayerPos(playerid, x, y, z);
    for (new i = 0; i < count; i++)
    {
        new Float:d = Dist3D(x, y, z, points[i][ptX], points[i][ptY], points[i][ptZ]);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

// Nearest 24/7 (business property), or -1.
static Gps_NearestShop(playerid)
{
    new Float:x, Float:y, Float:z, best = -1, Float:bestDist = 1.0e9;
    GetPlayerPos(playerid, x, y, z);
    for (new i = 0; i < KM_MAX_PROPERTIES; i++)
    {
        if (!gProp[i][prUsed] || gProp[i][prId] <= 0 || gProp[i][prType] != PROP_BUSINESS) continue;
        new Float:d = Dist3D(x, y, z, gProp[i][prX], gProp[i][prY], gProp[i][prZ]);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

static Gps_Mark(playerid, const point[E_POINT], const name[])
{
    Job_SetCheckpoint(playerid, point);
    gJob[playerid][jsGps] = true;
    SendClientMessage(playerid, COLOR_INFO, "%s is marked on your map.", name);
}

CMD:gps(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    new list[512] = "Nearest ATM\nNearest gas station\nNearest 24/7 shop\nCar dealership\nHospital\nPolice (LSPD)\nLicensing office";
    for (new job = JOB_NONE + 1; job < JOB_COUNT; job++)
    {
        strcat(list, "\nJob: ");
        strcat(list, gJobNames[job]);
    }
    ShowPlayerDialog(playerid, DIALOG_GPS, DIALOG_STYLE_LIST, "GPS | pick a place to mark it", list, "Mark", "Close");
    return 1;
}

stock Gps_OnDialog(playerid, dialogid, response, listitem)
{
    if (dialogid != DIALOG_GPS) return 0;
    if (!response || !Player_IsPlaying(playerid)) return 1;
    if (gJob[playerid][jsWorking]) return SendError(playerid, "Your shift is using the map marker. /stopwork first."), 1;
    new point[E_POINT];
    switch (listitem)
    {
        case GPS_ATM: Gps_Mark(playerid, gAtms[Gps_Nearest(playerid, gAtms, sizeof (gAtms))], "The nearest ATM");
        case GPS_GAS: Gps_Mark(playerid, gGasStations[Gps_Nearest(playerid, gGasStations, sizeof (gGasStations))], "The nearest gas station");
        case GPS_SHOP:
        {
            new slot = Gps_NearestShop(playerid);
            if (slot == -1) return SendError(playerid, "There are no shops yet."), 1;
            point[ptX] = gProp[slot][prX];
            point[ptY] = gProp[slot][prY];
            point[ptZ] = gProp[slot][prZ];
            Gps_Mark(playerid, point, gProp[slot][prName]);
        }
        case GPS_DEALER: Gps_Mark(playerid, gDealer, "The car dealership");
        case GPS_HOSPITAL: Gps_Mark(playerid, gHospital, "All Saints hospital");
        case GPS_LSPD: Gps_Mark(playerid, gJailRelease, "The LSPD");
        case GPS_LICENCE: Gps_Mark(playerid, gLicenceOffice, "The licensing office");
        default:
        {
            new job = listitem - GPS_FIRST_JOB + 1;
            if (!(JOB_NONE < job < JOB_COUNT)) return 1;
            Job_Site(job, point);
            new name[40];
            format(name, sizeof (name), "The %s job", gJobNames[job]);
            Gps_Mark(playerid, point, name);
        }
    }
    return 1;
}
