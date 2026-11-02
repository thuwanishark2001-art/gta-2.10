// Binco (gClothesShop): /clothes <skin> buys a new everyday outfit for
// KM_CLOTHES_PRICE. Any skin every copy of the game has (and the server's own
// custom skins), except the uniforms factions wear on duty.

stock bool:Clothes_IsUniform(skin)
{
    switch (skin)
    {
        case 71, 163, 164, 165, 166, 265, 266, 267, 274, 275, 276, 277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287, 288:
            return true;
    }
    return false;
}

stock bool:Clothes_CanWear(skin)
{
    return IsStockSkin(skin) && !Clothes_IsUniform(skin);
}

CMD:clothes(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    new skin;
    if (sscanf(params, "d", skin))
        return SendInfo(playerid, "Usage: /clothes [skin id]. Browse ids on a skin list online; uniforms aren't for sale.");
    if (!Place_Near(playerid, gClothesShop, 6.0)) return SendError(playerid, "You need to be at Binco in Ganton (/gps).");
    if (!Clothes_CanWear(skin)) return SendError(playerid, "That outfit isn't for sale. Pick another id (0-299).");
    if (skin == gChar[playerid][cSkin]) return SendError(playerid, "You're already wearing that.");
    if (!Money_Charge(playerid, KM_CLOTHES_PRICE, "clothes")) return SendClientMessage(playerid, COLOR_ERROR, "[!] New clothes cost $%d.", KM_CLOTHES_PRICE), 1;
    gChar[playerid][cSkin] = skin;
    Char_ApplySkin(playerid);
    new query[96];
    mysql_format(gDB, query, sizeof (query), "UPDATE characters SET skin = %d WHERE id = %d", skin, gChar[playerid][cId]);
    mysql_tquery(gDB, query);
    Chat_Action(playerid, "changes into new clothes.");
    SendClientMessage(playerid, COLOR_SUCCESS, "New outfit: skin %d ($%d).%s", skin, KM_CLOTHES_PRICE,
        gChar[playerid][cOnDuty] ? (" You'll wear it when you go off duty.") : (""));
    return 1;
}
