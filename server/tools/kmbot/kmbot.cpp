// kmbot: a headless SA:MP 0.3.7 client for end-to-end tests of the
// Kingmakers gamemode. It uses the mobile client's own RakNet and SA:MP
// handshake code, plays a fixed scenario through the real dialogs and
// commands, and exits 0 only if every step passed.
//
//   kmbot <host> <port> <account name> <character First_Last> [basic|economy]
//
// basic:   register, create a character, spawn, commands, UI events, log in
// economy: register, ATM deposit and withdrawal, then a full miner job loop
//          (walk to the checkpoints the server sets, mine, deliver, get paid)
// driving: RCON for money, pass the driving test, buy a vehicle at the
//          dealership (engine, lock, park), then one trucker delivery
// admin:   RCON login (KMBOT_RCON env), make itself owner, then property
//          (create, buy, enter, exit, sell), police (faction, duty, MDT),
//          and the report flow

#include "main.h"

#include <netdb.h>
#include <arpa/inet.h>
#include <chrono>
#include <cmath>
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

// Movement: once spawned, the bot reports its position like a client does,
// so the server's checkpoints and location checks see it.
struct Vec3 { float x, y, z; };
static bool g_spawned = false;
static Vec3 g_pos = {0, 0, 0};
static Vec3 g_checkpoint = {0, 0, 0};
static bool g_haveCheckpoint = false;
static uint32_t g_lastSync = 0;

#pragma pack(push, 1)
// ONFOOT_SYNC_DATA from the client's net/localplayer.h (68 bytes).
struct OnFootSync
{
    uint16_t lrAnalog, udAnalog, keys;
    float pos[3];
    float quat[4];
    uint8_t health, armour, weapon, specialAction;
    float moveSpeed[3];
    float surfOffsets[3];
    uint16_t surfId;
    uint32_t animation;
};
#pragma pack(pop)
static_assert(sizeof(OnFootSync) == 68, "on-foot sync layout");

#pragma pack(push, 1)
// INCAR_SYNC_DATA from the client's net/localplayer.h (63 bytes).
struct InCarSync
{
    uint16_t vehicleId, lrAnalog, udAnalog, keys;
    float quat[4];
    float pos[3];
    float moveSpeed[3];
    float carHealth;
    uint8_t health, armour, weapon, siren, landingGear;
    uint16_t trailerId;
    float trainSpeed;
};
#pragma pack(pop)
static_assert(sizeof(InCarSync) == 63, "in-car sync layout");

static uint16_t g_vehicle = 0;   // vehicle we're driving, 0 = on foot

static uint32_t Now()
{
    using namespace std::chrono;
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// ---- RPC handlers ----------------------------------------------------------

static void OnSetSpawnInfo(RPCParameters* params)
{
    RakNet::BitStream bs(params->input, (params->numberOfBitsOfData / 8) + 1, false);
    uint8_t team, unk; int skin;
    bs.Read(team); bs.Read(skin); bs.Read(unk);
    bs.Read(g_pos.x); bs.Read(g_pos.y); bs.Read(g_pos.z);
}

static void OnSetPlayerPos(RPCParameters* params)
{
    RakNet::BitStream bs(params->input, (params->numberOfBitsOfData / 8) + 1, false);
    bs.Read(g_pos.x); bs.Read(g_pos.y); bs.Read(g_pos.z);
}

static void OnSetCheckpoint(RPCParameters* params)
{
    RakNet::BitStream bs(params->input, (params->numberOfBitsOfData / 8) + 1, false);
    bs.Read(g_checkpoint.x); bs.Read(g_checkpoint.y); bs.Read(g_checkpoint.z);
    g_haveCheckpoint = true;
    printf("  << checkpoint %.1f %.1f %.1f\n", g_checkpoint.x, g_checkpoint.y, g_checkpoint.z);
}

static void OnDisableCheckpoint(RPCParameters*) { g_haveCheckpoint = false; }

static void OnPutInVehicle(RPCParameters* params)
{
    RakNet::BitStream bs(params->input, (params->numberOfBitsOfData / 8) + 1, false);
    uint16_t id; uint8_t seat;
    bs.Read(id); bs.Read(seat);
    g_vehicle = id;
    printf("  << put in vehicle %u\n", id);
}

static void OnRemoveFromVehicle(RPCParameters*) { g_vehicle = 0; }

static void OnVehicleRemoved(RPCParameters* params)
{
    RakNet::BitStream bs(params->input, (params->numberOfBitsOfData / 8) + 1, false);
    uint16_t id;
    bs.Read(id);
    if (id == g_vehicle) { g_vehicle = 0; printf("  << our vehicle %u was removed\n", id); }
}

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
        g_spawned = true;
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

static void SendSync()
{
    if (g_vehicle != 0)
    {
        InCarSync car = {};
        car.vehicleId = g_vehicle;
        car.quat[0] = 1.0f;
        car.pos[0] = g_pos.x; car.pos[1] = g_pos.y; car.pos[2] = g_pos.z;
        car.carHealth = 1000.0f;
        car.health = 100;
        RakNet::BitStream bs;
        bs.Write((uint8_t)ID_VEHICLE_SYNC);
        bs.Write((char*)&car, sizeof(car));
        g_client->Send(&bs, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 0);
        return;
    }
    OnFootSync sync = {};
    sync.pos[0] = g_pos.x; sync.pos[1] = g_pos.y; sync.pos[2] = g_pos.z;
    sync.quat[0] = 1.0f;
    sync.health = 100;
    RakNet::BitStream bs;
    bs.Write((uint8_t)ID_PLAYER_SYNC);
    bs.Write((char*)&sync, sizeof(sync));
    g_client->Send(&bs, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 1);
}

static void Pump(uint32_t ms)
{
    uint32_t end = Now() + ms;
    do {
        if (g_spawned && Now() - g_lastSync >= 150)
        {
            SendSync();
            g_lastSync = Now();
        }
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
    g_connected = false; g_spectating = true; g_spawned = false; g_haveCheckpoint = false; g_vehicle = 0;
    g_dialogs.clear(); g_messages.clear();
    g_client = RakNetworkFactory::GetRakClientInterface();
    g_client->RegisterAsRemoteProcedureCall(&RPC_InitGame, OnInitGame);
    g_client->RegisterAsRemoteProcedureCall(&RPC_DialogBox, OnDialog);
    g_client->RegisterAsRemoteProcedureCall(&RPC_ClientMessage, OnClientMessage);
    g_client->RegisterAsRemoteProcedureCall(&RPC_ScrTogglePlayerSpectating, OnToggleSpectating);
    g_client->RegisterAsRemoteProcedureCall(&RPC_ScrSetSpawnInfo, OnSetSpawnInfo);
    g_client->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerPos, OnSetPlayerPos);
    g_client->RegisterAsRemoteProcedureCall(&RPC_SetCheckpoint, OnSetCheckpoint);
    g_client->RegisterAsRemoteProcedureCall(&RPC_DisableCheckpoint, OnDisableCheckpoint);
    g_client->RegisterAsRemoteProcedureCall(&RPC_ScrPutPlayerInVehicle, OnPutInVehicle);
    g_client->RegisterAsRemoteProcedureCall(&RPC_ScrRemovePlayerFromVehicle, OnRemoveFromVehicle);
    g_client->RegisterAsRemoteProcedureCall(&RPC_WorldVehicleRemove, OnVehicleRemoved);
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

// Moves to a point and gives the server a moment to see it.
static void MoveTo(float x, float y, float z)
{
    g_pos = {x, y, z};
    printf("  >> move to %.1f %.1f %.1f\n", x, y, z);
    Pump(1200);
}

// Waits for the server to set a checkpoint (one we haven't walked into yet).
static bool ExpectCheckpoint(uint32_t timeout = 8000)
{
    uint32_t end = Now() + timeout;
    while (Now() < end && !g_haveCheckpoint) Pump(50);
    if (!g_haveCheckpoint) { Fail("no checkpoint was set"); return false; }
    return true;
}

static void EnterCheckpoint()
{
    Vec3 cp = g_checkpoint;
    g_haveCheckpoint = false;
    g_messages.clear();
    MoveTo(cp.x, cp.y, cp.z + 0.5f);
}

static const std::string kPassword = "kmbot-pass-123";
static void RegisterAndCreate(const std::string& character);

// RCON goes in packet 201 as a 32-bit length and the command text.
static void Rcon(const std::string& command)
{
    RakNet::BitStream bs;
    bs.Write((uint8_t)ID_RCON_COMMAND);
    bs.Write((uint32_t)command.size());
    bs.Write(command.c_str(), command.size());
    g_client->Send(&bs, HIGH_PRIORITY, RELIABLE, 0);
    printf("  >> rcon %s\n", command.substr(0, 5).c_str());
}

// Gets out like the client does: tell the server, then walk.
static void ExitVehicle()
{
    if (g_vehicle == 0) return;
    RakNet::BitStream bs;
    bs.Write(g_vehicle);
    g_client->RPC(&RPC_ExitVehicle, &bs, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, false, UNASSIGNED_NETWORK_ID, nullptr);
    printf("  >> exit vehicle %u\n", g_vehicle);
    g_vehicle = 0;
    g_pos.x += 2.0f;
    Pump(1000);
}

static bool BecomeOwner(const std::string& character)
{
    const char* rconPassword = getenv("KMBOT_RCON");
    if (!rconPassword) { Fail("KMBOT_RCON isn't set"); return false; }
    Rcon(std::string("login ") + rconPassword);
    Pump(1000);
    Command("/setadmin " + character + " 5");
    return ExpectMessage("Your admin level is now 5");
}

static void DrivingScenario(const char* host, int port, const std::string& character)
{
    printf("== register %s and create %s\n", g_name.c_str(), character.c_str());
    if (!Connect(host, port)) return;
    RegisterAndCreate(character);
    Pump(1500);
    if (!BecomeOwner(character)) return;
    Command("/givemoney " + character + " 60000 kmbot driving");
    ExpectMessage("Their bank balance is $60000");

    printf("== driving test\n");
    MoveTo(1490.0f, -1771.0f, 18.8f);
    g_haveCheckpoint = false;
    Command("/drivingtest");
    ExpectMessage("Drive through the");
    if (g_vehicle == 0) Fail("not put in the test car");
    for (int stop = 0; stop < 5; stop++)
    {
        if (!ExpectCheckpoint()) break;
        EnterCheckpoint();
    }
    ExpectMessage("You passed!", 0, 4000);
    g_vehicle = 0;  // the test car is removed
    Pump(1000);
    Command("/licence");
    ExpectMessage("You have a driving licence");

    printf("== dealership\n");
    MoveTo(2131.8f, -1150.5f, 24.1f);
    g_dialogs.clear();
    Command("/buyvehicle");
    if (ExpectDialog(9)) Respond(9, 1, 0, "");          // the first model: Faggio
    ExpectMessage("You bought a Faggio (plate KM-");
    // Driving a vehicle that uses fuel puts the fuel level on the HUD.
    ExpectMessage("fuel=100", kKmuiColour, 4000);
    if (g_vehicle == 0) Fail("not put in the new vehicle");
    Command("/engine");
    ExpectMessage("starts the engine");
    MoveTo(2150.0f, -1120.0f, 25.0f);
    Command("/park");
    ExpectMessage("Parked.");
    Command("/lock");
    ExpectMessage("locks their vehicle");
    Command("/myvehicles");
    Dialog list;
    if (ExpectDialog(8, &list) && list.text.find("Faggio") == std::string::npos) Fail("Faggio missing from /myvehicles");
    ExitVehicle();

    printf("== trucker delivery\n");
    MoveTo(2197.0f, -2662.9f, 13.5f);
    Command("/takejob");
    ExpectMessage("You're now working as: Trucker");
    g_haveCheckpoint = false;
    Command("/work");
    ExpectMessage("Shift started as Trucker");
    if (g_vehicle == 0) Fail("not put in the truck");
    if (ExpectCheckpoint())
    {
        EnterCheckpoint();
        ExpectMessage("for truck delivery", kKmuiColour, 5000);
    }
    Command("/stopwork");
    ExpectMessage("Shift ended");
    Disconnect();
}

static void AdminScenario(const char* host, int port, const std::string& character)
{
    const char* rconPassword = getenv("KMBOT_RCON");
    if (!rconPassword) { Fail("KMBOT_RCON isn't set"); return; }

    printf("== register %s and create %s\n", g_name.c_str(), character.c_str());
    if (!Connect(host, port)) return;
    RegisterAndCreate(character);
    Pump(1500);

    printf("== become owner\n");
    Rcon(std::string("login ") + rconPassword);
    Pump(1000);
    Command("/setadmin " + character + " 5");
    ExpectMessage("Your admin level is now 5");
    Command("/check " + character);
    ExpectMessage("admin 5");

    printf("== property\n");
    Command("/createproperty 0 1000 Bot House");
    ExpectMessage("Created property #");
    Pump(1000);
    Command("/buyproperty");
    ExpectMessage("You need $1000");
    Command("/givemoney " + character + " 2000 kmbot test");
    ExpectMessage("Their bank balance is $2000");
    Command("/buyproperty");
    ExpectMessage("You bought Bot House for $1000");
    Vec3 door = g_pos;
    Command("/enter");
    Pump(1500);
    // Interior preset 0 ("Small house") is at 223.0, 1287.1, 1082.1.
    if (fabsf(g_pos.x - 223.0f) > 2.0f || fabsf(g_pos.y - 1287.1f) > 2.0f) Fail("/enter didn't move us into the house");
    Command("/exit");
    Pump(1000);
    if (fabsf(g_pos.x - door.x) > 3.0f || fabsf(g_pos.y - door.y) > 3.0f) Fail("/exit didn't put us back at the door");
    Command("/myproperties");
    Pump(500);
    Command("/sellproperty confirm");
    ExpectMessage("Sold. $600 was paid");
    Command("/stats");
    ExpectMessage("Bank $1600");

    printf("== police\n");
    Command("/setfaction " + character + " 1 6");
    ExpectMessage("Los Santos Police Department at rank 6");
    MoveTo(1553.6f, -1675.6f, 16.2f);
    Command("/duty");
    ExpectMessage("is now on duty");
    g_dialogs.clear();
    Command("/mdt " + character);
    Dialog mdt;
    if (ExpectDialog(8, &mdt))
    {
        if (mdt.text.find("Driving licence: none") == std::string::npos) Fail("MDT is missing the licence line");
        if (mdt.text.find("Unpaid tickets: 0") == std::string::npos) Fail("MDT is missing the tickets line");
    }
    Command("/duty");
    ExpectMessage("is now off duty");

    printf("== reports\n");
    Command("/report Testing the report system");
    ExpectMessage("Request #");
    // Use the number the server gave the request.
    std::string id = "0";
    for (auto& m : g_messages)
    {
        size_t at = m.text.find("Request #");
        if (at != std::string::npos) id = std::to_string(atoi(m.text.c_str() + at + 9));
    }
    Command("/reports");
    ExpectMessage("Testing the report system");
    Command("/ar " + id);
    ExpectMessage(("is handling your request #" + id).c_str());
    Command("/cr " + id);
    ExpectMessage(("Your request #" + id + " was closed").c_str());
    Disconnect();
}

static void RegisterAndCreate(const std::string& character)
{
    if (ExpectDialog(D_REGISTER)) Respond(D_REGISTER, 1, -1, kPassword);
    if (ExpectDialog(D_REGISTER_CONFIRM)) Respond(D_REGISTER_CONFIRM, 1, -1, kPassword);
    if (ExpectDialog(D_CHAR_NAME)) Respond(D_CHAR_NAME, 1, -1, character);
    if (ExpectDialog(D_CHAR_GENDER)) Respond(D_CHAR_GENDER, 1, 0, "");
    if (ExpectDialog(D_CHAR_SKIN)) Respond(D_CHAR_SKIN, 1, 0, "");
    ExpectMessage("You are now playing as", 0, 10000);
}

static void EconomyScenario(const char* host, int port, const std::string& character)
{
    printf("== register %s and create %s\n", g_name.c_str(), character.c_str());
    if (!Connect(host, port)) return;
    RegisterAndCreate(character);
    Pump(1500);

    printf("== ATM (Idlewood gas station)\n");
    MoveTo(1928.6f, -1786.1f, 13.5f);
    Command("/deposit 200");
    ExpectMessage("Deposited $200");
    Command("/withdraw 50");
    ExpectMessage("Withdrew $50");
    Command("/balance");
    ExpectMessage("Cash: $350 | Bank: $150");
    Command("/deposit 99999");
    ExpectMessage("don't have that much cash");

    printf("== miner job (Hunter Quarry)\n");
    MoveTo(588.6f, 868.5f, -42.5f);
    Command("/takejob");
    ExpectMessage("You're now working as: Miner");
    g_haveCheckpoint = false;
    Command("/work");
    ExpectMessage("Shift started as Miner");
    if (ExpectCheckpoint())
    {
        EnterCheckpoint();                              // the rock face
        ExpectMessage("swings a pickaxe", 0, 4000);
        if (ExpectCheckpoint(10000))                    // mined: carry to the drop-off
        {
            EnterCheckpoint();
            ExpectMessage("KM1|toast|text=+$90 for ore", kKmuiColour, 5000);
            ExpectCheckpoint();                         // the next rock
        }
    }
    Command("/stopwork");
    ExpectMessage("Shift ended");
    Command("/stats");
    ExpectMessage("Cash $440");
    Disconnect();
}

int main(int argc, char** argv)
{
    if (argc < 5) { printf("usage: kmbot <host> <port> <account> <First_Last> [basic|economy|admin|driving]\n"); return 2; }
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
    const std::string& password = kPassword;
    if (argc > 5 && strcmp(argv[5], "basic") != 0)
    {
        if (strcmp(argv[5], "economy") == 0) EconomyScenario(host, port, character);
        else if (strcmp(argv[5], "admin") == 0) AdminScenario(host, port, character);
        else if (strcmp(argv[5], "driving") == 0) DrivingScenario(host, port, character);
        else { printf("unknown scenario %s\n", argv[5]); return 2; }
        printf(g_failures ? "KMBOT DONE: %d failure(s)\n" : "KMBOT DONE: all steps passed\n", g_failures);
        return g_failures ? 1 : 0;
    }

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
