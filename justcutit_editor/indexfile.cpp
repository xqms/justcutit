// TS Index file base class
// Author: Max Schwarz <Max@x-quadraht.de>

#include "indexfile.h"

#include <vector>

IndexFile::IndexFile(AVFormatContext* ctx)
 : m_ctx(ctx)
{
}

IndexFile::~IndexFile()
{
}

// FACTORY
struct RegEntry
{
	IndexFileFactory::Creator creator;
	IndexFileFactory::Detector detector;
};
typedef std::vector<RegEntry> RegEntryList;
RegEntryList* g_entryList;

void IndexFileFactory::registerIndexFile(
	IndexFileFactory::Detector detector,
	IndexFileFactory::Creator creator)
{
	if(!g_entryList)
		g_entryList = new RegEntryList;
	
	RegEntry entry = {creator, detector};
	g_entryList->push_back(entry);
}

IndexFileFactory::Registerer::Registerer(
	IndexFileFactory::Detector detector,
	IndexFileFactory::Creator creator)
{
	IndexFileFactory::registerIndexFile(detector, creator);
}

IndexFile* IndexFileFactory::detectIndexFile(
	AVFormatContext* input, const char* filename)
{
	if(!g_entryList)
		return NULL;
	
	for(int i = 0; i < g_entryList->size(); ++i)
	{
		const RegEntry& entry = g_entryList->at(i);
		
		if(!entry.detector(input, filename))
			continue;
		
		IndexFile* f = entry.creator(input);
		
		if(f->open(filename))
			return f;
		
		delete f;
	}
	
	return NULL;
}



