#pragma once

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SocketErrorCode Data //

// 아직 에러코드나 함수 리턴값을 받지 않은 상태를 구분하기 위해 내 생일을 넣어놓음
#define NOT_YET 0x20020315

// WSAStartup() -> 성공(0), 실패(!=0), 실패시 WSAGetLastError() 사용
inline int returnValueWSAStartup = NOT_YET;
inline int errorCodeWSAStartUp = NOT_YET;

// socket() -> 성공(소켓fd), 실패(INVALID_SOCKET), 실패시 WSAGetLastError() 사용
inline int errorCodeSocket = NOT_YET;

// bind() -> 성공(0), 실패(SOCKET_ERROR), 실패시 WSAGetLastError() 사용
inline int returnValueBind = NOT_YET;
inline int errorCodeBind = NOT_YET;

// listen() -> 성공(0), 실패(SOCKET_ERROR), 실패시 WSAGetLastError() 사용
inline int returnValueListen = NOT_YET;
inline int errorCodeListen = NOT_YET;

// accept() -> 성공(소켓fd), 실패(INVALID_SOCKET), 실패시 WSAGetLastError() 사용
inline int errorCodeAccept = NOT_YET;

// InetPton() -> 성공(1), 잘못된IP문자열형식(0), 실패(-1), 실패시 WSAGetLastError() 사용
inline int returnValueInetPton = NOT_YET;
inline int errorCodeInetPton = NOT_YET;

// connect() -> 성공(0), 실패(SOCKET_ERROR), 실패시 WSAGetLastError() 사용
// 논블로킹 SOCKET_ERROR -> WSAEWOULDBLOCK 체크
inline int returnValueConnect = NOT_YET;
inline int errorCodeConnect = NOT_YET;

// ioctlsocket() -> 성공(0), 실패(SOCKET_ERROR), 실패시 WSAGetLastError() 사용
inline int returnValueIoctlSocket = NOT_YET;
inline int errorCodeIoctlSocket = NOT_YET;

// setsockopt() -> 성공(0), 실패(SOCKET_ERROR), 실패시 WSAGetLastError() 사용
inline int returnValueSetSockOpt = NOT_YET;
inline int errorCodeSetSockOpt = NOT_YET;

// select() -> 성공(소켓개수), 실패(SOCKET_ERROR), 실패시 WSAGetLastError() 사용
inline int returnValueSelect = NOT_YET;
inline int errorCodeSelect = NOT_YET;

// recv() -> 성공(읽은 바이트수), 실패(SOCKET_ERROR), 종료(0), 실패시 WSAGetLastError() 사용
// 논블로킹 SOCKET_ERROR -> WSAEWOULDBLOCK 체크
inline int returnValueRecv = NOT_YET;
inline int errorCodeRecv = NOT_YET;

// send() -> 성공(송신버퍼에 복제한 바이트수), 실패(SOCKET_ERROR), 실패시 WSAGetLastError() 사용
// 논블로킹 SOCKET_ERROR -> WSAEWOULDBLOCK 체크
inline int returnValueSend = NOT_YET;
inline int errorCodeSend = NOT_YET;

// WSAAsyncSelect() -> 성공(0), 실패(SOCKET_ERROR), 실패시 WSAGetLastError() 사용
inline int returnValueWSAAsyncSelect = NOT_YET;
inline int errorCodeWSAAsyncSelect = NOT_YET;

// WSASend() -> 성공(0), 실패(SOCKET_ERROR), 실패시 WSAGetLastError() 사용
inline int returnValueWSASend = NOT_YET;
inline int errorCodeWSASend = NOT_YET;

// WSARecv() -> 성공(0), 실패(SOCKET_ERROR), 실패시 WSAGetLastError() 사용
inline int returnValueWSARecv = NOT_YET;
inline int errorCodeWSARecv = NOT_YET;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// My RingBuffer ErrorCheck //

inline int returnValuePush = NOT_YET;
inline int returnValuePop = NOT_YET;
inline int returnValuePeek = NOT_YET;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////