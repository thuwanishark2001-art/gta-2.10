// Protocol parsing, kept free of game headers so it can be tested on the
// host: see app/src/test/kmui/run.sh.
#include "kmui.h"

#include <cstring>

namespace KMUI
{
bool Parse(const char* text, std::string& event, Fields& fields)
{
    if (!text || strncmp(text, "KM1|", 4) != 0) return false;
    const char* start = text + 4;
    const char* bar = strchr(start, '|');
    if (!bar || bar == start) return false;
    event.assign(start, bar - start);

    fields.clear();
    const char* p = bar + 1;
    while (*p)
    {
        const char* end = strchr(p, ';');
        if (!end) end = p + strlen(p);
        const char* eq = (const char*)memchr(p, '=', end - p);
        if (eq && eq > p) fields[std::string(p, eq - p)] = std::string(eq + 1, end - eq - 1);
        p = *end ? end + 1 : end;
    }
    return true;
}
}
