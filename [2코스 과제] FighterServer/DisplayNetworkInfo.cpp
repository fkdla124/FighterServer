#include <cstdio>
#include "Network.h"

#include <windows.h>
#pragma comment(lib, "winmm.lib")

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// netInfo

// 초당 accept() 횟수
int netInfoAcceptCount;

// 현재 세션 수
// sessionList.size();

// 초당 송수신 바이트 recv, send
int netInfoRecvBytes;
int netInfoSendBytes;

// 초당 메시지 처리 횟수 -> 초당 ProcessMessage() 호출 횟수
int netInfoMessageCount;

////////////////////////////////////////////////////////////////////////////////////////////////////////////

// base tick -> DisPlayNetworkInfo()
unsigned int networkInfoTick = timeGetTime();

void DisplayNetworkInfo()
{
	// ms
	unsigned int elapsed = timeGetTime() - networkInfoTick;
	
	// 초단위 출력
	if (elapsed < 1000)
		return;

	// base 갱신
	networkInfoTick = timeGetTime();

	printf("[NETINFO] Recv: %5d B | Send: %5d B | Msg: %5d | Accept: %5d | Session: %5zu\n", netInfoRecvBytes, netInfoSendBytes, netInfoMessageCount, netInfoAcceptCount, sessionList.size());

	// 출력 후 초기화.
	netInfoAcceptCount = 0;
	netInfoRecvBytes = 0;
	netInfoSendBytes = 0;
	netInfoMessageCount = 0;
}