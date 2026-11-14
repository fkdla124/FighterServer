#pragma once

#include "Network.h"
#include "RingBuffer.h"

struct Session
{
	bool del;

	SOCKET sock;
	RingBuffer sendRingBuf;
	RingBuffer recvRingBuf;

	SOCKADDR_IN addr;

	int id;
	int hp;
	char direction;
	unsigned short x;
	unsigned short y;

	bool move;
};