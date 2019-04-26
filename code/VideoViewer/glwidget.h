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

#ifndef GLWIDGET_H
#define GLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include "viewerconfig.h"

QT_FORWARD_DECLARE_CLASS(QOpenGLShaderProgram);
QT_FORWARD_DECLARE_CLASS(QOpenGLTexture)

class GLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
	Q_OBJECT

public:
	explicit GLWidget(const ViewerConfig& config, const uchar* loadingImage, QWidget *parent = 0, int windWidth = 0, int windHeight = 0, int width = 0, int height = 0);
	~GLWidget();

	inline double getScale() { return m_scale; }
	inline void  getTexOffXY(double& offX, double& offY) { offX = m_texOffX; offY = m_texOffY; }

	void setData(QOpenGLTexture& texture, const uchar* data, int width, int height);
	void setData(QOpenGLTexture& texture, const std::vector<const uchar*>& dataPtrs, int rows, int cols, int width, int height);
	void setDataYUV(QOpenGLTexture& textureY, QOpenGLTexture& textureU, QOpenGLTexture& textureV,
		const uchar* dataY, const uchar* dataU, const uchar* dataV, int width, int height);
	void setDataYUV(QOpenGLTexture& textureY, QOpenGLTexture& textureU, QOpenGLTexture& textureV,
		const std::vector<const uchar*>& dataPtrs, int rows, int cols, int width, int height, int stride);

	void updateTexture(const char* path);
	void updateTexture(const uchar* img, int w, int h);
	void updateTexture(const std::vector<const uchar*>& dataPtrs, int rows, int cols, int width, int height, double scale);
	void updateYUVTexture(const uchar* imgY, const uchar* imgU, const uchar* imgV, int w, int h);
	void updateTextureData(const std::vector<const uchar*>& dataPtrs, int rows, int cols, int width, int height, double scale, double* coords);
	void updateTextureParams(int rows, int cols, int width, int height, double scale, double* coords);
	void updateTextureDataYUV(const std::vector<const uchar*>& dataPtrs, int rows, int cols, int width, int height, int stride, double scale, double* coords);
	void setClearColor(const QColor &color);

	//void updateScale(double delta);
	void setScale(double scale);
	void setTexOffXY(double offX, double offY);

signals:
	void clicked();


protected:
	void initializeGL() Q_DECL_OVERRIDE;
	void paintGL() Q_DECL_OVERRIDE;
	void resizeGL(int width, int height) Q_DECL_OVERRIDE;
	void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
	void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
	void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
	void wheelEvent(QWheelEvent *event) Q_DECL_OVERRIDE;
	void keyPressEvent(QKeyEvent* event) Q_DECL_OVERRIDE;

private:
	void makeObject();


	QColor m_clearColor;
	QPoint m_lastPos, m_zoomCenter;
	double m_scale, m_zoomScale, m_subScale;
	double m_xOffset, m_yOffset;
	double m_texOffX, m_texOffY;
	QOpenGLTexture *m_texture0;
	QOpenGLTexture *m_textureY, *m_textureU, *m_textureV;
	QOpenGLShaderProgram *m_program;
	const uchar* m_loadingImage;
	int m_loadingWidth, m_loadingHeight;
	int m_width, m_height;
	int m_curTextureNum;
	double m_lastScale, m_scaleMin, m_scaleMax;
	double m_texCoordXY[2]; // left-top coordinates of video texture
	double m_aspect, m_aspectScale;
	QCursor *m_zoomInCursor, *m_zoomOutCursor;
};

#endif