// Kathrein index file, found on model UFS-910
// Author: Max Schwarz <Max@x-quadraht.de>

#ifndef KATHREIN_H
#define KATHREIN_H

#include "../indexfile.h"

#include <stdint.h>

#ifdef __GNUC__
#define PACKED __attribute__((packed))
#else
#define PACKED
#endif

class KathreinIndexFile : public IndexFile
{
	public:
		KathreinIndexFile(AVFormatContext* ctx);
		virtual ~KathreinIndexFile();
		
		virtual bool open(const char* stream_filename);
		virtual loff_t bytePositionForPTS(int64_t pts);
		
		static bool detect(AVFormatContext* ctx, const char* stream_filename);
	private:
		struct PACKED TableEntry
		{
			uint64_t offset;
			uint32_t time_ms;
		};
		TableEntry* m_table;
		uint32_t m_count;
		
		static char* fabricateFilename(const char* stream_filename);
};

#endif // KATHREIN_H
