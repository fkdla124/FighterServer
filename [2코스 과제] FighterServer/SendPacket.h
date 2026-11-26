#pragma once

#include "List.h"
#include "Session.h"
#include "MsgBuf.h"

// 링버퍼 메시지 복사. 실제 send()는 FlushSendBuffer()
void SendPacket(List<Session*>& sessionList, MsgBuf& sendMsgBuf);
void FlushSendBuffer(Session* session);