#include <cstdio>

#include "GameLogic.h"
#include "Network.h"
#include "List.h"
#include "Session.h"
#include "PacketDefine.h"
#include "Debug.h"

// #define min(a, b) ((a) < (b) ? (a) : (b))

void GameLogic()
{
	for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end(); ++it)
	{
		if ((*it)->move == true)
		{
			switch ((*it)->direction)
			{
				case PACKET_MOVE_DIR_DD: // ↓
					(*it)->y = min((*it)->y + MOVE_CHARACTER_PER_FRAME, MAP_MAX_Y);
					break;
				case PACKET_MOVE_DIR_RD: // ↘
					(*it)->x = min((*it)->x + MOVE_CHARACTER_PER_FRAME, MAP_MAX_X);
					(*it)->y = min((*it)->y + MOVE_CHARACTER_PER_FRAME, MAP_MAX_Y);
					break;
				case PACKET_MOVE_DIR_RR: // →
					(*it)->x = min((*it)->x + MOVE_CHARACTER_PER_FRAME, MAP_MAX_X);
					break;
				case PACKET_MOVE_DIR_RU: // ↗
					(*it)->x = min((*it)->x + MOVE_CHARACTER_PER_FRAME, MAP_MAX_X);
					(*it)->y = max((*it)->y - MOVE_CHARACTER_PER_FRAME, 0);
					break;
				case PACKET_MOVE_DIR_UU: // ↑
					(*it)->y = max((*it)->y - MOVE_CHARACTER_PER_FRAME, 0);
					break;
				case PACKET_MOVE_DIR_LU: // ↖
					(*it)->x = max((*it)->x - MOVE_CHARACTER_PER_FRAME, 0);
					(*it)->y = min((*it)->y + MOVE_CHARACTER_PER_FRAME, MAP_MAX_Y);
					break;
				case PACKET_MOVE_DIR_LL: // ←
					(*it)->x = max((*it)->x - MOVE_CHARACTER_PER_FRAME, 0);
					break;
				case PACKET_MOVE_DIR_LD: // ↙
					(*it)->x = max((*it)->x - MOVE_CHARACTER_PER_FRAME, 0);
					(*it)->y = max((*it)->y - MOVE_CHARACTER_PER_FRAME, 0);
					break;
			}

			DebugPrint("ID: %d, Direction: %d, X: %d, Y: %d\n", (*it)->id, (*it)->direction, (*it)->x, (*it)->y);
		}

		clock_t currentTick = clock();

		(*it)->attack1Cooldown += currentTick - (*it)->attack1BaseTick;
		(*it)->attack2Cooldown += currentTick - (*it)->attack2BaseTick;
		(*it)->attack3Cooldown += currentTick - (*it)->attack3BaseTick;
		(*it)->attack1BaseTick = currentTick;
		(*it)->attack2BaseTick = currentTick;
		(*it)->attack3BaseTick = currentTick;
	}
}