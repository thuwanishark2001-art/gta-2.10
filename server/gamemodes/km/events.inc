// Server events run by lead admins: double pay (job pay and paydays) and
// double season pass points, for a set number of minutes.
//   /event                               what's running
//   /event doublepay|doublepass <min>    start (1-1440 minutes)
//   /event stop                          end all events
// Events live in memory: a server restart ends them.

// Event ids: config.inc (EVENT_*).

static const gEventNames[EVENT_COUNT][] = { "doublepay", "doublepass" };
static const gEventTitles[EVENT_COUNT][] = { "Double pay (jobs and paydays)", "Double season pass points" };
static gEventEnds[EVENT_COUNT];     // gettime() when it ends, 0 = off

stock bool:Event_Active(event)
{
    return 0 <= event < EVENT_COUNT && gEventEnds[event] > gettime();
}

// 2 while the event runs, else 1.
stock Event_Multiplier(event)
{
    return Event_Active(event) ? 2 : 1;
}

stock Event_FromName(const name[])
{
    for (new i = 0; i < EVENT_COUNT; i++)
    {
        if (strcmp(name, gEventNames[i], true) == 0) return i;
    }
    return -1;
}

static Event_Broadcast(const text[])
{
    SendClientMessageToAll(COLOR_BRAND, text);
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (Player_IsPlaying(i)) KMUI_Toast(i, text);
    }
}

stock Event_Start(event, minutes)
{
    gEventEnds[event] = gettime() + minutes * 60;
    new text[96];
    format(text, sizeof (text), "EVENT: %s for %d minutes!", gEventTitles[event], minutes);
    Event_Broadcast(text);
}

stock Event_End(event)
{
    if (gEventEnds[event] == 0) return;
    gEventEnds[event] = 0;
    new text[96];
    format(text, sizeof (text), "The event has ended: %s.", gEventTitles[event]);
    Event_Broadcast(text);
}

// From KM_Tick, once a second.
stock Event_Tick()
{
    new now = gettime();
    for (new i = 0; i < EVENT_COUNT; i++)
    {
        if (gEventEnds[i] != 0 && gEventEnds[i] <= now) Event_End(i);
    }
}

CMD:event(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return 1;
    new action[16], minutes;
    if (isnull(params))
    {
        new count = 0;
        for (new i = 0; i < EVENT_COUNT; i++)
        {
            if (!Event_Active(i)) continue;
            SendClientMessage(playerid, COLOR_BRAND, "Event: %s, %d min left.", gEventTitles[i], (gEventEnds[i] - gettime() + 59) / 60);
            count++;
        }
        if (count == 0) SendInfo(playerid, "No events are running right now.");
        return 1;
    }
    if (!Mod_CanAct(playerid, INVALID_PLAYER_ID, ADMIN_LEAD)) return 1;
    sscanf(params, "s[16]D(0)", action, minutes);
    if (strcmp(action, "stop", true) == 0)
    {
        new stopped = 0;
        for (new i = 0; i < EVENT_COUNT; i++)
        {
            if (gEventEnds[i] != 0) Event_End(i), stopped++;
        }
        if (stopped == 0) return SendInfo(playerid, "No events are running.");
        Log_Admin(Mod_ActorId(playerid), "stop_event", 0, "");
        return 1;
    }
    new event = Event_FromName(action);
    if (event == -1 || !(1 <= minutes <= 1440)) return SendInfo(playerid, "Usage: /event [doublepay/doublepass] [minutes, 1-1440], or /event stop");
    Event_Start(event, minutes);
    new details[48];
    format(details, sizeof (details), "%s %d min", gEventNames[event], minutes);
    Log_Admin(Mod_ActorId(playerid), "start_event", 0, details);
    return 1;
}
