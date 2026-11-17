#include <windows.h>
#include <cstdio>
#include <time.h>

#include "FrameControl.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 전역 데이터

// 50fps
// #define FRAMETIME (CLOCKS_PER_SEC / 50) 	// ※ FPS 제한 삭제. 네트워크 처리를 최대한 빨리 돌리기위함.

// fps 제어용 틱
// clock_t startFrameTick = clock(); 	// ※ FPS 제한 삭제. 네트워크 처리를 최대한 빨리 돌리기위함.

// fps 출력용 틱
unsigned int fpsCountTick = timeGetTime();
int logicFpsCount = 0;
int networkFpsCount = 0;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DisplayFPS()
{
	// FPS Display
	unsigned int elapsedFpsCountTick = timeGetTime() - fpsCountTick;
	if (elapsedFpsCountTick > CLOCKS_PER_SEC)
	{
		fpsCountTick += CLOCKS_PER_SEC;

		WCHAR title[64];
		swprintf_s(title, L"[Fighter Server] LogicFPS: %d, NetworkFPS: %d", logicFpsCount, networkFpsCount);
		logicFpsCount = 0;
		networkFpsCount = 0;

		SetConsoleTitle(title);
	}



	
	// FPS Control
	//clock_t elapsedFrameTime = clock() - startFrameTick;
	//startFrameTick += FRAMETIME;

	//if (elapsedFrameTime < FRAMETIME)						// ※ FPS 제한 삭제. 네트워크 처리를 최대한 빨리 돌리기위함.
	//{
	//	Sleep(FRAMETIME - elapsedFrameTime);
	//}
}