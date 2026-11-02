// Store robbery: /robstore inside a 24/7, armed with a gun. Stay inside for
// KM_ROB_SECONDS and walk off with KM_ROB_MIN-KM_ROB_MAX in cash and wanted
// level 3. On-duty police are alerted at once. Each shop can be robbed once
// every KM_ROB_SHOP_COOLDOWN, each player once every KM_ROB_PLAYER_COOLDOWN.

static gRobShop[MAX_PLAYERS] = { -1, ... };     // property slot being robbed
static gRobEnds[MAX_PLAYERS];
static gRobNext[MAX_PLAYERS];
static gShopRobbedAt[KM_MAX_PROPERTIES];

stock Rob_Reset(playerid)
{
    gRobShop[playerid] = -1;
    gRobNext[playerid] = 0;
}

// Guns only: pistols, shotguns, SMGs, rifles (22-34).
stock bool:Rob_IsGun(weapon)
{
    return 22 <= weapon <= 34;
}

stock Rob_Payout(roll)  // roll 0..(max-min)
{
    return KM_ROB_MIN + Clamp(roll, 0, KM_ROB_MAX - KM_ROB_MIN);
}

static Rob_AlertPolice(playerid, slot, const what[])
{
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (Fac_IsOnDutyType(i, FACTION_POLICE))
            SendClientMessage(i, COLOR_DEPARTMENT, "[Dispatch] %s at %s: suspect %s.", what, gProp[slot][prName], gChar[playerid][cName]);
    }
}

CMD:robstore(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    new slot = Prop_Inside(playerid);
    if (slot == -1 || gProp[slot][prType] != PROP_BUSINESS) return SendError(playerid, "You need to be inside a 24/7.");
    if (gRobShop[playerid] != -1) return SendError(playerid, "You're already robbing it. Stay inside!");
    if (gChar[playerid][cLevel] < KM_ROB_LEVEL) return SendClientMessage(playerid, COLOR_ERROR, "[!] You need to be level %d.", KM_ROB_LEVEL), 1;
    if (Fac_Type(gChar[playerid][cFaction]) == FACTION_POLICE) return SendError(playerid, "Police officers don't rob shops.");
    if (Prop_IsOwner(playerid, slot)) return SendError(playerid, "That's your own shop.");
    if (!Rob_IsGun(GetPlayerWeapon(playerid))) return SendError(playerid, "The clerk laughs at you. Hold a gun.");
    new now = gettime();
    if (now < gRobNext[playerid]) return SendClientMessage(playerid, COLOR_ERROR, "[!] Lie low for %d more minutes first.", (gRobNext[playerid] - now + 59) / 60), 1;
    if (now < gShopRobbedAt[slot] + KM_ROB_SHOP_COOLDOWN) return SendError(playerid, "This shop was robbed recently; the till is empty.");
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (gRobShop[i] == slot) return SendError(playerid, "Someone is already robbing this shop.");
    }
    gRobShop[playerid] = slot;
    gRobEnds[playerid] = now + KM_ROB_SECONDS;
    gShopRobbedAt[slot] = now;
    new query[96];
    mysql_format(gDB, query, sizeof (query), "UPDATE properties SET last_robbed_at = UTC_TIMESTAMP() WHERE id = %d", gProp[slot][prId]);
    mysql_tquery(gDB, query);
    Chat_Action(playerid, "points a gun at the clerk and demands the money.");
    SendClientMessage(playerid, COLOR_ERROR, "Robbery started! Stay inside for %d seconds.", KM_ROB_SECONDS);
    Rob_AlertPolice(playerid, slot, "Armed robbery in progress");
    return 1;
}

// From KM_Tick, for each playing character.
stock Rob_Tick(playerid)
{
    new slot = gRobShop[playerid];
    if (slot == -1) return;
    if (GetPlayerState(playerid) == PLAYER_STATE_WASTED || gChar[playerid][cCuffed] || Prop_Inside(playerid) != slot)
    {
        gRobShop[playerid] = -1;
        gRobNext[playerid] = gettime() + KM_ROB_PLAYER_COOLDOWN;
        Fac_SetWanted(playerid, gChar[playerid][cWanted] + 1);
        SendClientMessage(playerid, COLOR_ERROR, "The robbery failed: you left without the money.");
        return;
    }
    new left = gRobEnds[playerid] - gettime();
    if (left > 0)
    {
        new text[24];
        format(text, sizeof (text), "~r~Robbing: %d", left);
        GameTextForPlayer(playerid, text, 1100, 4);
        return;
    }
    gRobShop[playerid] = -1;
    gRobNext[playerid] = gettime() + KM_ROB_PLAYER_COOLDOWN;
    new loot = Rob_Payout(random(KM_ROB_MAX - KM_ROB_MIN + 1));
    Money_Give(playerid, loot, 0, "robbery", INVALID_PLAYER_ID, gProp[slot][prName]);
    Fac_SetWanted(playerid, gChar[playerid][cWanted] + 3);
    SendClientMessage(playerid, COLOR_SUCCESS, "You got away with $%d. The police are looking for you (wanted %d).", loot, gChar[playerid][cWanted]);
    Rob_AlertPolice(playerid, slot, "Robbery completed");
}

// Lead admins: reset all shop cooldowns (events, testing).
stock Rob_ClearCooldowns()
{
    for (new i = 0; i < KM_MAX_PROPERTIES; i++) gShopRobbedAt[i] = 0;
}
