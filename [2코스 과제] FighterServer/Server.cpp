#include "FrameControl.h"
#include "Network.h"
#include "GameLogic.h"

bool isServerRunning = true;

int main()
{
	// RST 링거, 논블로킹 소켓으로 서버 열기. 여러가지 초기화 포함
	linger _linger = { 1, 0 };
	ULONG nonBlocking = 1;
	StartServer(_linger, nonBlocking);

	// GameLogic만 FPS 제한!!!
	while (isServerRunning)
	{
		Network();

		GameLogic();
	}

	// isServerRunning = false 일 때 진입.
	// 게임로직이든 네트워크든 아무데서나 종료조건 만들면 됨.
	// 지금은 필요없지만 나중에는 DB나 파일에다가 여러 정보들을 기록하고 종료해야하기에 만들어보는 것.
	StopServer();

	return 0;
}