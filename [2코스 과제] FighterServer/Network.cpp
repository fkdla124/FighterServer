#include <cstdio>
#include <cmath>
#include <ctime>

#include "Network.h"
#include "RingBuffer.h"
#include "List.h"
#include "SocketErrorcode.h"
#include "Protocol.h"
#include "PacketDefine.h"
#include "Define.h"
#include "GameLogic.h"
#include "Session.h"
#include "Debug.h"

#include <windows.h>
#pragma comment(lib, "winmm.lib")

// todo
// 
//
// 네트워크 처리는 프레임과 관계없이 최대부하로 돌게끔 하고 로직처리만 50fps 나오게끔. 시간기반으로 short 좌표 말고 float 좌표로 계산
// 100% 프레임의존으로 간다 -> 1번의 게임 루프에 네트워크처리 + 50fps에 도달하려면 필요한 로직() 반복
// 시간 의존으로 간다 -> 1번의 게임 루프에 네트워크처리 + 로직처리 한 번씩만. 로직()에서 시간을 계산해 그만큼 float로 이동처리. 1초에 x=150 y=100 이니까 이거로 계산

// 나는 시간의존으로 간다.

// 디버깅프린트 테스트 완료 후 없애고나서 이걸 출력하면 됨. (디버그프린트 디파인 안 할 시에)
// 1. 지금 세션개수
// 2. 초당 메시지 몇건 송수신하는가, 바이트 처리량
// 3. 초당 accept() 횟수




////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 전역 데이터

#define SERVER_PORT 5000
SOCKET listenSocket;

List<Session*> sessionList;

// 세션별 할당 ID. 0부터 +1씩
int idCount = 0;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 함수

// 초기화, 서버종료
void StartServer(linger _linger, ULONG nonBlocking);
void StopServer();

// 네트워크
void Network();
void SelectSocketEvents(fd_set* rset, fd_set* wset);
void AcceptClient();
void RecvMessage(Session* session);
void ProcessMessage(Session* session);
void FlushSendBuffer(Session* session);

// 클라 연결종료 관련
void DisconnectSession(Session* session);	// 삭제 마킹, 플레이어 제거 메시지 보내기
void SweepMarkedSession();					// 마킹된 세션 세션리스트에서 삭제.

// 링버퍼 메시지 복사. 실제 send()는 FlushSendBuffer()
template <typename T>
void SendUnicast(Session* session, MessageHeader& msgHeader, const T& msg);
template <typename T>
void SendBroadcast(Session* exceptSession, MessageHeader& msgHeader, const T& msg);


// 공격 충돌처리
bool CheckCollision(char attackType, char attackerFacing, float attackerX, float attackerY, float targetX, float targetY);

// 메시지 실제 처리 함수
void HandleMoveStart(Session* session, char* msg);
void HandleMoveStop(Session* session, char* msg);
void HandleAttack1Request(Session* session, char* msg);
void HandleAttack2Request(Session* session, char* msg);
void HandleAttack3Request(Session* session, char* msg);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StartServer(linger _linger, ULONG nonBlocking)
{
	timeBeginPeriod(1);
	srand(0x20020315);

	SetConsoleTitle(L"[Fighter Server]");

	printf("===========================SERVER OPEN================================\n");

	// WSAStartup()
	WSADATA wsa;
	returnValueWSAStartup = WSAStartup(MAKEWORD(2, 2), &wsa);
	if (returnValueWSAStartup != 0)
	{
		errorCodeWSAStartUp = WSAGetLastError();
		printf("WSAStartup() error. %d\n", errorCodeWSAStartUp);
		Sleep(INFINITE);
	}
	printf("WSAStartup() Success.\n");

	// socket()
	listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenSocket == INVALID_SOCKET)
	{
		errorCodeSocket = WSAGetLastError();
		printf("socket() error. %d\n", errorCodeSocket);
		Sleep(INFINITE);
	}
	printf("socket() Success.\n");

	// linger set
	returnValueSetSockOpt = setsockopt(listenSocket, SOL_SOCKET, SO_LINGER, (char*)&_linger, sizeof(_linger));
	if (returnValueSetSockOpt == SOCKET_ERROR)
	{
		errorCodeSetSockOpt = WSAGetLastError();
		printf("setsockopt() LINGER{0, 0} setting error. %d\n", errorCodeSetSockOpt);
		Sleep(INFINITE);
	}
	printf("linger{1, 0} Set Success.\n");

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
	printf("bind() Success. IP: INADDR_ANY, Port: %d\n", SERVER_PORT);

	// listen()
	returnValueListen = listen(listenSocket, SOMAXCONN);
	if (returnValueListen == SOCKET_ERROR)
	{
		errorCodeListen = WSAGetLastError();
		printf("listen() error. %d\n", errorCodeListen);
		Sleep(INFINITE);
	}
	printf("listen() Success.\n");

	// nonblocking change
	returnValueIoctlSocket = ioctlsocket(listenSocket, FIONBIO, &nonBlocking);
	if (returnValueIoctlSocket == SOCKET_ERROR)
	{
		errorCodeIoctlSocket = WSAGetLastError();
		printf("ioctlsocket() error. %d\n", errorCodeIoctlSocket);
		Sleep(INFINITE);
	}
	printf("nonblocking socket change.\n");

	printf("======================================================================\n");

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

	// select 처리
	SelectSocketEvents(&rset, &wset);

	// accept
	if (FD_ISSET(listenSocket, &rset))
		AcceptClient();

	for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end(); ++it)
	{
		// rset 처리
		if (FD_ISSET((*it)->sock, &rset))
		{
			// TCP 수신버퍼 -> 수신링버퍼 메시지 수신 (WSARecv -> RingBuffer)
			RecvMessage((*it));

			// 수신링버퍼 메시지 처리
			ProcessMessage((*it));
		}

		// wset 처리
		if (FD_ISSET((*it)->sock, &wset))
		{
			// 송신링버퍼 플러쉬
			FlushSendBuffer((*it));
		}
	}

	// 지연삭제
	SweepMarkedSession();
}

void SelectSocketEvents(fd_set* rset, fd_set* wset)
{
	// listenSocket rset
	FD_SET(listenSocket, rset);

	// session rset, wset
	for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end(); ++it)
	{
		FD_SET((*it)->sock, rset);

		if ((*it)->sendRingBuf.empty() == false)
		{
			FD_SET((*it)->sock, wset);
		}
	}

	// select
	timeval tv = { 0, 0 };
	returnValueSelect = select(0, rset, wset, NULL, &tv);
	if (returnValueSelect == SOCKET_ERROR)
	{
		errorCodeSelect = WSAGetLastError();
		printf("select() error. %d\n", errorCodeSelect);
		Sleep(INFINITE);
	}
}

void AcceptClient()
{
	DebugPrint("\n================================AcceptClient()====================================\n");

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
		Session* acceptSession = new Session(acceptSocket, acceptAddr);
		sessionList.push_back(acceptSession);

		char acceptSessionIp[16];
		InetNtopA(AF_INET, &acceptSession->addr.sin_addr, acceptSessionIp, sizeof(acceptSessionIp));
		DebugPrint("[Client Accept %s:%d] id: %d, direction: %d, facing: %d, x: %d, y: %d\n"
			, acceptSessionIp, acceptSession->addr.sin_port, acceptSession->id, acceptSession->direction, acceptSession->facing, acceptSession->x, acceptSession->y);

		// 접속한 플레이어 생성 메시지
		MessageHeader msgHeaderCreatePlayer;
		CreatePlayer msgCreatePlayer;
		mpCreatePlayer(&msgHeaderCreatePlayer, &msgCreatePlayer, acceptSession->id, acceptSession->direction, acceptSession->x, acceptSession->y, acceptSession->hp);
		SendUnicast(acceptSession, msgHeaderCreatePlayer, msgCreatePlayer);

		// 접속한 플레이어를 다른 플레이어에게 알리기
		MessageHeader msgHeaderCreateOther;
		CreateOther msgCreateOther;
		mpCreateOther(&msgHeaderCreateOther, &msgCreateOther, acceptSession->id, acceptSession->direction, acceptSession->x, acceptSession->y, acceptSession->hp);
		SendBroadcast(acceptSession, msgHeaderCreateOther, msgCreateOther);

		// 접속한 플레이어에게 다른 플레이어들 생성하는 메시지
		for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end(); ++it)
		{
			if ((*it)->id != acceptSession->id)
			{
				MessageHeader msgHeaderCreateOther;
				CreateOther msgCreateOther;
				mpCreateOther(&msgHeaderCreateOther, &msgCreateOther, (*it)->id, (*it)->direction, (*it)->x, (*it)->y, (*it)->hp);
				SendUnicast(acceptSession, msgHeaderCreateOther, msgCreateOther);
			}
		}
	}
}

void RecvMessage(Session* session)
{
	DebugPrint("\n\n================================RecvMessage()====================================\n");
	// WSARecv() -> 링버퍼
	WSABUF wsaBuf[2];
	wsaBuf[0].buf = session->recvRingBuf.GetRear();
	wsaBuf[0].len = session->recvRingBuf.DirectPushSize();
	wsaBuf[1].buf = session->recvRingBuf.GetBuffer();
	wsaBuf[1].len = static_cast<ULONG>(session->recvRingBuf.GetFront() - wsaBuf[1].buf);

	DWORD bytesReceived;
	DWORD flags = 0;
	returnValueWSARecv = WSARecv(session->sock, wsaBuf, 2, &bytesReceived, &flags, NULL, NULL);
	if (returnValueWSARecv == SOCKET_ERROR)
	{
		errorCodeWSARecv = WSAGetLastError();

		if (errorCodeWSARecv == WSAECONNRESET)
		{
			// RST 전송으로 끊어진 경우
			DisconnectSession(session);
			return;
		}
		else if (errorCodeWSARecv == WSAECONNABORTED)
		{
			// 자세히 모르겠지만 끊어진 경우
			DisconnectSession(session);
			return;
		}
		else if (errorCodeWSARecv == WSAEWOULDBLOCK)
			return;

		// 그 외 에러
		printf("WSARecv() error. %d\n", errorCodeWSARecv);
		Sleep(INFINITE);
	}
	else if (bytesReceived == 0)
	{
		// FIN 전송으로 끊어진 경우
		DisconnectSession(session);
		return;
	}

	DebugPrint("WSARecv() success.. SessionID: %d, returnValueWSARecv: %d, bytesReceived: %d\n", session->id, returnValueWSARecv, bytesReceived);

	session->recvRingBuf.MoveRear(bytesReceived);
}

void ProcessMessage(Session* session)
{
	// 마킹된 세션은 처리 X
	if (session->del == true)
		return;

	DebugPrint("================================ProcessMessage()====================================\n");

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

		// 헤더 pop
		session->recvRingBuf.MoveFront(sizeof(header));

		// 페이로드 pop
		char msg[32];
		session->recvRingBuf.pop(msg, header.len);

		switch (header.type)
		{
			case PACKET_REQ_MOVE_START:
			{
				HandleMoveStart(session, msg);
				break;
			}
			case PACKET_REQ_MOVE_STOP:
			{
				HandleMoveStop(session, msg);
				break;
			}
			case PACKET_REQ_ATTACK1:
			{
				HandleAttack1Request(session, msg);
				break;
			}
			case PACKET_REQ_ATTACK2:
			{
				HandleAttack2Request(session, msg);
				break;
			}
			case PACKET_REQ_ATTACK3:
			{
				HandleAttack3Request(session, msg);
				break;
			}
		}
	}
}

void FlushSendBuffer(Session* session)
{
	DebugPrint("================================FlushSendBuffer()====================================\n");

	DWORD bufCount = 0;

	// 일단 버퍼 하나 넣음.
	WSABUF wsaBuf[2];
	wsaBuf[0].buf = session->sendRingBuf.GetFront();
	wsaBuf[0].len = session->sendRingBuf.DirectPopSize();
	bufCount++;

	// 보낼게 더있으면 하나 더 넣음.
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
			DisconnectSession(session);
			return;
		}
		else if (errorCodeWSASend == WSAECONNABORTED)
		{
			// 자세히 모르겠지만 끊어진 경우
			DisconnectSession(session);
			return;
		}
		else if (errorCodeWSASend == WSAEWOULDBLOCK)
			return;

		printf("WSASend() error. %d\n", errorCodeWSASend);
		Sleep(INFINITE);
	}

	DebugPrint("WSASend() success.. SessionID: %d, returnValueWSASend: %d, sendBytes: %d\n", session->id, returnValueWSASend, bytesSent);

	// 정말로 send() 함수가 보내기로한 len 이하로만 반활할까? 정말 msdn 문서와 동일할까?
	if (bytesSent > session->sendRingBuf.size())
	{
		printf("WSASend() return -> %d\n", returnValueWSASend);
		printf("WSAGetLastError() -> %d\n", WSAGetLastError());
		printf("send() msgLen -> %d\n", (int)(session->sendRingBuf.size()));
		printf("bytesSent -> %d\n", bytesSent);
		Sleep(INFINITE);
	}

	// 일부만 보내져도 괜찮음. Front를 그만큼 옮김.
	session->sendRingBuf.MoveFront(bytesSent);
}

void DisconnectSession(Session* session)
{
	DebugPrint("[Delete Mark] ID: %d\n", session->id);

	session->del = true;
	MessageHeader msgDeletePlayerHeader;
	DeletePlayer msgDeletePlayer;
	mpDeletePlayer(&msgDeletePlayerHeader, &msgDeletePlayer, session->id);
	SendBroadcast(session, msgDeletePlayerHeader, msgDeletePlayer);
}

void SweepMarkedSession()
{
	for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end();)
	{
		if ((*it)->del == true)
		{
			DebugPrint("[Sweep Marked Session] ID: %d\n", (*it)->id);
			closesocket((*it)->sock);
			it = sessionList.erase(it);
		}
		else
			++it;
	}
}

template <typename T>
void SendUnicast(Session* session, MessageHeader& msgHeader, const T& msg)
{
	// 패킷 생성
	char msgBuffer[sizeof(msgHeader) + sizeof(T)];
	memcpy(msgBuffer, &msgHeader, sizeof(msgHeader));
	memcpy(msgBuffer + sizeof(msgHeader), &msg, sizeof(T));

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
		sprintf_s(fileName, "[SendRingBuffer Full Error]%04d%02d%02d_%02d%02d%02d.txt", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);

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
		fwprintf(fp, L"MsgType = %d\n", msgHeader.type);
		fclose(fp);
		#pragma warning(default:6387 6328)

		// 지연삭제 마킹 및 연결끊기. SO_LINGER {0, 0} RST 전송
		DisconnectSession(session);

		printf("[!!!Error!!!] SendUnicast() -> session->sendRingBuf.push() error. capacity: %d, size: %d, pushSize: %d, return: %d\n",
			session->sendRingBuf.capacity(), session->sendRingBuf.size(), (int)sizeof(msgBuffer), returnValuePush);
	}

	// 디버깅용 출력
	switch (msgHeader.type)
	{
		case PACKET_RES_MOVE_START:
		{
			MoveStartOther* msgPtr = (MoveStartOther*)(&msg);
			DebugPrint("[Server MoveStartOther] SessionID: %d, Header(packetCode: 0x%02x, len: %d, type: PACKET_RES_MOVE_START), Payload(id: %d, direction: %d, x: %d, y: %d)\n",
				session->id, (unsigned char)msgHeader.packetCode, msgHeader.len, msgPtr->id, msgPtr->direction, msgPtr->x, msgPtr->y);
			break;
		}
		case PACKET_RES_MOVE_STOP:
		{
			MoveStopOther* msgPtr = (MoveStopOther*)(&msg);
			DebugPrint("[Server MoveStopOther] SessionID: %d, Header(packetCode: 0x%02x, len: %d, type: PACKET_RES_MOVE_STOP), Payload(id: %d, direction: %d, x: %d, y: %d)\n",
				session->id, (unsigned char)msgHeader.packetCode, msgHeader.len, msgPtr->id, msgPtr->direction, msgPtr->x, msgPtr->y);
			break;
		}
		case PACKET_RES_ATTACK1:
		{
			Attack1Response* msgPtr = (Attack1Response*)(&msg);
			DebugPrint("[Server Attack1Response] SessionID: %d, Header(packetCode: 0x%02x, len: %d, type: PACKET_RES_ATTACK1), Payload(id: %d, direction: %d, x: %d, y: %d)\n",
				session->id, (unsigned char)msgHeader.packetCode, msgHeader.len, msgPtr->id, msgPtr->direction, msgPtr->x, msgPtr->y);
			break;
		}
		case PACKET_RES_ATTACK2:
		{
			Attack2Response* msgPtr = (Attack2Response*)(&msg);
			DebugPrint("[Server Attack2Response] SessionID: %d, Header(packetCode: 0x%02x, len: %d, type: PACKET_RES_ATTACK2), Payload(id: %d, direction: %d, x: %d, y: %d)\n",
				session->id, (unsigned char)msgHeader.packetCode, msgHeader.len, msgPtr->id, msgPtr->direction, msgPtr->x, msgPtr->y);
			break;
		}
		case PACKET_RES_ATTACK3:
		{
			Attack3Response* msgPtr = (Attack3Response*)(&msg);
			DebugPrint("[Server Attack3Response] SessionID: %d, Header(packetCode: 0x%02x, len: %d, type: PACKET_RES_ATTACK3), Payload(id: %d, direction: %d, x: %d, y: %d)\n",
				session->id, (unsigned char)msgHeader.packetCode, msgHeader.len, msgPtr->id, msgPtr->direction, msgPtr->x, msgPtr->y);
			break;
		}
		case PACKET_RES_DAMAGE:
		{
			Damage* msgPtr = (Damage*)(&msg);
			DebugPrint("[Server Damage] SessionID: %d, Header(packetCode: 0x%02x, len: %d, type: PACKET_RES_DAMAGE), Payload(attackID: %d, targetID: %d, targetHP: %d)\n",
				session->id, (unsigned char)msgHeader.packetCode, msgHeader.len, msgPtr->attackID, msgPtr->damageID, msgPtr->damageHP);
			break;
		}
	}
}

template <typename T>
void SendBroadcast(Session* exceptSession, MessageHeader& header, const T& msg)
{
	if (exceptSession == nullptr)
	{
		for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end(); ++it)
		{
			SendUnicast((*it), header, msg);
		}
	}
	else
	{
		for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end(); ++it)
		{
			if ((*it)->sock != exceptSession->sock)
			{
				SendUnicast((*it), header, msg);
			}
		}
	}
}

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

void HandleMoveStart(Session* session, char* msg)
{
	MoveStart* msgPtr = (MoveStart*)msg;

	// 만약 클라의 좌표가 서버의 좌표와 두 프레임치(최소 40ms) 넘게 차이나면 연결 끊어버리기.
	// 네트워크 딜레이가 10ms 안쪽이라고 가정하고 만들었기 때문에 두프레임 넘게 차이나는 일이 거의 없을 것임.
	if (abs(msgPtr->x - session->x) > RANGE_MOVE_ERROR || abs(msgPtr->y - session->y) > RANGE_MOVE_ERROR)
	{
		DebugPrint("[MoveStart Error -> Disconnect Client] SessionID: %d, Direction: %d, X: %d, Y: %d, X-diff: %d, Y-diff: %d\n",
			session->id, msgPtr->direction, msgPtr->x, msgPtr->y, abs(msgPtr->x - session->x), abs(msgPtr->y - session->y));
		DisconnectSession(session);
		return;
	}

	session->direction = msgPtr->direction;
	session->move = true;
	session->x = msgPtr->x;
	session->y = msgPtr->y;

	switch (msgPtr->direction)
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

	DebugPrint("[Client MoveStart] SessionID: %d, Direction: %d, Facing(server): %d, X: %d, Y: %d\n", session->id, session->direction, session->facing, session->x, session->y);

	MessageHeader msgHeaderMoveStartOther;
	MoveStartOther msgMoveStartOther;
	mpMoveStartOther(&msgHeaderMoveStartOther, &msgMoveStartOther, session->id, session->direction, session->x, session->y);
	SendBroadcast(session, msgHeaderMoveStartOther, msgMoveStartOther);
}

void HandleMoveStop(Session* session, char* msg)
{
	MoveStop* msgPtr = (MoveStop*)msg;

	// 만약 클라의 좌표가 서버의 좌표와 두 프레임치(최소 40ms) 넘게 차이나면 연결 끊어버리기.
	// 네트워크 딜레이가 10ms 안쪽이라고 가정하고 만들었기 때문에 두프레임 넘게 차이나는 일이 거의 없을 것임.
	if (abs(msgPtr->x - session->x) > RANGE_MOVE_ERROR || abs(msgPtr->y - session->y) > RANGE_MOVE_ERROR)
	{
		DebugPrint("[!!! ERROR !!! Client MoveStop Error -> Disconnect Client] SessionID: %d, Direction: %d, X: %d, Y: %d, X-diff: %d, Y-diff: %d\n",
			session->id, msgPtr->direction, msgPtr->x, msgPtr->y, abs(msgPtr->x - session->x), abs(msgPtr->y - session->y));
		session->del = true;
		return;
	}

	session->direction = msgPtr->direction;
	session->move = false;
	session->x = msgPtr->x;
	session->y = msgPtr->y;

	switch (msgPtr->direction)
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

	DebugPrint("[Client MoveStop] SessionID: %d, Direction: %d, Facing(server): %d, X: %d, Y: %d\n", session->id, session->direction, session->facing, session->x, session->y);

	MessageHeader msgHeaderMoveStopOther;
	MoveStopOther msgMoveStopOther;
	mpMoveStopOther(&msgHeaderMoveStopOther, &msgMoveStopOther, session->id, session->direction, session->x, session->y);
	SendBroadcast(session, msgHeaderMoveStopOther, msgMoveStopOther);
}

void HandleAttack1Request(Session* session, char* msg)
{
	// 지난 공격의 쿨타임이 지나지 않았다면 이번패킷은 무시하고 다음메시지 처리
	if (timeGetTime() - session->lastAttackTick <= session->lastAttackDuration - ATTACK_TIME_TOLERANCE)
	{
		session->recvRingBuf.MoveFront(sizeof(Attack1Request));
		return;
	}

	// 쿨타임을 위해 해당 공격 타입과 현재 틱 저장
	session->lastAttackTick = timeGetTime();
	session->lastAttackDuration = ATTACK1_ANIMATION_DURATION;

	Attack1Request* msgPtr = (Attack1Request*)msg;

	// 만약 클라의 좌표가 서버의 좌표와 두 프레임치(최소 40ms) 넘게 차이나면 연결 끊어버리기.
	// 네트워크 딜레이가 10ms 안쪽이라고 가정하고 만들었기 때문에 두프레임 넘게 차이나는 일이 거의 없을 것임.
	if (abs(msgPtr->x - session->x) > RANGE_MOVE_ERROR || abs(msgPtr->y - session->y) > RANGE_MOVE_ERROR)
	{
		DebugPrint("[!!! ERROR !!! Client Attack1Request Error -> Disconnect Client] SessionID: %d, Direction: %d, X: %d, Y: %d, X-diff: %d, Y-diff: %d\n",
			session->id, msgPtr->direction, msgPtr->x, msgPtr->y, abs(msgPtr->x - session->x), abs(msgPtr->y - session->y));
		DisconnectSession(session);
		return;
	}

	session->direction = msgPtr->direction;
	session->x = msgPtr->x;
	session->y = msgPtr->y;

	switch (msgPtr->direction)
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

	DebugPrint("[Client Attack1Request] SessionID: %d, Direction: %d, Facing(server): %d, X: %d, Y: %d\n", session->id, session->direction, session->facing, session->x, session->y);

	MessageHeader msgAttack1ResponseHeader;
	Attack1Response msgAttack1Response;
	mpAttack1Response(&msgAttack1ResponseHeader, &msgAttack1Response, session->id, session->direction, session->x, session->y);
	SendBroadcast(session, msgAttack1ResponseHeader, msgAttack1Response);

	// 충돌체크
	for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end(); ++it)
	{
		if (session->id != (*it)->id && CheckCollision(PACKET_REQ_ATTACK1, session->facing, session->x, session->y, (*it)->x, (*it)->y))
		{
			// 데미지 메시지 생성. 모두에게 보내기
			(*it)->hp -= ATTACK1_DAMAGE;
			MessageHeader msgDamageHeader;
			Damage msgDamage;
			mpDamage(&msgDamageHeader, &msgDamage, session->id, (*it)->id, (*it)->hp);
			SendBroadcast(nullptr, msgDamageHeader, msgDamage);

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
}

void HandleAttack2Request(Session* session, char* msg)
{
	// 지난 공격의 쿨타임이 지나지 않았다면 이번패킷은 무시하고 다음메시지 처리
	if (timeGetTime() - session->lastAttackTick <= session->lastAttackDuration - ATTACK_TIME_TOLERANCE)
	{
		session->recvRingBuf.MoveFront(sizeof(Attack2Request));
		return;
	}

	// 쿨타임을 위해 해당 공격 타입과 현재 틱 저장
	session->lastAttackTick = timeGetTime();
	session->lastAttackDuration = ATTACK2_ANIMATION_DURATION;

	Attack2Request* msgPtr = (Attack2Request*)msg;

	// 만약 클라의 좌표가 서버의 좌표와 두 프레임치(최소 40ms) 넘게 차이나면 연결 끊어버리기.
	// 네트워크 딜레이가 10ms 안쪽이라고 가정하고 만들었기 때문에 두프레임 넘게 차이나는 일이 거의 없을 것임.
	if (abs(msgPtr->x - session->x) > RANGE_MOVE_ERROR || abs(msgPtr->y - session->y) > RANGE_MOVE_ERROR)
	{
		DebugPrint("[!!! ERROR !!! Client Attack2Request Error -> Disconnect Client] SessionID: %d, Direction: %d, X: %d, Y: %d, X-diff: %d, Y-diff: %d\n",
			session->id, msgPtr->direction, msgPtr->x, msgPtr->y, abs(msgPtr->x - session->x), abs(msgPtr->y - session->y));
		DisconnectSession(session);
		return;
	}

	session->direction = msgPtr->direction;
	session->x = msgPtr->x;
	session->y = msgPtr->y;

	switch (msgPtr->direction)
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

	DebugPrint("[Client Attack2Request] SessionID: %d, Direction: %d, Facing(server): %d, X: %d, Y: %d\n", session->id, session->direction, session->facing, session->x, session->y);

	// 공격액션 다른 클라에게 브로드캐스트
	MessageHeader msgAttack2ResponseHeader;
	Attack2Response msgAttack2Response;
	mpAttack2Response(&msgAttack2ResponseHeader, &msgAttack2Response, session->id, session->direction, session->x, session->y);
	SendBroadcast(session, msgAttack2ResponseHeader, msgAttack2Response);

	// 충돌체크
	for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end(); ++it)
	{
		if (session->id != (*it)->id && CheckCollision(PACKET_REQ_ATTACK2, session->facing, session->x, session->y, (*it)->x, (*it)->y))
		{
			// 데미지 메시지 생성. 모두에게 보내기
			(*it)->hp -= ATTACK2_DAMAGE;
			MessageHeader msgDamageHeader;
			Damage msgDamage;
			mpDamage(&msgDamageHeader, &msgDamage, session->id, (*it)->id, (*it)->hp);
			SendBroadcast(nullptr, msgDamageHeader, msgDamage);

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
}

void HandleAttack3Request(Session* session, char* msg)
{
	// 지난 공격의 쿨타임이 지나지 않았다면 이번패킷은 무시하고 다음메시지 처리
	if (timeGetTime() - session->lastAttackTick <= session->lastAttackDuration - ATTACK_TIME_TOLERANCE)
	{
		session->recvRingBuf.MoveFront(sizeof(Attack3Request));
		return;
	}

	// 쿨타임을 위해 해당 공격 타입과 현재 틱 저장
	session->lastAttackTick = timeGetTime();
	session->lastAttackDuration = ATTACK3_ANIMATION_DURATION;

	Attack3Request* msgPtr = (Attack3Request*)msg;

	// 만약 클라의 좌표가 서버의 좌표와 두 프레임치(최소 40ms) 넘게 차이나면 연결 끊어버리기.
	// 네트워크 딜레이가 10ms 안쪽이라고 가정하고 만들었기 때문에 두프레임 넘게 차이나는 일이 거의 없을 것임.
	if (abs(msgPtr->x - session->x) > RANGE_MOVE_ERROR || abs(msgPtr->y - session->y) > RANGE_MOVE_ERROR)
	{
		DebugPrint("[!!! ERROR !!! Client Attack3Request Error -> Disconnect Client] SessionID: %d, Direction: %d, X: %d, Y: %d, X-diff: %d, Y-diff: %d\n",
			session->id, msgPtr->direction, msgPtr->x, msgPtr->y, abs(msgPtr->x - session->x), abs(msgPtr->y - session->y));
		DisconnectSession(session);
		return;
	}

	session->direction = msgPtr->direction;
	session->x = msgPtr->x;
	session->y = msgPtr->y;

	switch (msgPtr->direction)
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

	DebugPrint("[Client Attack3Request] SessionID: %d, Direction: %d, Facing(server): %d, X: %d, Y: %d\n", session->id, session->direction, session->facing, session->x, session->y);

	MessageHeader msgAttack3ResponseHeader;
	Attack3Response msgAttack3Response;
	mpAttack3Response(&msgAttack3ResponseHeader, &msgAttack3Response, session->id, session->direction, session->x, session->y);
	SendBroadcast(session, msgAttack3ResponseHeader, msgAttack3Response);

	// 충돌체크
	for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end(); ++it)
	{
		if (session->id != (*it)->id && CheckCollision(PACKET_REQ_ATTACK3, session->facing, session->x, session->y, (*it)->x, (*it)->y))
		{
			// 데미지 메시지 생성. 모두에게 보내기
			(*it)->hp -= ATTACK3_DAMAGE;
			MessageHeader msgDamageHeader;
			Damage msgDamage;
			mpDamage(&msgDamageHeader, &msgDamage, session->id, (*it)->id, (*it)->hp);
			SendBroadcast(nullptr, msgDamageHeader, msgDamage);

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
}