// H.264 stream handler
// Author: Max Schwarz <Max@x-quadraht.de>

#ifndef H264_H
#define H264_H

#include "../streamhandler.h"

#include <stdint.h>

extern "C"
{
#include <libavcodec/avcodec.h>
}

struct DataBuffer
{
	uint8_t* data;
	int size;
};

struct H264Context;

class H264 : public StreamHandler
{
	public:
		H264(AVStream* stream);
		virtual ~H264();
		
		virtual int init();
		virtual int handlePacket(AVPacket* packet);
	private:
		typedef std::vector<AVPacket> PacketBuffer;
		
		H264Context* m_h;
		int64_t m_startDecodeOffset;
		AVFrame m_frame;
		AVCodec* m_codec;
		uint8_t* m_encodeBuffer;
		
		// State
		bool m_decoding;
		bool m_encoding;
		bool m_isCutout;
		bool m_syncing;
		int m_encFrameCount;
		
		const CutPoint* m_nc;
		
		PacketBuffer m_syncBuffer;
		
		int64_t m_syncPoint;
		
		// Input bitstream fragments to copy
		DataBuffer m_sps;
		DataBuffer m_pps;
		
		void setFrameFields(AVFrame* frame, int64_t pts);
		int writeOutputPacket(uint8_t* buf, int size, int64_t pts);
		
		void parseNAL(uint8_t* buf, int size);
};

#endif // H264_H
