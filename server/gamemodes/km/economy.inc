// Money: every change to cash or bank goes through Money_Give, which keeps
// both non-negative, saves them at once and writes a money_log row.
// Also ATMs (/balance, /deposit, /withdraw), /pay, /transfer and paydays.

// Changes cash and/or bank. Returns false (and changes nothing) if either
// would go below zero. `other` is the other character's player id, if any.
stock bool:Money_Give(playerid, cashDelta, bankDelta, const reason[], other = INVALID_PLAYER_ID, const details[] = "")
{
    if (gChar[playerid][cId] == 0) return false;
    new cash = gChar[playerid][cCash] + cashDelta;
    new bank = gChar[playerid][cBank] + bankDelta;
    // Overflow on a huge positive delta shows up as a negative balance too.
    if (cash < 0 || bank < 0) return false;

    gChar[playerid][cCash] = cash;
    gChar[playerid][cBank] = bank;
    if (cashDelta != 0) Char_SyncCash(playerid);

    new otherCharId = (other != INVALID_PLAYER_ID && IsPlayerConnected(other)) ? gChar[other][cId] : 0;
    new query[512];
    mysql_format(gDB, query, sizeof (query),
        "UPDATE characters SET cash = %d, bank = %d WHERE id = %d", cash, bank, gChar[playerid][cId]);
    mysql_tquery(gDB, query);
    mysql_format(gDB, query, sizeof (query),
        "INSERT INTO money_log (character_id, cash_delta, bank_delta, cash_after, bank_after, reason, other_character_id, details) \
         VALUES (%d, %d, %d, %d, %d, '%e', NULLIF(%d, 0), '%e')",
        gChar[playerid][cId], cashDelta, bankDelta, cash, bank, reason, otherCharId, details);
    mysql_tquery(gDB, query);
    return true;
}

// Pays from bank first, then cash. Used for big purchases (cars, houses).
stock bool:Money_Charge(playerid, amount, const reason[], const details[] = "")
{
    if (amount <= 0) return true;
    if (gChar[playerid][cBank] >= amount) return Money_Give(playerid, 0, -amount, reason, INVALID_PLAYER_ID, details);
    if (gChar[playerid][cBank] + gChar[playerid][cCash] < amount) return false;
    new fromCash = amount - gChar[playerid][cBank];
    return Money_Give(playerid, -fromCash, -gChar[playerid][cBank], reason, INVALID_PLAYER_ID, details);
}

stock bool:Eco_NearAtm(playerid)
{
    if (GetPlayerInterior(playerid) != 0) return false;
    for (new i = 0; i < sizeof (gAtms); i++)
    {
        if (Place_Near(playerid, gAtms[i], 3.0)) return true;
    }
    return false;
}

stock Eco_Init()
{
    for (new i = 0; i < sizeof (gAtms); i++)
    {
        Create3DTextLabel("ATM\n" EMBED_WHITE "/balance /deposit /withdraw", 0x7FD17FFF, gAtms[i][ptX], gAtms[i][ptY], gAtms[i][ptZ] + 0.6, 15.0, 0, true);
    }
}

// Called once per second for each playing character.
stock Eco_Tick(playerid)
{
    if (gChar[playerid][cJailSeconds] > 0) return; // no paydays in jail
    gChar[playerid][cPaydaySeconds]++;
    if (gChar[playerid][cPaydaySeconds] >= KM_PAYDAY_SECONDS)
    {
        gChar[playerid][cPaydaySeconds] = 0;
        Eco_Payday(playerid);
    }
}

stock Eco_Payday(playerid)
{
    new salary = Fac_Salary(gChar[playerid][cFaction], gChar[playerid][cFactionRank]);
    new interest = Payday_Interest(gChar[playerid][cBank]);
    new total = KM_PAYDAY_BASE + salary + interest;
    new details[64];
    format(details, sizeof (details), "base %d, salary %d, interest %d", KM_PAYDAY_BASE, salary, interest);
    Money_Give(playerid, 0, total, "payday", INVALID_PLAYER_ID, details);

    SendClientMessage(playerid, COLOR_BRAND, "---- Payday ----");
    SendClientMessage(playerid, COLOR_WHITE, "Base pay $%d | Salary $%d | Interest $%d", KM_PAYDAY_BASE, salary, interest);
    SendClientMessage(playerid, COLOR_WHITE, "Deposited $%d. Bank balance: $%d", total, gChar[playerid][cBank]);
    new toast[48];
    format(toast, sizeof (toast), "Payday: +$%d to your bank", total);
    KMUI_Toast(playerid, toast);

    gChar[playerid][cXp]++;
    new needed = Level_XpNeeded(gChar[playerid][cLevel]);
    if (gChar[playerid][cXp] >= needed)
    {
        gChar[playerid][cXp] -= needed;
        gChar[playerid][cLevel]++;
        SetPlayerScore(playerid, gChar[playerid][cLevel]);
        SendClientMessage(playerid, COLOR_SUCCESS, "You reached level %d.", gChar[playerid][cLevel]);
        format(toast, sizeof (toast), "Level up! You're level %d", gChar[playerid][cLevel]);
        KMUI_Toast(playerid, toast);
    }
    Char_Save(playerid);
}

// ---- commands ----------------------------------------------------------

CMD:balance(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    if (!Eco_NearAtm(playerid) && !Inv_Has(playerid, ITEM_PHONE)) return SendError(playerid, "Use an ATM, or buy a phone for mobile banking.");
    SendClientMessage(playerid, COLOR_INFO, "Cash: $%d | Bank: $%d | Next payday in %d min", gChar[playerid][cCash], gChar[playerid][cBank],
        (KM_PAYDAY_SECONDS - gChar[playerid][cPaydaySeconds] + 59) / 60);
    return 1;
}

CMD:deposit(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    new amount;
    if (sscanf(params, "d", amount) || amount <= 0) return SendInfo(playerid, "Usage: /deposit [amount]");
    if (!Eco_NearAtm(playerid)) return SendError(playerid, "You need to be at an ATM.");
    if (!Money_Give(playerid, -amount, amount, "deposit")) return SendError(playerid, "You don't have that much cash.");
    SendClientMessage(playerid, COLOR_SUCCESS, "Deposited $%d. Bank balance: $%d", amount, gChar[playerid][cBank]);
    return 1;
}

CMD:withdraw(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    new amount;
    if (sscanf(params, "d", amount) || amount <= 0) return SendInfo(playerid, "Usage: /withdraw [amount]");
    if (!Eco_NearAtm(playerid)) return SendError(playerid, "You need to be at an ATM.");
    if (!Money_Give(playerid, amount, -amount, "withdraw")) return SendError(playerid, "You don't have that much in the bank.");
    SendClientMessage(playerid, COLOR_SUCCESS, "Withdrew $%d. Bank balance: $%d", amount, gChar[playerid][cBank]);
    return 1;
}

// Cash handed to someone standing next to you.
CMD:pay(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    new targetid, amount;
    if (sscanf(params, "ud", targetid, amount)) return SendInfo(playerid, "Usage: /pay [player] [amount]");
    if (!Player_IsPlaying(targetid) || targetid == playerid) return SendError(playerid, "That player isn't available.");
    if (!(1 <= amount <= KM_PAY_MAX)) return SendClientMessage(playerid, COLOR_ERROR, "[!] Amount must be between $1 and $%d.", KM_PAY_MAX);
    if (!Player_IsNear(playerid, targetid, 4.0)) return SendError(playerid, "You need to be next to them.");
    if (!Money_Give(playerid, -amount, 0, "pay_out", targetid)) return SendError(playerid, "You don't have that much cash.");
    Money_Give(targetid, amount, 0, "pay_in", playerid);
    SendClientMessage(playerid, COLOR_SUCCESS, "You gave $%d to %s.", amount, gChar[targetid][cName]);
    SendClientMessage(targetid, COLOR_SUCCESS, "%s gave you $%d.", gChar[playerid][cName], amount);
    new action[64];
    format(action, sizeof (action), "hands some money to %s.", gChar[targetid][cName]);
    Chat_Action(playerid, action);
    return 1;
}

// Bank to bank, from anywhere with a phone.
CMD:transfer(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    new targetid, amount;
    if (sscanf(params, "ud", targetid, amount)) return SendInfo(playerid, "Usage: /transfer [player] [amount]");
    if (!Eco_NearAtm(playerid) && !Inv_Has(playerid, ITEM_PHONE)) return SendError(playerid, "Use an ATM, or buy a phone for mobile banking.");
    if (!Player_IsPlaying(targetid) || targetid == playerid) return SendError(playerid, "That player isn't available.");
    if (!(1 <= amount <= KM_PAY_MAX)) return SendClientMessage(playerid, COLOR_ERROR, "[!] Amount must be between $1 and $%d.", KM_PAY_MAX);
    if (!Money_Give(playerid, 0, -amount, "transfer_out", targetid)) return SendError(playerid, "You don't have that much in the bank.");
    Money_Give(targetid, 0, amount, "transfer_in", playerid);
    SendClientMessage(playerid, COLOR_SUCCESS, "Transferred $%d to %s. Bank balance: $%d", amount, gChar[targetid][cName], gChar[playerid][cBank]);
    SendClientMessage(targetid, COLOR_SUCCESS, "%s transferred $%d to your bank account.", gChar[playerid][cName], amount);
    return 1;
}
