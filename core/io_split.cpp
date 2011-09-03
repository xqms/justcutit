// Simple file output module for FFMPEG with file splitting
// Author: Max Schwarz <Max@x-quadraht.de>

#include "io_split.h"

extern "C"
{
#include <libavformat/avio.h>
}

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

const int BUFSIZE = 1024;

struct IOSplitContext
{
	FILE* f;
	unsigned int index;
	char* path;
	uint64_t split_size;
	uint64_t written_size;
};

static char* malloc_and_snprintf(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	char* buf = 0;
	size_t buf_len = strlen(fmt); // First guess
	int ret = 0;
	
	while(1)
	{
		buf = (char*)realloc(buf, buf_len + 1);
		
		if(!buf)
		{
			va_end(args);
			return NULL;
		}
		
		ret = vsnprintf(buf, buf_len, fmt, args);
		
		if(ret < 0)
		{
			free(buf);
			va_end(args);
			return NULL;
		}
		
		if(ret < buf_len)
			break;
		
		buf_len = ret;
	}
	
	va_end(args);
	return buf;
}

int io_split_write_packet(void* opaque, uint8_t* buf, int buf_size)
{
	IOSplitContext* d = (IOSplitContext*)opaque;
	int c = 0;
	
	if(!d->f || d->written_size + buf_size > d->split_size)
	{
		if(d->f)
			fclose(d->f);
		
		char* filename = malloc_and_snprintf(d->path, d->index);
		if(!filename)
		{
			perror("[io_split] Could not use filename template");
			return -1;
		}
		
		fprintf(stderr, "[io_split] Opening output file '%s'\n", filename);
		d->f = fopen64(filename, "wb");
		free(filename);
		
		if(!d->f)
		{
			perror("[io_split] Could not open output file");
			return -1;
		}
		
		d->written_size = 0;
		d->index++;
	}
	
	while(buf_size > 0)
	{
		int ret = fwrite(buf, 1, buf_size, d->f);
		
		if(ret <= 0)
		{
			perror("[io_split] Could not fwrite()");
			return ret;
		}
		
		buf_size -= ret;
		c += ret;
	}
	
	d->written_size += c;
	
	return c;
}

AVIOContext* io_split_create(const char* path, uint64_t split_size)
{
	IOSplitContext* d = new IOSplitContext;
	unsigned char* buffer = (unsigned char*)malloc(BUFSIZE);
	AVIOContext* ctx = 0;
	
	if(!buffer || !d)
		goto error;
	
	d->f = NULL;
	d->index = 0;
	d->path = (char*)malloc(strlen(path) + 1);
	
	if(!d->path)
		goto error_path;
	
	strcpy(d->path, path);
	
	d->split_size = split_size;
	d->written_size = 0;
	
	ctx = avio_alloc_context(
		buffer, BUFSIZE,
		1, /* write_flag */
		(void*)d,
		NULL, /* read_packet */
		&io_split_write_packet,
		NULL /* seek */
	);
	
	if(ctx)
		return ctx;
	
error_path:
	free(d->path);
error:
	free(buffer);
	delete d;
	return NULL;
}

void io_split_close(AVIOContext* ctx)
{
	IOSplitContext* d = (IOSplitContext*)(ctx->opaque);
	
	if(d->f)
	{
		fprintf(stderr, "[io_split] Closing output file, written size: %lldMiB\n",
			d->written_size / 1024 / 1024
		);
		fclose(d->f);
	}
	
	delete d;
}
