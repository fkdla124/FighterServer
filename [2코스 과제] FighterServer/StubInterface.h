#pragma once

#include "Session.h"
#include "Protocol.h"
#include "MsgBuf.h"

class StubInterface
{
public:
	virtual ~StubInterface() = default;
	virtual bool MessageDispatcher(Session* session, MessageHeader& header, MsgBuf& msg) = 0;
};

// RPC 활용을 위한 인터페이스
inline StubInterface* iStub = nullptr;
inline void SetStubInterface(StubInterface* stub)
{
	iStub = stub;
}