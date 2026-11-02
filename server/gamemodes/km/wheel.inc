// Lucky wheel: /spin once every 20 hours for a random prize (cash or season
// pass points). The database decides, in one UPDATE, whether a spin is
// allowed, so two quick /spin can't both pay.

#define WHEEL_COOLDOWN_HOURS    (20)

forward Wheel_OnSpin(playerid, race, charId);
forward Wheel_OnWait(playerid, race);

enum E_WHEEL_PRIZE
{
    wpWeight,       // chance out of 100
    wpCash,
    wpPassPoints,
    wpName[24]
}

// Weights add up to 100. Edit freely, keeping the sum at 100.
static const gWheelPrizes[][E_WHEEL_PRIZE] =
{
    { 30,   250,  0, "$250" },
    { 25,   500,  0, "$500" },
    { 15,  1000,  0, "$1000" },
    { 15,     0, 10, "10 season pass points" },
    {  8,     0, 25, "25 season pass points" },
    {  5,  2500,  0, "$2500" },
    {  2, 10000,  0, "the $10000 JACKPOT" }
};

static bool:gWheelBusy[MAX_PLAYERS];

stock Wheel_PrizeCount()
{
    return sizeof (gWheelPrizes);
}

// The prize for a roll of 0..99.
stock Wheel_PrizeFor(roll)
{
    new upTo = 0;
    for (new i = 0; i < sizeof (gWheelPrizes); i++)
    {
        upTo += gWheelPrizes[i][wpWeight];
        if (roll < upTo) return i;
    }
    return 0;
}

stock Wheel_TotalWeight()
{
    new total = 0;
    for (new i = 0; i < sizeof (gWheelPrizes); i++) total += gWheelPrizes[i][wpWeight];
    return total;
}

stock Wheel_PrizeCash(prize) { return gWheelPrizes[prize][wpCash]; }
stock Wheel_PrizePoints(prize) { return gWheelPrizes[prize][wpPassPoints]; }

stock Wheel_Reset(playerid)
{
    gWheelBusy[playerid] = false;
}

CMD:spin(playerid, params[])
{
    if (!Player_IsPlaying(playerid) || gWheelBusy[playerid]) return 1;
    gWheelBusy[playerid] = true;
    new query[256];
    mysql_format(gDB, query, sizeof (query),
        "UPDATE characters SET wheel_last_spin = UTC_TIMESTAMP() \
         WHERE id = %d AND (wheel_last_spin IS NULL OR wheel_last_spin <= UTC_TIMESTAMP() - INTERVAL %d HOUR)",
        gChar[playerid][cId], WHEEL_COOLDOWN_HOURS);
    mysql_tquery(gDB, query, "Wheel_OnSpin", "ddd", playerid, gPlayerRace[playerid], gChar[playerid][cId]);
    return 1;
}

public Wheel_OnSpin(playerid, race, charId)
{
    if (gPlayerRace[playerid] != race || gChar[playerid][cId] != charId) return 1;
    if (cache_affected_rows() == 0)
    {
        new query[256];
        mysql_format(gDB, query, sizeof (query),
            "SELECT GREATEST(1, TIMESTAMPDIFF(MINUTE, UTC_TIMESTAMP(), wheel_last_spin + INTERVAL %d HOUR)) AS wait FROM characters WHERE id = %d",
            WHEEL_COOLDOWN_HOURS, charId);
        mysql_tquery(gDB, query, "Wheel_OnWait", "dd", playerid, race);
        return 1;
    }
    gWheelBusy[playerid] = false;
    new prize = Wheel_PrizeFor(random(Wheel_TotalWeight()));
    new cash = gWheelPrizes[prize][wpCash], points = gWheelPrizes[prize][wpPassPoints];
    if (cash > 0) Money_Give(playerid, cash, 0, "wheel", INVALID_PLAYER_ID, gWheelPrizes[prize][wpName]);
    if (points > 0) Pass_AddPoints(playerid, points);
    SendClientMessage(playerid, COLOR_SUCCESS, "You spin the lucky wheel... you won %s! Spin again tomorrow.", gWheelPrizes[prize][wpName]);
    new toast[48];
    format(toast, sizeof (toast), "Lucky wheel: %s", gWheelPrizes[prize][wpName]);
    KMUI_Toast(playerid, toast);
    Chat_Action(playerid, "spins the lucky wheel.");
    if (cash >= 10000)
    {
        new announce[128];
        format(announce, sizeof (announce), "%s hit the lucky wheel jackpot: $%d!", gChar[playerid][cName], cash);
        SendClientMessageToAll(COLOR_BRAND, announce);
    }
    return 1;
}

public Wheel_OnWait(playerid, race)
{
    if (gPlayerRace[playerid] != race) return 1;
    gWheelBusy[playerid] = false;
    new wait = 0;
    if (cache_num_rows()) cache_get_value_name_int(0, "wait", wait);
    SendClientMessage(playerid, COLOR_INFO, "You've already spun the wheel today. Your next spin is ready in %dh %02dm.", wait / 60, wait % 60);
    return 1;
}
