// Logging helpers
// Author: Max Schwarz <Max@x-quadraht.de>

#ifndef LOG_H
#define LOG_H

#include <stdarg.h>
#include <stdio.h>
#include <errno.h>

#ifndef LOG_PREFIX
#error LOG_PREFIX not set!
#endif

#if DEBUG
static void log_debug(const char* msg, ...)
	__attribute__((format (printf, 1, 2)));

static void log_debug(const char* msg, ...)
{
	va_list l;
	va_start(l, msg);
	
	fputs(LOG_PREFIX " ", stderr);
	vfprintf(stderr, msg, l);
	fputc('\n', stderr);
	
	va_end(l);
}

static int log_debug_perror(const char* msg, ...)
	__attribute__((format (printf, 1, 2)));

static int log_debug_perror(const char* msg, ...)
{
	va_list l;
	va_start(l, msg);
	
	int error = errno;
	
	fputs(LOG_PREFIX " ", stderr);
	vfprintf(stderr, msg, l);
	fputs(": ", stderr);
	fputs(strerror(error), stderr);
	fputc('\n', stderr);
	
	va_end(l);
	
	return -1;
}
#else
inline void log_debug(const char* msg, ...)
{
}
inline int log_debug_perror(const char* msg, ...)
{
	return -1;
}
#endif

static int error(const char* msg, ...)
	__attribute__((format (printf, 1, 2)));

static int error(const char* msg, ...)
{
	va_list l;
	va_start(l, msg);
	
	fputs(LOG_PREFIX " Error: ", stderr);
	vfprintf(stderr, msg, l);
	fputc('\n', stderr);
	
	va_end(l);
	
	return -1;
}

#endif // LOG_H
