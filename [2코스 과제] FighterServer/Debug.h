#pragma once

#include <cstdio>


// 디버깅 정보 프린트 on/off
// #define DEBUG_PRINT



#ifdef DEBUG_PRINT
	#define DebugPrint(...) printf(__VA_ARGS__)
#else 
	#define DebugPrint(...) ((void)0)
#endif