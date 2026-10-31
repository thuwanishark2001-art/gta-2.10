#include "kmui.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>

#include "../main.h"
#include "../game/game.h"
#include "../net/netgame.h"
#include "../java/jniutil.h"
#include "../vendor/encoding/encoding.h"

extern CGame* pGame;
extern CNetGame* pNetGame;
extern CJavaWrapper* pJavaWrapper;

namespace KMUI
{
namespace
{
    std::mutex g_queueMutex;
    std::deque<std::string> g_commands;

    int g_lastSpeed = -1;
    bool g_lastInVehicle = false;
    uint32_t g_lastSpeedTick = 0;

    int FieldInt(const Fields& fields, const char* key, int fallback = 0)
    {
        auto it = fields.find(key);
        if (it == fields.end()) return fallback;
        return (int)strtol(it->second.c_str(), nullptr, 10);
    }

    std::string FieldText(const Fields& fields, const char* key)
    {
        auto it = fields.find(key);
        if (it == fields.end()) return std::string();
        // The server sends windows-1251 like every other SA:MP string.
        return Encoding::cp2utf(it->second);
    }

    void Dispatch(const std::string& event, const Fields& fields)
    {
        if (!pJavaWrapper) return;

        if (event == "hud")
        {
            pJavaWrapper->KmHud(FieldInt(fields, "cash"), FieldInt(fields, "bank"), FieldInt(fields, "lvl"),
                FieldInt(fields, "xp"), FieldInt(fields, "xpmax"), FieldText(fields, "job"),
                FieldInt(fields, "wanted"), FieldInt(fields, "fuel", -1));
        }
        else if (event == "toast")
        {
            pJavaWrapper->KmToast(FieldText(fields, "text"));
        }
        else if (event == "sms")
        {
            pJavaWrapper->KmSms(FieldInt(fields, "from"), FieldText(fields, "text"));
        }
        else if (event == "phone")
        {
            pJavaWrapper->KmPhone(FieldInt(fields, "has"), FieldInt(fields, "number"), FieldInt(fields, "bank"));
        }
        else if (event == "hello")
        {
            FLog("KMUI: server speaks protocol v%d", FieldInt(fields, "v"));
        }
        // Unknown events are ignored, so newer servers don't break older clients.
    }
}

bool HandleClientMessage(uint32_t colour, const char* text)
{
    if (colour != kColour) return false;
    std::string event;
    Fields fields;
    if (!Parse(text, event, fields)) return false;
    Dispatch(event, fields);
    return true;
}

void OnConnected()
{
    if (!pNetGame) return;
    char command[32];
    snprintf(command, sizeof(command), "/kmclient %d", kVersion);
    pNetGame->SendChatCommand(command);
    g_lastSpeed = -1;
    g_lastInVehicle = false;
}

void QueueCommand(const std::string& command)
{
    if (command.size() < 2 || command.size() > 127 || command[0] != '/') return;
    std::lock_guard<std::mutex> lock(g_queueMutex);
    if (g_commands.size() < 8) g_commands.push_back(command);
}

void Process()
{
    if (!pNetGame || pNetGame->GetGameState() != GAMESTATE_CONNECTED) return;

    {
        std::lock_guard<std::mutex> lock(g_queueMutex);
        while (!g_commands.empty())
        {
            pNetGame->SendChatCommand(g_commands.front().c_str());
            g_commands.pop_front();
        }
    }

    if (!pGame || !pJavaWrapper) return;
    uint32_t now = GetTickCount();
    if (now - g_lastSpeedTick < 200) return;
    g_lastSpeedTick = now;

    int speed = 0;
    bool inVehicle = false;
    CPlayerPed* ped = pGame->FindPlayerPed();
    if (ped && ped->IsInVehicle())
    {
        CVehicle* vehicle = ped->GetCurrentVehicle();
        if (vehicle && vehicle->m_pVehicle)
        {
            inVehicle = true;
            CVector& v = vehicle->m_pVehicle->GetMoveSpeed();
            // Same scale as GetVehicleVelocity on the server: x180 is km/h.
            speed = (int)lroundf(sqrtf(v.x * v.x + v.y * v.y + v.z * v.z) * 180.0f);
        }
    }
    if (speed == g_lastSpeed && inVehicle == g_lastInVehicle) return;
    g_lastSpeed = speed;
    g_lastInVehicle = inVehicle;
    pJavaWrapper->KmSpeed(speed, inVehicle ? 1 : 0);
}
}
