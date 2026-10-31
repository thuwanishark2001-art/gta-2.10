#pragma once

// Kingmakers client UI protocol, version 1. See docs/KMUI.md.
//
// The server sends UI events as client messages in a reserved colour with
// the text "KM1|<event>|k=v;k=v". They are drawn by the Java UI (HUD panel,
// speedometer, toasts, phone) and never shown in chat.

#include <cstdint>
#include <map>
#include <string>

namespace KMUI
{
    constexpr uint32_t kColour = 0x4B4D5501;
    constexpr int kVersion = 1;

    using Fields = std::map<std::string, std::string>;

    // Splits "KM1|event|k=v;k=v". Returns false if the text isn't a
    // version 1 event.
    bool Parse(const char* text, std::string& event, Fields& fields);

    // Called for every client message. Returns true if it was a UI event,
    // which must not be shown in chat.
    bool HandleClientMessage(uint32_t colour, const char* text);

    // After joining: tell the server this client understands the protocol.
    void OnConnected();

    // Every frame, on the game thread: speedometer updates and commands
    // queued by the Java UI.
    void Process();

    // From the Java UI thread. Only "/..." commands are accepted.
    void QueueCommand(const std::string& command);
}
