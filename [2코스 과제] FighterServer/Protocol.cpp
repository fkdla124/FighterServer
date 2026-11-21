#include "Protocol.h"
#include "PacketDefine.h"
#include "MsgBuf.h"

void mpCreatePlayer(MsgBuf* msgBuf, int id, char direction, float x, float y, char hp)
{
	*msgBuf << (char)0x89 << (char)10 << (char)PACKET_RES_CREATE_MY_CHARACTER;
	*msgBuf << id << direction << (short)x << (short)y << hp;
}

void mpCreateOther(MsgBuf* msgBuf, int id, char direction, float x, float y, char hp)
{
	*msgBuf << (char)0x89 << (char)10 << (char)PACKET_RES_CREATE_OTHER_CHARACTER;
	*msgBuf << id << direction << (short)x << (short)y << hp;
}

void mpDeletePlayer(MsgBuf* msgBuf, int id)
{
	*msgBuf << (char)0x89 << (char)4 << (char)PACKET_RES_DELETE_CHARACTER;
	*msgBuf << id;
}

void mpMoveStart(MsgBuf* msgBuf, char direction, float x, float y)
{
	*msgBuf << (char)0x89 << (char)5 << (char)PACKET_REQ_MOVE_START;
	*msgBuf << direction << (short)x << (short)y;
}

void mpMoveStartOther(MsgBuf* msgBuf, int id, char direction, float x, float y)
{
	*msgBuf << (char)0x89 << (char)9 << (char)PACKET_RES_MOVE_START;
	*msgBuf << id << direction << (short)x << (short)y;
}

void mpMoveStop(MsgBuf* msgBuf, char direction, float x, float y)
{
	*msgBuf << (char)0x89 << (char)5 << (char)PACKET_REQ_MOVE_STOP;
	*msgBuf << direction << (short)x << (short)y;
}

void mpMoveStopOther(MsgBuf* msgBuf, int id, char direction, float x, float y)
{
	*msgBuf << (char)0x89 << (char)9 << (char)PACKET_RES_MOVE_STOP;
	*msgBuf << id << direction << (short)x << (short)y;
}

void mpAttack1Request(MsgBuf* msgBuf, char direction, float x, float y)
{
	*msgBuf << (char)0x89 << (char)5 << (char)PACKET_REQ_ATTACK1;
	*msgBuf << direction << (short)x << (short)y;
}

void mpAttack1Response(MsgBuf* msgBuf, int id, char direction, float x, float y)
{
	*msgBuf << (char)0x89 << (char)9 << (char)PACKET_RES_ATTACK1;
	*msgBuf << id << direction << (short)x << (short)y;
}

void mpAttack2Request(MsgBuf* msgBuf, char direction, float x, float y)
{
	*msgBuf << (char)0x89 << (char)5 << (char)PACKET_REQ_ATTACK2;
	*msgBuf << direction << (short)x << (short)y;
}

void mpAttack2Response(MsgBuf* msgBuf, int id, char direction, float x, float y)
{
	*msgBuf << (char)0x89 << (char)9 << (char)PACKET_RES_ATTACK2;
	*msgBuf << id << direction << (short)x << (short)y;
}

void mpAttack3Request(MsgBuf* msgBuf, char direction, float x, float y)
{
	*msgBuf << (char)0x89 << (char)5 << (char)PACKET_REQ_ATTACK3;
	*msgBuf << direction << (short)x << (short)y;
}

void mpAttack3Response(MsgBuf* msgBuf, int id, char direction, float x, float y)
{
	*msgBuf << (char)0x89 << (char)9 << (char)PACKET_RES_ATTACK3;
	*msgBuf << id << direction << (short)x << (short)y;
}

void mpDamage(MsgBuf* msgBuf, int attackID, int damageID, char damageHP)
{
	*msgBuf << (char)0x89 << (char)9 << (char)PACKET_RES_DAMAGE;
	*msgBuf << attackID << damageID << damageHP;
}