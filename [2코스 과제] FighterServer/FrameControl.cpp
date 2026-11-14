#include <windows.h>
#include <cstdio>
#include <time.h>

#include "FrameControl.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 전역 데이터

// 50fps
#define FRAMETIME (CLOCKS_PER_SEC / 50)

// fps 제어용 틱
clock_t startFrameTick = clock();

// fps 출력용 틱
clock_t fpsCountTick = clock();
int fpsCount = 0;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FPS()
{
	// FPS Display
	fpsCount++;
	clock_t elapsedFpsCountTick = clock() - fpsCountTick;
	if (elapsedFpsCountTick > CLOCKS_PER_SEC)
	{
		fpsCountTick += CLOCKS_PER_SEC;

		WCHAR title[32];
		swprintf_s(title, L"[Fighter Server] %dFPS", fpsCount);
		fpsCount = 0;

		SetConsoleTitle(title);
	}

	// FPS Control
	clock_t elapsedFrameTime = clock() - startFrameTick;
	startFrameTick += FRAMETIME;

	if (elapsedFrameTime < FRAMETIME)
	{
		Sleep(FRAMETIME - elapsedFrameTime);
	}
}