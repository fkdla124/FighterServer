#include <cstdio>
#include <cmath>
#include <ctime>

#include "Network.h"
#include "RingBuffer.h"
#include "List.h"
#include "SocketErrorcode.h"
#include "Protocol.h"
#include "PacketDefine.h"
#include "GameLogic.h"
#include "Session.h"
#include "Debug.h"

#include <windows.h>
#pragma comment(lib, "winmm.lib")

// todo
// 
// V 고민사항 -> 해결 완료
// V 현재 SendUnicast(), SendBroadcast() 에서 메시지 보내면 다음 프레임에 실제 send()가 이루어짐.
// V 클라이언트가 다른 플레이어의 행동을 보는 것이 한 프레임 뒤가 됨.
// 
// 해결 방법 
// 네트워크처리 하단부에 Send 링버퍼를 한번에 flush하는 함수를 넣기. 
//   ㄴ-> 이렇게 하면 wset의 역할은 이전프레임에 특별한 이유(보통 WSAEWOULDBLOCK 또는 TCP 송신버퍼 가득참)로 송신하지 못한 것들만 송신하는 구조가됨.
//   ㄴ-> 네트워크 처리 한 번당 송신이 두 번 나오는 구조. wset에서 한 번, Network() 마지막에 한 번.



// 다음수업에 클라, 서버 세부사항이 공개되면 추가할 사항.
// 공격별 정확한 쿨타임, 정확한 타격점, 정확한 데미지
// 충돌처리 히트박스. (캐릭터에 딱 맞는 사각형 사이즈)
// 
// 지금 쿨타임 처리를 공격 개별로 해놨는데, 이렇게 되면 클라에서 공격 3개를 같은시간에 보내면 실제 공격처리가 돼버린다. (물론 조작한 클라만 되겠지만)
// 공격모션이 끝나야 다른 공격을 하는 건데, 서버에서 세션정보에 attackMotion = true 라는 상태를 추가해서 관리해야되나 고민이다.




////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 전역 데이터

#define SERVER_PORT 5000
SOCKET listenSocket;

List<Session*> sessionList;
int idCount = 0;



// 공격 맞는사람은 히트박스로. 타격하는사람은 점으로 타격

// 캐릭터 좌표가 양발 중앙쪽에 잡혀있을 것으로 예상하고 거기서 Direction 방향으로 +x +y 하는 값
// 타격점 좌표
#define ATTACK1_POINT_X 50
#define ATTACK1_POINT_Y 100

#define ATTACK2_POINT_X 50
#define ATTACK2_POINT_Y 100

#define ATTACK3_POINT_X 50
#define ATTACK3_POINT_Y 100

// 히트박스 크기. 캐릭터가 100x200 사이즈 정도라고 추측하고 만들었음.
#define HITBOX_WIDTH 100
#define HITBOX_HEIGHT 200

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 함수

void StartServer(linger _linger, ULONG nonBlocking);
void StopServer();

void Network();
void AcceptClient();
template <typename T>
void SendUnicast(Session* session, char msgType, const T& msg);
template <typename T>
void SendBroadcast(Session* exceptSession, char msgType, const T& msg);
void ProcessMessage(Session* session);
void FlushSendBuffer(Session* session);
void DisconnectMarkedSession();
bool CheckCollision(Session* attacker, Session* target, short attackPointX, short attackPointY);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StartServer(linger _linger, ULONG nonBlocking)
{
	timeBeginPeriod(1);

	SetConsoleTitle(L"[Fighter Server]");

	// WSAStartup()
	WSADATA wsa;
	returnValueWSAStartup = WSAStartup(MAKEWORD(2, 2), &wsa);
	if (returnValueWSAStartup != 0)
	{
		errorCodeWSAStartUp = WSAGetLastError();
		printf("WSAStartup() error. %d\n", errorCodeWSAStartUp);
		Sleep(INFINITE);
	}

	// socket()
	listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenSocket == INVALID_SOCKET)
	{
		errorCodeSocket = WSAGetLastError();
		printf("socket() error. %d\n", errorCodeSocket);
		Sleep(INFINITE);
	}

	// linger set
	returnValueSetSockOpt = setsockopt(listenSocket, SOL_SOCKET, SO_LINGER, (char*)&_linger, sizeof(_linger));
	if (returnValueSetSockOpt == SOCKET_ERROR)
	{
		errorCodeSetSockOpt = WSAGetLastError();
		printf("setsockopt() LINGER{0, 0} setting error. %d\n", errorCodeSetSockOpt);
		Sleep(INFINITE);
	}

	// Nagle
	//DWORD tcpNodelay = true;
	//returnValueSetSockOpt = setsockopt(listenSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&tcpNodelay, sizeof(tcpNodelay));
	//if (returnValueSetSockOpt == SOCKET_ERROR)
	//{
	//	errorCodeSetSockOpt = WSAGetLastError();
	//	printf("setsockopt() TCP_NODELAY setting error. %d\n", errorCodeSetSockOpt);
	//	Sleep(INFINITE);
	//}

	// serverAddr
	SOCKADDR_IN serverAddr = { 0, };
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	// bind()
	returnValueBind = bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
	if (returnValueBind == SOCKET_ERROR)
	{
		errorCodeBind = WSAGetLastError();
		printf("bind() error. %d\n", errorCodeBind);
		Sleep(INFINITE);
	}

	// listen()
	returnValueListen = listen(listenSocket, SOMAXCONN);
	if (returnValueListen == SOCKET_ERROR)
	{
		errorCodeListen = WSAGetLastError();
		printf("listen() error. %d\n", errorCodeListen);
		Sleep(INFINITE);
	}

	// nonblocking change
	returnValueIoctlSocket = ioctlsocket(listenSocket, FIONBIO, &nonBlocking);
	if (returnValueIoctlSocket == SOCKET_ERROR)
	{
		errorCodeIoctlSocket = WSAGetLastError();
		printf("ioctlsocket() error. %d\n", errorCodeIoctlSocket);
		Sleep(INFINITE);
	}

	return;
}

void StopServer()
{
	closesocket(listenSocket);
	sessionList.clear();
	WSACleanup();

	timeEndPeriod(1);
}

void Network()
{
	fd_set rset;
	fd_set wset;
	FD_ZERO(&rset);
	FD_ZERO(&wset);

	// listenSocket rset
	FD_SET(listenSocket, &rset);

	// session rset, wset
	for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end(); ++it)
	{
		FD_SET((*it)->sock, &rset);

		if ((*it)->sendRingBuf.empty() == false)
		{
			FD_SET((*it)->sock, &wset);
		}
	}

	// select
	timeval tv = { 0, 0 };
	returnValueSelect = select(0, &rset, &wset, NULL, &tv);
	if (returnValueSelect == SOCKET_ERROR)
	{
		errorCodeSelect = WSAGetLastError();
		printf("select() error. %d\n", errorCodeSelect);
		Sleep(INFINITE);
	}

	// accept
	if (FD_ISSET(listenSocket, &rset))
		AcceptClient();

	for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end(); ++it)
	{
		// rset 처리
		if (FD_ISSET((*it)->sock, &rset))
		{
			// WSARecv() -> 링버퍼
			WSABUF wsaBuf[2];
			wsaBuf[0].buf = (*it)->recvRingBuf.GetRear();
			wsaBuf[0].len = (*it)->recvRingBuf.DirectPushSize();
			wsaBuf[1].buf = (*it)->recvRingBuf.GetBuffer();
			wsaBuf[1].len = static_cast<ULONG>((*it)->recvRingBuf.GetFront() - wsaBuf[1].buf);

			DWORD bytesRecved;
			DWORD flags = 0;
			returnValueWSARecv = WSARecv((*it)->sock, wsaBuf, 2, &bytesRecved, &flags, NULL, NULL);
			if (returnValueWSARecv == SOCKET_ERROR)
			{
				errorCodeWSARecv = WSAGetLastError();

				if (errorCodeWSARecv == WSAECONNRESET)
				{
					// RST 전송으로 끊어진 경우
					(*it)->del = true;
					continue;
				}
				else if (errorCodeWSARecv == WSAECONNABORTED)
				{
					// 자세히 모르겠지만 끊어진 경우
					(*it)->del = true;
					continue;
				}
				else if (errorCodeWSARecv == WSAEWOULDBLOCK)
					continue;

				printf("WSARecv() error. %d\n", errorCodeWSARecv);
				Sleep(INFINITE);
			}
			else if (bytesRecved == 0)
			{
				// FIN 전송으로 끊어진 경우
				(*it)->del = true;
				continue;
			}

			// DebugPrint("returnValueWSARecv: %d, bytesRecvd: %d\n", returnValueWSARecv, bytesRecved);

			(*it)->recvRingBuf.MoveRear(bytesRecved);

			// 메시지 처리
			ProcessMessage((*it));
		}

		// wset 처리
		if (FD_ISSET((*it)->sock, &wset))
		{
			FlushSendBuffer((*it));
		}
	}

	// 지연삭제
	DisconnectMarkedSession();

	// 돌면서 다 보내기. (이번 프레임에 수신한 메시지 처리하면서 새로 송신한 것들 바로 보내기 위함)
	for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end(); ++it)
	{
		FlushSendBuffer((*it));
	}
}

template <typename T>
void SendUnicast(Session* session, char msgType, const T& msg)
{
	// 헤더생성
	MessageHeader header;
	header.packetCode = 0x89; // 없어도되는데 원장님 클라에 존재함.
	header.len = sizeof(T);
	header.type = msgType;

	// 메시지생성
	char msgBuffer[sizeof(header) + sizeof(T)];
	memcpy(msgBuffer, &header, sizeof(header));
	memcpy(msgBuffer + sizeof(header), &msg, sizeof(T));

	// 링버퍼 push
	returnValuePush = session->sendRingBuf.push(msgBuffer, sizeof(msgBuffer));
	if (returnValuePush != sizeof(msgBuffer))
	{
		// 송신링버퍼가 꽉찼음. 또는 나의 링버퍼 push가 잘못됨.
		// 파일로 로그 남기고 연결 끊기.
		tm t;
		time_t tt = time(NULL);
		localtime_s(&t, &tt);
		char fileName[256];
		sprintf_s(fileName, "[SendRIngBuffer Full Error]%04d%02d%02d_%02d%02d%02d.txt", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);

		#pragma warning(disable:6387 6328) // fp가 NULL일 수 있다. fwprintf() 정수 서식지정자 타입 불일치
		FILE* fp;
		fopen_s(&fp, fileName, "wb");

		// UTF-16 LE BOM
		unsigned char bom[] = { 0xFF, 0xFE };
		fwrite(bom, 1, 2, fp);

		fwprintf(fp, L"RingBuffer Full error. FILE: %s, LINE: %d, recvSessionID: %d \n", __FILEW__, __LINE__, session->id);
		fwprintf(fp, L"RingBuffer Capacity: %d\n", (int)(session->sendRingBuf.capacity()));
		fwprintf(fp, L"RingBuffer Size: %d\n", session->sendRingBuf.size());
		fwprintf(fp, L"Push MsgLen: %d\n", (int)(sizeof(msgBuffer)));
		fwprintf(fp, L"returnValuePush: %d\n", returnValuePush);
		fwprintf(fp, L"MsgType = %d\n", header.type);
		fclose(fp);
		#pragma warning(default:6387 6328)

		// 지연삭제 마킹 및 연결끊기. SO_LINGER {0, 0} RST 전송
		session->del = true;

		printf("[!!!Error!!!] SendUnicast() -> session->sendRingBuf.push() error. capacity: %d, size: %d, pushSize: %d, return: %d\n",
			session->sendRingBuf.capacity(), session->sendRingBuf.size(), (int)sizeof(msgBuffer), returnValuePush);
	}
}

template <typename T>
void SendBroadcast(Session* exceptSession, char msgType, const T& msg)
{
	if (exceptSession == nullptr)
	{
		for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end(); ++it)
		{
			SendUnicast((*it), msgType, msg);
		}
	}
	else
	{
		for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end(); ++it)
		{
			if ((*it)->sock != exceptSession->sock)
			{
				SendUnicast((*it), msgType, msg);
			}
		}
	}
}

void AcceptClient()
{
	// 현재 프레임에 쌓인 연결정보를 모두 빼내기.
	while (1)
	{
		SOCKADDR_IN acceptAddr = { 0, };
		int acceptAddrLen = sizeof(acceptAddr);
		SOCKET acceptSocket = accept(listenSocket, (sockaddr*)&acceptAddr, &acceptAddrLen);
		if (acceptSocket == INVALID_SOCKET)
		{
			errorCodeAccept = WSAGetLastError();

			// 더이상 accept할 연결정보가 없을 때 나가기.
			if (errorCodeAccept == WSAEWOULDBLOCK)
				break;

			printf("accept() error. %d\n", errorCodeAccept);
			Sleep(INFINITE);
		}

		// 세션정보 추가
		Session* acceptSession = new Session;
		acceptSession->del = false;
		acceptSession->move = false;
		acceptSession->sock = acceptSocket;
		acceptSession->addr = acceptAddr;
		acceptSession->id = idCount++;
		acceptSession->hp = 100;
		acceptSession->x = 300;
		acceptSession->y = 300;
		acceptSession->attack1BaseTick = clock();
		acceptSession->attack2BaseTick = clock();
		acceptSession->attack3BaseTick = clock();
		acceptSession->attack1Cooldown = 0;
		acceptSession->attack2Cooldown = 0;
		acceptSession->attack3Cooldown = 0;
		sessionList.push_back(acceptSession);

		char acceptSessionIp[16];
		InetNtopA(AF_INET, &acceptSession->addr.sin_addr, acceptSessionIp, sizeof(acceptSessionIp));
		DebugPrint("[Client Accept %s:%d] id: %d\n", acceptSessionIp, acceptSession->addr.sin_port, acceptSession->id);

		// 접속한 플레이어 생성 메시지
		CreatePlayer msgCreatePlayer;
		msgCreatePlayer.id = acceptSession->id;
		msgCreatePlayer.x = acceptSession->x;
		msgCreatePlayer.y = acceptSession->y;
		msgCreatePlayer.direction = PACKET_MOVE_DIR_RR;
		msgCreatePlayer.hp = 100;
		SendUnicast(acceptSession, PACKET_RES_CREATE_MY_CHARACTER, msgCreatePlayer);

		// 접속한 플레이어를 다른 플레이어에게 알리기
		CreateOther msgCreateOther;
		msgCreateOther.id = acceptSession->id;
		msgCreateOther.x = acceptSession->x;
		msgCreateOther.y = acceptSession->y;
		msgCreateOther.direction = acceptSession->direction;
		msgCreateOther.hp = acceptSession->hp;
		SendBroadcast(acceptSession, PACKET_RES_CREATE_OTHER_CHARACTER, msgCreateOther);

		// 접속한 플레이어에게 다른 플레이어들 생성하는 메시지
		for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end(); ++it)
		{
			if ((*it)->id != acceptSession->id)
			{
				CreateOther msgCreateOther;
				msgCreateOther.id = (*it)->id;
				msgCreateOther.x = (*it)->x;
				msgCreateOther.y = (*it)->y;
				msgCreateOther.direction = (*it)->direction;
				msgCreateOther.hp = (*it)->hp;
				SendUnicast(acceptSession, PACKET_RES_CREATE_OTHER_CHARACTER, msgCreateOther);
			}
		}
	}
}

void DisconnectMarkedSession()
{
	for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end();)
	{
		if ((*it)->del == true)
		{
			closesocket((*it)->sock);
			it = sessionList.erase(it);
		}
		else
			++it;
	}
}

void FlushSendBuffer(Session* session)
{
	// 이미 마킹된 세션이 들어올 가능성이 존재함. 미리 나가기
	if (session->del == true)
		return;

	if (session->sendRingBuf.empty() == true)
		return;

	DWORD bufCount = 0;

	WSABUF wsaBuf[2];
	wsaBuf[0].buf = session->sendRingBuf.GetFront();
	wsaBuf[0].len = session->sendRingBuf.DirectPopSize();
	bufCount++;

	if (session->sendRingBuf.DirectPopSize() < session->sendRingBuf.size())
	{
		wsaBuf[1].buf = session->sendRingBuf.GetBuffer();
		wsaBuf[1].len = static_cast<ULONG>(session->sendRingBuf.GetRear() - wsaBuf[1].buf);
		bufCount++;
	}

	DWORD bytesSent;
	returnValueWSASend = WSASend(session->sock, wsaBuf, bufCount, &bytesSent, NULL, NULL, NULL);
	if (returnValueWSASend == SOCKET_ERROR)
	{
		errorCodeWSASend = WSAGetLastError();

		if (errorCodeWSASend == WSAECONNRESET)
		{
			// RST 전송으로 끊어진 경우
			session->del = true;
			return;
		}
		else if (errorCodeWSASend == WSAECONNABORTED)
		{
			// 자세히 모르겠지만 끊어진 경우
			session->del = true;
			return;
		}
		else if (errorCodeWSASend == WSAEWOULDBLOCK)
			return;

		printf("WSASend() error. %d\n", errorCodeWSASend);
		Sleep(INFINITE);
	}

	// 정말로 send() 함수가 보내기로한 len 이하로만 반활할까? 정말 msdn 문서와 동일할까?
	if (bytesSent > session->sendRingBuf.size() || bytesSent == 0)
	{
		printf("WSASend() return -> %d\n", returnValueWSASend);
		printf("WSAGetLastError() -> %d\n", WSAGetLastError());
		printf("send() msgLen -> %d\n", session->sendRingBuf.size());
		printf("bytesSent -> %d\n", bytesSent);
		Sleep(INFINITE);
	}

	// 일부만 보내져도 괜찮음. Front를 그만큼 옮김.
	session->sendRingBuf.MoveFront(bytesSent);
}

void ProcessMessage(Session* session)
{
	while (1)
	{
		// 최소한 헤더는 받았는가?
		if (session->recvRingBuf.size() < sizeof(MessageHeader))
			break;

		MessageHeader header;
		session->recvRingBuf.peek((char*)&header, sizeof(header));

		// 최소한 헤더 + 메시지 받았는가?
		if (session->recvRingBuf.size() < sizeof(header) + header.len)
			break;

		session->recvRingBuf.MoveFront(sizeof(header));

		switch (header.type)
		{
			case PACKET_REQ_MOVE_START:
			{
				MoveStart msgMoveStart;
				session->recvRingBuf.pop((char*)&msgMoveStart, sizeof(msgMoveStart));

				// 만약 클라의 좌표가 서버의 좌표와 두 프레임치(최소 40ms) 넘게 차이나면 연결 끊어버리기.
				// 네트워크 딜레이가 10ms 안쪽이라고 가정하고 만들었기 때문에 두프레임 넘게 차이나는 일이 거의 없을 것임.
				if (abs(msgMoveStart.x - session->x) > RANGE_MOVE_ERROR || abs(msgMoveStart.y - session->y) > RANGE_MOVE_ERROR)
				{
					DebugPrint("[Disconnect Client -> MoveStart Error] ID: %d, Direction: %d, X: %d, Y: %d, x-diff: %d, y-diff: %d\n", 
						session->id, msgMoveStart.direction, msgMoveStart.x, msgMoveStart.y, abs(msgMoveStart.x - session->x), abs(msgMoveStart.y - session->y));
					session->del = true;
					return;
				}

				session->direction = msgMoveStart.direction;
				session->move = true;
				session->x = msgMoveStart.x;
				session->y = msgMoveStart.y;

				DebugPrint("[Client MoveStart] ID: %d, Direction: %d, X: %d, Y: %d\n", session->id, session->direction, session->x, session->y);

				MoveStartOther msgMoveStartOther;
				msgMoveStartOther.direction = session->direction;
				msgMoveStartOther.id = session->id;
				msgMoveStartOther.x = session->x;
				msgMoveStartOther.y = session->y;
				SendBroadcast(session, PACKET_RES_MOVE_START, msgMoveStartOther);
				break;
			}
			case PACKET_REQ_MOVE_STOP:
			{
				MoveStop msgMoveStop;
				session->recvRingBuf.pop((char*)&msgMoveStop, sizeof(msgMoveStop));

				// 만약 클라의 좌표가 서버의 좌표와 두 프레임치(최소 40ms) 넘게 차이나면 연결 끊어버리기.
				// 네트워크 딜레이가 10ms 안쪽이라고 가정하고 만들었기 때문에 두프레임 넘게 차이나는 일이 거의 없을 것임.
				if (abs(msgMoveStop.x - session->x) > RANGE_MOVE_ERROR || abs(msgMoveStop.y - session->y) > RANGE_MOVE_ERROR)
				{
					DebugPrint("[Disconnect Client -> Client MoveStop Error] ID: %d, Direction: %d, X: %d, Y: %d, x-diff: %d, y-diff: %d\n",
						session->id, msgMoveStop.direction, msgMoveStop.x, msgMoveStop.y, abs(msgMoveStop.x - session->x), abs(msgMoveStop.y - session->y));
					session->del = true;
					return;
				}

				session->direction = msgMoveStop.direction;
				session->move = false;
				session->x = msgMoveStop.x;
				session->y = msgMoveStop.y;

				DebugPrint("[Client MoveStop] ID: %d, Direction: %d, X: %d, Y: %d\n", session->id, session->direction, session->x, session->y);

				MoveStopOther msgMoveStopOther;
				msgMoveStopOther.direction = session->direction;
				msgMoveStopOther.id = session->id;
				msgMoveStopOther.x = session->x;
				msgMoveStopOther.y = session->y;
				SendBroadcast(session, PACKET_RES_MOVE_STOP, msgMoveStopOther);
				break;
			}
			case PACKET_REQ_ATTACK1:
			{
				// 쿨타임. 일단은 1/3 초
				if (session->attack1Cooldown < CLOCKS_PER_SEC / 3)
					break;

				session->attack1Cooldown = 0;

				Attack1Request msgAttack1Request;
				session->recvRingBuf.pop((char*)&msgAttack1Request, sizeof(msgAttack1Request));

				// 만약 클라의 좌표가 서버의 좌표와 두 프레임치(최소 40ms) 넘게 차이나면 연결 끊어버리기.
				// 네트워크 딜레이가 10ms 안쪽이라고 가정하고 만들었기 때문에 두프레임 넘게 차이나는 일이 거의 없을 것임.
				if (abs(msgAttack1Request.x - session->x) > RANGE_MOVE_ERROR || abs(msgAttack1Request.y - session->y) > RANGE_MOVE_ERROR)
				{
					DebugPrint("[Disconnect Client -> Client Attack1Request Error] ID: %d, Direction: %d, X: %d, Y: %d, x-diff: %d, y-diff: %d\n", 
						session->id, msgAttack1Request.direction, msgAttack1Request.x, msgAttack1Request.y, abs(msgAttack1Request.x - session->x), abs(msgAttack1Request.y - session->y));
					session->del = true;
					return;
				}

				session->direction = msgAttack1Request.direction;
				session->x = msgAttack1Request.x;
				session->y = msgAttack1Request.y;

				DebugPrint("[Client Attack1] ID: %d, Direction: %d, X: %d, Y: %d\n", session->id, session->direction, session->x, session->y);

				Attack1Response msgAttack1Response;
				msgAttack1Response.direction = session->direction;
				msgAttack1Response.id = session->id;
				msgAttack1Response.x = session->x;
				msgAttack1Response.y = session->y;
				SendBroadcast(session, PACKET_RES_ATTACK1, msgAttack1Response);

				// 충돌체크
				for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end(); ++it)
				{
					if (session->id != (*it)->id && CheckCollision(session, (*it), session->x + ATTACK1_POINT_X, session->y + ATTACK1_POINT_Y))
					{
						// 데미지 메시지 생성. 모두에게 보내기
						(*it)->hp -= 1;
						Damage msgDamage;
						msgDamage.AttackID = session->id;
						msgDamage.DamageID = (*it)->id;
						msgDamage.DamageHP = (*it)->hp;
						SendBroadcast(nullptr, PACKET_RES_DAMAGE, msgDamage);

						// 피격자 사망.
						if ((*it)->hp <= 0)
						{
							// 캐릭터 삭제 메시지. 모두에게 보내야할까? 삭제하는 플레이어 빼고 보내야할까?
							(*it)->del = true;
							DeletePlayer msgDeletePlayer;
							msgDeletePlayer.id = (*it)->id;
							SendBroadcast(nullptr, PACKET_RES_DELETE_CHARACTER, msgDeletePlayer);
						}
					}
				}
				break;
			}
			case PACKET_REQ_ATTACK2:
			{
				// 쿨타임. 일단은 1/3 초
				if (session->attack2Cooldown < CLOCKS_PER_SEC / 3)
					break;

				session->attack2Cooldown = 0;

				Attack2Request msgAttack2Request;
				session->recvRingBuf.pop((char*)&msgAttack2Request, sizeof(msgAttack2Request));

				// 만약 클라의 좌표가 서버의 좌표와 두 프레임치(최소 40ms) 넘게 차이나면 연결 끊어버리기.
				// 네트워크 딜레이가 10ms 안쪽이라고 가정하고 만들었기 때문에 두프레임 넘게 차이나는 일이 거의 없을 것임.
				if (abs(msgAttack2Request.x - session->x) > RANGE_MOVE_ERROR || abs(msgAttack2Request.y - session->y) > RANGE_MOVE_ERROR)
				{
					DebugPrint("[Disconnect Client -> Client Attack2Request Error] ID: %d, Direction: %d, X: %d, Y: %d, x-diff: %d, y-diff: %d\n",
						session->id, msgAttack2Request.direction, msgAttack2Request.x, msgAttack2Request.y, abs(msgAttack2Request.x - session->x), abs(msgAttack2Request.y - session->y));
					session->del = true;
					return;
				}

				DebugPrint("[Client Attack2] ID: %d, Direction: %d, X: %d, Y: %d\n", session->id, session->direction, session->x, session->y);

				session->direction = msgAttack2Request.direction;
				session->x = msgAttack2Request.x;
				session->y = msgAttack2Request.y;

				// 다른플레이어에게 브로드캐스트
				Attack2Response msgAttack2Response;
				msgAttack2Response.direction = session->direction;
				msgAttack2Response.id = session->id;
				msgAttack2Response.x = session->x;
				msgAttack2Response.y = session->y;
				SendBroadcast(session, PACKET_RES_ATTACK2, msgAttack2Response);

				// 충돌체크
				for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end(); ++it)
				{
					if (session->id != (*it)->id && CheckCollision(session, (*it), session->x + ATTACK2_POINT_X, session->y + ATTACK2_POINT_Y))
					{
						// 데미지 메시지 생성. 모두에게 보내기
						(*it)->hp -= 2;
						Damage msgDamage;
						msgDamage.AttackID = session->id;
						msgDamage.DamageID = (*it)->id;
						msgDamage.DamageHP = (*it)->hp;
						SendBroadcast(nullptr, PACKET_RES_DAMAGE, msgDamage);

						// 피격자 사망.
						if ((*it)->hp <= 0)
						{
							// 캐릭터 삭제 메시지. 모두에게 보내야할까? 삭제하는 플레이어 빼고 보내야할까?
							(*it)->del = true;
							DeletePlayer msgDeletePlayer;
							msgDeletePlayer.id = (*it)->id;
							SendBroadcast(nullptr, PACKET_RES_DELETE_CHARACTER, msgDeletePlayer);
						}
					}
				}
				break;
			}
			case PACKET_REQ_ATTACK3:
			{
				// 쿨타임. 일단은 1/3 초
				if (session->attack3Cooldown < CLOCKS_PER_SEC / 3)
					break;

				session->attack3Cooldown = 0;

				Attack3Request msgAttack3Request;
				session->recvRingBuf.pop((char*)&msgAttack3Request, sizeof(msgAttack3Request));

				// 만약 클라의 좌표가 서버의 좌표와 두 프레임치(최소 40ms) 넘게 차이나면 연결 끊어버리기.
				// 네트워크 딜레이가 10ms 안쪽이라고 가정하고 만들었기 때문에 두프레임 넘게 차이나는 일이 거의 없을 것임.
				if (abs(msgAttack3Request.x - session->x) > RANGE_MOVE_ERROR || abs(msgAttack3Request.y - session->y) > RANGE_MOVE_ERROR)
				{
					DebugPrint("[Disconnect Client -> Client Attack3Request Error] ID: %d, Direction: %d, X: %d, Y: %d, x-diff: %d, y-diff: %d\n",
						session->id, msgAttack3Request.direction, msgAttack3Request.x, msgAttack3Request.y, abs(msgAttack3Request.x - session->x), abs(msgAttack3Request.y - session->y));
					session->del = true;
					return;
				}

				DebugPrint("[Client Attack3] ID: %d, Direction: %d, X: %d, Y: %d\n", session->id, session->direction, session->x, session->y);

				session->direction = msgAttack3Request.direction;
				session->x = msgAttack3Request.x;
				session->y = msgAttack3Request.y;

				Attack3Response msgAttack3Response;
				msgAttack3Response.direction = session->direction;
				msgAttack3Response.id = session->id;
				msgAttack3Response.x = session->x;
				msgAttack3Response.y = session->y;
				SendBroadcast(session, PACKET_RES_ATTACK3, msgAttack3Response);

				// 충돌체크
				for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end(); ++it)
				{
					if (session->id != (*it)->id && CheckCollision(session, (*it), session->x + ATTACK3_POINT_X, session->y + ATTACK3_POINT_Y))
					{
						// 데미지 메시지 생성. 모두에게 보내기
						(*it)->hp -= 3;
						Damage msgDamage;
						msgDamage.AttackID = session->id;
						msgDamage.DamageID = (*it)->id;
						msgDamage.DamageHP = (*it)->hp;
						SendBroadcast(nullptr, PACKET_RES_DAMAGE, msgDamage);

						// 피격자 사망.
						if ((*it)->hp <= 0)
						{
							// 캐릭터 삭제 메시지. 모두에게 보내야할까? 삭제하는 플레이어 빼고 보내야할까?
							(*it)->del = true;
							DeletePlayer msgDeletePlayer;
							msgDeletePlayer.id = (*it)->id;
							SendBroadcast(nullptr, PACKET_RES_DELETE_CHARACTER, msgDeletePlayer);
						}
					}
				}
				break;
			}
		}
	}
}

bool CheckCollision(Session* attacker, Session* target, short attackPointX, short attackPointY)
{
	if (attacker == nullptr || target == nullptr)
		return false;

	// 타겟 히트박스
	short hitBoxStartX = target->x - (HITBOX_WIDTH / 2);
	short hitBoxEndX = target->x + (HITBOX_WIDTH / 2);
	short hitBoxStartY = target->y;
	short hitBoxEndY = target->y + HITBOX_HEIGHT;

	// 타격점(x, y)가 플레이어 히트박스 내부에 있다면 타격
	if (attackPointX >= hitBoxStartX && attackPointX <= hitBoxEndX && attackPointY >= hitBoxStartY && attackPointY <= hitBoxEndY)
		return true;

	return false;
}