# Kingmakers UI protocol (KMUI), version 1

The Kingmakers client draws a stats panel, a speedometer, toasts and a phone
screen. The server drives them with ordinary SA:MP 0.3.7 messages, so it
needs no network plugin and stock clients keep working.

| Side | File |
|---|---|
| Server | `server/gamemodes/km/kmui.inc` |
| Client, native | `app/src/main/cpp/samp/kmui/kmui.cpp` (events, speed, command queue), `kmui_parse.cpp` (parser, host-tested by `app/src/test/kmui/run.sh`) |
| Client, Java | `app/src/main/java/com/samp/mobile/game/ui/KmUi.java` (views) |

## Handshake

Right after joining (the `InitGame` RPC), the client sends the command

```
/kmclient 1
```

The number is the highest protocol version it speaks. The server accepts it
before login, remembers `min(client, server)` for that player and answers
with a `hello` event. Players who never send it are treated as stock
clients and get no UI events.

## Events (server to client)

An event is a client message (`SendClientMessage`) with colour
**`0x4B4D5501`** and the text

```
KM1|<event>|key=value;key=value
```

- The whole message is at most 143 characters, the SA:MP limit.
- Values never contain `|`, `;` or `=`. The server replaces them (and
  control characters) with spaces.
- Text is windows-1251, like all SA:MP strings. The client converts it to
  UTF-8.
- The client drops every message in that colour that parses as an event, so
  it never shows up in chat. It ignores unknown events and keys, so a newer
  server can add more.

| Event | Keys | When |
|---|---|---|
| `hello` | `v` agreed version | Answer to `/kmclient` |
| `hud` | `cash`, `bank`, `lvl`, `xp`, `xpmax` (paydays needed for the next level), `job` (name), `wanted`, `fuel` (0-100, or -1 when not driving a vehicle that uses fuel) | Checked every second, sent only when something changed |
| `toast` | `text` | Job pay, payday, level up, new SMS |
| `sms` | `from` (number), `text` | Incoming `/sms` (also shown in chat) |
| `phone` | `has` (0/1), `number`, `bank` | Answer to `/phone` |

Example:

```
KM1|hud|cash=120;bank=5000;lvl=3;xp=2;xpmax=12;job=Taxi driver;wanted=1;fuel=-1
```

## Client to server

The Java UI only sends normal chat commands. The native side queues them
and sends them from the game thread, at most 8 waiting, each 2-127
characters and starting with `/`:

| Button | Command |
|---|---|
| Phone (stats panel) | `/phone` |
| Phone: New message | `/sms <number> <text>` |
| Phone: Call a taxi | `/calltaxi` |
| Phone: Emergency | `/911 <text>` |

The server checks these like any typed command, so the buttons can't do
anything a player couldn't type.

## Client-side only

The speedometer reads the local vehicle's speed every 200 ms
(`|move speed| x 180`, the same km/h scale as `GetVehicleVelocity`). It
needs nothing from the server.

## Changing the protocol

- New events or keys: just add them. Old clients ignore them.
- Anything that changes the meaning of an existing key: bump the version in
  both `KMUI_VERSION` (server) and `KMUI::kVersion` (client), and keep
  sending version 1 to clients that announce 1.
