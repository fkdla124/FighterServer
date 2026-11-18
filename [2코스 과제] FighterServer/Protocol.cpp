#include "Protocol.h"
#include "PacketDefine.h"

void mpCreatePlayer(MessageHeader* header, CreatePlayer* msg, int id, char direction, float x, float y, char hp)
{
	header->packetCode = (char)0x89;
	header->len = sizeof(CreatePlayer);
	header->type = PACKET_RES_CREATE_MY_CHARACTER;

	msg->id = id;
	msg->direction = direction;
	msg->x = (short)x;
	msg->y = (short)y;
	msg->hp = hp;
}

void mpCreateOther(MessageHeader* header, CreateOther* msg, int id, char direction, float x, float y, char hp)
{
	header->packetCode = (char)0x89;
	header->len = sizeof(CreateOther);
	header->type = PACKET_RES_CREATE_OTHER_CHARACTER;

	msg->id = id;
	msg->direction = direction;
	msg->x = (short)x;
	msg->y = (short)y;
	msg->hp = hp;
}

void mpDeletePlayer(MessageHeader* header, DeletePlayer* msg, int id)
{
	header->packetCode = (char)0x89;
	header->len = sizeof(DeletePlayer);
	header->type = PACKET_RES_DELETE_CHARACTER;

	msg->id = id;
}

void mpMoveStart(MessageHeader* header, MoveStart* msg, char direction, float x, float y)
{
	header->packetCode = (char)0x89;
	header->len = sizeof(MoveStart);
	header->type = PACKET_REQ_MOVE_START;

	msg->direction = direction;
	msg->x = (short)x;
	msg->y = (short)y;
}

void mpMoveStartOther(MessageHeader* header, MoveStartOther* msg, int id, char direction, float x, float y)
{
	header->packetCode = (char)0x89;
	header->len = sizeof(MoveStartOther);
	header->type = PACKET_RES_MOVE_START;

	msg->id = id;
	msg->direction = direction;
	msg->x = (short)x;
	msg->y = (short)y;
}

void mpMoveStop(MessageHeader* header, MoveStop* msg, char direction, float x, float y)
{
	header->packetCode = (char)0x89;
	header->len = sizeof(MoveStop);
	header->type = PACKET_REQ_MOVE_STOP;

	msg->direction = direction;
	msg->x = (short)x;
	msg->y = (short)y;
}

void mpMoveStopOther(MessageHeader* header, MoveStopOther* msg, int id, char direction, float x, float y)
{
	header->packetCode = (char)0x89;
	header->len = sizeof(MoveStopOther);
	header->type = PACKET_RES_MOVE_STOP;

	msg->id = id;
	msg->direction = direction;
	msg->x = (short)x;
	msg->y = (short)y;
}

void mpAttack1Request(MessageHeader* header, Attack1Request* msg, char direction, float x, float y)
{
	header->packetCode = (char)0x89;
	header->len = sizeof(Attack1Request);
	header->type = PACKET_REQ_ATTACK1;

	msg->direction = direction;
	msg->x = (short)x;
	msg->y = (short)y;
}

void mpAttack1Response(MessageHeader* header, Attack1Response* msg, int id, char direction, float x, float y)
{
	header->packetCode = (char)0x89;
	header->len = sizeof(Attack1Response);
	header->type = PACKET_RES_ATTACK1;

	msg->id = id;
	msg->direction = direction;
	msg->x = (short)x;
	msg->y = (short)y;
}

void mpAttack2Request(MessageHeader* header, Attack2Request* msg, char direction, float x, float y)
{
	header->packetCode = (char)0x89;
	header->len = sizeof(Attack2Request);
	header->type = PACKET_REQ_ATTACK2;

	msg->direction = direction;
	msg->x = (short)x;
	msg->y = (short)y;
}

void mpAttack2Response(MessageHeader* header, Attack2Response* msg, int id, char direction, float x, float y)
{
	header->packetCode = (char)0x89;
	header->len = sizeof(Attack2Response);
	header->type = PACKET_RES_ATTACK2;

	msg->id = id;
	msg->direction = direction;
	msg->x = (short)x;
	msg->y = (short)y;
}

void mpAttack3Request(MessageHeader* header, Attack3Request* msg, char direction, float x, float y)
{
	header->packetCode = (char)0x89;
	header->len = sizeof(Attack3Request);
	header->type = PACKET_REQ_ATTACK3;

	msg->direction = direction;
	msg->x = (short)x;
	msg->y = (short)y;
}

void mpAttack3Response(MessageHeader* header, Attack3Response* msg, int id, char direction, float x, float y)
{
	header->packetCode = (char)0x89;
	header->len = sizeof(Attack3Response);
	header->type = PACKET_RES_ATTACK3;

	msg->id = id;
	msg->direction = direction;
	msg->x = (short)x;
	msg->y = (short)y;
}

void mpDamage(MessageHeader* header, Damage* msg, int attackID, int damageID, char damageHP)
{
	header->packetCode = (char)0x89;
	header->len = sizeof(Damage);
	header->type = PACKET_RES_DAMAGE;

	msg->attackID = attackID;
	msg->damageID = damageID;
	msg->damageHP = damageHP;
}