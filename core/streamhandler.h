// Abstract stream handler base class
// Author: Max Schwarz <Max@x-quadraht.de>

#ifndef STREAMHANDLER_H
#define STREAMHANDLER_H

#include "cutlist.h"

class AVStream;
class AVPacket;
class AVFormatContext;

class StreamHandler
{
	public:
		typedef StreamHandler*(*Creator)(AVStream* stream);
		
		StreamHandler(AVStream* stream);
		virtual ~StreamHandler();
		
		virtual int init() = 0;
		
		virtual int handlePacket(AVPacket* packet) = 0;
		
		// Set needed objects
		void setCutList(const CutPointList& list);
		void setOutputContext(AVFormatContext* ctx);
		void setOutputStream(AVStream* outputStream);
		
		inline AVStream* stream() const
		{ return m_stream; }
		inline const CutPointList& cutList() const
		{ return m_cutlist; }
		inline AVFormatContext* outputContext() const
		{ return m_octx; }
		inline AVStream* outputStream() const
		{ return m_ostream; }
	protected:
		virtual int writeInputPacket(AVPacket* packet);
		virtual void setTotalCutout(int64_t duration);
		
		inline int64_t totalCutout() const
		{ return m_totalCutout; }
	private:
		AVStream* m_stream;
		AVStream* m_ostream;
		AVFormatContext* m_octx;
		CutPointList m_cutlist;
		int64_t m_totalCutout;
};

class StreamHandlerFactory
{
	public:
		class Registerer
		{
			public:
				Registerer(int codecID, StreamHandler::Creator creator);
		};
		
		StreamHandler* createHandlerForStream(AVStream* stream);
		
		static void registerStreamHandler(int codecID, StreamHandler::Creator creator);
};

#define REGISTER_STREAM_HANDLER(codecID, name) \
	StreamHandler* create ## name (AVStream* stream) \
	{ return new name (stream); } \
	StreamHandlerFactory::Registerer reg ## name(codecID, create ## name);

#endif // STREAMHANDLER_H
