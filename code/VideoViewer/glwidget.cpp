/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "glwidget.h"
#include "window.h"
#include "viewerconfig.h"
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QMouseEvent>
#include "qopenglpixeltransferoptions.h"
#include <fstream>

using namespace std;

static double calOffset(int size)
{
	if (size % 2 == 0)
		return (size - 1.0) * 0.5;
	return size * 0.5;
}

GLWidget::GLWidget(const ViewerConfig& config, const uchar* loadingImage, QWidget *parent, int windWidth, int windHeight, int width, int height)
	: QOpenGLWidget(parent),
	m_loadingImage(loadingImage),
	m_loadingWidth(width),
	m_loadingHeight(height),
	m_clearColor(Qt::black),
	m_program(0),
	m_subScale(1.f)
{
	m_lastPos = QPoint(-1, -1);

	m_width = windWidth;
	m_height = windHeight;

	m_scale = max(m_width / (double)width, m_height / (double)height);
	m_lastScale = m_scale;
	m_zoomScale = m_scale;
	m_scaleMin = config.getScaleMin();
	m_scaleMax = config.getScaleMax();

	resize(m_width, m_height);

	m_xOffset = calOffset(m_width);
	m_yOffset = calOffset(m_height);
	m_texOffX = 0.f;
	m_texOffY = 0.f;
	
	m_texCoordXY[0] = 0.f; 
	m_texCoordXY[1] = 0.f; 
	m_aspect = 1.f;
	m_aspectScale = 1.f;

	// custom cursors
	m_zoomInCursor = new QCursor(QPixmap("zoom_in_small.png"), -1, -1);
	m_zoomOutCursor = new QCursor(QPixmap("zoom_out_small.png"), -1, -1);
}

GLWidget::~GLWidget()
{
	makeCurrent();
	
	delete m_textureY;
	delete m_textureU;
	delete m_textureV;

	delete m_program;
	doneCurrent();
}

void GLWidget::setData(QOpenGLTexture& texture, const uchar* data, int width, int height)
{
	texture.setFormat(QOpenGLTexture::RGB8_UNorm);
	texture.setSize(width, height);
	texture.setMipLevels(texture.maximumMipLevels());
	texture.allocateStorage();

	// Upload pixel data and generate mipmaps
	QOpenGLPixelTransferOptions uploadOptions;
	uploadOptions.setAlignment(1);
	texture.setData(0, QOpenGLTexture::BGR, QOpenGLTexture::UInt8, (const void*)data, &uploadOptions);
}

void GLWidget::setData(QOpenGLTexture& texture, const vector<const uchar*>& dataPtrs, int rows, int cols, int width, int height)
{
	int totalWidth = width * cols;
	int totalHeight = height * rows;
	assert(totalWidth <= 16384 && totalHeight <= 16384);

	texture.setFormat(QOpenGLTexture::RGB8_UNorm);
	texture.setSize(totalWidth, totalHeight);
	texture.setMipLevels(texture.maximumMipLevels());
	texture.allocateStorage();

	texture.setWrapMode(QOpenGLTexture::ClampToBorder);

	glActiveTexture(GL_TEXTURE0);
	texture.bind();


	for (int r = 0; r < rows; ++r)
	{
		for (int c = 0; c < cols; ++c)
		{
			glTexSubImage2D(GL_TEXTURE_2D, 0, c * width, r * height, width, height, GL_BGR, GL_UNSIGNED_BYTE, dataPtrs[r * cols + c]);
		}
	}

	texture.generateMipMaps();
}


void GLWidget::setDataYUV(QOpenGLTexture& textureY, QOpenGLTexture& textureU, QOpenGLTexture& textureV, 
	const uchar* dataY, const uchar* dataU, const uchar* dataV, int width, int height)
{
	textureY.setFormat(QOpenGLTexture::R8_UNorm);
	textureY.setSize(width, height);
	textureY.setMipLevels(textureY.maximumMipLevels());
	textureY.allocateStorage();

	textureU.setFormat(QOpenGLTexture::R8_UNorm);
	textureU.setSize(width >> 1, height >> 1);
	textureU.setMipLevels(textureU.maximumMipLevels());
	textureU.allocateStorage();

	textureV.setFormat(QOpenGLTexture::R8_UNorm);
	textureV.setSize(width >> 1, height >> 1);
	textureV.setMipLevels(textureV.maximumMipLevels());
	textureV.allocateStorage();

	// Upload pixel data and generate mipmaps
	QOpenGLPixelTransferOptions uploadOptions;
	uploadOptions.setAlignment(1);
	textureY.setData(0, QOpenGLTexture::Red, QOpenGLTexture::UInt8, (const void*)dataY, &uploadOptions);
	textureU.setData(0, QOpenGLTexture::Red, QOpenGLTexture::UInt8, (const void*)dataU, &uploadOptions);
	textureV.setData(0, QOpenGLTexture::Red, QOpenGLTexture::UInt8, (const void*)dataV, &uploadOptions);
}

void GLWidget::setDataYUV(QOpenGLTexture& textureY, QOpenGLTexture& textureU, QOpenGLTexture& textureV, 
	const vector<const uchar*>& dataPtrs, int rows, int cols, int width, int height, int stride)
{
	assert(stride == width);

	int totalWidth = stride * cols;
	int totalHeight = height * rows;
	assert(totalWidth <= 16384 && totalHeight <= 16384);

	textureY.setFormat(QOpenGLTexture::R8_UNorm);
	textureY.setSize(totalWidth, totalHeight);
	textureY.setMipLevels(2);
	textureY.allocateStorage();
	textureY.setWrapMode(QOpenGLTexture::ClampToBorder);

	textureU.setFormat(QOpenGLTexture::R8_UNorm);
	textureU.setSize(totalWidth >> 1, totalHeight >> 1);
	textureU.setMipLevels(2);
	textureU.allocateStorage();
	textureU.setWrapMode(QOpenGLTexture::ClampToBorder);

	textureV.setFormat(QOpenGLTexture::R8_UNorm);
	textureV.setSize(totalWidth >> 1, totalHeight >> 1);
	textureV.setMipLevels(2);
	textureV.allocateStorage();
	textureV.setWrapMode(QOpenGLTexture::ClampToBorder);

	textureY.bind();
	for (int r = 0; r < rows; ++r)
	{
		for (int c = 0; c < cols; ++c)
		{
			glTexSubImage2D(GL_TEXTURE_2D, 0, c * width, r * height, stride, height, GL_RED, GL_UNSIGNED_BYTE, dataPtrs[r * cols + c]);
		}
	}
	textureY.generateMipMaps();

	textureU.bind();
	for (int r = 0; r < rows; ++r)
	{
		for (int c = 0; c < cols; ++c)
		{
			glTexSubImage2D(GL_TEXTURE_2D, 0, c * (width >> 1), r * (height >> 1), stride >> 1, height >> 1, GL_RED, GL_UNSIGNED_BYTE, &(dataPtrs[r * cols + c][stride * height]));
		}
	}
	textureU.generateMipMaps();

	textureV.bind();
	for (int r = 0; r < rows; ++r)
	{
		for (int c = 0; c < cols; ++c)
		{
			glTexSubImage2D(GL_TEXTURE_2D, 0, c * (width >> 1), r * (height >> 1), stride >> 1, height >> 1, GL_RED, GL_UNSIGNED_BYTE, &(dataPtrs[r * cols + c][stride * height + stride * height / 4]));
		}
	}
	textureV.generateMipMaps();
}

void GLWidget::updateTexture(const char* path)
{
	m_texture0->destroy();
	m_texture0->create();
	m_texture0->setData(QImage(QString(path)).mirrored());

	update();
}

void GLWidget::updateTexture(const uchar* img, int w, int h)
{
	m_texture0->destroy();
	m_texture0->create();
	m_texture0->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);

	setData(*m_texture0, img, w, h);

	update();
}

void GLWidget::updateYUVTexture(const uchar* imgY, const uchar* imgU, const uchar* imgV, int w, int h)
{
	m_textureY->destroy();
	m_textureY->create();
	m_textureY->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);

	m_textureU->destroy();
	m_textureU->create();
	m_textureU->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);

	m_textureV->destroy();
	m_textureV->create();
	m_textureV->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);

	setDataYUV(*m_textureY, *m_textureU, *m_textureV, imgY, imgU, imgV, w, h);

	update();
}

void GLWidget::updateTexture(const std::vector<const uchar*>& dataPtrs, int rows, int cols, int width, int height, double scale)
{
	m_texture0->destroy();
	m_texture0->create();
	m_texture0->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);

	setData(*m_texture0, dataPtrs, rows, cols, width, height);
	
	update();
}

void GLWidget::updateTextureData(const std::vector<const uchar*>& dataPtrs, int rows, int cols, int width, int height, double scale, double* coords)
{
	m_texture0->destroy();
	m_texture0->create();
	m_texture0->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);

	setData(*m_texture0, dataPtrs, rows, cols, width, height);

	updateTextureParams(rows, cols, width, height, scale, coords);

	update();
}

void GLWidget::updateTextureParams(int rows, int cols, int width, int height, double scale, double* coords)
{
	m_texCoordXY[0] = coords[0];
	m_texCoordXY[1] = coords[1];

	m_aspect = (width * cols) * m_height / ((double)m_width * (double)(rows * height));
	m_aspectScale = scale;

	update();
}

void GLWidget::updateTextureDataYUV(const std::vector<const uchar*>& dataPtrs, int rows, int cols, int width, int height, int stride, double scale, double* coords)
{
	m_textureY->destroy();
	m_textureY->create();
	m_textureY->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);

	m_textureU->destroy();
	m_textureU->create();
	m_textureU->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);

	m_textureV->destroy();
	m_textureV->create();
	m_textureV->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);

	setDataYUV(*m_textureY, *m_textureU, *m_textureV, dataPtrs, rows, cols, width, height, stride);

	updateTextureParams(rows, cols, width, height, scale, coords);

	update();
}

void GLWidget::setClearColor(const QColor &color)
{
	m_clearColor = color;
	update();
}

void GLWidget::setScale(double scale)
{
	m_scale = max(min(scale, m_scaleMax), m_scaleMin);
	((Window*)(this->parent()))->fetchTexture(m_scale, m_texOffX, m_texOffY);
}

void GLWidget::setTexOffXY(double offX, double offY)
{
	m_texOffX = offX;
	m_texOffY = offY;
	((Window*)(this->parent()))->updateTextureCenter(m_scale, m_texOffX, m_texOffY);
}

void GLWidget::initializeGL()
{

#define PROGRAM_TEXCOORD_ATTRIBUTE 0

	initializeOpenGLFunctions();

	makeObject();

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	QOpenGLShader *fshader = new QOpenGLShader(QOpenGLShader::Fragment, this);
	const char *fsrc =
		"uniform sampler2D texture0;\n"
		"void main(void)\n"
		"{\n"
		"    vec2 texPos = gl_TexCoord[0].xy;\n"
		"    vec4 col = texture2D(texture0, texPos);\n"
		"    gl_FragColor = col;\n"
		"}\n";

	const char *fsrcYUV =
		"uniform sampler2D textureY;\n"
		"uniform sampler2D textureU;\n"
		"uniform sampler2D textureV;\n"
		"void main(void)\n"
		"{\n"
		"    vec2 texPos = gl_TexCoord[0].xy;\n"
		"    vec4 colY = texture2D(textureY, texPos);\n"
		"    vec4 colU = texture2D(textureU, texPos);\n"
		"    vec4 colV = texture2D(textureV, texPos);\n"
		"    float y = colY.r * 255;\n"
		"    float u = colU.r * 255;\n"
		"    float v = colV.r * 255;\n"
		"    vec3 vi = vec3(-16 * 298 + 128 - 409 * 128 + 409 * v, -16 * 298 + 128 + 100 * 128 + 208 * 128 - 100 * u - 208 * v, -16 * 298 + 128 - 516 * 128 + 516 * u);\n"
		"    float r = max(0, min(255, (vi.r + 298 * y) / 256));\n"
		"    float g = max(0, min(255, (vi.g + 298 * y) / 256));\n"
		"    float b = max(0, min(255, (vi.b + 298 * y) / 256));\n"
		"    gl_FragColor = vec4(r / 255, g / 255, b / 255, 1);\n"
		"}\n";

	fshader->compileSourceCode(fsrcYUV);
	//fshader->compileSourceCode(fsrc);
	
	m_program = new QOpenGLShaderProgram;
	m_program->addShader(fshader);
	m_program->bindAttributeLocation("texCoord", PROGRAM_TEXCOORD_ATTRIBUTE);
	m_program->link();

	m_program->bind();

	m_program->setUniformValue("textureY", 0);
	m_program->setUniformValue("textureU", 1);
	m_program->setUniformValue("textureV", 2);

}

void GLWidget::paintGL()
{
	m_program->enableAttributeArray(PROGRAM_TEXCOORD_ATTRIBUTE);
	m_program->setAttributeBuffer(PROGRAM_TEXCOORD_ATTRIBUTE, GL_FLOAT, 3 * sizeof(GLfloat), 2, 5 * sizeof(GLfloat));

	glPushAttrib(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	glActiveTexture(GL_TEXTURE0);
	m_textureY->bind();
	glActiveTexture(GL_TEXTURE1);
	m_textureU->bind();
	glActiveTexture(GL_TEXTURE2);
	m_textureV->bind();
	
	glBegin(GL_QUADS);
	glTexCoord2f(m_texCoordXY[0], m_texCoordXY[1]);
	glVertex2f(-1.f, 1.f);
	glTexCoord2f(m_texCoordXY[0] + m_aspectScale, m_texCoordXY[1]);
	glVertex2f(1.f, 1.f);
	glTexCoord2f(m_texCoordXY[0] + m_aspectScale, m_texCoordXY[1] + m_aspect * m_aspectScale);
	glVertex2f(1.f, -1.f);
	glTexCoord2f(m_texCoordXY[0], m_texCoordXY[1] + m_aspect * m_aspectScale);
	glVertex2f(-1.f, -1.f);
	glEnd();

	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);

	glPopAttrib();
}

void GLWidget::resizeGL(int width, int height)
{
	int oldWidth = m_width;
	int oldHeight = m_height;
	m_width = this->width();
	m_height = this->height();
	
	setFocusPolicy(Qt::StrongFocus);
	setFocus(Qt::PopupFocusReason);

	m_xOffset = m_xOffset / oldWidth * m_width;
	m_yOffset = m_yOffset / oldHeight * m_height;

	((Window*)(this->parent()))->updateTextureCenter(m_scale, m_texOffX, m_texOffY);
}

void GLWidget::mousePressEvent(QMouseEvent *event)
{
	// stop moving automatically first
	((Window*)(this->parent()))->stopAutoTimer();

	double offX = 0.f, offY = 0.f;
	((Window*)(this->parent()))->updateOffset(m_scale, offX, offY);
	m_xOffset = calOffset(this->width()) - offX;
	m_yOffset = calOffset(this->height()) - offY;
	m_texOffX = offX;
	m_texOffY = offY;

	m_lastPos = event->pos();

	if (event->buttons() & Qt::RightButton)
	{
		m_zoomScale = m_scale;
	}

	m_lastScale = m_scale;
}

void GLWidget::mouseMoveEvent(QMouseEvent *event)
{
	double dx = event->x() - m_lastPos.x();
	double dy = event->y() - m_lastPos.y();

	if (event->buttons() & Qt::LeftButton)
	{
		double xOffBefore = -(m_xOffset - calOffset(this->width()));
		double yOffBefore = -(m_yOffset - calOffset(this->height()));
		m_xOffset += dx;
		m_yOffset += dy;

		((Window*)(this->parent()))->updateMoveEnd(xOffBefore , yOffBefore, -(m_xOffset - calOffset(this->width())), -(m_yOffset - calOffset(this->height())));
	}
	else if (event->buttons() & Qt::RightButton)
	{
		double dis = sqrt(dx * dx + dy * dy);
		if (dx < 0 || dy < 0)
		{
			m_zoomScale = max(min(m_zoomScale - (double)dis * 0.01, m_scaleMax), m_scaleMin);
		}
		else
		{
			m_zoomScale = max(min(m_zoomScale + (double)dis * 0.01, m_scaleMax), m_scaleMin);
		}

		((Window*)(this->parent()))->updateScaleEnd(m_zoomScale);
	}

	m_lastPos = event->pos();
	
}

void GLWidget::mouseReleaseEvent(QMouseEvent * event)
{
	m_lastPos = event->pos();
}

void GLWidget::wheelEvent(QWheelEvent *event)
{
	int numDegrees = event->delta() / 8;
	int numSteps = numDegrees / 5;
	
	((Window*)(this->parent()))->updateScaleEnd(m_scale + (double)numSteps * 0.1f);

	event->accept();
}

void GLWidget::keyPressEvent(QKeyEvent *event)
{
	static int indataId = 0;
	static int outdataId = 0;
	if (event->key() == Qt::Key_Space)
	{
		((Window*)(this->parent()))->setAutoMoveEnabled();
	}
	else if (event->key() == Qt::Key_Up)
	{
		printf("Export\n");
		std::fstream file;
		std::string outName = "point_" + std::to_string(outdataId++) + ".data";
		file.open(outName.c_str(), std::fstream::out);
		string tmpStr;
		file << m_scale << " " << m_texOffX << " " << m_texOffY << " " << ((Window*)(this->parent()))->getCurID() << endl;
		file.close();
	}
	else if (event->key() == Qt::Key_Down)
	{
		int fid = 0;
		printf("Import, scale = %lf, texOffX = %lf, texOffY = %lf\n", m_scale, m_texOffX, m_texOffY);
		std::fstream file;
		std::string inName = "point_" + std::to_string(indataId++) + ".data";
		file.open(inName.c_str(), std::fstream::in);
		string tmpStr;
		file >> m_scale >> m_texOffX >> m_texOffY >> fid;
		file.close();
		printf("After, scale = %lf, texOffX = %lf, texOffY = %lf, fid = %d\n", m_scale, m_texOffX, m_texOffY, fid);

		((Window*)(this->parent()))->setCurID(fid);
		setTexOffXY(m_texOffX, m_texOffY);
	}

	event->accept();
}

void GLWidget::makeObject()
{
	m_textureY = new QOpenGLTexture(QOpenGLTexture::Target2D);
	m_textureU = new QOpenGLTexture(QOpenGLTexture::Target2D);
	m_textureV = new QOpenGLTexture(QOpenGLTexture::Target2D);

	m_textureY->setWrapMode(QOpenGLTexture::ClampToBorder);
	m_textureU->setWrapMode(QOpenGLTexture::ClampToBorder);
	m_textureV->setWrapMode(QOpenGLTexture::ClampToBorder);

	setDataYUV(*m_textureY, *m_textureU, *m_textureV, m_loadingImage, &m_loadingImage[m_loadingWidth * m_loadingHeight], 
		&m_loadingImage[m_loadingWidth * m_loadingHeight + m_loadingHeight * m_loadingWidth / 4], m_loadingWidth, m_loadingHeight);
	((Window*)(this->parent()))->updateTextureCenter(m_scale, m_texOffX, m_texOffY);
}