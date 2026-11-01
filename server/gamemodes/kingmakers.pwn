// Kingmakers RP gamemode (open.mp).
// Modules live in gamemodes/km/. Database schema: server/sql/migrations.

#define MAX_PLAYERS (100)

#include <open.mp>
#include <a_mysql>
#include <samp_bcrypt>
#include <sscanf2>

#pragma dynamic 16384

#include "km/config.inc"
#include "km/ids.inc"
#include "km/proto.inc"
#include "km/antispam.inc"
#include "km/players.inc"
#include "km/util.inc"
#include "km/achooks.inc"
#include "km/validate.inc"
#include "km/rpcalc.inc"
#include "km/places.inc"
#include "km/db.inc"
#include "km/cmd.inc"
#include "km/accounts.inc"
#include "km/characters.inc"
#include "km/moderation.inc"
#include "km/chat.inc"
#include "km/economy.inc"
#include "km/offers.inc"
#include "km/vehicles.inc"
#include "km/jobs.inc"
#include "km/property.inc"
#include "km/inventory.inc"
#include "km/factions.inc"
#include "km/support.inc"
#include "km/kmui.inc"
#include "km/anticheat.inc"
#include "km/licence.inc"
#include "km/radio.inc"

forward KM_Tick();

main()
{
    print("Kingmakers RP " KM_VERSION);
}

public OnGameModeInit()
{
    if (!DB_Connect())
    {
        print("[km] no database, shutting down");
        SendRconCommand("exit");
        return 1;
    }

    SetGameModeText(KM_MODE_TEXT);
    // Mobile clients need a class to reach OnPlayerRequestClass.
    AddPlayerClass(0, KM_SPAWN_X, KM_SPAWN_Y, KM_SPAWN_Z, KM_SPAWN_A);
    bcrypt_set_thread_limit(2);
    Fac_Load();
    Prop_Load();
    Eco_Init();
    Veh_Init();
    Job_Init();
    Lic_Init();
    SetTimer("Char_AutoSaveAll", KM_AUTOSAVE_MS, true);
    SetTimer("KM_Tick", 1000, true);
    return 1;
}

public OnGameModeExit()
{
    Char_AutoSaveAll();
    Veh_SaveAll();
    mysql_close(gDB);
    return 1;
}

public OnPlayerConnect(playerid)
{
    if (IsPlayerNPC(playerid)) return 1;
    gPlayerRace[playerid]++;
    Player_Reset(playerid);
    Char_Reset(playerid);
    Job_Reset(playerid);
    Offer_Reset(playerid);
    Inv_Reset(playerid);
    gFacInvite[playerid] = 0;
    gVehBuying[playerid] = false;
    gPropBuying[playerid] = false;
    KMUI_Reset(playerid);
    AC_Reset(playerid);
    Lic_Reset(playerid);
    Acc_OnConnect(playerid);
    return 1;
}

public OnPlayerDisconnect(playerid, reason)
{
    if (Player_IsPlaying(playerid))
    {
        Job_Stop(playerid);
        Lic_End(playerid, false);
        Fac_OnLeaveWhileCuffed(playerid);
        Char_Save(playerid);
        Veh_OnOwnerLeave(playerid);
    }
    Acc_OnDisconnect(playerid, reason);
    Chat_OnDisconnect(playerid);
    Sup_OnDisconnect(playerid);
    gPlayerRace[playerid]++;
    Player_Reset(playerid);
    Char_Reset(playerid);
    Job_Reset(playerid);
    Offer_Reset(playerid);
    Inv_Reset(playerid);
    KMUI_Reset(playerid);
    Radio_Reset(playerid);
    return 1;
}

public OnPlayerRequestClass(playerid, classid)
{
    if (gPlayer[playerid][pStage] != STAGE_PLAYING)
    {
        // Hide the class picker while logging in; a character spawn ends this.
        TogglePlayerSpectating(playerid, true);
        SetPlayerCameraPos(playerid, 1460.0, -1600.0, 120.0);
        SetPlayerCameraLookAt(playerid, 1540.0, -1675.0, 40.0);
    }
    return 1;
}

public OnPlayerRequestSpawn(playerid)
{
    return Player_IsPlaying(playerid) ? 1 : 0;
}

public OnPlayerSpawn(playerid)
{
    if (!Player_IsPlaying(playerid))
    {
        TogglePlayerSpectating(playerid, true);
        return 1;
    }
    AC_Grace(playerid);
    gAcHavePos[playerid] = false;
    Char_OnSpawn(playerid);
    return 1;
}

public OnPlayerDeath(playerid, killerid, WEAPON:reason)
{
    Job_Stop(playerid, "you were injured.");
    Lic_End(playerid, false, "you were injured.");
    Char_OnDeath(playerid);
    return 1;
}

public OnPlayerStateChange(playerid, PLAYER_STATE:newstate, PLAYER_STATE:oldstate)
{
    if (Player_IsPlaying(playerid))
    {
        Veh_OnStateChange(playerid, newstate);
        Radio_OnStateChange(playerid, newstate);
    }
    return 1;
}

public OnPlayerEnterCheckpoint(playerid)
{
    if (Player_IsPlaying(playerid) && !Lic_OnCheckpoint(playerid)) Job_OnCheckpoint(playerid);
    return 1;
}

public OnVehicleSpawn(vehicleid)
{
    Veh_OnSpawn(vehicleid);
    Radio_OnVehicleSpawn(vehicleid);
    return 1;
}

public OnVehicleDeath(vehicleid, killerid)
{
    Lic_OnVehicleDeath(vehicleid);
    Job_OnVehicleDeath(vehicleid);
    return 1;
}

public OnDialogResponse(playerid, dialogid, response, listitem, inputtext[])
{
    if (Acc_OnDialog(playerid, dialogid, response, inputtext)) return 1;
    if (Char_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
    if (Veh_OnDialog(playerid, dialogid, response, listitem)) return 1;
    if (Job_OnDialog(playerid, dialogid, response, listitem)) return 1;
    if (Inv_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
    if (Radio_OnDialog(playerid, dialogid, response, listitem)) return 1;
    return 0;
}

public OnPlayerText(playerid, text[])
{
    Chat_Say(playerid, text);
    return 0;
}

public OnPlayerCommandText(playerid, cmdtext[])
{
    // The Kingmakers client announces itself before logging in.
    if (strcmp(cmdtext, "/kmclient ", false, 10) == 0) return cmd_kmclient(playerid, cmdtext[10]), 1;
    if (!Player_IsLoggedIn(playerid))
    {
        SendError(playerid, "Log in first.");
        return 1;
    }
    new E_SPAM_RESULT:spam = Spam_Check(gCmdSpam[playerid], GetTickCount(), cmdtext, KM_CMD_WINDOW_MS, KM_CMD_MAX_IN_WINDOW, 0);
    if (spam == SPAM_FLOOD)
    {
        Mod_SpamWarning(playerid, spam);
        return 1;
    }
    if (!Cmd_Dispatch(playerid, cmdtext))
    {
        SendError(playerid, "Unknown command. Type /help.");
    }
    return 1;
}

// Once a second: paydays, jail time, cuffs, the cash display, the
// Kingmakers client HUD and the anti-cheat.
public KM_Tick()
{
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (!Player_IsPlaying(i) || !gChar[i][cSpawned]) continue;
        Eco_Tick(i);
        Jail_Tick(i);
        if (gChar[i][cCuffed] && GetPlayerSpecialAction(i) != SPECIAL_ACTION_CUFFED) SetPlayerSpecialAction(i, SPECIAL_ACTION_CUFFED);
        // The client's money is display only; put it back if it drifted.
        if (GetPlayerMoney(i) != gChar[i][cCash]) Char_SyncCash(i);
        KMUI_UpdateHud(i);
        Lic_Tick(i);
        if (GetPlayerState(i) != PLAYER_STATE_WASTED) AC_Tick(i);
    }
    return 1;
}

CMD:help(playerid, params[])
{
    SendClientMessage(playerid, COLOR_BRAND, "Kingmakers RP commands");
    SendClientMessage(playerid, COLOR_WHITE, "General: /stats /inv /balance /pay /transfer /accept /decline /staff /report /helpme");
    SendClientMessage(playerid, COLOR_WHITE, "Chat: /me /do /b /s /low /w /pm /r /o  Phone: /phone /number /sms /911");
    SendClientMessage(playerid, COLOR_WHITE, "Vehicles: /engine /lights /lock /radio /park /myvehicles /buyvehicle /sellvehicle /refuel /drivingtest /licence /showlicence /tickets /paytickets");
    SendClientMessage(playerid, COLOR_WHITE, "Jobs: /jobs /takejob /quitjob /work /stopwork /calltaxi  Property: /enter /exit /buyproperty /sellproperty /myproperties /shop /till");
    if (Fac_Valid(gChar[playerid][cFaction]))
    {
        SendClientMessage(playerid, COLOR_RADIO, "Faction: /f /duty /members /quitfaction  Leaders: /invite /uninvite /setrank /roster");
        switch (Fac_Type(gChar[playerid][cFaction]))
        {
            case FACTION_POLICE: SendClientMessage(playerid, COLOR_RADIO, "Police: /d /cuff /uncuff /su /clearwanted /arrest /ticket /mdt");
            case FACTION_MEDICAL: SendClientMessage(playerid, COLOR_RADIO, "Medical: /d /heal");
        }
    }
    if (gChar[playerid][cJob] == JOB_TAXI) SendClientMessage(playerid, COLOR_WHITE, "Taxi: /taxiaccept");
    if (gChar[playerid][cJob] == JOB_MECHANIC) SendClientMessage(playerid, COLOR_WHITE, "Mechanic: /repair /refill");
    if (Sup_IsStaff(playerid)) SendClientMessage(playerid, COLOR_INFO, "Staff: /reports /ar /cr /h");
    if (gPlayer[playerid][pAdmin] >= ADMIN_MOD) SendClientMessage(playerid, COLOR_ADMIN, "Moderator: /a /kick /mute /unmute /goto /gethere /freeze /unfreeze /check /ajail /togooc");
    if (gPlayer[playerid][pAdmin] >= ADMIN_ADMIN) SendClientMessage(playerid, COLOR_ADMIN, "Admin: /ban /unban /veh /dveh /playstream /stopstream");
    if (gPlayer[playerid][pAdmin] >= ADMIN_LEAD) SendClientMessage(playerid, COLOR_ADMIN, "Lead: /givemoney /setfaction /sethelper /createproperty /deleteproperty");
    if (gPlayer[playerid][pAdmin] >= ADMIN_OWNER) SendClientMessage(playerid, COLOR_ADMIN, "Owner: /setadmin /testblips");
    return 1;
}

CMD:stats(playerid, params[])
{
    if (!Player_IsPlaying(playerid)) return SendError(playerid, "Pick a character first.");
    new played[24], faction[64] = "none";
    FormatDuration(played, gettime() - gChar[playerid][cSessionStart]);
    if (Fac_Valid(gChar[playerid][cFaction]))
    {
        new rank[32];
        Fac_RankName(gChar[playerid][cFaction], gChar[playerid][cFactionRank], rank);
        format(faction, sizeof (faction), "%s (%s)", gFac[gChar[playerid][cFaction]][fShort], rank);
    }
    SendClientMessage(playerid, COLOR_BRAND, "%s", gChar[playerid][cName]);
    SendClientMessage(playerid, COLOR_WHITE, "Level %d (%d/%d) | Cash $%d | Bank $%d | Job %s | Faction %s",
        gChar[playerid][cLevel], gChar[playerid][cXp], Level_XpNeeded(gChar[playerid][cLevel]), gChar[playerid][cCash], gChar[playerid][cBank],
        gJobNames[gChar[playerid][cJob]], faction);
    SendClientMessage(playerid, COLOR_WHITE, "Account %s | This session %s | Next payday in %d min | Phone %d",
        gPlayer[playerid][pUsername], played, (KM_PAYDAY_SECONDS - gChar[playerid][cPaydaySeconds] + 59) / 60, gChar[playerid][cPhone]);
    return 1;
}
