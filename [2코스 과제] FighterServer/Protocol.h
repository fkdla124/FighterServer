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
	int hp;
};

// type = 1, RES
struct CreateOther
{
	int id;
	char direction;
	short x;
	short y;
	int hp;
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
	int AttackID; // 공격자
	int DamageID; // 피해자
	char DamageHP; // 피해자 남은 HP
};

/////////////////////////////////////////////////////////////////////////////////

#pragma pack(pop)