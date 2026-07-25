#ifndef RENEGADE_IO_H
#define RENEGADE_IO_H

#include <fcntl.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef _O_RDONLY
#define _O_RDONLY O_RDONLY
#endif
#ifndef _O_WRONLY
#define _O_WRONLY O_WRONLY
#endif
#ifndef _O_RDWR
#define _O_RDWR O_RDWR
#endif
#ifndef _O_CREAT
#define _O_CREAT O_CREAT
#endif
#ifndef _O_TRUNC
#define _O_TRUNC O_TRUNC
#endif
#ifndef _O_APPEND
#define _O_APPEND O_APPEND
#endif
#ifndef _O_BINARY
#define _O_BINARY O_BINARY
#endif
#ifndef _O_TEXT
#define _O_TEXT 0
#endif

#ifndef S_IREAD
#define S_IREAD S_IRUSR
#endif
#ifndef S_IWRITE
#define S_IWRITE S_IWUSR
#endif
#ifndef _S_IREAD
#define _S_IREAD S_IREAD
#endif
#ifndef _S_IWRITE
#define _S_IWRITE S_IWRITE
#endif

#ifndef _access
static inline int renegade_access(const char *path, int mode)
{
	(void)mode;
	return ::access(path, F_OK);
}
#define _access renegade_access
#endif

#ifndef _open
static inline int renegade_open(const char *filename, int oflag, ...)
{
	mode_t pmode = 0666;
	if (oflag & O_CREAT) {
		va_list ap;
		va_start(ap, oflag);
		pmode = (mode_t)va_arg(ap, int);
		va_end(ap);
	}
	return ::open(filename, oflag, pmode);
}
#define _open renegade_open
#endif

#ifndef _close
#define _close(fd) ::close(fd)
#endif

#ifndef _read
#define _read(fd, buf, count) ::read((fd), (buf), (count))
#endif

#ifndef _write
#define _write(fd, buf, count) ::write((fd), (buf), (count))
#endif

#ifndef _lseek
#define _lseek(fd, offset, origin) ::lseek((fd), (offset), (origin))
#endif

#ifndef _unlink
#define _unlink(path) ::unlink(path)
#endif

#ifndef _chmod
#define _chmod(path, mode) ::chmod((path), (mode))
#endif

#endif /* RENEGADE_IO_H */
