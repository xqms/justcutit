// AC3 audio stream handler
// Author: Max Schwarz <Max@x-quadraht.de>

#ifndef AC3_H
#define AC3_H

#include "../streamhandler.h"

class AC3 : public StreamHandler
{
	public:
		AC3(AVStream* stream);
		virtual ~AC3();
		
		virtual int init();
		virtual int handlePacket(AVPacket* packet);
	private:
		const CutPoint* m_nc;
		bool m_cutout;
		
		int16_t *m_sample_buf;
		int m_saved_samples;
};

#endif // AC3_H
