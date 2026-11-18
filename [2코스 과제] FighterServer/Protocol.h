#pragma once

// byte order
#pragma pack(push, 1)

/////////////////////////////////////////////////////////////////////////////////
// Header

struct MessageHeader
{
	char packetCode;
	char len;
	char type;
};

/////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////
// Player Create/Delete

// type = 0, RES
struct CreatePlayer
{
	int id;
	char direction;
	short x;
	short y;
	char hp;
};

// type = 1, RES
struct CreateOther
{
	int id;
	char direction;
	short x;
	short y;
	char hp;
};

// type = 2, RES
struct DeletePlayer
{
	int id;
};

/////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////
// Move

// type = 10, REQ
struct MoveStart
{
	char direction;

	// 목적지 기반 이동 동기화에선 안 넣는게 정석 구현.
	// 지금은 목적지(더미) 기반 이동 동기화가 아님!!
	short x;
	short y;
};

// type = 11, RES
struct MoveStartOther
{
	int id;
	char direction;

	short x;
	short y;
};

// type = 12, REQ
struct MoveStop
{
	char direction;
	short x;
	short y;
};

// type = 13, RES
struct MoveStopOther
{
	int id;
	char direction;
	short x;
	short y;
};

/////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////
// Attack

// type = 20, REQ
struct Attack1Request
{
	char direction;
	short x;
	short y;
};

// type = 21, RES
struct Attack1Response
{
	int id;
	char direction;
	short x;
	short y;
};

// type = 22, REQ
struct Attack2Request
{
	char direction;
	short x;
	short y;
};

// type = 23, RES
struct Attack2Response
{
	int id;
	char direction;
	short x;
	short y;
};

// type = 24, REQ
struct Attack3Request
{
	char direction;
	short x;
	short y;
};

// type = 25, RES
struct Attack3Response
{
	int id;
	char direction;
	short x;
	short y;
};

// type = 30, RES
struct Damage
{
	int attackID; // 공격자
	int damageID; // 피해자
	char damageHP; // 피해자 남은 HP
};

/////////////////////////////////////////////////////////////////////////////////

#pragma pack(pop)

// 패킷 생성 함수
void mpCreatePlayer(MessageHeader* header, CreatePlayer* msg, int id, char direction, float x, float y, char hp);
void mpCreateOther(MessageHeader* header, CreateOther* msg, int id, char direction, float x, float y, char hp);
void mpDeletePlayer(MessageHeader* header, DeletePlayer* msg, int id);
void mpMoveStart(MessageHeader* header, MoveStart* msg, char direction, float x, float y);
void mpMoveStartOther(MessageHeader* header, MoveStartOther* msg, int id, char direction, float x, float y);
void mpMoveStop(MessageHeader* header, MoveStop* msg, char direction, float x, float y);
void mpMoveStopOther(MessageHeader* header, MoveStopOther* msg, int id, char direction, float x, float y);
void mpAttack1Request(MessageHeader* header, Attack1Request* msg, char direction, float x, float y);
void mpAttack1Response(MessageHeader* header, Attack1Response* msg, int id, char direction, float x, float y);
void mpAttack2Request(MessageHeader* header, Attack2Request* msg, char direction, float x, float y);
void mpAttack2Response(MessageHeader* header, Attack2Response* msg, int id, char direction, float x, float y);
void mpAttack3Request(MessageHeader* header, Attack3Request* msg, char direction, float x, float y);
void mpAttack3Response(MessageHeader* header, Attack3Response* msg, int id, char direction, float x, float y);
void mpDamage(MessageHeader* header, Damage* msg, int attackID, int damageID, char damageHP);