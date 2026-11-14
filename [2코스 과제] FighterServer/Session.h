#pragma once

#include "Network.h"
#include "RingBuffer.h"
#include <ctime>

struct Session
{
	bool del;

	SOCKET sock;
	RingBuffer sendRingBuf{10 * 1024};
	RingBuffer recvRingBuf;

	SOCKADDR_IN addr;

	int id;
	int hp;
	char direction;
	unsigned short x;
	unsigned short y;

	bool move;

	clock_t attack1BaseTick;
	clock_t attack2BaseTick;
	clock_t attack3BaseTick;
	clock_t attack1Cooldown;
	clock_t attack2Cooldown;
	clock_t attack3Cooldown;
};