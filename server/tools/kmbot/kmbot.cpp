// kmbot: a headless SA:MP 0.3.7 client for end-to-end tests of the
// Kingmakers gamemode. It uses the mobile client's own RakNet and SA:MP
// handshake code, plays a fixed scenario through the real dialogs and
// commands, and exits 0 only if every step passed.
//
//   kmbot <host> <port> <account name> <character First_Last>

#include "main.h"

#include <netdb.h>
#include <arpa/inet.h>
#include <chrono>
#include <deque>
#include <string>
#include <thread>

#include "RakClientInterface.h"
#include "RakNetworkFactory.h"
#include "PacketEnumerations.h"
#include "BitStream.h"
#include "StringCompressor.h"
#include "SAMP/SAMPRPC.h"

void gen_auth_key(char* buf, char* auth_in);

// Same values as the mobile client (net/netgame.cpp).
static const char* kAuthBs = "E02262CF28BC542486C558D4BE9EFB716592AFAF8B";
static const int kNetgameVersion = 4057;

struct Dialog { int id; int style; std::string title; std::string text; };
struct Message { uint32_t colour; std::string text; };

static RakClientInterface* g_client = nullptr;
static std::string g_name;
static bool g_connected = false;   // InitGame received
static bool g_spectating = true;
static std::deque<Dialog> g_dialogs;
static std::deque<Message> g_messages;
static int g_failures = 0;

static uint32_t Now()
{
    using namespace std::chrono;
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// ---- RPC handlers ----------------------------------------------------------

static void OnInitGame(RPCParameters*)
{
    g_connected = true;
    // Like the client's class selection: ask for class 0. The gamemode
    // answers by spectating until a character is picked.
    RakNet::BitStream bs;
    bs.Write((int)0);
    g_client->RPC(&RPC_RequestClass, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, false, UNASSIGNED_NETWORK_ID, nullptr);
}

static void OnDialog(RPCParameters* params)
{
    RakNet::BitStream bs(params->input, (params->numberOfBitsOfData / 8) + 1, false);
    int16_t id; uint8_t style, len;
    char buf[4097];
    Dialog d;
    bs.Read(id); bs.Read(style);
    d.id = id; d.style = style;
    bs.Read(len); bs.Read(buf, len); buf[len] = 0; d.title = buf;
    bs.Read(len); bs.IgnoreBits(len * 8);   // button 1
    bs.Read(len); bs.IgnoreBits(len * 8);   // button 2
    // The body is Huffman-compressed in 0.3.7.
    char body[4097] = {0};
    StringCompressor::Instance()->DecodeString(body, 4096, &bs);
    d.text = body;
    printf("  << dialog %d \"%s\"\n", d.id, d.title.c_str());
    g_dialogs.push_back(d);
}

static void OnClientMessage(RPCParameters* params)
{
    RakNet::BitStream bs(params->input, (params->numberOfBitsOfData / 8) + 1, false);
    uint32_t colour, len;
    char buf[257] = {0};
    bs.Read(colour); bs.Read(len);
    if (len > 256) return;
    bs.Read(buf, len);
    printf("  << message %08X %s\n", colour, buf);
    g_messages.push_back({colour, buf});
}

static void OnToggleSpectating(RPCParameters* params)
{
    RakNet::BitStream bs(params->input, (params->numberOfBitsOfData / 8) + 1, false);
    uint32_t toggle = 1;
    bs.Read(toggle);
    g_spectating = toggle != 0;
    if (!g_spectating)
    {
        // A real client spawns now.
        RakNet::BitStream empty;
        g_client->RPC(&RPC_RequestSpawn, &empty, HIGH_PRIORITY, RELIABLE_ORDERED, 0, false, UNASSIGNED_NETWORK_ID, nullptr);
        g_client->RPC(&RPC_Spawn, &empty, HIGH_PRIORITY, RELIABLE_ORDERED, 0, false, UNASSIGNED_NETWORK_ID, nullptr);
        printf("  >> spawn\n");
    }
}

// ---- network -------------------------------------------------------------

static void SendJoin(Packet* pkt)
{
    RakNet::BitStream in(pkt->data, pkt->length, true);
    uint16_t myId; unsigned int challenge;
    in.IgnoreBits(8 + 32 + 16);
    in.Read(myId); in.Read(challenge);
    challenge ^= kNetgameVersion;

    RakNet::BitStream bs;
    bs.Write(kNetgameVersion);
    bs.Write((uint8_t)1);
    bs.Write((uint8_t)g_name.size()); bs.Write(g_name.c_str(), g_name.size());
    bs.Write(challenge);
    bs.Write((uint8_t)strlen(kAuthBs)); bs.Write(kAuthBs, strlen(kAuthBs));
    bs.Write((uint8_t)strlen(SAMP_VERSION)); bs.Write(SAMP_VERSION, strlen(SAMP_VERSION));
    g_client->RPC(&RPC_ClientJoin, &bs, HIGH_PRIORITY, RELIABLE, 0, false, UNASSIGNED_NETWORK_ID, nullptr);
}

static void SendAuthKey(Packet* pkt)
{
    RakNet::BitStream in(pkt->data, pkt->length, false);
    uint8_t len; char auth[260], key[269];
    in.IgnoreBits(8); in.Read(len); in.Read(auth, len); auth[len] = 0;
    gen_auth_key(key, auth);
    RakNet::BitStream out;
    out.Write((uint8_t)ID_AUTH_KEY);
    out.Write((uint8_t)strlen(key)); out.Write(key, strlen(key));
    g_client->Send(&out, SYSTEM_PRIORITY, RELIABLE, 0);
}

static void Pump(uint32_t ms)
{
    uint32_t end = Now() + ms;
    do {
        while (Packet* pkt = g_client->Receive())
        {
            unsigned char id = pkt->data[0];
            if (getenv("KMBOT_DEBUG")) printf("  << packet %d (%u bytes)\n", id, pkt->length);
            if (id == ID_AUTH_KEY) SendAuthKey(pkt);
            else if (id == ID_CONNECTION_REQUEST_ACCEPTED) SendJoin(pkt);
            else if (id == ID_CONNECTION_BANNED || id == ID_INVALID_PASSWORD || id == ID_NO_FREE_INCOMING_CONNECTIONS)
                printf("  << refused (packet %d)\n", id);
            else if (id == ID_DISCONNECTION_NOTIFICATION || id == ID_CONNECTION_LOST) { printf("  << disconnected\n"); g_connected = false; }
            g_client->DeallocatePacket(pkt);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (Now() < end);
}

static void Fail(const char* what)
{
    printf("KMBOT FAIL: %s\n", what);
    g_failures++;
}

static bool Connect(const char* host, int port)
{
    g_connected = false; g_spectating = true;
    g_dialogs.clear(); g_messages.clear();
    g_client = RakNetworkFactory::GetRakClientInterface();
    g_client->RegisterAsRemoteProcedureCall(&RPC_InitGame, OnInitGame);
    g_client->RegisterAsRemoteProcedureCall(&RPC_DialogBox, OnDialog);
    g_client->RegisterAsRemoteProcedureCall(&RPC_ClientMessage, OnClientMessage);
    g_client->RegisterAsRemoteProcedureCall(&RPC_ScrTogglePlayerSpectating, OnToggleSpectating);
    for (int attempt = 0; attempt < 10 && !g_connected; attempt++)
    {
        g_client->Connect(host, port, 0, 0, 5);
        for (int i = 0; i < 30 && !g_connected; i++) Pump(100);
    }
    if (!g_connected) { Fail("couldn't join the server"); return false; }
    // Announce the Kingmakers UI protocol like the mobile client.
    RakNet::BitStream bs;
    const char* cmd = "/kmclient 1";
    bs.Write((int)strlen(cmd)); bs.Write(cmd, strlen(cmd));
    g_client->RPC(&RPC_ServerCommand, &bs, HIGH_PRIORITY, RELIABLE, 0, false, UNASSIGNED_NETWORK_ID, nullptr);
    return true;
}

static void Disconnect()
{
    g_client->Disconnect(300);
    Pump(500);
    RakNetworkFactory::DestroyRakClientInterface(g_client);
    g_client = nullptr;
}

// ---- scenario steps ------------------------------------------------------

static bool ExpectDialog(int id, Dialog* out = nullptr, uint32_t timeout = 8000)
{
    uint32_t end = Now() + timeout;
    while (Now() < end)
    {
        while (!g_dialogs.empty())
        {
            Dialog d = g_dialogs.front();
            g_dialogs.pop_front();
            if (d.id == id) { if (out) *out = d; return true; }
        }
        Pump(50);
    }
    char what[64];
    snprintf(what, sizeof(what), "dialog %d didn't appear", id);
    Fail(what);
    return false;
}

// List dialogs take the chosen row; input, password and message boxes must
// send -1, or open.mp drops the response (as the mobile client does).
static void Respond(int id, int button, int item, const std::string& text)
{
    RakNet::BitStream bs;
    bs.Write((uint16_t)id); bs.Write((uint8_t)button); bs.Write((uint16_t)item);
    bs.Write((uint8_t)text.size()); bs.Write(text.c_str(), text.size());
    g_client->RPC(&RPC_DialogResponse, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, false, UNASSIGNED_NETWORK_ID, nullptr);
    printf("  >> dialog %d button %d item %d\n", id, button, item);
}

static void Command(const std::string& cmd)
{
    // Stay under the server's command flood limit (5 per 3 s).
    Pump(700);
    g_messages.clear();
    RakNet::BitStream bs;
    bs.Write((int)cmd.size()); bs.Write(cmd.c_str(), cmd.size());
    g_client->RPC(&RPC_ServerCommand, &bs, HIGH_PRIORITY, RELIABLE, 0, false, UNASSIGNED_NETWORK_ID, nullptr);
    printf("  >> %s\n", cmd.c_str());
}

// Waits for a client message containing `needle` (and the colour, if given).
static bool ExpectMessage(const char* needle, uint32_t colour = 0, uint32_t timeout = 6000)
{
    uint32_t end = Now() + timeout;
    while (Now() < end)
    {
        for (auto& m : g_messages)
        {
            if (m.text.find(needle) != std::string::npos && (colour == 0 || m.colour == colour)) return true;
        }
        Pump(50);
    }
    char what[160];
    snprintf(what, sizeof(what), "no message containing \"%s\"", needle);
    Fail(what);
    return false;
}

// Dialog ids from server/gamemodes/km/players.inc.
enum { D_REGISTER = 1, D_REGISTER_CONFIRM = 2, D_LOGIN = 3, D_CHAR_SELECT = 4, D_CHAR_NAME = 5, D_CHAR_GENDER = 6, D_CHAR_SKIN = 7 };
static const uint32_t kKmuiColour = 0x4B4D5501;

int main(int argc, char** argv)
{
    if (argc < 5) { printf("usage: kmbot <host> <port> <account> <First_Last>\n"); return 2; }
    setvbuf(stdout, nullptr, _IONBF, 0);
    char host[64];
    {
        // RakNet wants an IP address.
        hostent* he = gethostbyname(argv[1]);
        if (!he) { printf("KMBOT FAIL: can't resolve %s\n", argv[1]); return 1; }
        inet_ntop(AF_INET, he->h_addr_list[0], host, sizeof(host));
    }
    int port = atoi(argv[2]);
    g_name = argv[3];
    std::string character = argv[4];
    const std::string password = "kmbot-pass-123";

    printf("== register %s and create %s\n", g_name.c_str(), character.c_str());
    if (!Connect(host, port)) return 1;
    if (ExpectDialog(D_REGISTER)) Respond(D_REGISTER, 1, -1, password);
    if (ExpectDialog(D_REGISTER_CONFIRM)) Respond(D_REGISTER_CONFIRM, 1, -1, password);
    // Bad name first: the dialog comes back with an error.
    if (ExpectDialog(D_CHAR_NAME)) Respond(D_CHAR_NAME, 1, -1, "not a name");
    Dialog retry;
    if (ExpectDialog(D_CHAR_NAME, &retry))
    {
        if (retry.text.find("isn't a valid") == std::string::npos) Fail("invalid name wasn't explained");
        Respond(D_CHAR_NAME, 1, -1, character);
    }
    if (ExpectDialog(D_CHAR_GENDER)) Respond(D_CHAR_GENDER, 1, 0, "");
    if (ExpectDialog(D_CHAR_SKIN)) Respond(D_CHAR_SKIN, 1, 2, "");
    ExpectMessage("You are now playing as", 0, 10000);
    // The Kingmakers client HUD arrives on the next 1 s tick after spawning.
    ExpectMessage("KM1|hud|cash=500;bank=0;lvl=1", kKmuiColour, 5000);

    printf("== commands\n");
    Command("/stats");
    ExpectMessage("Level 1");
    ExpectMessage("Cash $500");
    Command("/balance");
    ExpectMessage("Use an ATM");
    Command("/notacommand");
    ExpectMessage("Unknown command");
    Command("/me waves");
    ExpectMessage("waves");
    Command("/jobs");
    Pump(1000);

    printf("== Kingmakers UI protocol\n");
    Command("/phone");
    ExpectMessage("KM1|phone|has=0", kKmuiColour);
    Disconnect();

    printf("== log back in\n");
    if (!Connect(host, port)) return 1;
    if (ExpectDialog(D_LOGIN)) Respond(D_LOGIN, 1, -1, "wrong-password");
    Dialog again;
    if (ExpectDialog(D_LOGIN, &again))
    {
        if (again.text.find("Wrong password") == std::string::npos) Fail("wrong password wasn't explained");
        Respond(D_LOGIN, 1, -1, password);
    }
    Dialog list;
    if (ExpectDialog(D_CHAR_SELECT, &list))
    {
        if (list.text.find(character) == std::string::npos) Fail("character missing from the list");
        Respond(D_CHAR_SELECT, 1, 0, "");
    }
    ExpectMessage("You are now playing as", 0, 10000);
    Command("/stats");
    ExpectMessage(character.c_str());
    Disconnect();

    printf(g_failures ? "KMBOT DONE: %d failure(s)\n" : "KMBOT DONE: all steps passed\n", g_failures);
    return g_failures ? 1 : 0;
}
