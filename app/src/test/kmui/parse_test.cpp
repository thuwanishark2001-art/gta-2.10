// Host test for the client's KMUI parser. Run: app/src/test/kmui/run.sh
#include "../../main/cpp/samp/kmui/kmui.h"

#include <cstdio>

static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); failures++; } } while (0)

int main()
{
    std::string event;
    KMUI::Fields f;

    CHECK(KMUI::Parse("KM1|hud|cash=120;bank=5000;job=Taxi driver;fuel=-1", event, f));
    CHECK(event == "hud");
    CHECK(f.size() == 4 && f["cash"] == "120" && f["job"] == "Taxi driver" && f["fuel"] == "-1");

    // empty payload, trailing and empty pairs, pairs without '='
    CHECK(KMUI::Parse("KM1|hello|", event, f) && event == "hello" && f.empty());
    CHECK(KMUI::Parse("KM1|toast|text=hi;;junk;=x;", event, f) && f.size() == 1 && f["text"] == "hi");
    CHECK(KMUI::Parse("KM1|toast|text=", event, f) && f.count("text") == 1 && f["text"].empty());

    // not events
    CHECK(!KMUI::Parse("Hello there", event, f));
    CHECK(!KMUI::Parse("KM2|hud|cash=1", event, f));
    CHECK(!KMUI::Parse("KM1|nopayload", event, f));
    CHECK(!KMUI::Parse("KM1||cash=1", event, f));
    CHECK(!KMUI::Parse(nullptr, event, f));

    std::printf(failures ? "kmui parse: %d failure(s)\n" : "kmui parse: all checks passed\n", failures);
    return failures ? 1 : 0;
}
