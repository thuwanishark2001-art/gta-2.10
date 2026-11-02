// Daily rewards: /daily once every 20 hours pays cash for the day of a
// 7-day streak (day 7 is the big one, then it starts over). Missing more than
// 48 hours resets the streak to day 1. The database decides, in one UPDATE,
// whether a claim is allowed and what day it is, so two quick /daily can't
// both pay.

#define DAILY_COOLDOWN_HOURS    (20)
#define DAILY_RESET_HOURS       (48)
#define DAILY_DAYS              (7)

forward Daily_OnClaim(playerid, race, charId);
forward Daily_OnStreak(playerid, race, charId);
forward Daily_OnWait(playerid, race);
forward Daily_OnCheck(playerid, race);

static bool:gDailyBusy[MAX_PLAYERS];

// Cash for day 1..7 of the streak.
stock Daily_Reward(day)
{
    if (day < 1 || day > DAILY_DAYS) return 0;
    if (day == DAILY_DAYS) return 3000;
    return 200 * day;
}

stock Daily_Reset(playerid)
{
    gDailyBusy[playerid] = false;
}

CMD:daily(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    if (gDailyBusy[playerid]) return 1;
    gDailyBusy[playerid] = true;
    new query[512];
    mysql_format(gDB, query, sizeof (query),
        "UPDATE characters SET \
            daily_streak = IF(daily_last_claim IS NOT NULL AND daily_last_claim > UTC_TIMESTAMP() - INTERVAL %d HOUR, MOD(daily_streak, %d) + 1, 1), \
            daily_last_claim = UTC_TIMESTAMP() \
         WHERE id = %d AND (daily_last_claim IS NULL OR daily_last_claim <= UTC_TIMESTAMP() - INTERVAL %d HOUR)",
        DAILY_RESET_HOURS, DAILY_DAYS, gChar[playerid][cId], DAILY_COOLDOWN_HOURS);
    mysql_tquery(gDB, query, "Daily_OnClaim", "ddd", playerid, gPlayerRace[playerid], gChar[playerid][cId]);
    return 1;
}

public Daily_OnClaim(playerid, race, charId)
{
    if (gPlayerRace[playerid] != race || gChar[playerid][cId] != charId) return 1;
    new query[256];
    if (cache_affected_rows() == 0)
    {
        mysql_format(gDB, query, sizeof (query),
            "SELECT GREATEST(1, TIMESTAMPDIFF(MINUTE, UTC_TIMESTAMP(), daily_last_claim + INTERVAL %d HOUR)) AS wait FROM characters WHERE id = %d",
            DAILY_COOLDOWN_HOURS, charId);
        mysql_tquery(gDB, query, "Daily_OnWait", "dd", playerid, race);
        return 1;
    }
    mysql_format(gDB, query, sizeof (query), "SELECT daily_streak FROM characters WHERE id = %d", charId);
    mysql_tquery(gDB, query, "Daily_OnStreak", "ddd", playerid, race, charId);
    return 1;
}

public Daily_OnStreak(playerid, race, charId)
{
    if (gPlayerRace[playerid] != race || gChar[playerid][cId] != charId) return 1;
    gDailyBusy[playerid] = false;
    new day = 1;
    if (cache_num_rows()) cache_get_value_name_int(0, "daily_streak", day);
    new reward = Daily_Reward(day);
    new details[16];
    format(details, sizeof (details), "day %d", day);
    Money_Give(playerid, reward, 0, "daily", INVALID_PLAYER_ID, details);
    if (day == DAILY_DAYS)
        SendClientMessage(playerid, COLOR_SUCCESS, "Daily reward, day 7 of 7: $%d! Your streak starts over tomorrow.", reward);
    else
        SendClientMessage(playerid, COLOR_SUCCESS, "Daily reward, day %d of 7: $%d. Come back tomorrow for $%d.", day, reward, Daily_Reward(day + 1));
    return 1;
}

public Daily_OnWait(playerid, race)
{
    if (gPlayerRace[playerid] != race) return 1;
    gDailyBusy[playerid] = false;
    new wait = 0;
    if (cache_num_rows()) cache_get_value_name_int(0, "wait", wait);
    SendClientMessage(playerid, COLOR_INFO, "You've already claimed today's reward. The next one is ready in %dh %02dm.", wait / 60, wait % 60);
    return 1;
}

// From Char_OnSpawn on the first spawn: say so if a reward is waiting.
stock Daily_OnFirstSpawn(playerid)
{
    new query[256];
    mysql_format(gDB, query, sizeof (query),
        "SELECT 1 FROM characters WHERE id = %d AND (daily_last_claim IS NULL OR daily_last_claim <= UTC_TIMESTAMP() - INTERVAL %d HOUR)",
        gChar[playerid][cId], DAILY_COOLDOWN_HOURS);
    mysql_tquery(gDB, query, "Daily_OnCheck", "dd", playerid, gPlayerRace[playerid]);
}

public Daily_OnCheck(playerid, race)
{
    if (gPlayerRace[playerid] != race || !cache_num_rows()) return 1;
    SendClientMessage(playerid, COLOR_BRAND, "Your daily reward is ready. Type /daily to claim it.");
    return 1;
}
