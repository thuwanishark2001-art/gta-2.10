// Database connection and the admin action log.

new MySQL:gDB;

stock DB_Connect()
{
    mysql_log(ERROR | WARNING);
    gDB = mysql_connect_file("mysql.ini");
    if (gDB == MYSQL_INVALID_HANDLE)
    {
        print("[db] connection failed: mysql.ini is missing or invalid");
        return 0;
    }
    if (mysql_errno(gDB) != 0)
    {
        new err[128];
        mysql_error(err, sizeof (err), gDB);
        printf("[db] connection failed (%d): %s", mysql_errno(gDB), err);
        return 0;
    }
    mysql_set_charset("utf8mb4", gDB);
    print("[db] connected");
    return 1;
}

public OnQueryError(errorid, const error[], const callback[], const query[], MySQL:handle)
{
    printf("[db] query error %d in %s: %s", errorid, callback, error);
    return 1;
}

// Writes one row to admin_log. Pass 0 for "no admin" (system) or "no target".
stock Log_Admin(adminAccountId, const action[], targetAccountId, const details[])
{
    new query[512];
    mysql_format(gDB, query, sizeof (query),
        "INSERT INTO admin_log (admin_account_id, action, target_account_id, details) VALUES (NULLIF(%d, 0), '%e', NULLIF(%d, 0), '%e')",
        adminAccountId, action, targetAccountId, details);
    mysql_tquery(gDB, query);
    printf("[admin] %s by #%d on #%d: %s", action, adminAccountId, targetAccountId, details);
}
