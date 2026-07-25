#include "winsock.h"
#include <string.h>

int WSAStartup(unsigned short version, LPWSADATA data)
{
	if (data != NULL) {
		memset(data, 0, sizeof(*data));
		data->wVersion = version;
		data->wHighVersion = version;
		strncpy(data->szDescription, "Linux Winsock stub", sizeof(data->szDescription) - 1);
		data->szDescription[sizeof(data->szDescription) - 1] = '\0';
	}
	return 0;
}

int WSACleanup(void)
{
	return 0;
}

int WSAGetLastError(void)
{
	return errno;
}

void WSASetLastError(int err)
{
	errno = err;
}

int ioctlsocket(SOCKET s, long cmd, u_long *argp)
{
	return ioctl(s, cmd, argp);
}
