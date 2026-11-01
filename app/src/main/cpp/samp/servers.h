#pragma once

// Kingmakers RP server endpoint.
// Override at build time, e.g. in app/build.gradle cmake arguments:
//   "-DKM_SERVER_HOST=203.0.113.10" "-DKM_SERVER_PORT=7777"
// 10.0.2.2 is the host machine as seen from the Android emulator.
#ifndef KM_SERVER_HOST
#define KM_SERVER_HOST "10.0.2.2"
#endif
#ifndef KM_SERVER_PORT
#define KM_SERVER_PORT 7777
#endif
// The server's content service (docs/CONTENT.md). The launcher downloads its
// files; audio streams from it play from that copy (audiostream.cpp).
#ifndef KM_CONTENT_PORT
#define KM_CONTENT_PORT 7780
#endif

// The client picks one of these by settings.ini [client] serverid.
// All slots point at the Kingmakers RP server for now.
#define SERVER_HOST_1 KM_SERVER_HOST
#define SERVER_PORT_1 KM_SERVER_PORT

#define SERVER_HOST_2 KM_SERVER_HOST
#define SERVER_PORT_2 KM_SERVER_PORT

#define SERVER_HOST_3 KM_SERVER_HOST
#define SERVER_PORT_3 KM_SERVER_PORT

#define SERVER_HOST_4 KM_SERVER_HOST
#define SERVER_PORT_4 KM_SERVER_PORT

#define SERVER_HOST_TEST KM_SERVER_HOST
#define SERVER_PORT_TEST KM_SERVER_PORT
