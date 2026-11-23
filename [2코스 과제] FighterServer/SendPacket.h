#pragma once

#include "Session.h"
#include "MsgBuf.h"

// 링버퍼 메시지 복사. 실제 send()는 FlushSendBuffer()
void SendUnicast(Session* session, MsgBuf& sendMsgBuf);
void SendBroadcast(Session* exceptSession, MsgBuf& sendMsgBuf);
void FlushSendBuffer(Session* session);