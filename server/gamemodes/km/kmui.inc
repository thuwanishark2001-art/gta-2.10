// Kingmakers client UI protocol, version 1 (docs/KMUI.md).
//
// The Kingmakers client announces itself with "/kmclient <version>" right
// after it connects. From then on the server sends it UI events as client
// messages in the colour KMUI_COLOUR with the text "KM1|<event>|k=v;k=v".
// The client draws them (HUD, speedometer, toasts, phone) instead of
// showing them in chat. Other clients never get these messages.

#define KMUI_VERSION        (1)
#define KMUI_COLOUR         (0x4B4D5501)    // "KMU" + 1, never used for chat
#define KMUI_MAX_TEXT       (144)           // SA:MP client message limit

new gKmClient[MAX_PLAYERS];                 // protocol version, 0 = standard client
new gKmHudHash[MAX_PLAYERS];                // last HUD payload sent

stock KMUI_Reset(playerid)
{
    gKmClient[playerid] = 0;
    gKmHudHash[playerid] = 0;
}

stock bool:KMUI_Enabled(playerid)
{
    return gKmClient[playerid] >= 1;
}

// Values can't contain the separators. Replaces | ; = and control
// characters with spaces, in place.
stock KMUI_Escape(text[])
{
    for (new i = 0; text[i] != '\0'; i++)
    {
        if (text[i] == '|' || text[i] == ';' || text[i] == '=' || text[i] < ' ') text[i] = ' ';
    }
}

// Builds "KM1|event|payload", cut to the client message limit.
stock KMUI_Build(dest[], const event[], const payload[], size = sizeof (dest))
{
    format(dest, size, "KM%d|%s|%s", KMUI_VERSION, event, payload);
    if (strlen(dest) >= KMUI_MAX_TEXT) dest[KMUI_MAX_TEXT - 1] = '\0';
}

stock KMUI_Send(playerid, const event[], const payload[])
{
    if (!KMUI_Enabled(playerid)) return 0;
    new text[KMUI_MAX_TEXT + 16];
    KMUI_Build(text, event, payload);
    return SendClientMessage(playerid, KMUI_COLOUR, "%s", text);
}

// The HUD payload: money, level progress, job, wanted level and, while
// driving a vehicle that uses fuel, the fuel level (-1 otherwise).
stock KMUI_HudPayload(dest[], cash, bank, level, xp, xpNeeded, const job[], wanted, fuel, size = sizeof (dest))
{
    new jobText[24];
    format(jobText, sizeof (jobText), "%s", job);
    KMUI_Escape(jobText);
    format(dest, size, "cash=%d;bank=%d;lvl=%d;xp=%d;xpmax=%d;job=%s;wanted=%d;fuel=%d",
        cash, bank, level, xp, xpNeeded, jobText, wanted, fuel);
}

// Sends the HUD if anything on it changed. Called from the 1 s tick.
stock KMUI_UpdateHud(playerid)
{
    if (!KMUI_Enabled(playerid) || !Player_IsPlaying(playerid)) return;
    new fuel = -1;
    if (GetPlayerState(playerid) == PLAYER_STATE_DRIVER)
    {
        new vehicleid = GetPlayerVehicleID(playerid);
        if (0 < vehicleid < MAX_VEHICLES && Veh_UsesFuel(vehicleid)) fuel = floatround(gVeh[vehicleid][vFuel], floatround_ceil);
    }
    new payload[128];
    KMUI_HudPayload(payload, gChar[playerid][cCash], gChar[playerid][cBank], gChar[playerid][cLevel], gChar[playerid][cXp],
        Level_XpNeeded(gChar[playerid][cLevel]), gJobNames[gChar[playerid][cJob]], gChar[playerid][cWanted], fuel);
    new hash = StrHashCI(payload);
    if (hash == gKmHudHash[playerid]) return;
    gKmHudHash[playerid] = hash;
    KMUI_Send(playerid, "hud", payload);
}

// A short pop-up. Standard clients already get the same news in chat.
stock KMUI_Toast(playerid, const text[])
{
    if (!KMUI_Enabled(playerid)) return;
    new payload[112];
    format(payload, sizeof (payload), "text=%s", text);
    KMUI_Escape(payload[5]);
    KMUI_Send(playerid, "toast", payload);
}

stock KMUI_Sms(playerid, fromNumber, const text[])
{
    if (!KMUI_Enabled(playerid)) return;
    new payload[128];
    format(payload, sizeof (payload), "from=%d;text=%s", fromNumber, text);
    // Only the text part needs escaping; the number can't contain separators.
    new start = strfind(payload, ";text=") + 6;
    KMUI_Escape(payload[start]);
    KMUI_Send(playerid, "sms", payload);
}

// Answer to /phone: whether they have a phone, its number and the balance.
stock KMUI_Phone(playerid)
{
    new payload[64];
    format(payload, sizeof (payload), "has=%d;number=%d;bank=%d", Inv_Has(playerid, ITEM_PHONE) ? 1 : 0, gChar[playerid][cPhone], gChar[playerid][cBank]);
    KMUI_Send(playerid, "phone", payload);
}

// Sent by the Kingmakers client, also before login.
CMD:kmclient(playerid, params[])
{
    new version;
    if (sscanf(params, "d", version) || version < 1) return 1;
    gKmClient[playerid] = version > KMUI_VERSION ? KMUI_VERSION : version;
    gKmHudHash[playerid] = 0;
    new payload[16];
    format(payload, sizeof (payload), "v=%d", gKmClient[playerid]);
    KMUI_Send(playerid, "hello", payload);
    return 1;
}

// Opens the phone screen on the Kingmakers client.
CMD:phone(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    if (!KMUI_Enabled(playerid))
    {
        if (!Inv_Has(playerid, ITEM_PHONE)) return SendError(playerid, "You don't have a phone. 24/7 shops sell them.");
        Inv_Use(playerid, ITEM_PHONE);
        return 1;
    }
    KMUI_Phone(playerid);
    return 1;
}
