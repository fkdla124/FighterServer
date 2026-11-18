// =====================================================================================
// DummyClient.cpp  -  Fighter Server 더미 클라이언트 (단일 파일)
//
// - 더미 100개가 각자 독립적인 상태머신으로 동작 (싱글스레드, 논블로킹 소켓)
// - 각 더미: [접속 -> 10~20초 랜덤 행동(이동/공격1/2/3) -> 종료] 사이클을 무한 반복 (Ctrl+C로 종료)
// - 서버가 끊은 세션도 휴식 후 처음부터 다시 사이클 시작
// - 첫 접속 시점, 라운드 사이 휴식시간이 랜덤이라 접속중/행동중/종료중이 섞임
// - 이동 시 x든 y든 범위를 벗어나면 좌표를 아예 변화시키지 않음 (서버 MovePlayer와 동일)
//
// 빌드: Visual Studio, Windows 콘솔 프로젝트. (ws2_32, winmm 은 pragma로 링크)
// 실행: DummyClient.exe [서버IP] [포트]     (기본값 127.0.0.1 5000)
// =====================================================================================

#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 테스트 설정

static const char* DEFAULT_SERVER_IP = "127.0.0.1";
static const int   DEFAULT_SERVER_PORT = 5000;

static const int DUMMY_COUNT = 100;				// 더미 세션 수

static const int ACTIVE_TIME_MIN_MS = 10000;	// 접속 후 행동 시간 (랜덤)
static const int ACTIVE_TIME_MAX_MS = 20000;

static const int FIRST_START_MAX_DELAY_MS = 10000;	// 첫 접속 시점 분산 (0 ~ 이 값 사이 랜덤)
static const int REST_MIN_MS = 500;					// 라운드 종료 후 다음 접속까지 휴식 (랜덤)
static const int REST_MAX_MS = 3000;

static const int MOVE_TIME_MIN_MS = 500;		// 한 번 이동 지속시간 (랜덤)
static const int MOVE_TIME_MAX_MS = 2000;
static const int ACTION_GAP_MIN_MS = 50;		// 이동 종료 후 다음 행동까지 간격 (랜덤)
static const int ACTION_GAP_MAX_MS = 300;

static const int CONNECT_TIMEOUT_MS = 5000;		// 연결 완료 대기 한계
static const int CREATE_TIMEOUT_MS = 0;			// 접속 후 내 캐릭터 생성 패킷 대기 한계. 0이면 무제한 (서버가 끊어줄 때까지 대기)

static const bool USE_TCP_NODELAY = true;		// 클라 Nagle 끄기 (작은 패킷 지연 방지)

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 서버 Define.h / PacketDefine.h 와 동일해야 하는 값

#define MOVE_CHARACTER_PER_FRAME_X 3
#define MOVE_CHARACTER_PER_FRAME_Y 2
#define GAME_LOGIC_FPS_LIMIT 50

#define MOVE_SPEED_X_PER_MILLISECONDS ((MOVE_CHARACTER_PER_FRAME_X * GAME_LOGIC_FPS_LIMIT) / 1000.0f)
#define MOVE_SPEED_Y_PER_MILLISECONDS ((MOVE_CHARACTER_PER_FRAME_Y * GAME_LOGIC_FPS_LIMIT) / 1000.0f)

#define RANGE_MOVE_TOP		50
#define RANGE_MOVE_LEFT		10
#define RANGE_MOVE_RIGHT	630
#define RANGE_MOVE_BOTTOM	470

#define ATTACK1_ANIMATION_DURATION 240
#define ATTACK2_ANIMATION_DURATION 320
#define ATTACK3_ANIMATION_DURATION 480

#define PACKET_CODE 0x89

#define PACKET_RES_CREATE_MY_CHARACTER	0
#define PACKET_REQ_MOVE_START			10
#define PACKET_REQ_MOVE_STOP			12
#define PACKET_REQ_ATTACK1				20
#define PACKET_REQ_ATTACK2				22
#define PACKET_REQ_ATTACK3				24
#define PACKET_RES_DAMAGE				30

#define PACKET_MOVE_DIR_LL	0
#define PACKET_MOVE_DIR_LU	1
#define PACKET_MOVE_DIR_UU	2
#define PACKET_MOVE_DIR_RU	3
#define PACKET_MOVE_DIR_RR	4
#define PACKET_MOVE_DIR_RD	5
#define PACKET_MOVE_DIR_DD	6
#define PACKET_MOVE_DIR_LD	7

// 서버 Protocol.h 와 동일한 바이트 레이아웃
#pragma pack(push, 1)
struct MessageHeader
{
	char packetCode;
	char len;
	char type;
};

// type = 0, RES
struct CreatePlayerMsg
{
	int id;
	char direction;
	short x;
	short y;
	char hp;
};

// MoveStart(10) / MoveStop(12) / Attack1~3 Request(20, 22, 24) 공통 레이아웃
struct PosMsg
{
	char direction;
	short x;
	short y;
};

// type = 30, RES
struct DamageMsg
{
	int attackID;
	int damageID;
	char damageHP;
};
#pragma pack(pop)

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 방향 테이블 (서버 GameLogic()의 switch와 동일)

struct DirVec { int dx; int dy; };
static const DirVec DIR_TABLE[8] =
{
	{ -1,  0 },	// LL
	{ -1, -1 },	// LU
	{  0, -1 },	// UU
	{  1, -1 },	// RU
	{  1,  0 },	// RR
	{  1,  1 },	// RD
	{  0,  1 },	// DD
	{ -1,  1 },	// LD
};

struct AttackInfo { char reqType; int animationMs; };
static const AttackInfo ATTACK_INFO[3] =
{
	{ PACKET_REQ_ATTACK1, ATTACK1_ANIMATION_DURATION },
	{ PACKET_REQ_ATTACK2, ATTACK2_ANIMATION_DURATION },
	{ PACKET_REQ_ATTACK3, ATTACK3_ANIMATION_DURATION },
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 더미 세션

enum class State { WaitStart, Connecting, WaitCreate, Active };
enum class EndReason { Normal, ServerClosed, ConnectFail, CreateTimeout, ProtocolError };

static const int RECV_BUF_SIZE = 16 * 1024;
static const int SEND_BUF_SIZE = 1024;

struct Dummy
{
	int index = 0;
	State state = State::WaitStart;
	int cycleCount = 0;				// 종료한 사이클 수 (누적)
	SOCKET sock = INVALID_SOCKET;

	DWORD startTick = 0;			// WaitStart: 이 시각이 되면 접속 시작
	DWORD stateTick = 0;			// Connecting/WaitCreate 타임아웃 기준 시각
	DWORD activeEndTick = 0;		// Active: 이 시각이 되면 종료

	char recvBuf[RECV_BUF_SIZE];
	int recvLen = 0;
	char sendBuf[SEND_BUF_SIZE];
	int sendLen = 0;

	// 서버가 알려준 내 캐릭터 정보 + 클라 시뮬레이션 좌표
	int id = -1;
	int hp = 0;
	float x = 0;
	float y = 0;
	char direction = PACKET_MOVE_DIR_RR;	// 이동 방향 (8방향)
	char facing = PACKET_MOVE_DIR_RR;		// 시점 (LL / RR)

	bool moving = false;
	DWORD lastSimTick = 0;			// 마지막 이동 계산 시각
	DWORD moveEndTick = 0;			// 이동 종료 시각
	DWORD nextActionTick = 0;		// 다음 행동 가능 시각
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 전역

static SOCKADDR_IN g_serverAddr;
static std::vector<Dummy> g_dummies;
static std::mt19937 g_rng{ std::random_device{}() };

// 통계
static volatile bool g_running = true;
static int g_totalCycles = 0;
static int g_connectOk = 0;
static int g_endNormal = 0;
static int g_endServerClosed = 0;
static int g_endConnectFail = 0;
static int g_endCreateTimeout = 0;
static int g_endProtocolError = 0;
static int g_cntMoveStart = 0;
static int g_cntMoveStop = 0;
static int g_cntAttack[3] = { 0, 0, 0 };
static int g_cntDamageToMe = 0;
static long long g_sendBytes = 0;
static long long g_recvBytes = 0;

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 유틸

static int RandRange(int lo, int hi)	// lo ~ hi 포함
{
	return std::uniform_int_distribution<int>(lo, hi)(g_rng);
}

// timeGetTime() 오버플로우 안전한 "now가 target에 도달했는가?"
static bool Reached(DWORD now, DWORD target)
{
	return (int)(now - target) >= 0;
}

static const char* ReasonString(EndReason reason)
{
	switch (reason)
	{
		case EndReason::Normal:			return "정상 종료(행동시간 끝)";
		case EndReason::ServerClosed:	return "서버가 연결 종료";
		case EndReason::ConnectFail:	return "접속 실패";
		case EndReason::CreateTimeout:	return "캐릭터 생성 패킷 타임아웃";
		case EndReason::ProtocolError:	return "프로토콜/버퍼 오류";
	}
	return "?";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 사이클 종료 처리: 소켓 닫고 다음 사이클 예약

static void EndRound(Dummy& d, DWORD now, EndReason reason)
{
	if (d.sock != INVALID_SOCKET)
	{
		closesocket(d.sock);
		d.sock = INVALID_SOCKET;
	}

	d.cycleCount++;
	g_totalCycles++;

	switch (reason)
	{
		case EndReason::Normal:			g_endNormal++; break;
		case EndReason::ServerClosed:	g_endServerClosed++; break;
		case EndReason::ConnectFail:	g_endConnectFail++; break;
		case EndReason::CreateTimeout:	g_endCreateTimeout++; break;
		case EndReason::ProtocolError:	g_endProtocolError++; break;
	}

	printf("[Dummy %03d] Cycle %d 종료 - %s (ID: %d, HP: %d)\n",
		d.index, d.cycleCount, ReasonString(reason), d.id, d.hp);

	d.recvLen = 0;
	d.sendLen = 0;
	d.moving = false;

	// 정상 종료든 서버가 끊었든 휴식 후 처음부터 다시 사이클 시작
	d.state = State::WaitStart;
	d.startTick = now + RandRange(REST_MIN_MS, REST_MAX_MS);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 송신

// 헤더 + 페이로드를 송신 버퍼에 쌓기. 실제 send()는 FlushSend()
static bool QueuePacket(Dummy& d, char type, const void* payload, int len)
{
	int total = (int)sizeof(MessageHeader) + len;
	if (d.sendLen + total > SEND_BUF_SIZE)
		return false;

	MessageHeader header;
	header.packetCode = (char)PACKET_CODE;
	header.len = (char)len;
	header.type = type;

	memcpy(d.sendBuf + d.sendLen, &header, sizeof(header));
	memcpy(d.sendBuf + d.sendLen + sizeof(header), payload, len);
	d.sendLen += total;
	return true;
}

// 현재 좌표로 이동시작/이동종료/공격 요청 패킷 생성
static bool SendPosPacket(Dummy& d, DWORD now, char type, char direction)
{
	PosMsg msg;
	msg.direction = direction;
	msg.x = (short)d.x;
	msg.y = (short)d.y;

	if (QueuePacket(d, type, &msg, sizeof(msg)) == false)
	{
		EndRound(d, now, EndReason::ProtocolError);
		return false;
	}
	return true;
}

// 송신 버퍼 -> TCP. 연결 끊김이면 false (이미 EndRound 처리됨)
static bool FlushSend(Dummy& d, DWORD now)
{
	while (d.sendLen > 0)
	{
		int sent = send(d.sock, d.sendBuf, d.sendLen, 0);
		if (sent == SOCKET_ERROR)
		{
			int err = WSAGetLastError();
			if (err == WSAEWOULDBLOCK)
				break;

			EndRound(d, now, EndReason::ServerClosed);
			return false;
		}

		g_sendBytes += sent;
		memmove(d.sendBuf, d.sendBuf + sent, d.sendLen - sent);
		d.sendLen -= sent;
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 수신

static void HandlePacket(Dummy& d, DWORD now, int type, const char* payload, int len)
{
	// 디버깅: 캐릭터 생성 대기 중 처음 받은 패킷 몇 개의 type/len 확인용
	static int debugLogCount = 0;
	if (d.state == State::WaitCreate && debugLogCount < 5)
	{
		debugLogCount++;
		printf("[Dummy %03d] 생성 대기 중 수신 패킷 type: %d, len: %d\n", d.index, type, len);
	}

	switch (type)
	{
		case PACKET_RES_CREATE_MY_CHARACTER:
		{
			if (d.state != State::WaitCreate)
				break;

			// ID(4) + Direction(1) + X(2) + Y(2) + HP(1) = 10바이트
			if (len < (int)sizeof(CreatePlayerMsg))
			{
				printf("[Dummy %03d] CreatePlayer 길이 부족. len: %d\n", d.index, len);
				break;
			}

			int id;
			short px, py;
			memcpy(&id, payload, 4);
			char dir = payload[4];
			memcpy(&px, payload + 5, 2);
			memcpy(&py, payload + 7, 2);

			int hp = (unsigned char)payload[9];

			d.id = id;
			d.hp = hp;
			d.x = px;
			d.y = py;
			d.direction = dir;
			d.facing = dir;

			// 여기서부터 행동 시작
			d.state = State::Active;
			d.moving = false;
			d.lastSimTick = now;
			d.activeEndTick = now + RandRange(ACTIVE_TIME_MIN_MS, ACTIVE_TIME_MAX_MS);
			d.nextActionTick = now + RandRange(ACTION_GAP_MIN_MS, ACTION_GAP_MAX_MS);
			break;
		}
		case PACKET_RES_DAMAGE:
		{
			if (len < (int)sizeof(DamageMsg))
				break;

			DamageMsg msg;
			memcpy(&msg, payload, sizeof(msg));
			if (msg.damageID == d.id)
			{
				d.hp = msg.damageHP;
				g_cntDamageToMe++;
			}
			break;
		}
		default:
			// 다른 캐릭터 생성/삭제/이동/공격 브로드캐스트는 더미라서 무시 (수신만 해서 비워줌)
			break;
	}
}

// 수신 버퍼에서 완성된 패킷 파싱. 오류면 false (이미 EndRound 처리됨)
static bool ParsePackets(Dummy& d, DWORD now)
{
	int offset = 0;
	while (d.recvLen - offset >= (int)sizeof(MessageHeader))
	{
		MessageHeader header;
		memcpy(&header, d.recvBuf + offset, sizeof(header));

		if ((unsigned char)header.packetCode != PACKET_CODE)
		{
			printf("[Dummy %03d] 잘못된 packetCode: 0x%02x\n", d.index, (unsigned char)header.packetCode);
			EndRound(d, now, EndReason::ProtocolError);
			return false;
		}

		int payloadLen = (unsigned char)header.len;
		int total = (int)sizeof(MessageHeader) + payloadLen;
		if (d.recvLen - offset < total)
			break;

		HandlePacket(d, now, header.type, d.recvBuf + offset + sizeof(MessageHeader), payloadLen);
		offset += total;
	}

	if (offset > 0)
	{
		memmove(d.recvBuf, d.recvBuf + offset, d.recvLen - offset);
		d.recvLen -= offset;
	}
	return true;
}

// TCP -> 수신 버퍼 -> 파싱. 끊김/오류면 false (이미 EndRound 처리됨)
static bool RecvAndParse(Dummy& d, DWORD now)
{
	while (true)
	{
		int space = RECV_BUF_SIZE - d.recvLen;
		if (space <= 0)
		{
			EndRound(d, now, EndReason::ProtocolError);
			return false;
		}

		int received = recv(d.sock, d.recvBuf + d.recvLen, space, 0);
		if (received > 0)
		{
			d.recvLen += received;
			g_recvBytes += received;

			if (ParsePackets(d, now) == false)
				return false;

			if (received < space)
				break;	// 더 읽을 게 거의 없음. 다음 루프에서 계속.
		}
		else if (received == 0)
		{
			// FIN. 서버가 끊음 (사망 포함)
			EndRound(d, now, EndReason::ServerClosed);
			return false;
		}
		else
		{
			int err = WSAGetLastError();
			if (err == WSAEWOULDBLOCK)
				break;

			// RST 등 그 외 에러도 연결 끊김으로 처리
			EndRound(d, now, EndReason::ServerClosed);
			return false;
		}
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 이동 / 행동

static void UpdateFacing(Dummy& d, char dir)
{
	switch (dir)
	{
		case PACKET_MOVE_DIR_RR:
		case PACKET_MOVE_DIR_RU:
		case PACKET_MOVE_DIR_RD:
			d.facing = PACKET_MOVE_DIR_RR;
			break;
		case PACKET_MOVE_DIR_LL:
		case PACKET_MOVE_DIR_LU:
		case PACKET_MOVE_DIR_LD:
			d.facing = PACKET_MOVE_DIR_LL;
			break;
		default:	// UU, DD는 시점 유지 (서버와 동일)
			break;
	}
}

// 서버 MovePlayer()와 같은 방식으로 좌표 계산.
// 다음 좌표가 x든 y든 하나라도 범위를 벗어나면 좌표를 아예 변화시키지 않음.
static void StepMove(Dummy& d, DWORD now, bool force)
{
	DWORD dt = now - d.lastSimTick;

	// 서버 로직 FPS(50)와 비슷한 간격으로만 계산
	if (force == false && dt < (DWORD)(1000 / GAME_LOGIC_FPS_LIMIT))
		return;
	if (dt == 0)
		return;

	d.lastSimTick = now;

	const DirVec& v = DIR_TABLE[(int)d.direction];
	float nextX = d.x + dt * MOVE_SPEED_X_PER_MILLISECONDS * v.dx;
	float nextY = d.y + dt * MOVE_SPEED_Y_PER_MILLISECONDS * v.dy;

	if (nextX <= RANGE_MOVE_LEFT || nextX >= RANGE_MOVE_RIGHT || nextY <= RANGE_MOVE_TOP || nextY >= RANGE_MOVE_BOTTOM)
		return;

	d.x = nextX;
	d.y = nextY;
}

static void StopMove(Dummy& d, DWORD now)
{
	// 마지막까지 계산한 최신 좌표로 정지 패킷 전송
	StepMove(d, now, true);
	d.moving = false;

	if (SendPosPacket(d, now, PACKET_REQ_MOVE_STOP, d.facing) == false)
		return;
	g_cntMoveStop++;

	d.nextActionTick = now + RandRange(ACTION_GAP_MIN_MS, ACTION_GAP_MAX_MS);
}

// 이동, 공격1, 공격2, 공격3 중 랜덤 행동 하나 시작
static void DoRandomAction(Dummy& d, DWORD now)
{
	int action = RandRange(0, 3);	// 0: 이동, 1~3: 공격1~3

	if (action == 0)
	{
		char dir = (char)RandRange(PACKET_MOVE_DIR_LL, PACKET_MOVE_DIR_LD);
		d.direction = dir;
		UpdateFacing(d, dir);

		d.moving = true;
		d.lastSimTick = now;
		d.moveEndTick = now + RandRange(MOVE_TIME_MIN_MS, MOVE_TIME_MAX_MS);

		if (SendPosPacket(d, now, PACKET_REQ_MOVE_START, dir) == false)
			return;
		g_cntMoveStart++;
	}
	else
	{
		int attackIndex = action - 1;
		const AttackInfo& info = ATTACK_INFO[attackIndex];

		// 공격은 좌/우 시점만 사용. 랜덤으로 좌우 번갈아 공격
		d.facing = (RandRange(0, 1) == 0) ? PACKET_MOVE_DIR_LL : PACKET_MOVE_DIR_RR;
		d.direction = d.facing;

		if (SendPosPacket(d, now, info.reqType, d.facing) == false)
			return;
		g_cntAttack[attackIndex]++;

		// 서버 공격 쿨타임(애니메이션 시간)보다 짧게 보내면 서버가 무시하므로 그만큼 대기
		d.nextActionTick = now + info.animationMs + RandRange(0, 150);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 상태별 업데이트

static void StartConnect(Dummy& d, DWORD now)
{
	d.sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (d.sock == INVALID_SOCKET)
	{
		printf("[Dummy %03d] socket() error. %d\n", d.index, WSAGetLastError());
		EndRound(d, now, EndReason::ConnectFail);
		return;
	}

	u_long nonBlocking = 1;
	ioctlsocket(d.sock, FIONBIO, &nonBlocking);

	if (USE_TCP_NODELAY)
	{
		BOOL nodelay = TRUE;
		setsockopt(d.sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));
	}

	// 이전 라운드 정보 초기화
	d.id = -1;
	d.hp = 0;
	d.recvLen = 0;
	d.sendLen = 0;
	d.moving = false;

	int ret = connect(d.sock, (sockaddr*)&g_serverAddr, sizeof(g_serverAddr));
	if (ret == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != WSAEWOULDBLOCK)
		{
			printf("[Dummy %03d] connect() error. %d\n", d.index, err);
			EndRound(d, now, EndReason::ConnectFail);
			return;
		}
	}

	d.state = State::Connecting;
	d.stateTick = now;
}

static void UpdateConnecting(Dummy& d, DWORD now)
{
	fd_set wset, eset;
	FD_ZERO(&wset);
	FD_ZERO(&eset);
	FD_SET(d.sock, &wset);
	FD_SET(d.sock, &eset);

	timeval tv = { 0, 0 };
	int ret = select(0, NULL, &wset, &eset, &tv);

	if (ret == SOCKET_ERROR)
	{
		printf("[Dummy %03d] select() error. %d\n", d.index, WSAGetLastError());
		EndRound(d, now, EndReason::ConnectFail);
		return;
	}

	if (ret > 0)
	{
		if (FD_ISSET(d.sock, &eset))
		{
			int soError = 0;
			int soErrorLen = sizeof(soError);
			getsockopt(d.sock, SOL_SOCKET, SO_ERROR, (char*)&soError, &soErrorLen);
			printf("[Dummy %03d] 연결 실패. SO_ERROR: %d\n", d.index, soError);
			EndRound(d, now, EndReason::ConnectFail);
			return;
		}

		if (FD_ISSET(d.sock, &wset))
		{
			g_connectOk++;
			d.state = State::WaitCreate;
			d.stateTick = now;
			return;
		}
	}

	if (Reached(now, d.stateTick + CONNECT_TIMEOUT_MS))
	{
		printf("[Dummy %03d] 연결 타임아웃\n", d.index);
		EndRound(d, now, EndReason::ConnectFail);
	}
}

static void UpdateActive(Dummy& d, DWORD now)
{
	// 행동 시간 끝 -> 종료
	if (Reached(now, d.activeEndTick))
	{
		EndRound(d, now, EndReason::Normal);
		return;
	}

	if (d.moving)
	{
		StepMove(d, now, false);

		if (Reached(now, d.moveEndTick))
			StopMove(d, now);
	}
	else if (Reached(now, d.nextActionTick))
	{
		DoRandomAction(d, now);
	}
}

static void UpdateDummy(Dummy& d, DWORD now)
{
	switch (d.state)
	{
		case State::WaitStart:
		{
			if (Reached(now, d.startTick))
				StartConnect(d, now);
			break;
		}
		case State::Connecting:
		{
			UpdateConnecting(d, now);
			break;
		}
		case State::WaitCreate:
		case State::Active:
		{
			if (RecvAndParse(d, now) == false)
				break;

			if (CREATE_TIMEOUT_MS > 0 && d.state == State::WaitCreate && Reached(now, d.stateTick + CREATE_TIMEOUT_MS))
			{
				printf("[Dummy %03d] 내 캐릭터 생성 패킷을 못 받음\n", d.index);
				EndRound(d, now, EndReason::CreateTimeout);
				break;
			}

			if (d.state == State::Active)
				UpdateActive(d, now);

			// 위에서 EndRound 됐다면 소켓이 없음
			if (d.sock != INVALID_SOCKET)
				FlushSend(d, now);
			break;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 1초마다 통계 출력

static void PrintStats(DWORD now)
{
	static DWORD lastTick = now;
	static long long lastSendBytes = 0;
	static long long lastRecvBytes = 0;

	if (now - lastTick < 1000)
		return;
	lastTick = now;

	int count[4] = { 0, };
	for (size_t i = 0; i < g_dummies.size(); i++)
		count[(int)g_dummies[i].state]++;

	printf("[STAT] Wait: %3d | Connecting: %3d | WaitCreate: %3d | Active: %3d | Cycles: %5d | Send: %6lld B/s | Recv: %7lld B/s\n",
		count[0], count[1], count[2], count[3], g_totalCycles,
		g_sendBytes - lastSendBytes, g_recvBytes - lastRecvBytes);

	lastSendBytes = g_sendBytes;
	lastRecvBytes = g_recvBytes;
}

static void PrintSummary()
{
	printf("\n==================== 더미 테스트 결과 ====================\n");
	printf("더미 %d개 / 완료한 사이클 총 %d\n", DUMMY_COUNT, g_totalCycles);
	printf("연결 성공 횟수          : %d\n", g_connectOk);
	printf("정상 종료               : %d\n", g_endNormal);
	printf("서버가 연결 종료        : %d (사망/오류패킷 판정 등)\n", g_endServerClosed);
	printf("접속 실패               : %d\n", g_endConnectFail);
	printf("캐릭터 생성 타임아웃    : %d\n", g_endCreateTimeout);
	printf("프로토콜/버퍼 오류      : %d\n", g_endProtocolError);
	printf("전송 행동: MoveStart %d, MoveStop %d, Attack1 %d, Attack2 %d, Attack3 %d\n",
		g_cntMoveStart, g_cntMoveStop, g_cntAttack[0], g_cntAttack[1], g_cntAttack[2]);
	printf("내가 맞은 Damage 패킷   : %d\n", g_cntDamageToMe);
	printf("총 송신 %lld B, 총 수신 %lld B\n", g_sendBytes, g_recvBytes);
	printf("==========================================================\n");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

static BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType)
{
	if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_CLOSE_EVENT || ctrlType == CTRL_BREAK_EVENT)
	{
		g_running = false;
		return TRUE;
	}
	return FALSE;
}

int main(int argc, char* argv[])
{
	SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

	const char* serverIp = (argc >= 2) ? argv[1] : DEFAULT_SERVER_IP;
	int serverPort = (argc >= 3) ? atoi(argv[2]) : DEFAULT_SERVER_PORT;

	timeBeginPeriod(1);

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		printf("WSAStartup() error. %d\n", WSAGetLastError());
		return 1;
	}

	memset(&g_serverAddr, 0, sizeof(g_serverAddr));
	g_serverAddr.sin_family = AF_INET;
	g_serverAddr.sin_port = htons((u_short)serverPort);
	if (InetPtonA(AF_INET, serverIp, &g_serverAddr.sin_addr) != 1)
	{
		printf("잘못된 서버 IP: %s\n", serverIp);
		WSACleanup();
		return 1;
	}

	printf("===== Fighter Dummy Client =====\n");
	printf("서버 %s:%d / 더미 %d개 / 행동시간 %d~%d ms / Ctrl+C로 종료\n\n",
		serverIp, serverPort, DUMMY_COUNT, ACTIVE_TIME_MIN_MS, ACTIVE_TIME_MAX_MS);

	// 더미 생성. 첫 접속 시점을 랜덤으로 흩뿌림 -> 일제히 접속 X
	g_dummies.resize(DUMMY_COUNT);
	DWORD startNow = timeGetTime();
	for (int i = 0; i < DUMMY_COUNT; i++)
	{
		g_dummies[i].index = i;
		g_dummies[i].startTick = startNow + RandRange(0, FIRST_START_MAX_DELAY_MS);
	}

	while (g_running)
	{
		DWORD now = timeGetTime();

		for (int i = 0; i < DUMMY_COUNT; i++)
			UpdateDummy(g_dummies[i], now);

		PrintStats(now);

		Sleep(1);
	}

	// 남은 소켓 정리
	for (int i = 0; i < DUMMY_COUNT; i++)
	{
		if (g_dummies[i].sock != INVALID_SOCKET)
			closesocket(g_dummies[i].sock);
	}

	PrintSummary();

	WSACleanup();
	timeEndPeriod(1);
	return 0;
}
