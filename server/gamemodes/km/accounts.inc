// Accounts: ban check, register, login, sessions.
//
// Flow on connect:
//   ban check -> account lookup -> register (password, confirm) or login
//   -> session row -> character select (characters.inc)

forward Acc_OnBanCheck(playerid, race);
forward Acc_OnLookup(playerid, race);
forward Acc_OnRegisterHash(playerid, race, hashid);
forward Acc_OnRegisterConfirm(playerid, bool:success, race);
forward Acc_OnRegistered(playerid, race);
forward Acc_OnLoginVerify(playerid, bool:success, race);
forward Acc_OnSessionStarted(playerid, race);
forward Acc_OnLoginTimeout(playerid, race);

stock Acc_OnConnect(playerid)
{
    GetPlayerName(playerid, gPlayer[playerid][pUsername], MAX_PLAYER_NAME);
    GetPlayerIp(playerid, gPlayer[playerid][pIp], 16);

    SendClientMessage(playerid, COLOR_BRAND, "Welcome to Kingmakers RP.");

    if (!IsValidAccountName(gPlayer[playerid][pUsername]))
    {
        SendError(playerid, "Your nickname must be 3-20 letters, digits or underscores. Change it in the client settings and reconnect.");
        KickDelayed(playerid);
        return;
    }

    gPlayer[playerid][pStage] = STAGE_CHECKING;
    gPlayer[playerid][pLoginTimer] = SetTimerEx("Acc_OnLoginTimeout", KM_LOGIN_TIMEOUT_MS, false, "dd", playerid, gPlayerRace[playerid]);

    new query[512];
    mysql_format(gDB, query, sizeof (query),
        "SELECT reason, IFNULL(UNIX_TIMESTAMP(expires_at), 0) AS expires FROM bans \
         WHERE lifted_at IS NULL AND (expires_at IS NULL OR expires_at > UTC_TIMESTAMP()) \
         AND (ip = '%e' OR account_id = (SELECT id FROM accounts WHERE username = '%e')) \
         ORDER BY expires_at IS NULL DESC, expires_at DESC LIMIT 1",
        gPlayer[playerid][pIp], gPlayer[playerid][pUsername]);
    mysql_tquery(gDB, query, "Acc_OnBanCheck", "dd", playerid, gPlayerRace[playerid]);
}

stock Acc_OnDisconnect(playerid, reason)
{
    if (gPlayer[playerid][pLoginTimer] != -1)
    {
        KillTimer(gPlayer[playerid][pLoginTimer]);
        gPlayer[playerid][pLoginTimer] = -1;
    }
    if (gPlayer[playerid][pSessionId] != 0)
    {
        static const reasons[][] = { "timeout", "quit", "kick" };
        new query[160];
        mysql_format(gDB, query, sizeof (query),
            "UPDATE account_sessions SET ended_at = UTC_TIMESTAMP(), end_reason = '%s' WHERE id = %d",
            (0 <= reason < sizeof (reasons)) ? reasons[reason] : "other", gPlayer[playerid][pSessionId]);
        mysql_tquery(gDB, query);
    }
}

public Acc_OnBanCheck(playerid, race)
{
    if (gPlayerRace[playerid] != race) return 1;

    if (cache_num_rows() > 0)
    {
        new reason[128], expires;
        cache_get_value_name(0, "reason", reason);
        cache_get_value_name_int(0, "expires", expires);
        if (expires == 0)
        {
            SendClientMessage(playerid, COLOR_ERROR, "You are permanently banned. Reason: %s", reason);
        }
        else
        {
            new left[24];
            FormatDuration(left, expires - gettime());
            SendClientMessage(playerid, COLOR_ERROR, "You are banned for another %s. Reason: %s", left, reason);
        }
        KickDelayed(playerid);
        return 1;
    }

    new query[512];
    mysql_format(gDB, query, sizeof (query),
        "SELECT a.id, a.password_hash, a.admin_level, a.helper_level, \
         IFNULL(UNIX_TIMESTAMP(a.muted_until), 0) AS muted_until, IFNULL(a.mute_reason, '') AS mute_reason, \
         (SELECT COUNT(*) FROM login_attempts l WHERE l.ip = '%e' AND l.success = 0 \
          AND l.created_at > UTC_TIMESTAMP() - INTERVAL %d MINUTE) AS ip_fails \
         FROM accounts a WHERE a.username = '%e'",
        gPlayer[playerid][pIp], KM_IP_FAIL_WINDOW_MIN, gPlayer[playerid][pUsername]);
    mysql_tquery(gDB, query, "Acc_OnLookup", "dd", playerid, race);
    return 1;
}

public Acc_OnLookup(playerid, race)
{
    if (gPlayerRace[playerid] != race) return 1;

    if (cache_num_rows() == 0)
    {
        gPlayer[playerid][pStage] = STAGE_REGISTER;
        Acc_ShowRegister(playerid, "");
        return 1;
    }

    new ipFails;
    cache_get_value_name_int(0, "id", gPlayer[playerid][pAccountId]);
    cache_get_value_name(0, "password_hash", gPlayer[playerid][pPasswordHash], BCRYPT_HASH_LENGTH);
    cache_get_value_name_int(0, "admin_level", gPlayer[playerid][pAdmin]);
    cache_get_value_name_int(0, "helper_level", gPlayer[playerid][pHelper]);
    cache_get_value_name_int(0, "muted_until", gPlayer[playerid][pMutedUntil]);
    cache_get_value_name(0, "mute_reason", gPlayer[playerid][pMuteReason], 128);
    cache_get_value_name_int(0, "ip_fails", ipFails);

    if (ipFails >= KM_IP_FAIL_LIMIT)
    {
        SendClientMessage(playerid, COLOR_ERROR, "Too many wrong passwords from your connection. Try again in %d minutes.", KM_IP_FAIL_WINDOW_MIN);
        KickDelayed(playerid);
        return 1;
    }

    gPlayer[playerid][pStage] = STAGE_LOGIN;
    Acc_ShowLogin(playerid, "");
    return 1;
}

stock Acc_ShowRegister(playerid, const error[])
{
    new body[384];
    format(body, sizeof (body),
        EMBED_WHITE "Welcome, " EMBED_BRAND "%s" EMBED_WHITE ".\n\nThis name isn't registered yet. Choose a password to create your account.\n" \
        EMBED_GREY "%d-%d characters. Don't reuse a password from another game.",
        gPlayer[playerid][pUsername], KM_PASSWORD_MIN, KM_PASSWORD_MAX);
    Dialog_AppendError(body, error);
    ShowPlayerDialog(playerid, DIALOG_REGISTER, DIALOG_STYLE_PASSWORD, "Kingmakers RP | Register", body, "Next", "Quit");
}

stock Acc_ShowLogin(playerid, const error[])
{
    new body[320];
    format(body, sizeof (body),
        EMBED_WHITE "Welcome back, " EMBED_BRAND "%s" EMBED_WHITE ".\n\nEnter your password to log in.",
        gPlayer[playerid][pUsername]);
    Dialog_AppendError(body, error);
    ShowPlayerDialog(playerid, DIALOG_LOGIN, DIALOG_STYLE_PASSWORD, "Kingmakers RP | Login", body, "Login", "Quit");
}

// Returns 1 if the dialog belonged to this module.
stock Acc_OnDialog(playerid, dialogid, response, const inputtext[])
{
    switch (dialogid)
    {
        case DIALOG_REGISTER:
        {
            if (gPlayer[playerid][pStage] != STAGE_REGISTER) return 1;
            if (!response) return Kick(playerid), 1;

            new E_PASSWORD_CHECK:check = CheckPassword(inputtext, gPlayer[playerid][pUsername]);
            if (check != PASSWORD_OK)
            {
                new msg[96];
                PasswordCheckMessage(check, msg);
                Acc_ShowRegister(playerid, msg);
                return 1;
            }
            // Hash first, then ask to confirm by verifying against the hash,
            // so the plain password is never kept in memory between dialogs.
            bcrypt_hash(playerid, "Acc_OnRegisterHash", inputtext, KM_BCRYPT_COST, "d", gPlayerRace[playerid]);
            return 1;
        }
        case DIALOG_REGISTER_CONFIRM:
        {
            if (gPlayer[playerid][pStage] != STAGE_REGISTER_CONFIRM) return 1;
            if (!response)
            {
                gPlayer[playerid][pStage] = STAGE_REGISTER;
                Acc_ShowRegister(playerid, "");
                return 1;
            }
            bcrypt_verify(playerid, "Acc_OnRegisterConfirm", inputtext, gPlayer[playerid][pPasswordHash], "d", gPlayerRace[playerid]);
            return 1;
        }
        case DIALOG_LOGIN:
        {
            if (gPlayer[playerid][pStage] != STAGE_LOGIN) return 1;
            if (!response) return Kick(playerid), 1;
            if (strlen(inputtext) == 0 || strlen(inputtext) > KM_PASSWORD_MAX)
            {
                Acc_ShowLogin(playerid, "Enter your password.");
                return 1;
            }
            bcrypt_verify(playerid, "Acc_OnLoginVerify", inputtext, gPlayer[playerid][pPasswordHash], "d", gPlayerRace[playerid]);
            return 1;
        }
    }
    return 0;
}

// samp-bcrypt 0.4.1 orders callback arguments differently per call:
//   bcrypt_hash   -> callback(playerid, <our args>, hashid)
//   bcrypt_verify -> callback(playerid, success, <our args>)
// tests/km_tests.pwn checks both orders.
public Acc_OnRegisterHash(playerid, race, hashid)
{
    if (gPlayerRace[playerid] != race) return 1;
    bcrypt_get_hash(gPlayer[playerid][pPasswordHash], BCRYPT_HASH_LENGTH);
    gPlayer[playerid][pStage] = STAGE_REGISTER_CONFIRM;
    ShowPlayerDialog(playerid, DIALOG_REGISTER_CONFIRM, DIALOG_STYLE_PASSWORD, "Kingmakers RP | Register",
        EMBED_WHITE "Type the same password again to confirm it.", "Register", "Back");
    return 1;
}

public Acc_OnRegisterConfirm(playerid, bool:success, race)
{
    if (gPlayerRace[playerid] != race) return 1;
    if (!success)
    {
        gPlayer[playerid][pStage] = STAGE_REGISTER;
        Acc_ShowRegister(playerid, "The passwords didn't match. Try again.");
        return 1;
    }

    new query[320];
    mysql_format(gDB, query, sizeof (query),
        "INSERT IGNORE INTO accounts (username, password_hash, register_ip, last_ip, last_login_at) VALUES ('%e', '%e', '%e', '%e', UTC_TIMESTAMP())",
        gPlayer[playerid][pUsername], gPlayer[playerid][pPasswordHash], gPlayer[playerid][pIp], gPlayer[playerid][pIp]);
    mysql_tquery(gDB, query, "Acc_OnRegistered", "dd", playerid, race);
    return 1;
}

public Acc_OnRegistered(playerid, race)
{
    if (gPlayerRace[playerid] != race) return 1;
    gPlayer[playerid][pAccountId] = cache_insert_id();
    if (gPlayer[playerid][pAccountId] <= 0)
    {
        // Most likely someone registered the same name a moment earlier.
        SendError(playerid, "That name was just registered by someone else. Reconnect with another name.");
        KickDelayed(playerid);
        return 1;
    }
    printf("[account] registered #%d %s from %s", gPlayer[playerid][pAccountId], gPlayer[playerid][pUsername], gPlayer[playerid][pIp]);
    SendClientMessage(playerid, COLOR_SUCCESS, "Your account is ready. Now create your first character.");
    Acc_StartSession(playerid);
    return 1;
}

public Acc_OnLoginVerify(playerid, bool:success, race)
{
    if (gPlayerRace[playerid] != race) return 1;

    new query[200];
    mysql_format(gDB, query, sizeof (query),
        "INSERT INTO login_attempts (account_id, ip, success) VALUES (%d, '%e', %d)",
        gPlayer[playerid][pAccountId], gPlayer[playerid][pIp], success ? 1 : 0);
    mysql_tquery(gDB, query);

    if (!success)
    {
        gPlayer[playerid][pLoginAttempts]++;
        if (gPlayer[playerid][pLoginAttempts] >= KM_MAX_LOGIN_ATTEMPTS)
        {
            SendError(playerid, "Too many wrong passwords.");
            KickDelayed(playerid);
            return 1;
        }
        new msg[64];
        format(msg, sizeof (msg), "Wrong password. %d attempt(s) left.", KM_MAX_LOGIN_ATTEMPTS - gPlayer[playerid][pLoginAttempts]);
        Acc_ShowLogin(playerid, msg);
        return 1;
    }

    // One session per account. The character name replaces the account
    // nickname after spawn, so the client alone can't prevent a double login.
    for (new i = 0; i < MAX_PLAYERS; i++)
    {
        if (i != playerid && Player_IsLoggedIn(i) && gPlayer[i][pAccountId] == gPlayer[playerid][pAccountId])
        {
            SendError(playerid, "This account is already logged in.");
            KickDelayed(playerid);
            return 1;
        }
    }

    mysql_format(gDB, query, sizeof (query),
        "UPDATE accounts SET last_ip = '%e', last_login_at = UTC_TIMESTAMP() WHERE id = %d",
        gPlayer[playerid][pIp], gPlayer[playerid][pAccountId]);
    mysql_tquery(gDB, query);

    printf("[account] login #%d %s from %s", gPlayer[playerid][pAccountId], gPlayer[playerid][pUsername], gPlayer[playerid][pIp]);
    Acc_StartSession(playerid);
    return 1;
}

stock Acc_StartSession(playerid)
{
    Owner_OnSession(playerid);
    Content_OnSession(playerid);
    if (gPlayer[playerid][pLoginTimer] != -1)
    {
        KillTimer(gPlayer[playerid][pLoginTimer]);
        gPlayer[playerid][pLoginTimer] = -1;
    }
    // The hash isn't needed after login.
    gPlayer[playerid][pPasswordHash][0] = '\0';
    gPlayer[playerid][pStage] = STAGE_CHAR_SELECT;

    new query[160];
    mysql_format(gDB, query, sizeof (query),
        "INSERT INTO account_sessions (account_id, ip) VALUES (%d, '%e')",
        gPlayer[playerid][pAccountId], gPlayer[playerid][pIp]);
    mysql_tquery(gDB, query, "Acc_OnSessionStarted", "dd", playerid, gPlayerRace[playerid]);
}

public Acc_OnSessionStarted(playerid, race)
{
    if (gPlayerRace[playerid] != race) return 1;
    gPlayer[playerid][pSessionId] = cache_insert_id();
    if (gPlayer[playerid][pAdmin] > 0)
    {
        SendClientMessage(playerid, COLOR_ADMIN, "You are logged in as admin level %d.", gPlayer[playerid][pAdmin]);
    }
    if (Player_IsMuted(playerid))
    {
        new left[24];
        FormatDuration(left, gPlayer[playerid][pMutedUntil] - gettime());
        SendClientMessage(playerid, COLOR_ERROR, "You are muted for another %s.", left);
    }
    Char_LoadList(playerid);
    return 1;
}

public Acc_OnLoginTimeout(playerid, race)
{
    if (gPlayerRace[playerid] != race) return 1;
    gPlayer[playerid][pLoginTimer] = -1;
    if (gPlayer[playerid][pStage] < STAGE_CHAR_SELECT)
    {
        SendError(playerid, "You took too long to log in.");
        KickDelayed(playerid);
    }
    return 1;
}
