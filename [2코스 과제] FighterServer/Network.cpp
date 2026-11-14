#include <cstdio>
#include <cmath>

#include "Network.h"
#include "RingBuffer.h"
#include "List.h"
#include "SocketErrorcode.h"
#include "Protocol.h"
#include "PacketDefine.h"
#include "GameLogic.h"
#include "Session.h"

#include <windows.h>
#pragma comment(lib, "winmm.lib")

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 전역 데이터

#define SERVER_PORT 25000
SOCKET listenSocket;

List<Session*> sessionList;

int idCount = 0;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 함수

void StartServer(linger _linger, ULONG nonBlocking);
void StopServer();

void Network();
void AcceptClient();
template <typename T>
void SendUnicast(Session* session, char msgType, const T& msg);
void DisconnectMarkedSession();
void FlushSendBuffer();

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
				printf("WSARecv() error. %d\n", errorCodeWSARecv);
				Sleep(INFINITE);
			}

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
}

template <typename T>
void SendUnicast(Session* session, char msgType, const T& msg)
{
	// 헤더생성
	MessageHeader header;
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
		printf("SendUnicast() -> session->sendRingBuf.push() error. capacity: %d, size: %d, pushSize: %d, return: %d\n",
			session->sendRingBuf.capacity(), session->sendRingBuf.size(), sizeof(msgBuffer), returnValuePush);
		Sleep(INFINITE);
	}
}

template <typename T>
void SendBroadcast(Session* exceptSession, char msgType, const T& msg)
{
	// 헤더생성
	MessageHeader header;
	header.len = sizeof(T);
	header.type = msgType;

	// 메시지생성
	char msgBuffer[sizeof(header) + sizeof(T)];
	memcpy(msgBuffer, &header, sizeof(header));
	memcpy(msgBuffer + sizeof(header), &msg, sizeof(T));

	for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end(); ++it)
	{
		if ((*it)->sock != exceptSession->sock)
		{
			// 링버퍼 push
			returnValuePush = (*it)->sendRingBuf.push(msgBuffer, sizeof(msgBuffer));
			if (returnValuePush != sizeof(msgBuffer))
			{
				printf("SendBroadcast() -> (*it)->sendRingBuf.push() error. capacity: %d, size: %d, pushSize: %d, return: %d\n",
					session->sendRingBuf.capacity(), session->sendRingBuf.size(), sizeof(msgBuffer), returnValuePush);
				Sleep(INFINITE);
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
		acceptSession->x = 0;
		acceptSession->y = 0;
		sessionList.push_back(acceptSession);

		// 접속한 플레이어 생성 메시지
		CreatePlayer msg;
		msg.id = acceptSession->id;
		msg.x = acceptSession->x;
		msg.y = acceptSession->y;
		msg.direction = PACKET_MOVE_DIR_RR;
		msg.hp = 100;
		SendUnicast(acceptSession, PACKET_RES_CREATE_MY_CHARACTER, msg);

		// 다른 플레이어 생성 메시지
		for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end(); ++it)
		{
			if ((*it)->id != acceptSession->id)
			{
				CreateOther msg;
				msg.id = (*it)->id;
				msg.x = (*it)->x;
				msg.y = (*it)->y;
				msg.direction = (*it)->direction;
				msg.hp = (*it)->hp;
				SendUnicast(acceptSession, PACKET_RES_CREATE_OTHER_CHARACTER, msg);
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
	WSABUF wsaBuf[2];
	wsaBuf[0].buf = session->sendRingBuf.GetFront();
	wsaBuf[0].len = session->sendRingBuf.DirectPopSize();
	wsaBuf[1].buf = session->sendRingBuf.GetBuffer();
	wsaBuf[1].len = static_cast<ULONG>(session->sendRingBuf.GetRear() - wsaBuf[1].buf);

	DWORD bytesSent;
	returnValueWSASend = WSASend(session->sock, wsaBuf, 2, &bytesSent, NULL, NULL, NULL);
	if (returnValueWSASend == SOCKET_ERROR)
	{
		errorCodeWSASend = WSAGetLastError();
		printf("WSASend() error. %d\n", errorCodeWSASend);
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
		session->recvRingBuf.pop((char*)&header, sizeof(header));

		// 최소한 헤더 + 메시지 받았는가?
		if (session->recvRingBuf.size() < sizeof(header) + header.len)
			break;

		switch (header.type)
		{
			case PACKET_REQ_MOVE_START:
			{
				MoveStart msgMoveStart;
				session->recvRingBuf.pop((char*)&msgMoveStart, sizeof(msgMoveStart));

				// 만약 클라의 좌표가 서버의 좌표와 두 프레임치(최소 40ms) 넘게 차이나면 연결 끊어버리기.
				// 네트워크 딜레이가 10ms 안쪽이라고 가정하고 만들었기 때문에 두프레임 넘게 차이나는 일이 거의 없을 것임.
				if (abs(msgMoveStart.x - session->x) > MOVE_CHARACTER_PER_FRAME * 2 || abs(msgMoveStart.y - session->y) > MOVE_CHARACTER_PER_FRAME * 2)
				{
					session->del = true;
					return;
				}

				session->direction = msgMoveStart.direction;
				session->move = true;
				session->x = msgMoveStart.x;
				session->y = msgMoveStart.y;

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
				if (abs(msgMoveStop.x - session->x) > MOVE_CHARACTER_PER_FRAME * 2 || abs(msgMoveStop.y - session->y) > MOVE_CHARACTER_PER_FRAME * 2)
				{
					session->del = true;
					return;
				}

				session->direction = msgMoveStop.direction;
				session->move = false;
				session->x = msgMoveStop.x;
				session->y = msgMoveStop.y;

				MoveStopOther msgMoveStopOther;
				msgMoveStopOther.direction = session->direction;
				msgMoveStopOther.id = session->id;
				msgMoveStopOther.x = session->x;
				msgMoveStopOther.y = session->y;
				SendBroadcast(session, PACKET_RES_MOVE_START, msgMoveStopOther);
				break;
			}
			case PACKET_REQ_ATTACK1:
			{
				Attack1Request msgAttack1Request;
				session->recvRingBuf.pop((char*)&msgAttack1Request, sizeof(msgAttack1Request));

				// 만약 클라의 좌표가 서버의 좌표와 두 프레임치(최소 40ms) 넘게 차이나면 연결 끊어버리기.
				// 네트워크 딜레이가 10ms 안쪽이라고 가정하고 만들었기 때문에 두프레임 넘게 차이나는 일이 거의 없을 것임.
				if (abs(msgAttack1Request.x - session->x) > MOVE_CHARACTER_PER_FRAME * 2 || abs(msgAttack1Request.y - session->y) > MOVE_CHARACTER_PER_FRAME * 2)
				{
					session->del = true;
					return;
				}

				session->direction = msgAttack1Request.direction;
				session->x = msgAttack1Request.x;
				session->y = msgAttack1Request.y;

				// 공격처리 해야하는데.. 히트박스로 하나? 어찌하나? 모르겠음 아직.
				break;
			}
			case PACKET_REQ_ATTACK2:
			{
				Attack2Request msgAttack2Request;
				session->recvRingBuf.pop((char*)&msgAttack2Request, sizeof(msgAttack2Request));

				// 만약 클라의 좌표가 서버의 좌표와 두 프레임치(최소 40ms) 넘게 차이나면 연결 끊어버리기.
				// 네트워크 딜레이가 10ms 안쪽이라고 가정하고 만들었기 때문에 두프레임 넘게 차이나는 일이 거의 없을 것임.
				if (abs(msgAttack2Request.x - session->x) > MOVE_CHARACTER_PER_FRAME * 2 || abs(msgAttack2Request.y - session->y) > MOVE_CHARACTER_PER_FRAME * 2)
				{
					session->del = true;
					return;
				}

				session->direction = msgAttack2Request.direction;
				session->x = msgAttack2Request.x;
				session->y = msgAttack2Request.y;

				// 공격처리 해야하는데.. 히트박스로 하나? 어찌하나? 모르겠음 아직.
				break;
			}
			case PACKET_REQ_ATTACK3:
			{
				Attack3Request msgAttack3Request;
				session->recvRingBuf.pop((char*)&msgAttack3Request, sizeof(msgAttack3Request));

				// 만약 클라의 좌표가 서버의 좌표와 두 프레임치(최소 40ms) 넘게 차이나면 연결 끊어버리기.
				// 네트워크 딜레이가 10ms 안쪽이라고 가정하고 만들었기 때문에 두프레임 넘게 차이나는 일이 거의 없을 것임.
				if (abs(msgAttack3Request.x - session->x) > MOVE_CHARACTER_PER_FRAME * 2 || abs(msgAttack3Request.y - session->y) > MOVE_CHARACTER_PER_FRAME * 2)
				{
					session->del = true;
					return;
				}

				session->direction = msgAttack3Request.direction;
				session->x = msgAttack3Request.x;
				session->y = msgAttack3Request.y;

				// 공격처리 해야하는데.. 히트박스로 하나? 어찌하나? 모르겠음 아직.
				break;
			}
		}
	}
}