// Server content: files in server/content/ that the app downloads (see
// docs/CONTENT.md). The content service serves them over HTTP; the app keeps
// a copy, so sounds play from the phone instead of streaming each time.
//
// The entrypoint writes scriptfiles/content.cfg:
//   line 1: KM_CONTENT_URL, e.g. http://65.20.88.67:7780
//   line 2: KM_WELCOME_SOUND, a sound played after login, e.g. sounds/welcome.mp3

#define CONTENT_PATH_MAX    (64)

static gContentUrl[RADIO_URL_MAX - CONTENT_PATH_MAX];
static gWelcomeSound[CONTENT_PATH_MAX];

static Content_ReadLine(File:f, dest[], size)
{
    dest[0] = '\0';
    new line[RADIO_URL_MAX];
    if (!fread(f, line)) return;
    new len = strlen(line);
    while (len > 0 && line[len - 1] <= ' ') line[--len] = '\0';
    new start = 0;
    while (line[start] != '\0' && line[start] <= ' ') start++;
    strcat(dest, line[start], size);
}

// A relative path inside the content folder: letters, digits, _ - . /,
// no "..", no leading "/", ending in an audio extension the client plays.
stock bool:Content_IsValidSoundPath(const path[])
{
    new len = strlen(path);
    if (len < 5 || len >= CONTENT_PATH_MAX || path[0] == '/' || path[0] == '.') return false;
    for (new i = 0; i < len; i++)
    {
        new c = path[i];
        if (!(('a' <= c <= 'z') || ('A' <= c <= 'Z') || ('0' <= c <= '9') || c == '_' || c == '-' || c == '.' || c == '/')) return false;
        if (c == '.' && i + 1 < len && path[i + 1] == '.') return false;
        if (c == '/' && i + 1 < len && path[i + 1] == '/') return false;
    }
    return strfind(path, ".mp3", true) == len - 4 || strfind(path, ".ogg", true) == len - 4 || strfind(path, ".wav", true) == len - 4;
}

stock Content_Load()
{
    gContentUrl[0] = '\0';
    gWelcomeSound[0] = '\0';
    if (!fexist("content.cfg")) return;
    new File:f = fopen("content.cfg", io_read);
    if (!f) return;
    Content_ReadLine(f, gContentUrl, sizeof (gContentUrl));
    Content_ReadLine(f, gWelcomeSound, sizeof (gWelcomeSound));
    fclose(f);
    // Drop a trailing slash: paths are joined with one.
    new len = strlen(gContentUrl);
    if (len > 0 && gContentUrl[len - 1] == '/') gContentUrl[len - 1] = '\0';
    if (gContentUrl[0] != '\0' && !Radio_IsValidUrl(gContentUrl))
    {
        printf("[content] KM_CONTENT_URL \"%s\" isn't a usable http(s) URL; ignored", gContentUrl);
        gContentUrl[0] = '\0';
    }
    if (gWelcomeSound[0] != '\0' && !Content_IsValidSoundPath(gWelcomeSound))
    {
        printf("[content] KM_WELCOME_SOUND \"%s\" isn't a usable sound path; ignored", gWelcomeSound);
        gWelcomeSound[0] = '\0';
    }
    if (gContentUrl[0] != '\0') printf("[content] content server: %s", gContentUrl);
}

stock bool:Content_SoundUrl(const path[], url[], size)
{
    if (gContentUrl[0] == '\0' || !Content_IsValidSoundPath(path)) return false;
    format(url, size, "%s/%s", gContentUrl, path);
    return true;
}

// From Acc_StartSession: the welcome sound, if one is set.
stock Content_OnSession(playerid)
{
    if (gWelcomeSound[0] == '\0') return;
    new url[RADIO_URL_MAX];
    if (Content_SoundUrl(gWelcomeSound, url, sizeof (url))) PlayAudioStreamForPlayer(playerid, url);
}

// "/playsound all sounds/x.mp3" or "/playsound <player> sounds/x.mp3".
// Stop it with /stopstream, like any admin stream.
CMD:playsound(playerid, params[])
{
    if (gPlayer[playerid][pAdmin] < ADMIN_ADMIN) return SendError(playerid, "You don't have permission to use this command.");
    new target[24], path[CONTENT_PATH_MAX + 8];
    if (sscanf(params, "s[24]s[72]", target, path)) return SendInfo(playerid, "Usage: /playsound [player/all] [file, e.g. sounds/welcome.mp3]");
    if (gContentUrl[0] == '\0') return SendError(playerid, "The content server isn't set up (KM_CONTENT_URL in server/.env).");
    new url[RADIO_URL_MAX];
    if (!Content_SoundUrl(path, url, sizeof (url))) return SendError(playerid, "Use a .mp3, .ogg or .wav path inside the content folder, e.g. sounds/welcome.mp3.");
    if (strcmp(target, "all", true) == 0)
    {
        new count;
        for (new i = 0; i < MAX_PLAYERS; i++)
        {
            if (Player_IsPlaying(i)) Radio_AdminPlay(i, url), count++;
        }
        SendClientMessage(playerid, COLOR_ADMIN, "Playing %s for %d players. Stop it with /stopstream all.", path, count);
        return 1;
    }
    new targetid;
    if (sscanf(target, "u", targetid) || targetid == INVALID_PLAYER_ID || !Player_IsPlaying(targetid)) return SendError(playerid, "That player isn't playing.");
    Radio_AdminPlay(targetid, url);
    SendClientMessage(playerid, COLOR_ADMIN, "Playing %s for %s.", path, gChar[targetid][cName]);
    return 1;
}
