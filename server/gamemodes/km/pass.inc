// Season pass: points for playing (paydays, daily rewards, job pay) unlock
// tiers; /pass shows them and claims the next one. Each season (KM_PASS_SEASON)
// keeps its own progress, so bumping the number starts a new season for all.

// Season number, tiers and points: config.inc (KM_PASS_*, PASS_POINTS_*).

forward Pass_OnLoad(playerid, race, charId);
forward Pass_OnClaim(playerid, race, charId, tier);

static gPassPoints[MAX_PLAYERS];
static gPassClaimed[MAX_PLAYERS];
static bool:gPassLoaded[MAX_PLAYERS];
static bool:gPassBusy[MAX_PLAYERS];

// Cash for tier 1..KM_PASS_TIERS: $250, $500 ... and $10000 for the last.
stock Pass_TierReward(tier)
{
    if (tier < 1 || tier > KM_PASS_TIERS) return 0;
    if (tier == KM_PASS_TIERS) return 10000;
    return 250 * tier;
}

// How many tiers these points unlock.
stock Pass_TiersUnlocked(points)
{
    new tiers = points / KM_PASS_TIER_POINTS;
    return tiers > KM_PASS_TIERS ? KM_PASS_TIERS : (tiers < 0 ? 0 : tiers);
}

stock Pass_Reset(playerid)
{
    gPassPoints[playerid] = 0;
    gPassClaimed[playerid] = 0;
    gPassLoaded[playerid] = false;
    gPassBusy[playerid] = false;
}

// From Char_OnSpawn on the first spawn.
stock Pass_Load(playerid)
{
    Pass_Reset(playerid);
    new query[160];
    mysql_format(gDB, query, sizeof (query),
        "SELECT points, claimed_tier FROM pass_progress WHERE character_id = %d AND season = %d",
        gChar[playerid][cId], KM_PASS_SEASON);
    mysql_tquery(gDB, query, "Pass_OnLoad", "ddd", playerid, gPlayerRace[playerid], gChar[playerid][cId]);
}

public Pass_OnLoad(playerid, race, charId)
{
    if (gPlayerRace[playerid] != race || gChar[playerid][cId] != charId) return 1;
    if (cache_num_rows())
    {
        cache_get_value_name_int(0, "points", gPassPoints[playerid]);
        cache_get_value_name_int(0, "claimed_tier", gPassClaimed[playerid]);
    }
    gPassLoaded[playerid] = true;
    if (Pass_TiersUnlocked(gPassPoints[playerid]) > gPassClaimed[playerid])
        SendClientMessage(playerid, COLOR_BRAND, "You have season pass rewards to claim. Type /pass.");
    return 1;
}

// Adds season pass points (saved at once) and says when a tier unlocks.
stock Pass_AddPoints(playerid, points, bool:eventBoost = true)
{
    if (points <= 0 || gChar[playerid][cId] == 0) return;
    if (eventBoost) points *= Event_Multiplier(EVENT_DOUBLEPASS);
    new before = Pass_TiersUnlocked(gPassPoints[playerid]);
    gPassPoints[playerid] += points;
    new query[256];
    mysql_format(gDB, query, sizeof (query),
        "INSERT INTO pass_progress (character_id, season, points) VALUES (%d, %d, %d) \
         ON DUPLICATE KEY UPDATE points = points + VALUES(points)",
        gChar[playerid][cId], KM_PASS_SEASON, points);
    mysql_tquery(gDB, query);
    new after = Pass_TiersUnlocked(gPassPoints[playerid]);
    if (gPassLoaded[playerid]) Pass_Send(playerid, 0);   // refreshes the app's panel if open
    if (gPassLoaded[playerid] && after > before)
    {
        SendClientMessage(playerid, COLOR_SUCCESS, "Season pass: tier %d unlocked! Claim it with /pass.", after);
        new toast[48];
        format(toast, sizeof (toast), "Season pass tier %d unlocked", after);
        KMUI_Toast(playerid, toast);
    }
}

// Kingmakers app: the pass panel (KMUI "pass" event, docs/KMUI.md). open=1
// opens it; open=0 only refreshes it if it's already open.
stock Pass_Send(playerid, open)
{
    new payload[160], rewards[96];
    for (new t = 1; t <= KM_PASS_TIERS; t++)
    {
        format(rewards, sizeof (rewards), "%s%s%d", rewards, t > 1 ? "," : "", Pass_TierReward(t));
    }
    format(payload, sizeof (payload), "s=%d;p=%d;c=%d;per=%d;open=%d;r=%s",
        KM_PASS_SEASON, gPassPoints[playerid], gPassClaimed[playerid], KM_PASS_TIER_POINTS, open, rewards);
    KMUI_Send(playerid, "pass", payload);
}

stock Pass_Show(playerid)
{
    new list[1024], line[80];
    new unlocked = Pass_TiersUnlocked(gPassPoints[playerid]);
    for (new t = 1; t <= KM_PASS_TIERS; t++)
    {
        if (t <= gPassClaimed[playerid])
            format(line, sizeof (line), "Tier %d: $%d  {7FD17F}claimed", t, Pass_TierReward(t));
        else if (t <= unlocked)
            format(line, sizeof (line), "Tier %d: $%d  {D4AF37}tap to claim", t, Pass_TierReward(t));
        else
            format(line, sizeof (line), "Tier %d: $%d  {9A9A9A}%d points", t, Pass_TierReward(t), t * KM_PASS_TIER_POINTS);
        if (t > 1) strcat(list, "\n");
        strcat(list, line);
    }
    new title[64];
    format(title, sizeof (title), "Season %d pass | %d points", KM_PASS_SEASON, gPassPoints[playerid]);
    ShowPlayerDialog(playerid, DIALOG_PASS, DIALOG_STYLE_LIST, title, list, "Claim", "Close");
}

// "/pass" opens the pass (a panel in the Kingmakers app, a list elsewhere);
// "/pass claim" claims the next unlocked tier (the app's Claim button).
CMD:pass(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    if (!gPassLoaded[playerid]) return SendInfo(playerid, "Your season pass is still loading. Try again in a moment.");
    if (!isnull(params) && strcmp(params, "claim", true) == 0)
    {
        Pass_ClaimNext(playerid);
        return 1;
    }
    if (KMUI_Enabled(playerid)) Pass_Send(playerid, 1);
    else Pass_Show(playerid);
    return 1;
}

// Claims the next tier if it's unlocked.
stock Pass_ClaimNext(playerid)
{
    new tier = gPassClaimed[playerid] + 1;
    if (tier > KM_PASS_TIERS) return SendInfo(playerid, "You've claimed every tier this season."), 0;
    if (tier > Pass_TiersUnlocked(gPassPoints[playerid]))
        return SendClientMessage(playerid, COLOR_INFO, "Tier %d needs %d points; you have %d.", tier, tier * KM_PASS_TIER_POINTS, gPassPoints[playerid]), 0;
    if (gPassBusy[playerid]) return 0;
    gPassBusy[playerid] = true;
    new query[256];
    mysql_format(gDB, query, sizeof (query),
        "UPDATE pass_progress SET claimed_tier = claimed_tier + 1 \
         WHERE character_id = %d AND season = %d AND claimed_tier = %d AND points >= %d",
        gChar[playerid][cId], KM_PASS_SEASON, tier - 1, tier * KM_PASS_TIER_POINTS);
    mysql_tquery(gDB, query, "Pass_OnClaim", "dddd", playerid, gPlayerRace[playerid], gChar[playerid][cId], tier);
    return 1;
}

stock Pass_OnDialog(playerid, dialogid, response, listitem)
{
    if (dialogid != DIALOG_PASS) return 0;
    if (!response || !Player_IsPlaying(playerid)) return 1;
    new tier = listitem + 1;
    if (tier <= gPassClaimed[playerid]) return SendClientMessage(playerid, COLOR_INFO, "You've already claimed tier %d.", tier), 1;
    if (tier > Pass_TiersUnlocked(gPassPoints[playerid]))
        return SendClientMessage(playerid, COLOR_INFO, "Tier %d needs %d points; you have %d.", tier, tier * KM_PASS_TIER_POINTS, gPassPoints[playerid]), 1;
    // Tiers are claimed in order; tapping a later one claims the next one first.
    Pass_ClaimNext(playerid);
    return 1;
}

public Pass_OnClaim(playerid, race, charId, tier)
{
    if (gPlayerRace[playerid] != race || gChar[playerid][cId] != charId) return 1;
    gPassBusy[playerid] = false;
    if (cache_affected_rows() == 0) return SendError(playerid, "That tier couldn't be claimed. Try /pass again."), 1;
    gPassClaimed[playerid] = tier;
    new reward = Pass_TierReward(tier), details[32];
    format(details, sizeof (details), "season %d tier %d", KM_PASS_SEASON, tier);
    Money_Give(playerid, reward, 0, "pass", INVALID_PLAYER_ID, details);
    SendClientMessage(playerid, COLOR_SUCCESS, "Season pass tier %d claimed: $%d.", tier, reward);
    if (KMUI_Enabled(playerid)) Pass_Send(playerid, 0);
    else if (tier < Pass_TiersUnlocked(gPassPoints[playerid])) Pass_Show(playerid);
    return 1;
}

// Lead admins: give or take season pass points (events, prizes).
CMD:givepass(playerid, params[])
{
    new targetid, points;
    if (sscanf(params, "ud", targetid, points)) return SendInfo(playerid, "Usage: /givepass [player] [points]");
    if (!Mod_CanAct(playerid, INVALID_PLAYER_ID, ADMIN_LEAD)) return 1;
    if (!Player_IsPlaying(targetid)) return SendError(playerid, "That player isn't playing.");
    if (!(1 <= points <= 1000)) return SendError(playerid, "Points must be 1 to 1000.");
    Pass_AddPoints(targetid, points, false);
    new details[32];
    format(details, sizeof (details), "%d points", points);
    Log_Admin(Mod_ActorId(playerid), "give_pass", gPlayer[targetid][pAccountId], details);
    SendClientMessage(playerid, COLOR_ADMIN, "Gave %s %d season pass points (%d now).", gChar[targetid][cName], points, gPassPoints[targetid]);
    return 1;
}
