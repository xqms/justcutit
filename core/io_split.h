// Simple file output module for FFMPEG with file splitting
// Author: Max Schwarz <Max@x-quadraht.de>

#ifndef IO_SPLIT_H
#define IO_SPLIT_H

#include <stdint.h>

extern "C"
{
#include <libavformat/avio.h>
}

/**
 * @brief Create io_split IO context
 * 
 * @param path Path template, needs to contain a %d for the file number
 *   (evaluated with snprintf)
 * @param split_size Maximum file size
 * */
AVIOContext* io_split_create(const char* path, uint64_t split_size);

void io_split_close(AVIOContext* ctx);

#endif // IO_SPLIT_H
