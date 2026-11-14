#include "FrameControl.h"
#include "Network.h"
#include "GameLogic.h"

int main()
{
	// RST 링거, 논블로킹 소켓으로 서버 열기
	linger _linger = { 1, 0 };
	ULONG nonBlocking = 1;
	StartServer(_linger, nonBlocking);

	while (1)
	{
		Network();

		GameLogic();

		// 50FPS
		FPS();
	}

	// 무한루프라서 여기가 실행되지는 않지만 일단 넣어놓음..
	StopServer();

	return 0;
}