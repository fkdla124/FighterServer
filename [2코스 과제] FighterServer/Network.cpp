#define FD_SETSIZE 65535

#include <cstdio>
#include <cmath>
#include <ctime>

#include "FrameControl.h"
#include "Network.h"
#include "RingBuffer.h"
#include "List.h"
#include "MsgBuf.h"
#include "SocketErrorcode.h"
#include "Protocol.h"
#include "PacketDefine.h"
#include "Define.h"
#include "GameLogic.h"
#include "Debug.h"
#include "SendPacket.h"
#include "Session.h"

// RPC
#include "RPC_ServerProxy.h"
#include "RPC_ServerStub.h"
#include "GameStub.h"

#include <windows.h>
#pragma comment(lib, "winmm.lib")

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 전역 데이터

#define SERVER_PORT 5000
static SOCKET listenSocket;

fd_set rset;
fd_set wset;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 함수

// 초기화, 서버종료
void StartServer(linger _linger, ULONG nonBlocking);
void StopServer();

// 네트워크
void Network();
static int SelectSocketEvents(fd_set* rset, fd_set* wset);
static void AcceptClient();
static void RecvMessage(Session* session);
static void ProcessMessage(Session* session);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StartServer(linger _linger, ULONG nonBlocking)
{
	timeBeginPeriod(1);
	srand(0x20020315);

	SetConsoleTitle(L"[Fighter Server]");

	printf("===========================SERVER START================================\n");

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
	printf("bind() Success. (IP: INADDR_ANY, Port: %d)\n", SERVER_PORT);

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
	printf("socket changed to non-blocking.\n");

	printf("=======================================================================\n");

	return;
}

void StopServer()
{
	// 세션 동적할당 해제
	for (auto session : sessionList)
		delete session;

	// 리스트 클리어
	sessionList.clear();

	closesocket(listenSocket);
	WSACleanup();
	timeEndPeriod(1);
}

void Network()
{
	// 네트워크 FPS 표시용.
	networkFpsCount++;

	FD_ZERO(&rset);
	FD_ZERO(&wset);

	// select 처리
	int selectCount = SelectSocketEvents(&rset, &wset);

	// accept
	if (selectCount > 0 && FD_ISSET(listenSocket, &rset))
	{
		AcceptClient();
		--selectCount;
	}

	for (List<Session*>::Iterator it = sessionList.begin(); (selectCount > 0 && it != sessionList.end()); ++it)
	{
		// rset 처리
		if (FD_ISSET((*it)->sock, &rset))
		{
			// TCP 수신버퍼 -> 수신링버퍼 메시지 수신 (WSARecv -> RingBuffer)
			RecvMessage((*it));

			// 수신링버퍼 메시지 처리
			ProcessMessage((*it));

			// 처리할 세션이 더 이상 없다면 나머지 세션은 확인 X
			--selectCount;
		}

		// wset 처리
		if (FD_ISSET((*it)->sock, &wset))
		{
			// 송신링버퍼 플러쉬
			FlushSendBuffer((*it));

			// 처리할 세션이 더 이상 없다면 나머지 세션은 확인 X
			--selectCount;
		}
	}

	// 지연삭제
	SweepMarkedSession();
}

int SelectSocketEvents(fd_set* rset, fd_set* wset)
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

	return returnValueSelect;
}

void AcceptClient()
{
	// 백로그 큐에 쌓인 연결정보를 모두 빼내기.
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

		// 세션 63개까지 가능. 그 뒤로는 closesocket()
		if (sessionList.size() >= FD_SETSIZE - 1)
		{
			closesocket(acceptSocket);
			return;
		}

		NetInfoIncreaseAcceptCount();

		// 세션정보 추가
		Session* acceptSession = new Session(acceptSocket, acceptAddr);
		sessionList.push_back(acceptSession);

		// 디버깅 정보 프린트
		char acceptSessionIp[16];
		InetNtopA(AF_INET, &acceptSession->addr.sin_addr, acceptSessionIp, sizeof(acceptSessionIp));
		DebugPrint("[Client Accept %s:%d] ID: %d, Direction: %d, Facing: %d, X: %f, Y: %f\n"
			, acceptSessionIp, acceptSession->addr.sin_port, acceptSession->id, acceptSession->direction, acceptSession->facing, acceptSession->x, acceptSession->y);

		// 접속한 플레이어 생성 메시지
		RPC::CreatePlayer(SendType::Unicast, acceptSession, acceptSession->id, acceptSession->direction, acceptSession->x, acceptSession->y, acceptSession->hp);

		// 접속한 플레이어를 다른 플레이어에게 알리기
		RPC::CreateOther(SendType::Broadcast, acceptSession, acceptSession->id, acceptSession->direction, acceptSession->x, acceptSession->y, acceptSession->hp);

		// 접속한 플레이어에게 다른 플레이어들 생성하는 메시지
		for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end(); ++it)
		{
			if ((*it)->id != acceptSession->id)
			{
				RPC::CreateOther(SendType::Unicast, acceptSession, (*it)->id, (*it)->direction, (*it)->x, (*it)->y, (*it)->hp);

				if ((*it)->move == true)
					RPC::MoveStart(SendType::Unicast, acceptSession, (*it)->id, (*it)->direction, (*it)->x, (*it)->y);
			}
		}
	}
}

void RecvMessage(Session* session)
{
	// 수신링버퍼에 공간이 없으면 나감. 
	// 로그 남기면 좋을듯. 메시지 크기가 굉장히 작고, 보내는 양도 많지 않아서 거의 없는 상황이기는 함.
	if (session->recvRingBuf.full())
		return;

	// WSARecv() -> 링버퍼
	WSABUF wsaBuf[2];
	wsaBuf[0].buf = session->recvRingBuf.GetRear();
	wsaBuf[0].len = session->recvRingBuf.DirectPushSize();
	wsaBuf[1].buf = session->recvRingBuf.GetBuffer();
	wsaBuf[1].len = static_cast<ULONG>(session->recvRingBuf.space() - session->recvRingBuf.DirectPushSize());

	DWORD bytesReceived;
	DWORD flags = 0;
	returnValueWSARecv = WSARecv(session->sock, wsaBuf, 2, &bytesReceived, &flags, NULL, NULL);
	if (returnValueWSARecv == SOCKET_ERROR)
	{
		errorCodeWSARecv = WSAGetLastError();

		if (errorCodeWSARecv == WSAECONNRESET)
		{
			// 클라측 RST 전송으로 끊어진 경우
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
			// select()로 TCP 수신버퍼에 받을 게 있고, 링버퍼 공간도 여유있을 때 들어왔음.
			// 그런데도 WOULDBLOCK이면 운영체제단에서 할 수 없다는 의미이니 다음으로 미룸.
			return;

		// 그 외 에러
		printf("WSARecv() error. %d\n", errorCodeWSARecv);
		Sleep(INFINITE);
	}
	else if (bytesReceived == 0)
	{
		// 클라측 FIN 전송으로 끊어진 경우
		DisconnectSession(session);
		return;
	}

	NetInfoIncreaseRecvBytes(bytesReceived);
	session->recvRingBuf.MoveRear(bytesReceived);
}

void ProcessMessage(Session* session)
{
	// 마킹된 세션은 처리 X
	if (session->del == true)
		return;

	while (1)
	{
		// 최소한 헤더는 받았는가?
		if (session->recvRingBuf.size() < sizeof(MessageHeader))
			break;

		MessageHeader header;
		session->recvRingBuf.peek((char*)&header, sizeof(header));

		// 우리게임 메시지가 아님 or 우리게임 메시지 이지만 기대하는 프로토콜 규격에 안 맞는 메시지임
		if (header.packetCode != (char)0x89 || header.len > PROCESS_MESSAGE_PAYLOAD_BUF_SIZE)
		{
			DisconnectSession(session);
			return;
		}

		// 최소한 헤더 + 메시지 받았는가?
		if (session->recvRingBuf.size() < sizeof(header) + header.len)
			break;

		// 헤더 pop
		session->recvRingBuf.MoveFront(sizeof(header));

		// 페이로드 직렬화버퍼 pop
		MsgBuf msg;
		int popLen = session->recvRingBuf.pop(msg.GetBuffer(), header.len);
		msg.MoveRear(popLen);

		GameStub gameStub;
		StubInterface* iStub = &gameStub;
		if (iStub->MessageProc(session, header, msg) == false)
		{
			DisconnectSession(session);
			return;
		}
	}
}