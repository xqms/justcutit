// TS Index file base class
// Author: Max Schwarz <Max@x-quadraht.de>

#ifndef INDEXFILE_H
#define INDEXFILE_H

struct AVFormatContext;

#include <sys/types.h>

#ifdef _WIN32
#include <stdint.h>

typedef uint64_t loff_t;
#endif


/**
 * Subclassing information:
 * 
 * Your subclass should have a static "detect()" function with the
 * following signature:
 * 
 * @code
 * bool detect(AVFormatContext* input, const char* stream_filename)
 * @endcode
 * 
 * It should try to detect a present index file for the stream represented
 * by @c input, which was opened from file @c stream_filename. If successful,
 * it should return true, otherwise false.
 * */
class IndexFile
{
	public:
		IndexFile(AVFormatContext* ctx);
		virtual ~IndexFile();
		
		/**
		 * @brief Open index file for stream
		 * */
		virtual bool open(const char* stream_filename) = 0;
		
		/**
		 * Determine byte position of frame with specified PTS timestamp.
		 * 
		 * The returned offset should point to a few frames before
		 * the actual frame so that the codec has enough data to
		 * actually decode the requested frame.
		 * 
		 * @param pts PTS in AV_TIME_BASE units (i.e. 1µs)
		 * @return byte position, (loff_t)-1 on error
		 * */
		virtual loff_t bytePositionForPTS(int64_t pts) = 0;
		
		inline AVFormatContext* context()
		{ return m_ctx; }
	private:
		AVFormatContext* m_ctx;
};

class IndexFileFactory
{
	public:
		IndexFile* detectIndexFile(
			AVFormatContext* input, const char* filename
		);
		
		//! @name Registering
		//@{
		typedef bool (*Detector)(AVFormatContext*, const char*);
		typedef IndexFile* (*Creator)(AVFormatContext*);
		
		static void registerIndexFile(Detector detector, Creator creator);
		
		class Registerer
		{
			public:
				Registerer(Detector detector, Creator creator);
		};
		//@}
};

// Helper macro for registration
#define REGISTER_INDEX_FILE(name) \
	IndexFile* create_ ## name (AVFormatContext* ctx) \
	{ return new name (ctx); } \
	IndexFileFactory::Registerer register_ ## name ( \
		name::detect, create_ ## name);

#endif // INDEXFILE_H
