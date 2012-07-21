// MPEG-2 video stream handler
// Author: Max Schwarz <Max@x-quadraht.de>

#ifndef MP2V_H
#define MP2V_H

#include "../streamhandler.h"

extern "C"
{
#include <libavcodec/avcodec.h>
}

#include <map>

class MP2V : public StreamHandler
{
	public:
		typedef std::vector<AVPacket> PacketBuffer;
		
		MP2V(AVStream* stream);
		virtual ~MP2V();
		
		virtual int handlePacket(AVPacket* packet);
		virtual int init();
	private:
		AVCodec* m_encoder;
		AVFrame* m_frame;
		
		int64_t m_startDecodeOffset;
		
		bool m_decoding;
		bool m_encoding;
		
		const CutPoint* m_nc;
		bool m_currentIsCutout;
		
		int m_waitKeyFrames;
		
		AVPacket m_outputPacket;
		
		// Timestamp handling
		int64_t m_gopMinPTS;
		int64_t m_lastDirectPTS;
		
		// Buffering
		PacketBuffer m_copyPacketBuffer;
		PacketBuffer m_encodedPacketBuffer;

		int m_outputErrorCount;
};

#endif // MP2V_H
