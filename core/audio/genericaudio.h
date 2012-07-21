// Generic audio stream handler
// Author: Max Schwarz <Max@x-quadraht.de>

#ifndef GENERICAUDIO_H
#define GENERICAUDIO_H

#include "../streamhandler.h"

class GenericAudio : public StreamHandler
{
	public:
		GenericAudio(AVStream* stream);
		virtual ~GenericAudio();
		
		virtual int init();
		virtual int handlePacket(AVPacket* packet);
	private:
		const CutPoint* m_nc;
		bool m_cutout;
		
		int16_t *m_cutout_buf;
		int16_t *m_cutin_buf;
		int m_saved_samples;

		int m_outputErrorCount;
};

#endif // GENERICAUDIO_H
