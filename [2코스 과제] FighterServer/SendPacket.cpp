#include "SendPacket.h"
#include "Session.h"
#include "MsgBuf.h"
#include "SocketErrorcode.h"
#include "List.h"
#include "Debug.h"
#include "Protocol.h"

#include <cstdio>


void SendUnicast(Session* session, MsgBuf& sendMsgBuf)
{
	// 링버퍼 push
	returnValuePush = session->sendRingBuf.push(sendMsgBuf.GetBuffer(), sendMsgBuf.size());
	if (returnValuePush != sendMsgBuf.size())
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
		fwprintf(fp, L"[!!!Error!!!] SendUnicast() -> session->sendRingBuf.push() error. capacity: %zu, size: %zu, pushSize: %zu, return: %d\n",
			session->sendRingBuf.capacity(), session->sendRingBuf.size(), sendMsgBuf.size(), returnValuePush);

		// 보내려고 했던 메시지 내용 출력
		char* msgBuffer = sendMsgBuf.GetBuffer();
		fwprintf(fp, L"msgBuffer -> [ ");
		for (int i = 0; i < sendMsgBuf.size(); i++)
		{
			fwprintf(fp, L"%02X ", (unsigned char)msgBuffer[i]);
		}
		fwprintf(fp, L"]\n");

		fclose(fp);
		#pragma warning(default:6387 6328)

		// 지연삭제 마킹 및 연결끊기. SO_LINGER {0, 0} RST 전송
		DisconnectSession(session);

		printf("[!!!Error!!!] SendUnicast() -> session->sendRingBuf.push() error. capacity: %zu, size: %zu, pushSize: %zu, return: %d\n",
			session->sendRingBuf.capacity(), session->sendRingBuf.size(), sendMsgBuf.size(), returnValuePush);
	}

	// 디버깅용 출력 (직렬화버퍼로 변경 후 구조체 안 쓰는 게 맞지만 귀찮아서 그냥 씀..)
	#ifdef DEBUG_PRINT
	MessageHeader* msgHeader = (MessageHeader*)sendMsgBuf.GetBuffer();
	switch (msgHeader->type)
	{
		case PACKET_RES_MOVE_START:
		{
			MoveStartOther* msgPtr = (MoveStartOther*)(sendMsgBuf.GetBuffer() + sizeof(MessageHeader));
			DebugPrint("[Server MoveStartOther] SessionID: %d, Header(packetCode: 0x%02x, len: %d, type: PACKET_RES_MOVE_START), Payload(id: %d, direction: %d, X: %d, Y: %d)\n",
				session->id, (unsigned char)msgHeader->packetCode, msgHeader->len, msgPtr->id, msgPtr->direction, msgPtr->x, msgPtr->y);
			break;
		}
		case PACKET_RES_MOVE_STOP:
		{
			MoveStopOther* msgPtr = (MoveStopOther*)(sendMsgBuf.GetBuffer() + sizeof(MessageHeader));
			DebugPrint("[Server MoveStopOther] SessionID: %d, Header(packetCode: 0x%02x, len: %d, type: PACKET_RES_MOVE_STOP), Payload(id: %d, direction: %d, X: %d, Y: %d)\n",
				session->id, (unsigned char)msgHeader->packetCode, msgHeader->len, msgPtr->id, msgPtr->direction, msgPtr->x, msgPtr->y);
			break;
		}
		case PACKET_RES_ATTACK1:
		{
			Attack1Response* msgPtr = (Attack1Response*)(sendMsgBuf.GetBuffer() + sizeof(MessageHeader));
			DebugPrint("[Server Attack1Response] SessionID: %d, Header(packetCode: 0x%02x, len: %d, type: PACKET_RES_ATTACK1), Payload(id: %d, direction: %d, X: %d, Y: %d)\n",
				session->id, (unsigned char)msgHeader->packetCode, msgHeader->len, msgPtr->id, msgPtr->direction, msgPtr->x, msgPtr->y);
			break;
		}
		case PACKET_RES_ATTACK2:
		{
			Attack2Response* msgPtr = (Attack2Response*)(sendMsgBuf.GetBuffer() + sizeof(MessageHeader));
			DebugPrint("[Server Attack2Response] SessionID: %d, Header(packetCode: 0x%02x, len: %d, type: PACKET_RES_ATTACK2), Payload(id: %d, direction: %d, X: %d, Y: %d)\n",
				session->id, (unsigned char)msgHeader->packetCode, msgHeader->len, msgPtr->id, msgPtr->direction, msgPtr->x, msgPtr->y);
			break;
		}
		case PACKET_RES_ATTACK3:
		{
			Attack3Response* msgPtr = (Attack3Response*)(sendMsgBuf.GetBuffer() + sizeof(MessageHeader));
			DebugPrint("[Server Attack3Response] SessionID: %d, Header(packetCode: 0x%02x, len: %d, type: PACKET_RES_ATTACK3), Payload(id: %d, direction: %d, X: %d, Y: %d)\n",
				session->id, (unsigned char)msgHeader->packetCode, msgHeader->len, msgPtr->id, msgPtr->direction, msgPtr->x, msgPtr->y);
			break;
		}
		case PACKET_RES_DAMAGE:
		{
			Damage* msgPtr = (Damage*)(sendMsgBuf.GetBuffer() + sizeof(MessageHeader));
			DebugPrint("[Server Damage] SessionID: %d, Header(packetCode: 0x%02x, len: %d, type: PACKET_RES_DAMAGE), Payload(attackID: %d, targetID: %d, targetHP: %d)\n",
				session->id, (unsigned char)msgHeader->packetCode, msgHeader->len, msgPtr->attackID, msgPtr->damageID, msgPtr->damageHP);
			break;
		}
	}
	#endif
}

void SendBroadcast(Session* exceptSession, MsgBuf& sendMsgBuf)
{
	if (exceptSession == nullptr)
	{
		for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end(); ++it)
		{
			SendUnicast((*it), sendMsgBuf);
		}
	}
	else
	{
		for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end(); ++it)
		{
			if ((*it)->sock != exceptSession->sock)
			{
				SendUnicast((*it), sendMsgBuf);
			}
		}
	}
}

void FlushSendBuffer(Session* session)
{
	if (session->del == true)
		return;

	WSABUF wsaBuf[2];
	wsaBuf[0].buf = session->sendRingBuf.GetFront();
	wsaBuf[0].len = session->sendRingBuf.DirectPopSize();
	wsaBuf[1].buf = session->sendRingBuf.GetBuffer();
	wsaBuf[1].len = static_cast<ULONG>(session->sendRingBuf.size() - session->sendRingBuf.DirectPopSize());

	DWORD bytesSent;
	returnValueWSASend = WSASend(session->sock, wsaBuf, 2, &bytesSent, NULL, NULL, NULL);
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
			// TCP 송신버퍼가 꽉 찼거나, 운영체제단에서 지금은 할 수 없음.
			return;

		printf("WSASend() error. %d\n", errorCodeWSASend);
		Sleep(INFINITE);
	}

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
	NetInfoIncreaseSendBytes(bytesSent);
	session->sendRingBuf.MoveFront(bytesSent);
}