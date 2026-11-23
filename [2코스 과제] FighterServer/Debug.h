#pragma once

#include <cstdio>

// 디버깅 정보 프린트 on/off
#define DEBUG_PRINT



#ifdef DEBUG_PRINT
	#define DebugPrint(...) printf(__VA_ARGS__)
#else 
	#define DebugPrint(...) 
#endif



#include "DisplayNetworkInfo.h"

// 네트워크 정보 프린트 on/off
#define NETINFO_PRINT

#ifdef NETINFO_PRINT
	#define NetInfoPrint() DisplayNetworkInfo()
	#define NetInfoIncreaseAcceptCount() netInfoAcceptCount++
	#define NetInfoIncreaseRecvBytes(bytesReceived) netInfoRecvBytes += bytesReceived
	#define NetInfoIncreaseSendBytes(bytesSent) netInfoSendBytes += bytesSent
	#define NetInfoIncreaseMessageCount() netInfoMessageCount++
#else
	#define NetInfoPrint()
	#define NetInfoIncreaseAcceptCount()
	#define NetInfoIncreaseRecvBytes(bytesReceived)
	#define NetInfoIncreaseSendBytes(bytesSent)
	#define NetInfoIncreaseMessageCount()
#endif