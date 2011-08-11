// OpenGL video display widget
// Author: Max Schwarz <Max@x-quadraht.de>

#ifndef GLDISPLAY_H
#define GLDISPLAY_H

#include <QtOpenGL/QGLWidget>
#include <QtOpenGL/QGLShaderProgram>

extern "C"
{
#include <libavcodec/avcodec.h>
}

class GLDisplay : public QGLWidget
{
	Q_OBJECT
	
	public:
		GLDisplay(QWidget* parent);
		virtual ~GLDisplay();
		
		virtual void initializeGL();
		virtual void resizeGL(int w, int h);
		virtual void paintGL();
		
		virtual QSize sizeHint() const;
		
		void paintFrame(AVFrame* frame, int w, int h);
	private:
		AVFrame* m_frame;
		int m_w;
		int m_h;
		
		QGLShaderProgram m_yuvShader;
		
		GLuint m_shader_sampler[3];
		
		GLuint m_gl_textures[3];
};

#endif // GLDISPLAY_H
