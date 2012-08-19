// Abstract stream handler base class
// Author: Max Schwarz <Max@x-quadraht.de>

#include "streamhandler.h"

extern "C"
{
#include <libavformat/avformat.h>
}

#define DEBUG 0
#define LOG_PREFIX "[STREAM]"
#include <common/log.h>

#include <map>

typedef std::map<int, StreamHandler::Creator> CreatorMap;
CreatorMap* g_creatorMap;

StreamHandler::StreamHandler(AVStream* stream)
 : m_stream(stream)
 , m_totalCutout(0)
 , m_lastDTS(-1)
 , m_nonMonotonic(false)
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
	const int64_t MASK = 0xFFFFFFFFFFFFFFFFLL >> (64 - m_stream->pts_wrap_bits);
	const AVRational MAX_SKIP_SECONDS = {600,1};
	const int64_t MAX_SKIP = av_rescale_q(1, MAX_SKIP_SECONDS, m_stream->time_base);

	packet->stream_index = m_ostream->index;
	packet->pts -= m_totalCutout;

	int64_t diff = (m_lastDTS - packet->dts) & MASK;

	if(m_lastDTS != -1 && !m_nonMonotonic && packet->dts < m_lastDTS && diff < MAX_SKIP)
	{
		log_warning("Non-monotonic input packet detected. Starting skip at DTS %10lld (new) < %10lld (old)", packet->dts, m_lastDTS);
		log_warning("diff is %10lld, which is greater than %ds (%10lld)", diff, MAX_SKIP_SECONDS.num, MAX_SKIP);
		m_nonMonotonic = true;

		return 0;
	}

	if(m_nonMonotonic)
	{
		if(packet->dts > m_lastDTS)
		{
			log_warning("Non-monotonic input ended with DTS %20lld", packet->dts);
			m_nonMonotonic = false;
		}
		else
			return 0;
	}

	if(packet->dts != AV_NOPTS_VALUE)
		m_lastDTS = packet->dts;

	packet->dts = AV_NOPTS_VALUE;

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
