#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include "RingBuffer.h"
#include "PacketDefine.h"
#include "Define.h"
#include <ctime>

// 세션별 할당 ID. 0부터 +1씩
static inline int idCount = 0;

struct Session
{
	bool del;

	SOCKET sock;
	RingBuffer sendRingBuf{ 10 * 1024 };
	RingBuffer recvRingBuf;

	SOCKADDR_IN addr;

	int id;
	int hp;

	// 이동 방향 (8방향)
	char direction;
	float x;
	float y;

	// 지금 이동중인가?
	bool move;

	// timeGetTime() 직전 공격 틱. 공격 쿨타임에 사용.
	unsigned int lastAttackTick;
	// 직전 공격의 애니메이션 시간
	short lastAttackDuration;

	// 시점 방향 (오른쪽, 왼쪽 두방향)
	char facing;

	Session(SOCKET sock, SOCKADDR_IN addr) : sock(sock), addr(addr)
	{
		del = false;
		move = false;
		id = idCount++;
		hp = 100;
		direction = PACKET_MOVE_DIR_RR;
		x = (float)(rand() % 441 + 100); // 스폰좌표 100 ~ 540 맵 최대 X=640
		y = (float)(rand() % 281 + 100); // 스폰좌표 100 ~ 380 맵 최대 Y=480
		lastAttackTick = 0;
		lastAttackDuration = ATTACK_TIME_TOLERANCE;
		facing = PACKET_MOVE_DIR_RR;
	}
};