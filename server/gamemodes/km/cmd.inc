// Minimal command processor.
//   CMD:name(playerid, params[]) { ... return 1; }
// "/Name args" calls cmd_name with params = "args" ("\1" when empty, see isnull).

#define CMD:%0(%1) forward cmd_%0(%1); public cmd_%0(%1)

// Splits "/cmd rest" into a lowercase command name and the parameters.
// Returns false if the text isn't a usable command.
stock bool:Cmd_Parse(const cmdtext[], name[], nameSize, params[], paramsSize)
{
    if (cmdtext[0] != '/') return false;
    new pos = 1;
    while (cmdtext[pos] > ' ') pos++;
    if (pos == 1 || pos - 1 >= nameSize) return false;
    strmid(name, cmdtext, 1, pos, nameSize);
    StrToLower(name);
    for (new i = 0; name[i] != '\0'; i++)
    {
        new c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    while (cmdtext[pos] == ' ') pos++;
    if (cmdtext[pos] == '\0')
    {
        params[0] = '\1';
        params[1] = '\0';
    }
    else
    {
        strmid(params, cmdtext, pos, strlen(cmdtext), paramsSize);
    }
    return true;
}

stock bool:Cmd_Dispatch(playerid, const cmdtext[])
{
    new name[28], params[128], func[32];
    if (!Cmd_Parse(cmdtext, name, sizeof (name), params, sizeof (params))) return false;
    format(func, sizeof (func), "cmd_%s", name);
    if (funcidx(func) == -1) return false;
    CallLocalFunction(func, "is", playerid, params);
    return true;
}
