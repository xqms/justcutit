// Abstract stream handler base class
// Author: Max Schwarz <Max@x-quadraht.de>

#include "streamhandler.h"

extern "C"
{
#include <libavformat/avformat.h>
}

#include <map>

typedef std::map<int, StreamHandler::Creator> CreatorMap;
CreatorMap* g_creatorMap;

StreamHandler::StreamHandler(AVStream* stream)
 : m_stream(stream)
 , m_totalCutout(0)
 , m_active(true)
{
}

StreamHandler::~StreamHandler()
{
}

void StreamHandler::setCutList(const CutPointList& list)
{
	m_cutlist = list.rescale(AV_TIME_BASE_Q, m_stream->time_base);
}

void StreamHandler::setOutputContext(AVFormatContext* ctx)
{
	m_octx = ctx;
}

void StreamHandler::setOutputStream(AVStream* outputStream)
{
	m_ostream = outputStream;
}

void StreamHandler::setTotalCutout(int64_t duration)
{
	m_totalCutout = duration;
}

void StreamHandler::setActive(bool active)
{
	m_active = active;
}

int StreamHandler::writeInputPacket(AVPacket* packet)
{
	packet->stream_index = m_ostream->index;
	packet->dts = AV_NOPTS_VALUE;
	packet->pts -= m_totalCutout;
	
	return av_interleaved_write_frame(m_octx, packet);
}

int64_t StreamHandler::pts_rel(int64_t pts) const
{
	const int64_t mask = 0xFFFFFFFFFFFFFFFFLL >> (64 - m_stream->pts_wrap_bits);
	
	return (pts - m_startTime) & mask;
}

void StreamHandler::setStartPTS_AV(int64_t start_av)
{
	m_startTime = av_rescale_q(start_av, AV_TIME_BASE_Q, m_stream->time_base);
	printf("StreamHandler: got start time %'10lld, own stream start time would be %'10lld\n", m_startTime, m_stream->start_time);
}

// StreamHandlerFactory

StreamHandler* StreamHandlerFactory::createHandlerForStream(AVStream* stream)
{
	if(!g_creatorMap)
		return 0;
	
	int codecID = stream->codec->codec_id;
	CreatorMap::iterator it = g_creatorMap->find(codecID);
	
	if(it == g_creatorMap->end())
		return 0;
	
	return it->second(stream);
}

StreamHandlerFactory::Registerer::Registerer(int codecID, StreamHandler::Creator creator)
{
	StreamHandlerFactory::registerStreamHandler(codecID, creator);
}

void StreamHandlerFactory::registerStreamHandler(int codecID, StreamHandler::Creator creator)
{
	if(!g_creatorMap)
		g_creatorMap = new CreatorMap;
	
	(*g_creatorMap)[codecID] = creator;
}
