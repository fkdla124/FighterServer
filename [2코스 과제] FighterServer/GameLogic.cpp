#include <cstdio>

#include "Network.h"
#include "List.h"
#include "Session.h"
#include "PacketDefine.h"
#include "Define.h"
#include "Debug.h"
#include "FrameControl.h"

// #define min(a, b) ((a) < (b) ? (a) : (b))

// deltaTime 계산 기준 틱
static unsigned int gameLogicTick = timeGetTime();

// ms 단위 deltaTime
static unsigned int deltaTime;

// ms단위 이동속도
static float moveSpeedX = MOVE_SPEED_X_PER_MILLISECONDS;
static float moveSpeedY = MOVE_SPEED_Y_PER_MILLISECONDS;

static void MovePlayer(Session* session, char directionX, char directionY);

void GameLogic()
{
	// 이번 로직 deltaTime 계산
	deltaTime = timeGetTime() - gameLogicTick;

	// Logic FPS 제한
	if (deltaTime < (1000 / GAME_LOGIC_FPS_LIMIT))
		return;

	// 기준틱 갱신
	gameLogicTick = timeGetTime();

	// ※ FPS 제한 삭제. 네트워크 처리를 최대한 빨리 돌리기위함.
	// 현재는 FPS 출력만하는중.
	logicFpsCount++;
	DisplayFPS();
	NetInfoPrint();

	for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end(); ++it)
	{
		if ((*it)->move == true)
		{
			switch ((*it)->direction)
			{
				case PACKET_MOVE_DIR_DD: // ↓
					MovePlayer((*it), 0, 1);
					break;
				case PACKET_MOVE_DIR_RD: // ↘
					MovePlayer((*it), 1, 1);
					break;
				case PACKET_MOVE_DIR_RR: // →
					MovePlayer((*it), 1, 0);
					break;
				case PACKET_MOVE_DIR_RU: // ↗
					MovePlayer((*it), 1, -1);
					break;
				case PACKET_MOVE_DIR_UU: // ↑
					MovePlayer((*it), 0, -1);
					break;
				case PACKET_MOVE_DIR_LU: // ↖
					MovePlayer((*it), -1, -1);
					break;
				case PACKET_MOVE_DIR_LL: // ←
					MovePlayer((*it), -1, 0);
					break;
				case PACKET_MOVE_DIR_LD: // ↙
					MovePlayer((*it), -1, 1);
					break;
			}

			DebugPrint("SessionID: %d, Direction: %d, X: %f, Y: %f\n", (*it)->id, (*it)->direction, (*it)->x, (*it)->y);
		}
	}
}

// direction: -1, 0, 1
static void MovePlayer(Session* session, char directionX, char directionY)
{
	float nextX = session->x + deltaTime * moveSpeedX * directionX;
	float nextY = session->y + deltaTime * moveSpeedY * directionY;
	if (nextX <= RANGE_MOVE_LEFT || nextX >= RANGE_MOVE_RIGHT || nextY <= RANGE_MOVE_TOP || nextY >= RANGE_MOVE_BOTTOM)
		return;

	session->x = nextX;
	session->y = nextY;
}