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
		
		/**
		 * @brief Initialisation
		 * 
		 * Called just before cutting begins.
		 * 
		 * @return non-zero on error
		 * */
		virtual int init() = 0;
		
		/**
		 * Handle one data packet. Packet may be modified,
		 * but is freed by caller.
		 * 
		 * @return non-zero on error
		 * */
		virtual int handlePacket(AVPacket* packet) = 0;
		
		/**
		 * Is the stream handler still active?
		 * This should be false when the last cut
		 * point is a cut out and it has been reached.
		 * */
		inline bool active()
		{ return m_active; }
		
		// Set needed objects
		void setCutList(const CutPointList& list);
		void setOutputContext(AVFormatContext* ctx);
		void setOutputStream(AVStream* outputStream);
		void setStartPTS_AV(int64_t start_av);
		
		inline AVStream* stream() const
		{ return m_stream; }
		inline const CutPointList& cutList() const
		{ return m_cutlist; }
		inline AVFormatContext* outputContext() const
		{ return m_octx; }
		inline AVStream* outputStream() const
		{ return m_ostream; }
	protected:
		/**
		 * Write packet with correct parameters and
		 * offset (see setTotalCutout())
		 * */
		virtual int writeInputPacket(AVPacket* packet);
		
		/**
		 * Set total cutout time till now to enable
		 * correct PTS calculation.
		 * 
		 * @note The initial cutout (0 - first_cut_in) is not included here!
		 * @param duration Total cutout time in stream time base
		 * */
		virtual void setTotalCutout(int64_t duration);
		
		inline int64_t totalCutout() const
		{ return m_totalCutout; }
		
		void setActive(bool active);
		
		/**
		 * Calculate PTS relative to stream start time.
		 * Cutpoints are defined relative to start time, so use
		 * this function whenever you are using a raw PTS.
		 * */
		int64_t pts_rel(int64_t pts) const;
	private:
		AVStream* m_stream;
		AVStream* m_ostream;
		AVFormatContext* m_octx;
		CutPointList m_cutlist;
		int64_t m_totalCutout;
		int64_t m_startTime;
		bool m_active;
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
