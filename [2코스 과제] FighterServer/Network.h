#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include "Session.h"
#include "List.h"

void StartServer(linger _linger, ULONG nonBlocking);
void StopServer();
void Network();

extern List<Session*> sessionList;