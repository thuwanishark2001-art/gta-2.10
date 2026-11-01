// Getting started: shown on a new character's first spawn, and with /guide.

stock Guide_Show(playerid)
{
    static const text[] =
        "Welcome to Kingmakers RP!\n\n\
" EMBED_WHITE "1. Get a job: /gps or /jobs shows where. /takejob and /work there.\n\
   Bus, taxi and pizza are good first jobs.\n\
2. Every hour you play you get a payday and XP towards the next level.\n\
3. Pass the driving test at the licensing office (/drivingtest),\n\
   then buy a car at the dealership (/buyvehicle).\n\
4. Buy a phone at a 24/7 shop (/shop) to call and text other players.\n\
5. Keep your cash safe at an ATM (/deposit).\n\n\
Stay in character: /me and /do for actions, /b for out-of-character chat.\n\
Stuck? /helpme asks staff, /report reports a player, /help lists commands.\n\
/guide shows this again.";
    ShowPlayerDialog(playerid, DIALOG_GUIDE, DIALOG_STYLE_MSGBOX, "Getting started", text, "Close", "");
}

CMD:guide(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    Guide_Show(playerid);
    return 1;
}
