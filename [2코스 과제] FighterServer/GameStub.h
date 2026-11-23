#pragma once

#include "Session.h"
#include "RPC_ServerStub.h"
#include "RPC_ServerProxy.h"

static bool CheckCollision(char attackType, char attackerFacing, float attackerX, float attackerY, float targetX, float targetY);

class GameStub : public Stub
{
public:
	bool MoveStart(Session* session, char direction, short x, short y) override
	{
		// 방향 0~7 이외가 오면 연결끊기.
		if (direction < PACKET_MOVE_DIR_LL || direction > PACKET_MOVE_DIR_LD)
		{
			DebugPrint("[MoveStart Direction Error -> Disconnect Client] SessionID: %d, Direction: %d, X: %f, Y: %f\n",
				session->id, direction, x, y);
			return false;
		}

		// 만약 서버의 좌표와 50 넘게 차이나거나, 맵 밖이라면 연결 끊어버리기.
		if (abs(x - session->x) > RANGE_MOVE_ERROR || abs(y - session->y) > RANGE_MOVE_ERROR
			|| x <= RANGE_MOVE_LEFT || x >= RANGE_MOVE_RIGHT || y <= RANGE_MOVE_TOP || y >= RANGE_MOVE_BOTTOM)
		{
			DebugPrint("[MoveStart Position Error -> Disconnect Client] SessionID: %d, Direction: %d, X: %f, Y: %f, X-diff: %f, Y-diff: %f\n",
				session->id, direction, x, y, abs(x - session->x), abs(y - session->y));
			return false;
		}

		// 서버 게임로직에 반영
		session->direction = direction;
		session->move = true;
		session->x = x;
		session->y = y;

		// facing 방향 처리
		switch (direction)
		{
			case PACKET_MOVE_DIR_RR:
			case PACKET_MOVE_DIR_RU:
			case PACKET_MOVE_DIR_RD:
				session->facing = PACKET_MOVE_DIR_RR;
				break;
			case PACKET_MOVE_DIR_LU:
			case PACKET_MOVE_DIR_LL:
			case PACKET_MOVE_DIR_LD:
				session->facing = PACKET_MOVE_DIR_LL;
				break;
		}

		// 디버깅 프린트
		DebugPrint("[Client MoveStart] SessionID(server): %d, Direction: %d, Facing(server): %d, X: %f, Y: %f\n", session->id, session->direction, session->facing, session->x, session->y);

		// RPC
		RPC::MoveStart(SendType::Broadcast, session, session->id, session->direction, session->x, session->y);
		return true;
	}

	bool MoveStop(Session* session, char direction, short x, short y) override
	{
		// 방향 0~7 이외가 오면 연결끊기.
		if (direction < PACKET_MOVE_DIR_LL || direction > PACKET_MOVE_DIR_LD)
		{
			DebugPrint("[MoveStop Direction Error -> Disconnect Client] SessionID: %d, Direction: %d, X: %f, Y: %f\n",
				session->id, direction, x, y);
			return false;
		}

		// 만약 서버의 좌표와 50 넘게 차이나거나, 맵 밖이라면 연결 끊어버리기.
		if (abs(x - session->x) > RANGE_MOVE_ERROR || abs(y - session->y) > RANGE_MOVE_ERROR
			|| x <= RANGE_MOVE_LEFT || x >= RANGE_MOVE_RIGHT || y <= RANGE_MOVE_TOP || y >= RANGE_MOVE_BOTTOM)
		{
			DebugPrint("[MoveStop Position Error -> Disconnect Client] SessionID: %d, Direction: %d, X: %f, Y: %f, X-diff: %f, Y-diff: %f\n",
				session->id, direction, x, y, abs(x - session->x), abs(y - session->y));
			return false;
		}

		session->direction = direction;
		session->move = false;
		session->x = x;
		session->y = y;

		switch (direction)
		{
			case PACKET_MOVE_DIR_RR:
			case PACKET_MOVE_DIR_RU:
			case PACKET_MOVE_DIR_RD:
				session->facing = PACKET_MOVE_DIR_RR;
				break;
			case PACKET_MOVE_DIR_LU:
			case PACKET_MOVE_DIR_LL:
			case PACKET_MOVE_DIR_LD:
				session->facing = PACKET_MOVE_DIR_LL;
				break;
		}

		DebugPrint("[Client MoveStop] SessionID(server): %d, Direction: %d, Facing(server): %d, X: %f, Y: %f\n", session->id, session->direction, session->facing, session->x, session->y);

		RPC::MoveStop(SendType::Broadcast, session, session->id, session->direction, session->x, session->y);
		return true;
	}

	bool Attack1(Session* session, char direction, short x, short y) override
	{
		// 지난 공격의 쿨타임이 지나지 않았다면 이번패킷은 무시하고 다음메시지 처리
		unsigned int attackTickDiff = timeGetTime() - session->lastAttackTick;
		if (attackTickDiff <= session->lastAttackDuration - ATTACK_TIME_TOLERANCE)
		{
			DebugPrint("[Attack1 Tolerance] lastAttackTick: %d, lastAttackDuration: %d, ATTACK_TIME_TOLERANCE: %d, attackTickDiff: %d\n", session->lastAttackTick, session->lastAttackDuration, ATTACK_TIME_TOLERANCE, attackTickDiff);
			return true; // 이 패킷은 무시하기만 할거임.
		}

		// 쿨타임을 위해 해당 공격 타입과 현재 틱 저장
		session->lastAttackTick = timeGetTime();
		session->lastAttackDuration = ATTACK1_ANIMATION_DURATION;

		// 방향 0~7 이외가 오면 연결끊기.
		if (direction < PACKET_MOVE_DIR_LL || direction > PACKET_MOVE_DIR_LD)
		{
			DebugPrint("[Attack1Request Direction Error -> Disconnect Client] SessionID: %d, Direction: %d, X: %f, Y: %f\n",
				session->id, direction, x, y);
			return false;
		}

		// 만약 서버의 좌표와 50 넘게 차이나거나, 맵 밖이라면 연결 끊어버리기.
		if (abs(x - session->x) > RANGE_MOVE_ERROR || abs(y - session->y) > RANGE_MOVE_ERROR
			|| x <= RANGE_MOVE_LEFT || x >= RANGE_MOVE_RIGHT || y <= RANGE_MOVE_TOP || y >= RANGE_MOVE_BOTTOM)
		{
			DebugPrint("[Attack1Request Position Error -> Disconnect Client] SessionID: %d, Direction: %d, X: %f, Y: %f, X-diff: %f, Y-diff: %f\n",
				session->id, direction, x, y, abs(x - session->x), abs(y - session->y));
			return false;
		}

		session->direction = direction;
		session->x = x;
		session->y = y;

		switch (direction)
		{
			case PACKET_MOVE_DIR_RR:
			case PACKET_MOVE_DIR_RU:
			case PACKET_MOVE_DIR_RD:
				session->facing = PACKET_MOVE_DIR_RR;
				break;
			case PACKET_MOVE_DIR_LU:
			case PACKET_MOVE_DIR_LL:
			case PACKET_MOVE_DIR_LD:
				session->facing = PACKET_MOVE_DIR_LL;
				break;
		}

		DebugPrint("[Client Attack1Request] SessionID(server): %d, Direction: %d, Facing(server): %d, X: %f, Y: %f\n", session->id, session->direction, session->facing, session->x, session->y);

		RPC::Attack1(SendType::Broadcast, session, session->id, session->direction, session->x, session->y);

		// 충돌체크
		for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end(); ++it)
		{
			if (session->id != (*it)->id && CheckCollision(PACKET_REQ_ATTACK1, session->facing, session->x, session->y, (*it)->x, (*it)->y))
			{
				// 데미지 메시지 생성. 모두에게 보내기
				(*it)->hp -= ATTACK1_DAMAGE;
				RPC::Damage(SendType::Broadcast, nullptr, session->id, (*it)->id, (*it)->hp);

				// 피격자 사망.
				if ((*it)->hp <= 0)
				{
					// 세션 삭제
					DisconnectSession((*it));
				}

				// 범위 안에 한명만 처리하고 break. 여러명 해도됨.
				break;
			}
		}

		return true;
	}

	bool Attack2(Session* session, char direction, short x, short y) override
	{
		// 지난 공격의 쿨타임이 지나지 않았다면 이번패킷은 무시하고 다음메시지 처리
		unsigned int attackTickDiff = timeGetTime() - session->lastAttackTick;
		if (attackTickDiff <= session->lastAttackDuration - ATTACK_TIME_TOLERANCE)
		{
			DebugPrint("[Attack2 Tolerance] lastAttackTick: %d, lastAttackDuration: %d, ATTACK_TIME_TOLERANCE: %d, attackTickDiff: %d\n", session->lastAttackTick, session->lastAttackDuration, ATTACK_TIME_TOLERANCE, attackTickDiff);
			return true;
		}

		// 쿨타임을 위해 해당 공격 타입과 현재 틱 저장
		session->lastAttackTick = timeGetTime();
		session->lastAttackDuration = ATTACK2_ANIMATION_DURATION;

		// 방향 0~7 이외가 오면 연결끊기.
		if (direction < PACKET_MOVE_DIR_LL || direction > PACKET_MOVE_DIR_LD)
		{
			DebugPrint("[Attack2Request Direction Error -> Disconnect Client] SessionID: %d, Direction: %d, X: %f, Y: %f\n",
				session->id, direction, x, y);
			return false;
		}

		// 만약 서버의 좌표와 50 넘게 차이나거나, 맵 밖이라면 연결 끊어버리기.
		if (abs(x - session->x) > RANGE_MOVE_ERROR || abs(y - session->y) > RANGE_MOVE_ERROR
			|| x <= RANGE_MOVE_LEFT || x >= RANGE_MOVE_RIGHT || y <= RANGE_MOVE_TOP || y >= RANGE_MOVE_BOTTOM)
		{
			DebugPrint("[Attack2Request Position Error -> Disconnect Client] SessionID: %d, Direction: %d, X: %f, Y: %f, X-diff: %f, Y-diff: %f\n",
				session->id, direction, x, y, abs(x - session->x), abs(y - session->y));
			return false;
		}

		session->direction = direction;
		session->x = x;
		session->y = y;

		switch (direction)
		{
			case PACKET_MOVE_DIR_RR:
			case PACKET_MOVE_DIR_RU:
			case PACKET_MOVE_DIR_RD:
				session->facing = PACKET_MOVE_DIR_RR;
				break;
			case PACKET_MOVE_DIR_LU:
			case PACKET_MOVE_DIR_LL:
			case PACKET_MOVE_DIR_LD:
				session->facing = PACKET_MOVE_DIR_LL;
				break;
		}

		DebugPrint("[Client Attack2Request] SessionID(server): %d, Direction: %d, Facing(server): %d, X: %f, Y: %f\n", session->id, session->direction, session->facing, session->x, session->y);

		// 공격액션 다른 클라에게 브로드캐스트
		RPC::Attack2(SendType::Broadcast, session, session->id, session->direction, session->x, session->y);

		// 충돌체크
		for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end(); ++it)
		{
			if (session->id != (*it)->id && CheckCollision(PACKET_REQ_ATTACK2, session->facing, session->x, session->y, (*it)->x, (*it)->y))
			{
				// 데미지 메시지 생성. 모두에게 보내기
				(*it)->hp -= ATTACK2_DAMAGE;
				RPC::Damage(SendType::Broadcast, nullptr, session->id, (*it)->id, (*it)->hp);

				// 피격자 사망.
				if ((*it)->hp <= 0)
				{
					// 세션 삭제
					DisconnectSession((*it));
				}

				// 범위 안에 한명만 처리하고 break. 여러명 해도됨.
				break;
			}
		}

		return true;
	}

	bool Attack3(Session* session, char direction, short x, short y) override
	{
		// 지난 공격의 쿨타임이 지나지 않았다면 이번패킷은 무시하고 다음메시지 처리
		unsigned int attackTickDiff = timeGetTime() - session->lastAttackTick;
		if (attackTickDiff <= session->lastAttackDuration - ATTACK_TIME_TOLERANCE)
		{
			DebugPrint("[Attack3 Tolerance] lastAttackTick: %d, lastAttackDuration: %d, ATTACK_TIME_TOLERANCE: %d, attackTickDiff: %d\n", session->lastAttackTick, session->lastAttackDuration, ATTACK_TIME_TOLERANCE, attackTickDiff);
			return true;
		}

		// 쿨타임을 위해 해당 공격 타입과 현재 틱 저장
		session->lastAttackTick = timeGetTime();
		session->lastAttackDuration = ATTACK3_ANIMATION_DURATION;

		// 방향 0~7 이외가 오면 연결끊기.
		if (direction < PACKET_MOVE_DIR_LL || direction > PACKET_MOVE_DIR_LD)
		{
			DebugPrint("[Attack3Request Direction Error -> Disconnect Client] SessionID: %d, Direction: %d, X: %f, Y: %f\n",
				session->id, direction, x, y);
			return false;
		}

		// 만약 서버의 좌표와 50 넘게 차이나거나, 맵 밖이라면 연결 끊어버리기.
		if (abs(x - session->x) > RANGE_MOVE_ERROR || abs(y - session->y) > RANGE_MOVE_ERROR
			|| x <= RANGE_MOVE_LEFT || x >= RANGE_MOVE_RIGHT || y <= RANGE_MOVE_TOP || y >= RANGE_MOVE_BOTTOM)
		{
			DebugPrint("[Attack3Request Position Error -> Disconnect Client] SessionID: %d, Direction: %d, X: %f, Y: %f, X-diff: %f, Y-diff: %f\n",
				session->id, direction, x, y, abs(x - session->x), abs(y - session->y));
			return false;
		}

		session->direction = direction;
		session->x = x;
		session->y = y;

		switch (direction)
		{
			case PACKET_MOVE_DIR_RR:
			case PACKET_MOVE_DIR_RU:
			case PACKET_MOVE_DIR_RD:
				session->facing = PACKET_MOVE_DIR_RR;
				break;
			case PACKET_MOVE_DIR_LU:
			case PACKET_MOVE_DIR_LL:
			case PACKET_MOVE_DIR_LD:
				session->facing = PACKET_MOVE_DIR_LL;
				break;
		}

		DebugPrint("[Client Attack3Request] SessionID(server): %d, Direction: %d, Facing(server): %d, X: %f, Y: %f\n", session->id, session->direction, session->facing, session->x, session->y);

		RPC::Attack3(SendType::Broadcast, session, session->id, session->direction, session->x, session->y);

		// 충돌체크
		for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end(); ++it)
		{
			if (session->id != (*it)->id && CheckCollision(PACKET_REQ_ATTACK3, session->facing, session->x, session->y, (*it)->x, (*it)->y))
			{
				// 데미지 메시지 생성. 모두에게 보내기
				(*it)->hp -= ATTACK3_DAMAGE;
				RPC::Damage(SendType::Broadcast, nullptr, session->id, (*it)->id, (*it)->hp);

				// 피격자 사망.
				if ((*it)->hp <= 0)
				{
					// 세션 삭제
					DisconnectSession((*it));
				}

				// 범위 안에 한명만 처리하고 break. 여러명 해도됨.
				break;
			}
		}

		return true;
	}
};

bool CheckCollision(char attackType, char attackerFacing, float attackerX, float attackerY, float targetX, float targetY)
{
	switch (attackType)
	{
		case PACKET_REQ_ATTACK1:
			if (attackerFacing == PACKET_MOVE_DIR_RR)
			{
				if (targetX >= attackerX && targetX <= attackerX + ATTACK1_RANGE_X && targetY <= attackerY + ATTACK1_RANGE_Y && targetY >= attackerY - ATTACK1_RANGE_Y)
					return true;
				else
					return false;
			}
			else
			{
				if (targetX <= attackerX && targetX >= attackerX + ATTACK1_RANGE_X * -1 && targetY <= attackerY + ATTACK1_RANGE_Y && targetY >= attackerY - ATTACK1_RANGE_Y)
					return true;
				else
					return false;
			}
			break;
		case PACKET_REQ_ATTACK2:
			if (attackerFacing == PACKET_MOVE_DIR_RR)
			{
				if (targetX >= attackerX && targetX <= attackerX + ATTACK2_RANGE_X && targetY <= attackerY + ATTACK2_RANGE_Y && targetY >= attackerY - ATTACK2_RANGE_Y)
					return true;
				else
					return false;
			}
			else
			{
				if (targetX <= attackerX && targetX >= attackerX + ATTACK2_RANGE_X * -1 && targetY <= attackerY + ATTACK2_RANGE_Y && targetY >= attackerY - ATTACK2_RANGE_Y)
					return true;
				else
					return false;
			}
			break;
		case PACKET_REQ_ATTACK3:
			if (attackerFacing == PACKET_MOVE_DIR_RR)
			{
				if (targetX >= attackerX && targetX <= attackerX + ATTACK3_RANGE_X && targetY <= attackerY + ATTACK3_RANGE_Y && targetY >= attackerY - ATTACK3_RANGE_Y)
					return true;
				else
					return false;
			}
			else
			{
				if (targetX <= attackerX && targetX >= attackerX + ATTACK3_RANGE_X * -1 && targetY <= attackerY + ATTACK3_RANGE_Y && targetY >= attackerY - ATTACK3_RANGE_Y)
					return true;
				else
					return false;
			}
			break;



			// 이상한 공격타입
		default:
			return false;
	}
}