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
		 * 
		 * @param filename Index file filename (may be NULL)
		 * @param stream_filename Filename of stream (may be NULL)
		 * */
		virtual bool open(const char* filename,
			const char* stream_filename) = 0;
		
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
			AVFormatContext* input, const char* stream_filename
		);
		
		IndexFile* openWith(
			const char* format_name, const char* filename,
			AVFormatContext* input, const char* stream_filename
		);
		
		//! @name Registering
		//@{
		typedef bool (*Detector)(AVFormatContext*, const char*);
		typedef IndexFile* (*Creator)(AVFormatContext*);
		
		/**
		 * @param name Name of index format (should be short), statically
		 *	allocated (we make no copy)
		 * */
		static void registerIndexFile(const char* name,
			Detector detector, Creator creator);
		static int formatCount();
		static const char* formatName(int idx);
		
		class Registerer
		{
			public:
				Registerer(const char* name, Detector detector, Creator creator);
		};
		//@}
};

// Helper macro for registration
#define REGISTER_INDEX_FILE(name, cls) \
	IndexFile* create_ ## cls (AVFormatContext* ctx) \
	{ return new cls (ctx); } \
	IndexFileFactory::Registerer register_ ## cls ( \
		name, cls::detect, create_ ## cls);

#endif // INDEXFILE_H
