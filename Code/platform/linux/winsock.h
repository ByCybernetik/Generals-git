#ifndef RENEGADE_WINSOCK_H
#define RENEGADE_WINSOCK_H

#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#ifndef INADDR_ANY
#define INADDR_ANY ((uint32_t)0x00000000)
#endif
#ifndef INADDR_BROADCAST
#define INADDR_BROADCAST ((uint32_t)0xffffffff)
#endif
#ifndef INADDR_NONE
#define INADDR_NONE ((uint32_t)0xffffffff)
#endif
#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK ((uint32_t)0x7f000001)
#endif

typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr_in *PSOCKADDR_IN;
typedef struct sockaddr_in *LPSOCKADDR_IN;
typedef struct sockaddr *LPSOCKADDR;

#define ZeroMemory(Destination, Length) memset((void *)(Destination), 0, (Length))
typedef unsigned short u_short;
typedef unsigned long u_long;
typedef char *LPSTR;

#define INVALID_SOCKET ((SOCKET)(~0))
#define SOCKET_ERROR (-1)
#define closesocket close
#define SD_BOTH SHUT_RDWR

#define WSAEINTR EINTR
#define WSAEBADF EBADF
#define WSAEACCES EACCES
#define WSAEFAULT EFAULT
#define WSAEINVAL EINVAL
#define WSAEMFILE EMFILE
#define WSAEWOULDBLOCK EWOULDBLOCK
#define WSAEINPROGRESS EINPROGRESS
#define WSAEALREADY EALREADY
#define WSAENOTSOCK ENOTSOCK
#define WSAEDESTADDRREQ EDESTADDRREQ
#define WSAEMSGSIZE EMSGSIZE
#define WSAEPROTOTYPE EPROTOTYPE
#define WSAENOPROTOOPT ENOPROTOOPT
#define WSAEPROTONOSUPPORT EPROTONOSUPPORT
#define WSAESOCKTNOSUPPORT ESOCKTNOSUPPORT
#define WSAEOPNOTSUPP EOPNOTSUPP
#define WSAEPFNOSUPPORT EPFNOSUPPORT
#define WSAEAFNOSUPPORT EAFNOSUPPORT
#define WSAEADDRINUSE EADDRINUSE
#define WSAEADDRNOTAVAIL EADDRNOTAVAIL
#define WSAENETDOWN ENETDOWN
#define WSAENETUNREACH ENETUNREACH
#define WSAENETRESET ENETRESET
#define WSAECONNABORTED ECONNABORTED
#define WSAECONNRESET ECONNRESET
#define WSAENOBUFS ENOBUFS
#define WSAEISCONN EISCONN
#define WSAENOTCONN ENOTCONN
#define WSAESHUTDOWN ESHUTDOWN
#define WSAETOOMANYREFS ETOOMANYREFS
#define WSAETIMEDOUT ETIMEDOUT
#define WSAECONNREFUSED ECONNREFUSED
#define WSAELOOP ELOOP
#define WSAENAMETOOLONG ENAMETOOLONG
#define WSAEHOSTDOWN EHOSTDOWN
#define WSAEHOSTUNREACH EHOSTUNREACH
#define WSAENOTEMPTY ENOTEMPTY
#define WSAEPROCLIM 10067
#define WSAEUSERS EUSERS
#define WSAEDQUOT EDQUOT
#define WSAESTALE ESTALE
#define WSAEREMOTE EREMOTE
#define WSASYSNOTREADY 10091
#define WSAVERNOTSUPPORTED 10092
#define WSANOTINITIALISED 10093
#define WSAEDISCON 10101

typedef struct linger LINGER;

typedef struct in_addr IN_ADDR;
typedef struct hostent HOSTENT, *LPHOSTENT;

#ifndef FIONREAD
#define FIONREAD 0x541B
#endif
#ifndef FIONBIO
#define FIONBIO 0x5421
#endif

typedef struct WSAData {
	unsigned short wVersion;
	unsigned short wHighVersion;
	char szDescription[257];
	char szSystemStatus[129];
} WSADATA, *LPWSADATA;

#ifdef __cplusplus
extern "C" {
#endif

int WSAStartup(unsigned short version, LPWSADATA data);
int WSACleanup(void);
int WSAGetLastError(void);
void WSASetLastError(int err);
int ioctlsocket(SOCKET s, long cmd, u_long *argp);
int shutdown(SOCKET s, int how);

#ifdef __cplusplus
}
#endif

#define MAKEWORD(low, high) ((unsigned short)(((unsigned char)(low)) | (((unsigned short)((unsigned char)(high))) << 8)))

#ifndef WSABASEERR
#define WSABASEERR 10000
#endif
#ifndef WSAHOST_NOT_FOUND
#define WSAHOST_NOT_FOUND (WSABASEERR + HOST_NOT_FOUND)
#endif
#ifndef WSATRY_AGAIN
#define WSATRY_AGAIN (WSABASEERR + TRY_AGAIN)
#endif
#ifndef WSANO_RECOVERY
#define WSANO_RECOVERY (WSABASEERR + NO_RECOVERY)
#endif
#ifndef WSANO_DATA
#define WSANO_DATA (WSABASEERR + NO_DATA)
#endif

#endif
