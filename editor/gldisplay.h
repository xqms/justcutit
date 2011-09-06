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

/**
 * @brief OpenGL video display widget
 * 
 * Displays an AVFrame using OpenGL methods.
 * YUV420 conversion is accelerated using OpenGL shaders.
 * */
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
		
		//! @name Public API
		//@{
		
		/**
		 * Set the picture size.
		 * 
		 * @warning This @b must be called before the first
		 * call of paintFrame()!
		 * */
		void setSize(int w, int h, float aspectRatio = 1.0f);
		
		/**
		 * Paint a frame. This does an @b immediate repaint() to
		 * provide fast response.
		 * */
		void paintFrame(AVFrame* frame);
		//@}
	private:
		AVFrame* m_frame;
		int m_w;
		int m_h;
		float m_aspectRatio;
		bool m_updateTextures;
		
		QGLShaderProgram m_yuvShader;
		
		GLuint m_shader_sampler[3];
		
		GLuint m_gl_textures[3];
};

#endif // GLDISPLAY_H
