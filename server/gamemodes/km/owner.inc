// The server owner from server/.env (KM_OWNER_ACCOUNT). The entrypoint writes
// it to scriptfiles/owner.cfg; that account is made owner when it logs in or
// registers, so setting up a server needs no SQL.

static gOwnerAccount[MAX_PLAYER_NAME];

stock Owner_Load()
{
    gOwnerAccount[0] = '\0';
    if (!fexist("owner.cfg")) return;
    new File:f = fopen("owner.cfg", io_read);
    if (!f) return;
    new line[MAX_PLAYER_NAME + 8];
    fread(f, line);
    fclose(f);
    // Trim the line ending and surrounding spaces.
    new len = strlen(line);
    while (len > 0 && line[len - 1] <= ' ') line[--len] = '\0';
    new start = 0;
    while (line[start] != '\0' && line[start] <= ' ') start++;
    if (line[start] == '\0') return;
    if (!IsValidAccountName(line[start]))
    {
        printf("[owner] KM_OWNER_ACCOUNT \"%s\" isn't a valid account name; ignored", line[start]);
        return;
    }
    strcat(gOwnerAccount, line[start]);
    printf("[owner] owner account: %s", gOwnerAccount);
}

// From Acc_StartSession: promotes the configured owner account.
stock Owner_OnSession(playerid)
{
    if (gOwnerAccount[0] == '\0' || gPlayer[playerid][pAdmin] >= ADMIN_OWNER) return;
    if (strcmp(gPlayer[playerid][pUsername], gOwnerAccount, true) != 0) return;
    gPlayer[playerid][pAdmin] = ADMIN_OWNER;
    new query[96];
    mysql_format(gDB, query, sizeof (query), "UPDATE accounts SET admin_level = %d WHERE id = %d", ADMIN_OWNER, gPlayer[playerid][pAccountId]);
    mysql_tquery(gDB, query);
    Log_Admin(0, "owner_from_config", gPlayer[playerid][pAccountId], gPlayer[playerid][pUsername]);
    SendClientMessage(playerid, COLOR_ADMIN, "You're the server owner (KM_OWNER_ACCOUNT). /help lists your commands.");
}
