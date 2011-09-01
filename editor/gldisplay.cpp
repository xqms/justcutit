// OpenGL video display widget
// Author: Max Schwarz <Max@x-quadraht.de>

#include <GL/glew.h>

#include "gldisplay.h"
#include <GL/glext.h>

GLDisplay::GLDisplay(QWidget* parent)
 : QGLWidget(parent)
 , m_frame(0)
 , m_w(0)
 , m_h(0)
{
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

GLDisplay::~GLDisplay()
{
}

void GLDisplay::initializeGL()
{
	glewInit();
	
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glEnable(GL_TEXTURE_2D);
	
	m_yuvShader.addShaderFromSourceFile(QGLShader::Vertex, "yuvtorgb_vertex.glsl");
	m_yuvShader.addShaderFromSourceFile(QGLShader::Fragment, "yuvtorgb_fragment.glsl");
	m_yuvShader.bind();
	m_yuvShader.link();
	
	printf("GLSL log: %s\n", m_yuvShader.log().toAscii().constData());
	
	m_shader_sampler[0] = m_yuvShader.uniformLocation("y_sampler");
	m_shader_sampler[1] = m_yuvShader.uniformLocation("u_sampler");
	m_shader_sampler[2] = m_yuvShader.uniformLocation("v_sampler");
	
	glGenTextures(3, m_gl_textures);
	
	for(int i = 0; i < 3; ++i)
	{
		glBindTexture(GL_TEXTURE_2D, m_gl_textures[i]);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		
		m_yuvShader.setUniformValue(m_shader_sampler[i], i);
	}
}

void GLDisplay::resizeGL(int w, int h)
{
	float scale_w = (float)w / (m_aspectRatio*m_w);
	float scale_h = (float)h / m_h;
	float scale = qMin(scale_w, scale_h);
	
	glViewport(0, 0, (GLint)(scale * m_aspectRatio * m_w), (GLint)(scale * m_h));
	
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, 1.0, 0.0, 1.0, 1.0, -1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void GLDisplay::setSize(int w, int h, float aspectRatio)
{
	if(m_w != w || m_h != h)
	{
		m_w = w;
		m_h = h;
		m_aspectRatio = aspectRatio;
		updateGeometry();
	}
}

void GLDisplay::paintFrame(AVFrame* frame)
{
	m_frame = frame;
	
	makeCurrent();
	
	// YUV420 to RGB conversion is done on the GPU using
	// a fragment shader. We just have to extract the
	// Y, U, V images from the YUV420 input.
	
	for(int i = 0; i < 3; ++i)
	{
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, m_gl_textures[i]);
		
		glTexImage2D(
			GL_TEXTURE_2D, // target
			0, // level
			1, // type
			frame->linesize[i], // width
			(i == 0) ? m_h : m_h/2,
			0, // border
			GL_LUMINANCE,
			GL_UNSIGNED_BYTE,
			m_frame->data[i]
		);
	}
	
	doneCurrent();
	
	repaint();
}

QSize GLDisplay::sizeHint() const
{
	return QSize(m_aspectRatio * m_w / 2, m_h/2);
}

void GLDisplay::paintGL()
{
	if(!m_frame)
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		return;
	}
	
	GLfloat points[] = {
		0.0, 0.0,
		1.0, 0.0,
		1.0, 1.0,
		0.0, 1.0,
	};
	
	GLfloat texcoords[] = {
		0.0, 1.0,
		
		// frame may be bigger than image width
		(float)m_w / m_frame->linesize[0], 1.0,
		(float)m_w / m_frame->linesize[0], 0.0,
		
		0.0, 0.0
	};
	
	// Set plane coordinates
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, points);
	
	// Set texture coordinates
	for(int i = 0; i < 3; ++i)
	{
		glClientActiveTexture(GL_TEXTURE0 + i);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
	}
	
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	glDrawArrays(GL_POLYGON, 0, 4);
	
	glFlush();
}

#include "gldisplay.moc"
