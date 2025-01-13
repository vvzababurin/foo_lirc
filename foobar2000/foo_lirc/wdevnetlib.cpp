#include "stdafx.h"
#include "wdevnetlib.h"

namespace wdevnamespace
{
	int init()
	{
#ifdef _DEBUG
		log(LOG_DEBUG, "initialize network");
#endif
#ifdef _WWDEVENV
		WSAData wd;
		return WSAStartup( MAKEWORD( 2, 2 ), &wd );
#else
		return 0;
#endif
	}

	void term()
	{
#ifdef _WWDEVENV
		WSACleanup();
#endif
#ifdef _DEBUG
		log(LOG_DEBUG, "network terminate");
#endif
	}

	void log(int level, char* _fmt, ...)
	{
		__time64_t lt;
		struct tm u;

		_time64(&lt);
		_localtime64_s(&u, &lt);

		char buf[40] = { 0 };
		char ch[255] = { 0 };

		_itoa_s(level, ch, 255, 10);

		strftime(buf, 40, "%d.%m.%Y %H:%M:%S", &u);

		char* ls = ch;

		if (level == LOG_DEBUG) {
			ls = "debug";
		} else if ( level == LOG_WARNING){
			ls = "warning";
		} else if (level == LOG_ERROR) {
			ls = "error";
		}

		printf("Level:%s [ %s ] \"", ls, buf );

		va_list list;
		va_start(list, _fmt);
		
		vprintf(_fmt, list);

		va_end(list);

		printf("\"\n");
	}

	int setnonblockopt( int s, unsigned long _nonblock = 1 )
	{
#ifdef _WWDEVENV
		const unsigned long on = _nonblock ? 1 : 0;
		return ioctlsocket( s, FIONBIO, (unsigned long *)&on );
#elif
		int flags = 0;
		if ( ( flags = fcntl( s, F_GETFL, 0 ) ) < 0 )
			return -1;
		if ( _nonblock > 0 )
			if ( fcntl( s, F_SETFL, flags | O_NONBLOCK) < 0 )
				return -1;
		else
			if ( fcntl( s, F_SETFL, flags & O_NONBLOCK) < 0 )
				return -1;
		return 0;
#endif
	}
	
	int readn( int s, char*p, int l )
	{
		int c = 0;
		do
		{
			int r = recv( s, p, l - c, 0 );
			if ( r == 0 ) return 0; // gracefully closed
			if ( r < 0 ) {
				if (getsocketerrno == WSAEWOULDBLOCK)
					if (c > 0) return c;
				return -1;
			}
			c += r;
			p += r;
		} while ( c != l );

		return c; // all data has been read
	}

	int sendn( int s, char*p, int l )
	{
		int c = l;
		do
		{
			int r = send( s, p, c, 0 );
			if ( r < 0 ) {
				if (getsocketerrno == WSAEWOULDBLOCK) continue;
				return -1;
			}
			c =- r;
			p += r;
		} while ( c != 0 );

		return l;
	}

	int create_tcp_client ( const char* addr, int port )
	{
		struct sockaddr_in peer;
		int s;

		peer.sin_family = AF_INET;
		peer.sin_port = htons( port );
		peer.sin_addr.s_addr = inet_addr( addr );

		s = (int)socket ( AF_INET, SOCK_STREAM, 0 );
		if (!isvalidsock(s)) {
#ifdef _DEBUG
			log(LOG_ERROR,"%s %d", "socket() failed: ", getsocketerrno);
#endif
			goto out;
		}

		if ( connect ( s, ( struct sockaddr*)&peer, sizeof ( peer ) ) < 0 ) {
#ifdef _DEBUG
			log(LOG_ERROR,"%s %d", "connect() failed: ", getsocketerrno);
#endif
			goto out;
		}

		if (setnonblockopt(s, 1) < 0) {
#ifdef _DEBUG
			log(LOG_ERROR,"%s %d", "setnonblockopt() failed: ", getsocketerrno);
#endif
			goto out;
		}

		return s;

	out:
		closesock( s );
		return s;
	}

	int create_tcp_server( const char* addr, int port, int listeners )
	{
		struct sockaddr_in local;
		const int on = 1;

		int s = (int)socket ( AF_INET, SOCK_STREAM, 0 );
		if ( !isvalidsock ( s ) ) {
#ifdef _DEBUG
			log(LOG_ERROR,"%s %d", "socket() failed", getsocketerrno);
#endif
			goto out;
		}

		local.sin_family = AF_INET;
		local.sin_port = htons( port );

		if ( strcmp(addr, "INADDR_ANY") == 0 )
			local.sin_addr.s_addr = ntohl(INADDR_ANY);
		else
			local.sin_addr.s_addr = inet_addr(addr);

		if ( setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on)) )
		{
#ifdef _DEBUG
			log(LOG_ERROR, "%s %d", "setsockopt() failed", getsocketerrno);
#endif
			goto out;
		}

		if ( bind ( s, ( struct sockaddr* )&local, sizeof ( local ) ) ) {
#ifdef _DEBUG
			log(LOG_ERROR, "%s %d", "bind() failed", getsocketerrno);
#endif
			goto out;
		}

		if ( listen ( s, listeners ) ) {
#ifdef _DEBUG
			log(LOG_ERROR, "%s %d", "listen() failed", getsocketerrno);
#endif
			goto out;
		}

		if ( setnonblockopt( s, true ) < 0 ) {
#ifdef _DEBUG
			log(LOG_ERROR, "%s %d", "setnonblockopt() failed", getsocketerrno);
#endif
			goto out;
		}

		return s;

	out:
		closesock( s );
		return -1;
	}
}