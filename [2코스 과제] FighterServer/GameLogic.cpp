#include <cstdio>

#include "Network.h"
#include "List.h"
#include "Session.h"
#include "PacketDefine.h"
#include "Define.h"
#include "Debug.h"
#include "FrameControl.h"

// #define min(a, b) ((a) < (b) ? (a) : (b))

unsigned int gameLogicTick = timeGetTime();

void GameLogic()
{
	// ms 단위 deltaTime
	unsigned int deltaTime = timeGetTime() - gameLogicTick;

	// fps 리밋에 맞게 제한
	if (deltaTime < (1000 / GAME_LOGIC_FPS_LIMIT))
		return;

	// 기준틱 갱신
	gameLogicTick = timeGetTime();

	// ms단위 이동속도
	float moveSpeedX = MOVE_SPEED_X_PER_MILLISECONDS;
	float moveSpeedY = MOVE_SPEED_Y_PER_MILLISECONDS;

	// ※ FPS 제한 삭제. 네트워크 처리를 최대한 빨리 돌리기위함.
	// 현재는 FPS 출력만하는중.
	logicFpsCount++;
	DisplayFPS();

	for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end(); ++it)
	{
		if ((*it)->move == true)
		{
			switch ((*it)->direction)
			{
				case PACKET_MOVE_DIR_DD: // ↓
					if ((*it)->y + deltaTime * moveSpeedY >= RANGE_MOVE_BOTTOM)
						break;

					(*it)->y += deltaTime * moveSpeedY;
					break;
				case PACKET_MOVE_DIR_RD: // ↘
					if ((*it)->y + deltaTime * moveSpeedY >= RANGE_MOVE_BOTTOM || (*it)->x + deltaTime * moveSpeedX >= RANGE_MOVE_RIGHT)
						break;

					(*it)->x += deltaTime * moveSpeedX;
					(*it)->y += deltaTime * moveSpeedY;
					break;
				case PACKET_MOVE_DIR_RR: // →
					if ((*it)->x + deltaTime * moveSpeedX >= RANGE_MOVE_RIGHT)
						break;

					(*it)->x += deltaTime * moveSpeedX;
					break;
				case PACKET_MOVE_DIR_RU: // ↗
					if ((*it)->y - deltaTime * moveSpeedY <= RANGE_MOVE_TOP || (*it)->x + deltaTime * moveSpeedX >= RANGE_MOVE_RIGHT)
						break;

					(*it)->x += deltaTime * moveSpeedX;
					(*it)->y -= deltaTime * moveSpeedY;
					break;
				case PACKET_MOVE_DIR_UU: // ↑
					if ((*it)->y - deltaTime * moveSpeedY <= RANGE_MOVE_TOP)
						break;

					(*it)->y -= deltaTime * moveSpeedY;
					break;
				case PACKET_MOVE_DIR_LU: // ↖
					if ((*it)->y - deltaTime * moveSpeedY <= RANGE_MOVE_TOP || (*it)->x - deltaTime * moveSpeedX <= RANGE_MOVE_LEFT)
						break;

					(*it)->x -= deltaTime * moveSpeedX;
					(*it)->y -= deltaTime * moveSpeedY;
					break;
				case PACKET_MOVE_DIR_LL: // ←
					if ((*it)->x - deltaTime * moveSpeedX <= RANGE_MOVE_LEFT)
						break;

					(*it)->x -= deltaTime * moveSpeedX;
					break;
				case PACKET_MOVE_DIR_LD: // ↙
					if ((*it)->y + deltaTime * moveSpeedY >= RANGE_MOVE_BOTTOM || (*it)->x - deltaTime * moveSpeedX <= RANGE_MOVE_LEFT)
						break;

					(*it)->x -= deltaTime * moveSpeedX;
					(*it)->y += deltaTime * moveSpeedY;
					break;
			}

			DebugPrint("ID: %d, Direction: %d, X: %f, Y: %f\n", (*it)->id, (*it)->direction, (*it)->x, (*it)->y);
		}
	}
}