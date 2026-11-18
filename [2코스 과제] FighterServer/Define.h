#pragma once

// 프레임당 이동 칸
#define MOVE_CHARACTER_PER_FRAME_X 3
#define MOVE_CHARACTER_PER_FRAME_Y 2

// 로직FPS 제한
#define GAME_LOGIC_FPS_LIMIT 50 // 1000을 딱떨어지게 나눌 수 있으면 좋음

// 이동속도 (초, 밀리초)단위
#define MOVE_SPEED_X_PER_SECONDS ((MOVE_CHARACTER_PER_FRAME_X) * (GAME_LOGIC_FPS_LIMIT))
#define MOVE_SPEED_X_PER_MILLISECONDS ((MOVE_SPEED_X_PER_SECONDS) / (1000.0f))
#define MOVE_SPEED_Y_PER_SECONDS ((MOVE_CHARACTER_PER_FRAME_Y) * (GAME_LOGIC_FPS_LIMIT))
#define MOVE_SPEED_Y_PER_MILLISECONDS ((MOVE_SPEED_Y_PER_SECONDS) / (1000.0f))

// 맵 최대 이동 제한
#define RANGE_MOVE_TOP	50
#define RANGE_MOVE_LEFT	10
#define RANGE_MOVE_RIGHT	630
#define RANGE_MOVE_BOTTOM	470

// 이동 오류체크 범위
#define RANGE_MOVE_ERROR 50

// 공격 범위 (공격자 좌표 기준 DIR방향으로 사각형)
#define ATTACK1_RANGE_X		80
#define ATTACK2_RANGE_X		90
#define ATTACK3_RANGE_X		100
#define ATTACK1_RANGE_Y		10
#define ATTACK2_RANGE_Y		10
#define ATTACK3_RANGE_Y		20

// 클라 공격 애니메이션 시간
#define ATTACK1_ANIMATION_DURATION 240
#define ATTACK2_ANIMATION_DURATION 320
#define ATTACK3_ANIMATION_DURATION 480

// 허용해줄 공격 쿨타임 오차.
// 공격 메시지 수신시 이전 수신과 현재 수신의 간격이 (ATTACK_ANIMATION_DURATION - ATTACK_TIME_TOLERANCE) 보다 빠르다면 무시.
// 따로 카운팅해서 로그남기고 연결끊기 뭐 이런 거는 안 했음.
#define ATTACK_TIME_TOLERANCE 60


// 공격 데미지
#define ATTACK1_DAMAGE 2
#define ATTACK2_DAMAGE 3
#define ATTACK3_DAMAGE 4