// Houses and businesses (24/7 shops). Admins place them with
// /createproperty; players /buyproperty, /enter, /exit, /lock and
// /sellproperty. A shop's sales go into its till, which the owner empties
// with /till. Each property has its own virtual world inside.

enum E_INTERIOR { intType, intName[24], intId, Float:intX, Float:intY, Float:intZ, Float:intA }

stock const gInteriors[][E_INTERIOR] = {
    { PROP_HOUSE, "Small house", 1, 223.0, 1287.1, 1082.1, 0.0 },
    { PROP_HOUSE, "Medium house", 2, 226.3, 1240.0, 1082.1, 90.0 },
    { PROP_HOUSE, "Family house", 3, 235.3, 1186.7, 1080.3, 0.0 },
    { PROP_HOUSE, "Large house", 15, 295.1, 1472.3, 1080.3, 0.0 },
    { PROP_HOUSE, "Mansion", 7, 225.7, 1021.4, 1084.0, 0.0 },
    { PROP_BUSINESS, "24/7 small", 17, -25.9, -188.0, 1003.5, 0.0 },
    { PROP_BUSINESS, "24/7 large", 10, 6.1, -29.3, 1003.5, 0.0 }
};

enum E_PROP
{
    bool:prUsed,
    prId,
    prType,
    prName[48],
    prPrice,
    prOwner,                // character id, 0 = for sale
    prOwnerName[MAX_PLAYER_NAME],
    Float:prX,
    Float:prY,
    Float:prZ,
    Float:prA,
    prInterior,             // index into gInteriors
    bool:prLocked,
    prTill,
    prPickup,
    Text3D:prLabel
}

new gProp[KM_MAX_PROPERTIES][E_PROP];
new bool:gPropBuying[MAX_PLAYERS];  // a purchase is waiting for the database

forward Prop_OnLoaded();
forward Prop_OnCreated(playerid, race, slot);
forward Prop_OnBought(playerid, race, slot, price);

stock Prop_Load()
{
    mysql_tquery(gDB,
        "SELECT p.id, p.type, p.name, p.price, IFNULL(p.owner_character_id, 0) AS owner, IFNULL(c.name, '') AS owner_name, \
         p.ent_x, p.ent_y, p.ent_z, p.ent_a, p.interior_preset, p.locked, p.till \
         FROM properties p LEFT JOIN characters c ON c.id = p.owner_character_id ORDER BY p.id",
        "Prop_OnLoaded");
}

public Prop_OnLoaded()
{
    new rows = cache_num_rows();
    if (rows > KM_MAX_PROPERTIES)
    {
        printf("[property] %d properties in the database, only the first %d are loaded", rows, KM_MAX_PROPERTIES);
        rows = KM_MAX_PROPERTIES;
    }
    for (new i = 0; i < rows; i++)
    {
        new locked;
        gProp[i][prUsed] = true;
        cache_get_value_name_int(i, "id", gProp[i][prId]);
        cache_get_value_name_int(i, "type", gProp[i][prType]);
        cache_get_value_name(i, "name", gProp[i][prName], 48);
        cache_get_value_name_int(i, "price", gProp[i][prPrice]);
        cache_get_value_name_int(i, "owner", gProp[i][prOwner]);
        cache_get_value_name(i, "owner_name", gProp[i][prOwnerName], MAX_PLAYER_NAME);
        cache_get_value_name_float(i, "ent_x", gProp[i][prX]);
        cache_get_value_name_float(i, "ent_y", gProp[i][prY]);
        cache_get_value_name_float(i, "ent_z", gProp[i][prZ]);
        cache_get_value_name_float(i, "ent_a", gProp[i][prA]);
        cache_get_value_name_int(i, "interior_preset", gProp[i][prInterior]);
        cache_get_value_name_int(i, "locked", locked);
        cache_get_value_name_int(i, "till", gProp[i][prTill]);
        gProp[i][prLocked] = locked != 0;
        if (!(0 <= gProp[i][prInterior] < sizeof (gInteriors))) gProp[i][prInterior] = 0;
        Prop_CreateMarker(i);
    }
    printf("[property] loaded %d properties", rows);
    return 1;
}

stock Prop_CreateMarker(slot)
{
    new model;
    if (gProp[slot][prType] == PROP_BUSINESS) model = 1274;
    else model = gProp[slot][prOwner] == 0 ? 1273 : 1272;
    gProp[slot][prPickup] = CreatePickup(model, 1, gProp[slot][prX], gProp[slot][prY], gProp[slot][prZ], 0);
    gProp[slot][prLabel] = Create3DTextLabel(" ", COLOR_WHITE, gProp[slot][prX], gProp[slot][prY], gProp[slot][prZ] + 0.8, 12.0, 0, true);
    Prop_UpdateLabel(slot);
}

stock Prop_DestroyMarker(slot)
{
    DestroyPickup(gProp[slot][prPickup]);
    Delete3DTextLabel(gProp[slot][prLabel]);
}

stock Prop_UpdateLabel(slot)
{
    new text[192];
    if (gProp[slot][prOwner] == 0)
    {
        format(text, sizeof (text), EMBED_BRAND "%s\n" EMBED_WHITE "For sale: $%d\n%s\n/buyproperty",
            gProp[slot][prName], gProp[slot][prPrice], gInteriors[gProp[slot][prInterior]][intName]);
    }
    else
    {
        new owner[MAX_PLAYER_NAME];
        format(owner, sizeof (owner), "%s", gProp[slot][prOwnerName]);
        Chat_FixName(owner);
        format(text, sizeof (text), EMBED_BRAND "%s\n" EMBED_WHITE "Owner: %s\n%s",
            gProp[slot][prName], owner, gProp[slot][prType] == PROP_BUSINESS ? ("Open for business - /enter") : ("/enter"));
    }
    Update3DTextLabelText(gProp[slot][prLabel], COLOR_WHITE, text);
}

// Re-creates the pickup so its colour matches for-sale / owned.
stock Prop_Refresh(slot)
{
    Prop_DestroyMarker(slot);
    Prop_CreateMarker(slot);
}

stock Prop_World(slot)
{
    return KM_PROPERTY_WORLD_BASE + gProp[slot][prId];
}

// The property whose entrance the player is standing at, or -1.
stock Prop_AtEntrance(playerid)
{
    if (GetPlayerInterior(playerid) != 0 || GetPlayerVirtualWorld(playerid) != 0) return -1;
    for (new i = 0; i < KM_MAX_PROPERTIES; i++)
    {
        if (gProp[i][prUsed] && IsPlayerInRangeOfPoint(playerid, 2.5, gProp[i][prX], gProp[i][prY], gProp[i][prZ])) return i;
    }
    return -1;
}

// The property the player is inside, or -1.
stock Prop_Inside(playerid)
{
    new world = GetPlayerVirtualWorld(playerid);
    if (world <= KM_PROPERTY_WORLD_BASE) return -1;
    for (new i = 0; i < KM_MAX_PROPERTIES; i++)
    {
        if (gProp[i][prUsed] && Prop_World(i) == world) return i;
    }
    return -1;
}

stock Prop_CountOwned(characterId)
{
    new count = 0;
    for (new i = 0; i < KM_MAX_PROPERTIES; i++)
    {
        if (gProp[i][prUsed] && gProp[i][prOwner] == characterId) count++;
    }
    return count;
}

stock bool:Prop_IsOwner(playerid, slot)
{
    return gProp[slot][prOwner] != 0 && gProp[slot][prOwner] == gChar[playerid][cId];
}

stock Prop_Save(slot)
{
    new query[200];
    mysql_format(gDB, query, sizeof (query), "UPDATE properties SET owner_character_id = NULLIF(%d, 0), locked = %d, till = %d WHERE id = %d",
        gProp[slot][prOwner], gProp[slot][prLocked] ? 1 : 0, gProp[slot][prTill], gProp[slot][prId]);
    mysql_tquery(gDB, query);
}

// Used by /lock. Returns true if it handled the command.
stock bool:Prop_TryLock(playerid)
{
    new slot = Prop_AtEntrance(playerid);
    if (slot == -1) slot = Prop_Inside(playerid);
    if (slot == -1 || !Prop_IsOwner(playerid, slot)) return false;
    gProp[slot][prLocked] = !gProp[slot][prLocked];
    Prop_Save(slot);
    GameTextForPlayer(playerid, gProp[slot][prLocked] ? ("~r~Locked") : ("~g~Unlocked"), 1500, 4);
    Chat_Action(playerid, gProp[slot][prLocked] ? ("locks the door.") : ("unlocks the door."));
    return true;
}

// Money from shop sales goes into the till of the shop the player is in.
stock Prop_AddToTill(slot, amount)
{
    if (slot == -1 || gProp[slot][prOwner] == 0) return;
    gProp[slot][prTill] += amount;
    Prop_Save(slot);
}

// ---- commands ----------------------------------------------------------

CMD:enter(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    new slot = Prop_AtEntrance(playerid);
    if (slot == -1) return SendError(playerid, "There's no door here.");
    if (gChar[playerid][cCuffed]) return SendError(playerid, "You can't do that while cuffed.");
    new bool:open = gProp[slot][prType] == PROP_BUSINESS && gProp[slot][prOwner] != 0;
    if (gProp[slot][prLocked] && !Prop_IsOwner(playerid, slot) && !open) return GameTextForPlayer(playerid, "~r~Locked", 1500, 4), 1;
    if (gProp[slot][prOwner] == 0 && gProp[slot][prType] == PROP_HOUSE)
    {
        // Viewing a house for sale is fine.
        SendInfo(playerid, "You're viewing a house for sale. /exit to leave.");
    }
    new p = gProp[slot][prInterior];
    SetPlayerInterior(playerid, gInteriors[p][intId]);
    SetPlayerVirtualWorld(playerid, Prop_World(slot));
    SetPlayerPos(playerid, gInteriors[p][intX], gInteriors[p][intY], gInteriors[p][intZ]);
    SetPlayerFacingAngle(playerid, gInteriors[p][intA]);
    SetCameraBehindPlayer(playerid);
    if (gProp[slot][prType] == PROP_BUSINESS) SendInfo(playerid, "Welcome! Type /shop to see what's for sale.");
    return 1;
}

CMD:exit(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    new slot = Prop_Inside(playerid);
    if (slot == -1) return SendError(playerid, "You're not inside a property.");
    new p = gProp[slot][prInterior];
    if (!IsPlayerInRangeOfPoint(playerid, 3.0, gInteriors[p][intX], gInteriors[p][intY], gInteriors[p][intZ])) return SendError(playerid, "Go back to the door first.");
    SetPlayerInterior(playerid, 0);
    SetPlayerVirtualWorld(playerid, 0);
    SetPlayerPos(playerid, gProp[slot][prX], gProp[slot][prY], gProp[slot][prZ]);
    SetPlayerFacingAngle(playerid, gProp[slot][prA]);
    SetCameraBehindPlayer(playerid);
    return 1;
}

CMD:buyproperty(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    new slot = Prop_AtEntrance(playerid);
    if (slot == -1) return SendError(playerid, "Stand at the door of a property for sale.");
    if (gProp[slot][prOwner] != 0) return SendError(playerid, "This property isn't for sale.");
    if (Prop_CountOwned(gChar[playerid][cId]) >= KM_MAX_OWNED_PROPERTIES) return SendClientMessage(playerid, COLOR_ERROR, "[!] You can own at most %d properties.", KM_MAX_OWNED_PROPERTIES);
    if (gPropBuying[playerid]) return SendError(playerid, "Your last purchase is still going through.");
    new price = gProp[slot][prPrice];
    if (gChar[playerid][cBank] + gChar[playerid][cCash] < price) return SendClientMessage(playerid, COLOR_ERROR, "[!] You need $%d (bank and cash).", price);

    // Claim it in the database first, so two buyers can't both get it, then
    // pay in Prop_OnBought. A failed claim never costs money.
    gPropBuying[playerid] = true;
    new query[160];
    mysql_format(gDB, query, sizeof (query), "UPDATE properties SET owner_character_id = %d, locked = 1 WHERE id = %d AND owner_character_id IS NULL",
        gChar[playerid][cId], gProp[slot][prId]);
    mysql_tquery(gDB, query, "Prop_OnBought", "dddd", playerid, gPlayerRace[playerid], slot, price);
    return 1;
}

public Prop_OnBought(playerid, race, slot, price)
{
    new claimed = cache_affected_rows();
    new bool:present = gPlayerRace[playerid] == race;
    if (present) gPropBuying[playerid] = false;
    if (claimed == 0)
    {
        if (present) SendError(playerid, "Someone else just bought it. You weren't charged.");
        return 1;
    }
    if (!present || !Money_Charge(playerid, price, "property_buy", gProp[slot][prName]))
    {
        // Left before paying, or can't pay any more: put it back on the market.
        new query[128];
        mysql_format(gDB, query, sizeof (query), "UPDATE properties SET owner_character_id = NULL, locked = 0 WHERE id = %d", gProp[slot][prId]);
        mysql_tquery(gDB, query);
        if (present) SendError(playerid, "You no longer have enough money. You weren't charged.");
        return 1;
    }
    gProp[slot][prOwner] = gChar[playerid][cId];
    format(gProp[slot][prOwnerName], MAX_PLAYER_NAME, "%s", gChar[playerid][cName]);
    gProp[slot][prLocked] = true;
    gProp[slot][prTill] = 0;
    Prop_Refresh(slot);
    SendClientMessage(playerid, COLOR_SUCCESS, "You bought %s for $%d. /lock locks and unlocks the door.", gProp[slot][prName], price);
    return 1;
}

CMD:sellproperty(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    new slot = Prop_AtEntrance(playerid);
    if (slot == -1 || !Prop_IsOwner(playerid, slot)) return SendError(playerid, "Stand at the door of a property you own.");
    new refund = gProp[slot][prPrice] / 100 * KM_PROPERTY_SELL_PERCENT;
    if (strcmp(params, "confirm", true) != 0)
    {
        SendClientMessage(playerid, COLOR_INFO, "The state will pay $%d (plus $%d in the till). Type /sellproperty confirm.", refund, gProp[slot][prTill]);
        return 1;
    }
    new till = gProp[slot][prTill];
    gProp[slot][prOwner] = 0;
    gProp[slot][prOwnerName][0] = '\0';
    gProp[slot][prLocked] = false;
    gProp[slot][prTill] = 0;
    Prop_Save(slot);
    Prop_Refresh(slot);
    Money_Give(playerid, 0, refund + till, "property_sell", INVALID_PLAYER_ID, gProp[slot][prName]);
    SendClientMessage(playerid, COLOR_SUCCESS, "Sold. $%d was paid into your bank account.", refund + till);
    return 1;
}

CMD:till(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    new slot = Prop_Inside(playerid);
    if (slot == -1 || gProp[slot][prType] != PROP_BUSINESS || !Prop_IsOwner(playerid, slot)) return SendError(playerid, "You need to be inside your own business.");
    new amount = gProp[slot][prTill];
    if (amount <= 0) return SendInfo(playerid, "The till is empty.");
    gProp[slot][prTill] = 0;
    Prop_Save(slot);
    Money_Give(playerid, amount, 0, "business_till", INVALID_PLAYER_ID, gProp[slot][prName]);
    SendClientMessage(playerid, COLOR_SUCCESS, "You took $%d from the till.", amount);
    return 1;
}

CMD:myproperties(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    new body[512], line[96], count = 0;
    body = "Property\tType\tTill\n";
    for (new i = 0; i < KM_MAX_PROPERTIES; i++)
    {
        if (!gProp[i][prUsed] || !Prop_IsOwner(playerid, i)) continue;
        format(line, sizeof (line), "%s\t%s\t$%d\n", gProp[i][prName], gInteriors[gProp[i][prInterior]][intName], gProp[i][prTill]);
        strcat(body, line);
        count++;
    }
    if (count == 0) return SendInfo(playerid, "You don't own any property. Look for green house icons, or ask an estate agent.");
    ShowPlayerDialog(playerid, DIALOG_INFO, DIALOG_STYLE_TABLIST_HEADERS, "Your property", body, "Close", "");
    return 1;
}

// ---- admin ---------------------------------------------------------------

CMD:createproperty(playerid, params[])
{
    if (!Mod_CanAct(playerid, INVALID_PLAYER_ID, ADMIN_LEAD)) return 1;
    new preset, price, name[48];
    if (sscanf(params, "dds[48]", preset, price, name))
    {
        SendInfo(playerid, "Usage: /createproperty [interior] [price] [name]. Stand where the door should be.");
        new line[64];
        for (new i = 0; i < sizeof (gInteriors); i++)
        {
            format(line, sizeof (line), "  %d: %s (%s)", i, gInteriors[i][intName], gInteriors[i][intType] == PROP_HOUSE ? ("house") : ("business"));
            SendClientMessage(playerid, COLOR_GREY, "%s", line);
        }
        return 1;
    }
    if (!(0 <= preset < sizeof (gInteriors))) return SendError(playerid, "Unknown interior. Type /createproperty to list them.");
    if (!(1 <= price <= 50000000)) return SendError(playerid, "Price must be between $1 and $50,000,000.");
    if (GetPlayerInterior(playerid) != 0 || GetPlayerVirtualWorld(playerid) != 0) return SendError(playerid, "Doors have to be outside.");
    new slot = -1;
    for (new i = 0; i < KM_MAX_PROPERTIES; i++)
    {
        if (!gProp[i][prUsed]) { slot = i; break; }
    }
    if (slot == -1) return SendError(playerid, "The property limit is reached.");

    new Float:x, Float:y, Float:z, Float:a;
    GetPlayerPos(playerid, x, y, z);
    GetPlayerFacingAngle(playerid, a);
    gProp[slot][prUsed] = true; // reserved until the insert returns
    gProp[slot][prId] = 0;
    gProp[slot][prType] = gInteriors[preset][intType];
    format(gProp[slot][prName], 48, "%s", name);
    gProp[slot][prPrice] = price;
    gProp[slot][prOwner] = 0;
    gProp[slot][prOwnerName][0] = '\0';
    gProp[slot][prX] = x;
    gProp[slot][prY] = y;
    gProp[slot][prZ] = z;
    gProp[slot][prA] = a + 180.0;
    gProp[slot][prInterior] = preset;
    gProp[slot][prLocked] = false;
    gProp[slot][prTill] = 0;

    new query[400];
    mysql_format(gDB, query, sizeof (query),
        "INSERT INTO properties (type, name, price, ent_x, ent_y, ent_z, ent_a, interior_preset) VALUES (%d, '%e', %d, %f, %f, %f, %f, %d)",
        gProp[slot][prType], name, price, x, y, z, gProp[slot][prA], preset);
    mysql_tquery(gDB, query, "Prop_OnCreated", "ddd", playerid, gPlayerRace[playerid], slot);
    return 1;
}

public Prop_OnCreated(playerid, race, slot)
{
    gProp[slot][prId] = cache_insert_id();
    if (gProp[slot][prId] <= 0)
    {
        gProp[slot][prUsed] = false;
        if (gPlayerRace[playerid] == race) SendError(playerid, "The property couldn't be saved.");
        return 1;
    }
    Prop_CreateMarker(slot);
    if (gProp[slot][prType] == PROP_BUSINESS) MapIcons_RefreshShops();
    new details[80];
    format(details, sizeof (details), "property #%d %s $%d", gProp[slot][prId], gProp[slot][prName], gProp[slot][prPrice]);
    Log_Admin(gPlayerRace[playerid] == race ? Mod_ActorId(playerid) : 0, "create_property", 0, details);
    if (gPlayerRace[playerid] == race) SendClientMessage(playerid, COLOR_ADMIN, "Created property #%d.", gProp[slot][prId]);
    return 1;
}

CMD:deleteproperty(playerid, params[])
{
    if (!Mod_CanAct(playerid, INVALID_PLAYER_ID, ADMIN_LEAD)) return 1;
    new slot = Prop_AtEntrance(playerid);
    if (slot == -1) return SendError(playerid, "Stand at the property's door.");
    if (gProp[slot][prId] == 0) return SendError(playerid, "That property is still being created.");
    if (gProp[slot][prOwner] != 0 && strcmp(params, "confirm", true) != 0)
    {
        return SendError(playerid, "Someone owns this. They won't be refunded. Type /deleteproperty confirm to delete it anyway.");
    }
    new query[96], details[80];
    mysql_format(gDB, query, sizeof (query), "DELETE FROM properties WHERE id = %d", gProp[slot][prId]);
    mysql_tquery(gDB, query);
    format(details, sizeof (details), "property #%d %s, owner character #%d", gProp[slot][prId], gProp[slot][prName], gProp[slot][prOwner]);
    Log_Admin(Mod_ActorId(playerid), "delete_property", 0, details);
    Prop_DestroyMarker(slot);
    new bool:wasShop = gProp[slot][prType] == PROP_BUSINESS;
    gProp[slot][prUsed] = false;
    if (wasShop) MapIcons_RefreshShops();
    SendClientMessage(playerid, COLOR_ADMIN, "Property deleted.");
    return 1;
}
