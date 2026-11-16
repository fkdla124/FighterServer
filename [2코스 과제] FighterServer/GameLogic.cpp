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
					if ((*it)->y + MOVE_CHARACTER_PER_FRAME_Y >= RANGE_MOVE_BOTTOM)
						break;

					(*it)->y += MOVE_CHARACTER_PER_FRAME_Y;
					break;
				case PACKET_MOVE_DIR_RD: // ↘
					if ((*it)->y + MOVE_CHARACTER_PER_FRAME_Y >= RANGE_MOVE_BOTTOM || (*it)->x + MOVE_CHARACTER_PER_FRAME_X >= RANGE_MOVE_RIGHT)
						break;

					(*it)->x += MOVE_CHARACTER_PER_FRAME_X;
					(*it)->y += MOVE_CHARACTER_PER_FRAME_Y;
					break;
				case PACKET_MOVE_DIR_RR: // →
					if ((*it)->x + MOVE_CHARACTER_PER_FRAME_X >= RANGE_MOVE_RIGHT)
						break;

					(*it)->x += MOVE_CHARACTER_PER_FRAME_X;
					break;
				case PACKET_MOVE_DIR_RU: // ↗
					if ((*it)->y - MOVE_CHARACTER_PER_FRAME_Y <= RANGE_MOVE_TOP || (*it)->x + MOVE_CHARACTER_PER_FRAME_X >= RANGE_MOVE_RIGHT)
						break;

					(*it)->x += MOVE_CHARACTER_PER_FRAME_X;
					(*it)->y -= MOVE_CHARACTER_PER_FRAME_Y;
					break;
				case PACKET_MOVE_DIR_UU: // ↑
					if ((*it)->y - MOVE_CHARACTER_PER_FRAME_Y <= RANGE_MOVE_TOP)
						break;

					(*it)->y -= MOVE_CHARACTER_PER_FRAME_Y;
					break;
				case PACKET_MOVE_DIR_LU: // ↖
					if ((*it)->y - MOVE_CHARACTER_PER_FRAME_Y <= RANGE_MOVE_TOP || (*it)->x - MOVE_CHARACTER_PER_FRAME_X <= RANGE_MOVE_LEFT)
						break;

					(*it)->x -= MOVE_CHARACTER_PER_FRAME_X;
					(*it)->y -= MOVE_CHARACTER_PER_FRAME_Y;
					break;
				case PACKET_MOVE_DIR_LL: // ←
					if ((*it)->x - MOVE_CHARACTER_PER_FRAME_X <= RANGE_MOVE_LEFT)
						break;

					(*it)->x -= MOVE_CHARACTER_PER_FRAME_X;
					break;
				case PACKET_MOVE_DIR_LD: // ↙
					if ((*it)->y + MOVE_CHARACTER_PER_FRAME_Y >= RANGE_MOVE_BOTTOM || (*it)->x - MOVE_CHARACTER_PER_FRAME_X <= RANGE_MOVE_LEFT)
						break;

					(*it)->x -= MOVE_CHARACTER_PER_FRAME_X;
					(*it)->y += MOVE_CHARACTER_PER_FRAME_Y;
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