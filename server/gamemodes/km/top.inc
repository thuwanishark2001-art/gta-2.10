// /top: the richest characters and the highest levels.

forward Top_OnLoaded(playerid, race);

CMD:top(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    mysql_tquery(gDB,
        "(SELECT 'rich' AS board, name, cash + bank AS score FROM characters ORDER BY cash + bank DESC, id LIMIT 5) \
         UNION ALL (SELECT 'level', name, level FROM characters ORDER BY level DESC, xp DESC, id LIMIT 5)",
        "Top_OnLoaded", "dd", playerid, gPlayerRace[playerid]);
    return 1;
}

public Top_OnLoaded(playerid, race)
{
    if (gPlayerRace[playerid] != race) return 1;
    new body[600] = "Richest\n", line[64], board[8], name[MAX_PLAYER_NAME + 1], score;
    new rank = 0, bool:levels = false;
    for (new i = 0, rows = cache_num_rows(); i < rows; i++)
    {
        cache_get_value_name(i, "board", board);
        cache_get_value_name(i, "name", name);
        cache_get_value_name_int(i, "score", score);
        if (!levels && !strcmp(board, "level"))
        {
            levels = true;
            rank = 0;
            strcat(body, "\nHighest level\n");
        }
        rank++;
        if (levels) format(line, sizeof (line), "%d. %s - level %d\n", rank, name, score);
        else format(line, sizeof (line), "%d. %s - $%d\n", rank, name, score);
        strcat(body, line);
    }
    ShowPlayerDialog(playerid, DIALOG_TOP, DIALOG_STYLE_MSGBOX, "Kingmakers RP | top players", body, "Close", "");
    return 1;
}
