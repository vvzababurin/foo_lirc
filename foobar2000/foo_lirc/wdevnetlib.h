#pragma once

#ifndef __WDEVNETLIB
#define __WDEVNETLIB

#if _WIN32 || _WIN64
#if _WIN64
#define _WWDEVENV64 1
#else
#define _WWDEVENV32 1
#endif
#endif

// Check GCC
#if __GNUC__
#if __x86_64__ || __ppc64__
#define _GWDEVENV64 1
#else
#define _GWDEVENV32 1
#endif
#endif

#if defined _WWDEVENV64 || _WWDEVENV32
#define _WWDEVENV 1
#elif defined _GWDEVENV64 || _GWDEVENV32
#define _GWDEVENV 1
#endif

#ifdef _WWDEVENV

//#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <winsock.h>

#pragma comment(lib, "ws2_32.lib")

#elif _GWDEVENV

#include <sys/types.h>
#include <sys/socket.h>

#endif

#ifdef _WWDEVENV

#define closesock(s)			{ closesocket(s); s = -1; }
#define getsocketerrno			WSAGetLastError()
#define setsocketerrno(e)		WSASetLastError(e)
#define isvalidsock(s)			( (s) > 0 ) 

#elif _GWDEVENV

#define WSAEWOULDBLOCK			EWOULDBLOCK

#define closesock(s)			{ close(s); s = -1; }
#define getsocketerrno			errno
#define setsocketerrno(e)		seterrno(e);
#define isvalidsock(s)			( (s) > 0 ) 

#endif

#define BUF_SIZ 4096

#define LOG_DEBUG				0x0
#define LOG_WARNING				0x1
#define LOG_ERROR				0x2

#include <stdio.h>
#include <time.h>

namespace wdevnamespace
{
	int init();
	void term();

	void log(int level, char* _fmt, ...);

	int setnonblockopt(int s, unsigned long _block);

	int readn(int s, char* p, int l);
	int sendn(int s, char* p, int l);

	int create_tcp_client(const char* addr, int port);
	int create_tcp_server(const char* addr, int port, int listeners);
}

#endif