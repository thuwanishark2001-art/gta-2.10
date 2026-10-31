// Stand-in for the client's main.h: the shared RakNet sources only need
// these definitions to build as a SA:MP 0.3.7 client on Linux.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <iterator>

#define SAMP_VERSION "0.3.7"
#define RAKSAMP_CLIENT
#define NETCODE_CONNCOOKIELULZ 0x6969
// The client always defines this (true or false); samp_auth.cpp only checks
// that it exists, and then hashes with 32-bit words like the real client.
#define VER_x32 false
