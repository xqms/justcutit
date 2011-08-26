// Fast HTTP module for FFmpeg
// Author: Max Schwarz <Max@x-quadraht.de>

extern "C"
{
#include <libavformat/avio.h>
}

AVIOContext* io_http_create(const char* str_url);
