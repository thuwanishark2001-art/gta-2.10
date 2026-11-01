// Per-player state shared by the account, character and moderation modules.

enum E_STAGE
{
    STAGE_NONE,          // connected, nothing loaded yet
    STAGE_CHECKING,      // ban / account lookup in flight
    STAGE_REGISTER,      // choosing a password
    STAGE_REGISTER_CONFIRM,
    STAGE_LOGIN,         // entering the password
    STAGE_CHAR_SELECT,   // logged in, picking a character
    STAGE_CHAR_CREATE,   // creating a character
    STAGE_PLAYING        // spawned as a character
}

enum E_PLAYER
{
    E_STAGE:pStage,
    pAccountId,
    pUsername[MAX_PLAYER_NAME],
    pIp[16],
    pPasswordHash[BCRYPT_HASH_LENGTH],
    pAdmin,
    pHelper,
    pMutedUntil,             // unix time, 0 = not muted
    pMuteReason[128],
    pLoginAttempts,
    pLoginTimer,
    pSessionId,
    pSpamWarnings
}

// Incremented on every connect and disconnect. Async callbacks carry the
// value they started with and drop their result if it changed, so a slow
// query can never act on the next player who gets the same id.
new gPlayerRace[MAX_PLAYERS];
new gPlayer[MAX_PLAYERS][E_PLAYER];
new gChatSpam[MAX_PLAYERS][E_SPAM];
new gCmdSpam[MAX_PLAYERS][E_SPAM];

stock Player_Reset(playerid)
{
    static const empty[E_PLAYER];
    gPlayer[playerid] = empty;
    gPlayer[playerid][pLoginTimer] = -1;
    Spam_Reset(gChatSpam[playerid]);
    Spam_Reset(gCmdSpam[playerid]);
}

stock bool:Player_IsPlaying(playerid)
{
    return IsPlayerConnected(playerid) && gPlayer[playerid][pStage] == STAGE_PLAYING;
}

stock bool:Player_IsLoggedIn(playerid)
{
    return IsPlayerConnected(playerid) && gPlayer[playerid][pStage] >= STAGE_CHAR_SELECT;
}

stock bool:Player_IsMuted(playerid)
{
    return gPlayer[playerid][pMutedUntil] > gettime();
}

enum
{
    DIALOG_NONE,
    DIALOG_REGISTER,
    DIALOG_REGISTER_CONFIRM,
    DIALOG_LOGIN,
    DIALOG_CHAR_SELECT,
    DIALOG_CHAR_NAME,
    DIALOG_CHAR_GENDER,
    DIALOG_CHAR_SKIN,
    DIALOG_INFO,
    DIALOG_DEALER,
    DIALOG_JOBS,
    DIALOG_SHOP,
    DIALOG_INV,
    DIALOG_INV_ACTION,
    DIALOG_INV_GIVE,
    DIALOG_RADIO
}
