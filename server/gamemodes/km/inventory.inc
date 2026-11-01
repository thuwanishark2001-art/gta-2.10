// Inventory: items are counted per character and saved on every change.
// Bought in 24/7 shops (/shop), used or handed over from /inv.
// The phone gives a number for /sms and /911, and mobile banking.

enum E_ITEM { itName[16], itPrice, itMax }

stock const gItems[ITEM_COUNT][E_ITEM] = {
    { "", 0, 0 },
    { "Phone", 250, 1 },
    { "Water", 5, 10 },
    { "Burger", 12, 10 },
    { "Medkit", 150, 3 },
    { "Fuel can", 60, 2 }
};

new gInv[MAX_PLAYERS][ITEM_COUNT];
new gInvSelected[MAX_PLAYERS];
new gInvLoaded[MAX_PLAYERS];

forward Inv_OnLoaded(playerid, race);
forward Inv_OnPhoneNumber(playerid, race, number, attempt);

stock Inv_Reset(playerid)
{
    for (new i = 0; i < ITEM_COUNT; i++) gInv[playerid][i] = 0;
    gInvSelected[playerid] = ITEM_NONE;
    gInvLoaded[playerid] = false;
}

stock Inv_Load(playerid)
{
    Inv_Reset(playerid);
    new query[128];
    mysql_format(gDB, query, sizeof (query), "SELECT item_id, quantity FROM character_items WHERE character_id = %d", gChar[playerid][cId]);
    mysql_tquery(gDB, query, "Inv_OnLoaded", "dd", playerid, gPlayerRace[playerid]);
}

public Inv_OnLoaded(playerid, race)
{
    if (gPlayerRace[playerid] != race) return 1;
    for (new row = 0, rows = cache_num_rows(); row < rows; row++)
    {
        new item, quantity;
        cache_get_value_name_int(row, "item_id", item);
        cache_get_value_name_int(row, "quantity", quantity);
        if (0 < item < ITEM_COUNT) gInv[playerid][item] = quantity;
    }
    gInvLoaded[playerid] = true;
    return 1;
}

stock bool:Inv_Has(playerid, item, quantity = 1)
{
    return gInv[playerid][item] >= quantity;
}

stock Inv_Save(playerid, item)
{
    new query[200];
    if (gInv[playerid][item] > 0)
    {
        mysql_format(gDB, query, sizeof (query),
            "INSERT INTO character_items (character_id, item_id, quantity) VALUES (%d, %d, %d) ON DUPLICATE KEY UPDATE quantity = VALUES(quantity)",
            gChar[playerid][cId], item, gInv[playerid][item]);
    }
    else
    {
        mysql_format(gDB, query, sizeof (query), "DELETE FROM character_items WHERE character_id = %d AND item_id = %d", gChar[playerid][cId], item);
    }
    mysql_tquery(gDB, query);
}

// Returns false if it would go over the item's carry limit.
stock bool:Inv_Add(playerid, item, quantity)
{
    if (!gInvLoaded[playerid] || !(0 < item < ITEM_COUNT) || quantity <= 0) return false;
    if (gInv[playerid][item] + quantity > gItems[item][itMax]) return false;
    gInv[playerid][item] += quantity;
    Inv_Save(playerid, item);
    return true;
}

stock bool:Inv_Remove(playerid, item, quantity)
{
    if (!(0 < item < ITEM_COUNT) || quantity <= 0 || gInv[playerid][item] < quantity) return false;
    gInv[playerid][item] -= quantity;
    Inv_Save(playerid, item);
    return true;
}

stock Inv_Heal(playerid, Float:amount)
{
    new Float:health;
    GetPlayerHealth(playerid, health);
    health += amount;
    SetPlayerHealth(playerid, health > 100.0 ? 100.0 : health);
}

// Using an item. Returns true if one was used up.
stock bool:Inv_Use(playerid, item)
{
    switch (item)
    {
        case ITEM_PHONE:
        {
            if (gChar[playerid][cPhone] == 0) SendInfo(playerid, "Your phone is still being activated. Try again in a moment.");
            else SendClientMessage(playerid, COLOR_INFO, "Your number is %d. /sms [number] [text], /911 [emergency], /balance, /transfer.", gChar[playerid][cPhone]);
            return false;
        }
        case ITEM_WATER:
        {
            Inv_Heal(playerid, 10.0);
            Chat_Action(playerid, "drinks a bottle of water.");
        }
        case ITEM_BURGER:
        {
            Inv_Heal(playerid, 25.0);
            Chat_Action(playerid, "eats a burger.");
        }
        case ITEM_MEDKIT:
        {
            if (IsPlayerInAnyVehicle(playerid)) return SendError(playerid, "Not inside a vehicle."), false;
            Inv_Heal(playerid, 50.0);
            Chat_Action(playerid, "patches up their wounds with a medkit.");
        }
        case ITEM_FUELCAN:
        {
            new vehicleid = GetPlayerVehicleID(playerid);
            if (vehicleid == 0 || !Veh_UsesFuel(vehicleid)) return SendError(playerid, "Sit in your vehicle to pour the fuel in."), false;
            new Float:fuel = gVeh[vehicleid][vFuel] + 25.0;
            Veh_SetFuel(vehicleid, fuel > 100.0 ? 100.0 : fuel);
            Chat_Action(playerid, "pours a can of fuel into the tank.");
        }
        default: return false;
    }
    return Inv_Remove(playerid, item, 1);
}

CMD:inv(playerid, params[])
{
    return Inv_Show(playerid);
}

stock Inv_Show(playerid)
{
    if (!Player_IsPlaying(playerid)) return 1;
    new body[400], line[48], count = 0;
    body = "Item\tAmount\n";
    for (new i = 1; i < ITEM_COUNT; i++)
    {
        if (gInv[playerid][i] <= 0) continue;
        format(line, sizeof (line), "%s\t%d\n", gItems[i][itName], gInv[playerid][i]);
        strcat(body, line);
        count++;
    }
    if (count == 0) return SendInfo(playerid, "Your pockets are empty. 24/7 shops sell food, phones and more.");
    ShowPlayerDialog(playerid, DIALOG_INV, DIALOG_STYLE_TABLIST_HEADERS, "Inventory", body, "Select", "Close");
    return 1;
}

// The n-th item the player actually has, as listed in /inv.
stock Inv_ItemAt(playerid, listitem)
{
    for (new i = 1, n = 0; i < ITEM_COUNT; i++)
    {
        if (gInv[playerid][i] <= 0) continue;
        if (n == listitem) return i;
        n++;
    }
    return ITEM_NONE;
}

CMD:shop(playerid, params[])
{
    return Inv_ShowShop(playerid);
}

stock Inv_ShowShop(playerid)
{
    if (!Player_IsPlaying(playerid)) return 1;
    new slot = Prop_Inside(playerid);
    if (slot == -1 || gProp[slot][prType] != PROP_BUSINESS) return SendError(playerid, "You need to be inside a 24/7.");
    new body[400], line[48];
    body = "Item\tPrice\n";
    for (new i = 1; i < ITEM_COUNT; i++)
    {
        format(line, sizeof (line), "%s\t$%d\n", gItems[i][itName], gItems[i][itPrice]);
        strcat(body, line);
    }
    ShowPlayerDialog(playerid, DIALOG_SHOP, DIALOG_STYLE_TABLIST_HEADERS, gProp[slot][prName], body, "Buy", "Close");
    return 1;
}

stock Inv_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
    switch (dialogid)
    {
        case DIALOG_SHOP:
        {
            if (!response || !Player_IsPlaying(playerid)) return 1;
            new item = listitem + 1;
            new slot = Prop_Inside(playerid);
            if (!(0 < item < ITEM_COUNT) || slot == -1 || gProp[slot][prType] != PROP_BUSINESS) return 1;
            if (gInv[playerid][item] >= gItems[item][itMax]) return SendClientMessage(playerid, COLOR_ERROR, "[!] You can't carry more than %d.", gItems[item][itMax]), 1;
            new price = gItems[item][itPrice];
            if (!Money_Give(playerid, -price, 0, "shop", INVALID_PLAYER_ID, gItems[item][itName])) return SendError(playerid, "You don't have enough cash."), 1;
            if (!Inv_Add(playerid, item, 1))
            {
                Money_Give(playerid, price, 0, "shop_refund", INVALID_PLAYER_ID, gItems[item][itName]);
                return SendError(playerid, "Couldn't add that to your inventory."), 1;
            }
            Prop_AddToTill(slot, price);
            SendClientMessage(playerid, COLOR_SUCCESS, "You bought a %s for $%d.", gItems[item][itName], price);
            if (item == ITEM_PHONE && gChar[playerid][cPhone] == 0) Inv_AssignNumber(playerid);
            Inv_ShowShop(playerid);
            return 1;
        }
        case DIALOG_INV:
        {
            if (!response) return 1;
            new item = Inv_ItemAt(playerid, listitem);
            if (item == ITEM_NONE) return 1;
            gInvSelected[playerid] = item;
            ShowPlayerDialog(playerid, DIALOG_INV_ACTION, DIALOG_STYLE_LIST, gItems[item][itName], "Use\nGive to someone nearby\nThrow away", "OK", "Back");
            return 1;
        }
        case DIALOG_INV_ACTION:
        {
            new item = gInvSelected[playerid];
            if (!response || !Inv_Has(playerid, item)) return Inv_Show(playerid), 1;
            switch (listitem)
            {
                case 0: Inv_Use(playerid, item);
                case 1: ShowPlayerDialog(playerid, DIALOG_INV_GIVE, DIALOG_STYLE_INPUT, "Give item", EMBED_WHITE "Enter the player id and amount, e.g. " EMBED_BRAND "12 1", "Give", "Back");
                case 2:
                {
                    if (item == ITEM_PHONE) return SendError(playerid, "Keep your phone. You can give it to someone instead."), 1;
                    Inv_Remove(playerid, item, 1);
                    SendClientMessage(playerid, COLOR_INFO, "You threw away a %s.", gItems[item][itName]);
                }
            }
            return 1;
        }
        case DIALOG_INV_GIVE:
        {
            new item = gInvSelected[playerid];
            if (!response) return Inv_Show(playerid), 1;
            new targetid, quantity;
            if (sscanf(inputtext, "ud", targetid, quantity) || quantity <= 0) return SendError(playerid, "Enter a player id and an amount."), 1;
            if (!Player_IsPlaying(targetid) || targetid == playerid || !Player_IsNear(playerid, targetid, 3.0)) return SendError(playerid, "They need to be next to you."), 1;
            if (!Inv_Has(playerid, item, quantity)) return SendError(playerid, "You don't have that many."), 1;
            if (gInv[targetid][item] + quantity > gItems[item][itMax]) return SendError(playerid, "They can't carry that many."), 1;
            Inv_Remove(playerid, item, quantity);
            Inv_Add(targetid, item, quantity);
            if (item == ITEM_PHONE)
            {
                // The giver's number goes with their SIM; the phone gets a new one.
                gChar[playerid][cPhone] = 0;
                new query[96];
                mysql_format(gDB, query, sizeof (query), "UPDATE characters SET phone_number = NULL WHERE id = %d", gChar[playerid][cId]);
                mysql_tquery(gDB, query);
                if (gChar[targetid][cPhone] == 0) Inv_AssignNumber(targetid);
            }
            new action[64];
            format(action, sizeof (action), "hands %d %s to %s.", quantity, gItems[item][itName], Chat_Name(targetid));
            Chat_Action(playerid, action);
            return 1;
        }
    }
    return 0;
}

// Picks a random free 7-digit number. Retries if it's taken.
stock Inv_AssignNumber(playerid, attempt = 0)
{
    new number = 5550000 + random(4440000);
    new query[160];
    mysql_format(gDB, query, sizeof (query), "UPDATE IGNORE characters SET phone_number = %d WHERE id = %d", number, gChar[playerid][cId]);
    mysql_tquery(gDB, query, "Inv_OnPhoneNumber", "dddd", playerid, gPlayerRace[playerid], number, attempt);
}

public Inv_OnPhoneNumber(playerid, race, number, attempt)
{
    if (gPlayerRace[playerid] != race) return 1;
    if (cache_affected_rows() == 0)
    {
        if (attempt < 5) Inv_AssignNumber(playerid, attempt + 1);
        else print("[inventory] couldn't find a free phone number");
        return 1;
    }
    gChar[playerid][cPhone] = number;
    SendClientMessage(playerid, COLOR_INFO, "Your phone number is %d.", number);
    return 1;
}

stock Phone_FindNumber(number)
{
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (Player_IsPlaying(i) && gChar[i][cPhone] == number && Inv_Has(i, ITEM_PHONE)) return i;
    }
    return INVALID_PLAYER_ID;
}

CMD:number(playerid, params[])
{
    if (!Inv_Has(playerid, ITEM_PHONE)) return SendError(playerid, "You don't have a phone.");
    return Inv_Use(playerid, ITEM_PHONE), 1;
}

CMD:sms(playerid, params[])
{
    new number, text[128];
    if (sscanf(params, "ds[128]", number, text)) return SendInfo(playerid, "Usage: /sms [number] [text]");
    if (!Inv_Has(playerid, ITEM_PHONE) || gChar[playerid][cPhone] == 0) return SendError(playerid, "You need a phone.");
    if (!Chat_CanTalk(playerid, text)) return 1;
    new targetid = Phone_FindNumber(number);
    if (targetid == INVALID_PLAYER_ID) return SendError(playerid, "The message couldn't be delivered.");
    SendClientMessage(targetid, COLOR_YELLOW, "SMS from %d: %s", gChar[playerid][cPhone], text);
    KMUI_Sms(targetid, gChar[playerid][cPhone], text);
    SendClientMessage(playerid, COLOR_YELLOW, "SMS to %d: %s", number, text);
    Chat_Action(playerid, "types a message on their phone.");
    return 1;
}

CMD:911(playerid, params[])
{
    if (isnull(params)) return SendInfo(playerid, "Usage: /911 [what's happening and where]");
    if (!Inv_Has(playerid, ITEM_PHONE)) return SendError(playerid, "You need a phone.");
    if (!Chat_CanTalk(playerid, params)) return 1;
    new sent = Fac_Dispatch(playerid, params);
    if (sent == 0) return SendError(playerid, "Nobody answers. No police or medics are on duty.");
    SendClientMessage(playerid, COLOR_SUCCESS, "Dispatch: your call was passed to %d unit(s).", sent);
    return 1;
}
