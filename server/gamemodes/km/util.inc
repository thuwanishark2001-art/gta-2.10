// Shared helpers: colours, messages, delayed kick, string utilities.

#define COLOR_WHITE     (0xFFFFFFFF)
#define COLOR_GREY      (0xAFAFAFFF)
#define COLOR_ERROR     (0xE05050FF)
#define COLOR_INFO      (0x7FB2E5FF)
#define COLOR_SUCCESS   (0x7FD17FFF)
#define COLOR_ADMIN     (0xF2B04BFF)
#define COLOR_BRAND     (0xD4AF37FF)    // Kingmakers gold

#define EMBED_BRAND     "{D4AF37}"
#define EMBED_WHITE     "{FFFFFF}"
#define EMBED_GREY      "{AFAFAF}"
#define EMBED_ERROR     "{E05050}"

// "\1" is passed for empty command params (CallLocalFunction can't pass "")
#define isnull(%1)      ((%1)[0] == 0 || ((%1)[0] == 1 && (%1)[1] == 0))

stock SendError(playerid, const text[])
{
    return SendClientMessage(playerid, COLOR_ERROR, "[!] %s", text);
}

stock SendInfo(playerid, const text[])
{
    return SendClientMessage(playerid, COLOR_INFO, "%s", text);
}

forward KM_DoKick(playerid, race);

// Kick after a short delay so the client receives the last messages first.
stock KickDelayed(playerid)
{
    SetTimerEx("KM_DoKick", 250, false, "dd", playerid, gPlayerRace[playerid]);
}

public KM_DoKick(playerid, race)
{
    if (IsPlayerConnected(playerid) && gPlayerRace[playerid] == race)
    {
        Kick(playerid);
    }
    return 1;
}

stock StrToLower(str[])
{
    for (new i = 0; str[i] != '\0'; i++)
    {
        str[i] = tolower(str[i]);
    }
}

// djb2 hash of a string, case-insensitive. Used by the repeat-message check.
stock StrHashCI(const str[])
{
    new h = 5381;
    for (new i = 0; str[i] != '\0'; i++)
    {
        h = ((h << 5) + h) + tolower(str[i]);
    }
    return h;
}

// "1h 05m", "12m 30s", "45s"
stock FormatDuration(dest[], seconds, size = sizeof (dest))
{
    if (seconds < 0) seconds = 0;
    new h = seconds / 3600, m = (seconds % 3600) / 60, s = seconds % 60;
    if (h > 0) format(dest, size, "%dh %02dm", h, m);
    else if (m > 0) format(dest, size, "%dm %02ds", m, s);
    else format(dest, size, "%ds", s);
}

// Adds a red error line under a dialog body, if there is one.
stock Dialog_AppendError(body[], const error[], size = sizeof (body))
{
    if (error[0] == '\0') return;
    strcat(body, "\n\n" EMBED_ERROR, size);
    strcat(body, error, size);
}
