// Casino: roulette at gCasino (Pershing Square).
//   /roulette red|black|even|odd|green|<0-36> <bet>
// Red, black, even and odd pay 2x (0 loses them); a number, or green (0),
// pays 36x. Bets are cash, KM_CASINO_MIN_BET to KM_CASINO_MAX_BET.

enum
{
    BET_RED,
    BET_BLACK,
    BET_EVEN,
    BET_ODD,
    BET_NUMBER
}

static gCasinoNext[MAX_PLAYERS];

stock bool:Roulette_IsRed(number)
{
    switch (number)
    {
        case 1, 3, 5, 7, 9, 12, 14, 16, 18, 19, 21, 23, 25, 27, 30, 32, 34, 36: return true;
    }
    return false;
}

// What a bet returns (0 = lost, else the multiple of the stake paid back).
stock Roulette_Payout(kind, number, result)
{
    if (kind == BET_NUMBER) return number == result ? 36 : 0;
    if (result == 0) return 0;
    switch (kind)
    {
        case BET_RED: return Roulette_IsRed(result) ? 2 : 0;
        case BET_BLACK: return Roulette_IsRed(result) ? 0 : 2;
        case BET_EVEN: return result % 2 == 0 ? 2 : 0;
        case BET_ODD: return result % 2 == 1 ? 2 : 0;
    }
    return 0;
}

// Parses red/black/even/odd/green/0-36; false if it's none of them.
stock bool:Roulette_ParseBet(const text[], &kind, &number)
{
    number = 0;
    if (!strcmp(text, "red", true)) { kind = BET_RED; return true; }
    if (!strcmp(text, "black", true)) { kind = BET_BLACK; return true; }
    if (!strcmp(text, "even", true)) { kind = BET_EVEN; return true; }
    if (!strcmp(text, "odd", true)) { kind = BET_ODD; return true; }
    if (!strcmp(text, "green", true)) { kind = BET_NUMBER; number = 0; return true; }
    new len = strlen(text);
    if (len < 1 || len > 2) return false;
    for (new i = 0; i < len; i++) if (!('0' <= text[i] <= '9')) return false;
    number = strval(text);
    if (!(0 <= number <= 36)) return false;
    kind = BET_NUMBER;
    return true;
}

stock Casino_Reset(playerid)
{
    gCasinoNext[playerid] = 0;
}

CMD:roulette(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    new what[8], bet, kind, number;
    if (sscanf(params, "s[8]d", what, bet) || !Roulette_ParseBet(what, kind, number))
        return SendInfo(playerid, "Usage: /roulette [red/black/even/odd/green/0-36] [bet]. Colours, even and odd pay 2x; a number pays 36x.");
    if (!IsPlayerInRangeOfPoint(playerid, 8.0, gCasino[ptX], gCasino[ptY], gCasino[ptZ]))
        return SendError(playerid, "You need to be at the casino in Pershing Square (/gps).");
    if (!(KM_CASINO_MIN_BET <= bet <= KM_CASINO_MAX_BET))
        return SendClientMessage(playerid, COLOR_ERROR, "[!] Bets are $%d to $%d.", KM_CASINO_MIN_BET, KM_CASINO_MAX_BET), 1;
    if (gChar[playerid][cCash] < bet) return SendError(playerid, "You don't have that much cash.");
    if (gettime() < gCasinoNext[playerid]) return SendError(playerid, "The wheel is still spinning.");
    gCasinoNext[playerid] = gettime() + 3;

    new result = random(37);
    new payout = Roulette_Payout(kind, number, result);
    new net = payout ? bet * (payout - 1) : -bet;
    new details[48];
    format(details, sizeof (details), "roulette %s $%d, ball %d", what, bet, result);
    Money_Give(playerid, net, 0, "casino", INVALID_PLAYER_ID, details);
    new colour[8];
    format(colour, sizeof (colour), "%s", result == 0 ? "green" : (Roulette_IsRed(result) ? "red" : "black"));
    if (payout)
        SendClientMessage(playerid, COLOR_SUCCESS, "The ball lands on %d %s. You win $%d!", result, colour, net);
    else
        SendClientMessage(playerid, COLOR_INFO, "The ball lands on %d %s. You lose $%d.", result, colour, bet);
    Chat_Action(playerid, "places a bet at the roulette table.");
    return 1;
}
