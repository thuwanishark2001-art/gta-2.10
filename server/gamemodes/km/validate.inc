// Pure validation helpers. No server state, so they are unit tested
// directly in tests/km_tests.pwn.

static stock bool:KM_IsUpper(c) { return c >= 'A' && c <= 'Z'; }
static stock bool:KM_IsLower(c) { return c >= 'a' && c <= 'z'; }
static stock bool:KM_IsDigit(c) { return c >= '0' && c <= '9'; }

// Account names are the SA:MP nickname the player connects with:
// 3-20 characters, letters, digits and underscore.
stock bool:IsValidAccountName(const name[])
{
    new len = strlen(name);
    if (len < 3 || len > 20) return false;
    for (new i = 0; i < len; i++)
    {
        new c = name[i];
        if (!KM_IsUpper(c) && !KM_IsLower(c) && !KM_IsDigit(c) && c != '_') return false;
    }
    return true;
}

// Character names are First_Last: each part 2-11 letters, an uppercase
// letter followed by lowercase letters, joined by exactly one underscore.
stock bool:IsValidCharacterName(const name[])
{
    new len = strlen(name);
    if (len < 5 || len > 23) return false;

    new sep = -1;
    for (new i = 0; i < len; i++)
    {
        if (name[i] == '_')
        {
            if (sep != -1) return false; // second underscore
            sep = i;
        }
    }
    if (sep == -1) return false;

    // first part: [0, sep), last part: (sep, len)
    if (sep < 2 || sep > 11) return false;
    if (len - sep - 1 < 2 || len - sep - 1 > 11) return false;
    if (!KM_IsUpper(name[0]) || !KM_IsUpper(name[sep + 1])) return false;

    for (new i = 1; i < len; i++)
    {
        if (i == sep || i == sep + 1) continue;
        if (!KM_IsLower(name[i])) return false;
    }
    return true;
}

enum E_PASSWORD_CHECK
{
    PASSWORD_OK,
    PASSWORD_TOO_SHORT,
    PASSWORD_TOO_LONG,
    PASSWORD_SAME_AS_NAME,
    PASSWORD_BAD_CHAR
}

stock E_PASSWORD_CHECK:CheckPassword(const password[], const username[])
{
    new len = strlen(password);
    if (len < KM_PASSWORD_MIN) return PASSWORD_TOO_SHORT;
    if (len > KM_PASSWORD_MAX) return PASSWORD_TOO_LONG;
    for (new i = 0; i < len; i++)
    {
        // printable ASCII only, so the same password works on any keyboard
        if (password[i] < ' ' || password[i] > '~') return PASSWORD_BAD_CHAR;
    }
    if (strcmp(password, username, true) == 0) return PASSWORD_SAME_AS_NAME;
    return PASSWORD_OK;
}

stock PasswordCheckMessage(E_PASSWORD_CHECK:result, dest[], size = sizeof (dest))
{
    switch (result)
    {
        case PASSWORD_TOO_SHORT: format(dest, size, "Your password needs at least %d characters.", KM_PASSWORD_MIN);
        case PASSWORD_TOO_LONG: format(dest, size, "Your password can have at most %d characters.", KM_PASSWORD_MAX);
        case PASSWORD_SAME_AS_NAME: format(dest, size, "Your password can't be the same as your name.");
        case PASSWORD_BAD_CHAR: format(dest, size, "Use only letters, digits and standard symbols.");
        default: dest[0] = '\0';
    }
}
