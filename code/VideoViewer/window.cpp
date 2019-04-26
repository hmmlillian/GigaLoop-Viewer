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

#include <omp.h>

#include "window.h"

extern QMutex g_mutex;
static bool g_sliderUpdate = true;
static bool g_autoMove = false;


Window::Window(const ViewerConfig& config, VideoMipmap& mipmapManager, const char* mipmapNoDy)
	: m_mipmapManager(mipmapManager)
{
	m_loaderThread = &(LoaderThread::getInstance());
	m_curID = 0;
	m_frameNum = m_mipmapManager.GetFrameNum();
	m_imgWidth  = m_mipmapManager.GetWidth();
	m_imgHeight = m_mipmapManager.GetHeight();

	m_windWidth = config.getWindWidth();
	m_windHeight = config.getWindHeight();
	resize(m_windWidth, m_windHeight);

	m_centX = 0.f;
	m_centY = 0.f;
	m_origHeight = 0;
	m_origWidth  = 0;

	m_scaleMin = config.getScaleMin();
	m_scaleMax = config.getScaleMax();

	m_levelLast = -1;
	m_tlLast[0] = -1;
	m_tlLast[1] = -1;
	m_brLast[0] = -1;
	m_brLast[1] = -1;

	m_dataY = NULL;
	m_dataU = NULL;
	m_dataV = NULL;
	m_dataWidth = 0;
	m_dataHeight = 0;

	m_autoEnabled = false;
	m_scaleDelta = UI_UPDATE_SCALE_UNIT;

	QColor clearColor(0, 0, 0);
	m_glWidget = new GLWidget(config, m_mipmapManager.GetLoadingImage(), this, m_windWidth, m_windHeight, m_imgWidth, m_imgHeight);
	m_glWidget->setClearColor(clearColor);

	string loadImgPath = m_mipmapManager.GetMipmapStr() + LOADING_IMG_NAME;
	QImage image(loadImgPath.c_str());
	QImage image2 = image.scaled(256, 256, Qt::KeepAspectRatio);
	m_pixmap = QPixmap::fromImage(image2);
	m_viewerLabel = new QLabel(m_glWidget);
	m_viewerLabel->move(0, 0);
	m_viewerLabel->setScaledContents(true);
	m_viewerLabel->setPixmap(m_pixmap);	

	m_autoMoveCheckBox = new QCheckBox(this);
	m_autoMoveCheckBox->move(m_windWidth - 20, 0);
	m_autoMoveCheckBox->resize(25, 25);
	connect(m_autoMoveCheckBox, SIGNAL(stateChanged(int)), this, SLOT(autoMoveCheckBoxChecked(int)));

	m_zoomInButton = new QPushButton(this);
	m_zoomInButton->move(m_windWidth - 25, m_autoMoveCheckBox->height());
	m_zoomInButton->resize(25, 25);
	m_zoomInButton->setStyleSheet("qproperty-icon: url(zoom_in.png);");
	connect(m_zoomInButton, SIGNAL(pressed()), this, SLOT(zoomInButtonPressed()));
	connect(m_zoomInButton, SIGNAL(released()), this, SLOT(zoomInButtonReleased()));
	connect(m_zoomInButton, SIGNAL(clicked()), this, SLOT(zoomInButtonClicked()));

	m_zoomOutButton = new QPushButton(this);
	m_zoomOutButton->move(m_zoomInButton->x(), m_zoomInButton->y() + m_zoomInButton->height());
	m_zoomOutButton->resize(m_zoomInButton->width(), m_zoomInButton->height());
	m_zoomOutButton->setStyleSheet("qproperty-icon: url(zoom_out.png);");
	connect(m_zoomOutButton, SIGNAL(pressed()), this, SLOT(zoomOutButtonPressed()));
	connect(m_zoomOutButton, SIGNAL(released()), this, SLOT(zoomOutButtonReleased()));
	connect(m_zoomOutButton, SIGNAL(clicked()), this, SLOT(zoomOutButtonClicked()));

	m_zoomSlider = new QSlider(this);
	m_zoomSlider->move(m_zoomOutButton->x(), m_zoomOutButton->y() + m_zoomOutButton->height() + 2);
	m_zoomSlider->resize(m_zoomInButton->width(), m_zoomInButton->height() * 8);
	m_zoomSlider->setStyleSheet("* { background-color: rgba(255,255,255,55) }");
	m_zoomSlider->setMinimum(0);
	m_zoomSlider->setMaximum(100);
	m_zoomSlider->setValue(0);
	connect(m_zoomSlider, SIGNAL(valueChanged(int)), this, SLOT(zoomSliderValueChanged(int)));

	m_zoomTimer = new QTimer(this);
	connect(m_zoomTimer, SIGNAL(timeout()), this, SLOT(updateZoomScale()));

	m_moveTimer = new QTimer(this);
	connect(m_moveTimer, SIGNAL(timeout()), this, SLOT(updateMovePos()));

	m_playTimer = new QTimer(this);
	connect(m_playTimer, SIGNAL(timeout()), this, SLOT(updateFrameCPU()));
	m_playTimer->start(PLAY_INTERVAL);

	if (mipmapNoDy)
	{
		m_mipmapNoDyDir = string(mipmapNoDy);
	}
	else
	{
		m_mipmapNoDyDir = "";
	}

	m_wndImgCounter = 0;
	m_wndImgRenderEnable = false;

	setWindowTitle(tr("Panoramic Video Viewer"));
	printf("Window Width = %d, Window Height = %d\n", this->width(), this->height());
}

Window::~Window()
{
	m_loaderThread->terminate();
	printf("Exit Window!\n");
}

void Window::updateOffset(double scale, double& offsetX, double& offsetY)
{
	offsetX = m_centX * (m_imgWidth * scale - 1.0);
	offsetY = m_centY * (m_imgHeight * scale - 1.0);
}

void Window::updateTextureCenter(double scale, double xOffset, double yOffset)
{
	m_centX = xOffset / (m_imgWidth * scale - 1.0);
	m_centY = yOffset / (m_imgHeight * scale - 1.0);

	fetchTexture(scale, xOffset, yOffset);
}

void Window::fetchTexture(double scale, int xOffset, int yOffset)
{
	double level = log2(scale);
	int totalLevels = m_mipmapManager.GetLevelNum() - 1;
	int lowerLevel = min((int)floor(level), totalLevels);
	m_level = min((int)ceil(level), totalLevels);

	double curX = (m_imgWidth * scale - 1.0) * (m_centX + 0.5);
	double curY = (m_imgHeight * scale - 1.0) * (m_centY + 0.5);
	double winX = (m_windWidth - 1.0) * 0.5;
	double winY = (m_windHeight - 1.0) * 0.5;

	m_offX = (curX - winX) / scale;
	m_offY = (curY - winY) / scale;
	m_origWidth = m_windWidth / scale;
	m_origHeight = m_windHeight / scale;

	// tiles covered at lower level
	int tileWidth = m_mipmapManager.GetTileWidth();
	int tileHeight = m_mipmapManager.GetTileHeight();
	m_tl[0] = max((int)(m_offX * LEVEL_TILE_NUM[m_level] / (double)tileWidth), 0);
	m_tl[1] = max((int)(m_offY * LEVEL_TILE_NUM[m_level] / (double)tileHeight), 0);

	m_br[0] = min((int)((m_offX + m_origWidth) * LEVEL_TILE_NUM[m_level] / (double)tileWidth), m_mipmapManager.GetLevelCols(m_level) - 1);
	m_br[1] = min((int)((m_offY + m_origHeight) * LEVEL_TILE_NUM[m_level] / (double)tileHeight), m_mipmapManager.GetLevelRows(m_level) - 1);

	m_mipmapManager.UpdateMipmapQueue(m_centX, m_centY, scale, m_level);

	// update manually rather than controlled by timer
	//if (!m_autoEnabled)
	{
		updateTextureCPU(false);
	}

	// updates UI
	double mipmapScale = max(m_imgWidth / 256.f, m_imgHeight / 256.f);
	double dx = m_windWidth / (mipmapScale * scale);
	double dy = m_windHeight / (mipmapScale * scale);
	double sx = (m_centX + 0.5f) * m_pixmap.width() - 0.5f * (dx - 1);
	double sy = (m_centY + 0.5f) * m_pixmap.height() - 0.5f * (dy - 1);

	QPixmap pixmap = m_pixmap;
	m_viewerLabel->setPixmap(pixmap);

	if (g_sliderUpdate)
	{
		m_zoomSlider->blockSignals(true);
		m_zoomSlider->setValue((scale - m_scaleMin) / (double)(LEVEL_TILE_NUM[totalLevels] - m_scaleMin) * 100.f);
		m_zoomSlider->blockSignals(false);
	}
}

void Window::resizeEvent(QResizeEvent * event)
{
	const QSize& sz = event->size();
	m_windWidth = sz.width();
	m_windHeight = sz.height();

	m_autoMoveCheckBox->move(m_windWidth - 20, 0);
	m_zoomInButton->move(m_windWidth - 25, m_autoMoveCheckBox->height());
	m_zoomOutButton->move(m_zoomInButton->x(), m_zoomInButton->y() + m_zoomInButton->height());
	m_zoomSlider->move(m_zoomOutButton->x(), m_zoomOutButton->y() + m_zoomOutButton->height() + 2);
	
	m_glWidget->resize(m_windWidth, m_windHeight);
}

void Window::closeEvent(QCloseEvent *event)
{
	printf("Exit\n");
	m_loaderThread->quit(); 
	exit(0);
}

static void copyTo(void* dst, const Rect& dstRect, const int dstStride, const void* src, const Rect& srcRect, const int srcStride)
{
	assert(dstRect.width == srcRect.width && dstRect.height == srcRect.height);
	int dataSize = sizeof(Vec3b);
	int dstOff = dstRect.x * dataSize;
	int srcOff = srcRect.x * dataSize;

	omp_set_num_threads(4);

#pragma omp parallel for 
	for (int y = 0; y < srcRect.height; ++y)
	{
		int dstY = y + dstRect.y;
		int srcY = y + srcRect.y;
		uchar* dstData = &((uchar*)dst)[dstY * dstStride + dstOff];
		uchar* srcData = &((uchar*)src)[srcY * srcStride + srcOff];
		memcpy(dstData, srcData, srcRect.width * dataSize);
	}
}

static void copyTo2(void* dst, const Rect& dstRect, const int dstStride, const void* src, const Rect& srcRect, const int srcStride)
{
	assert(dstRect.width == srcRect.width && dstRect.height == srcRect.height);
	int dstOff = dstRect.x;
	int srcOff = srcRect.x;

	omp_set_num_threads(4);

#pragma omp parallel for 
	for (int y = 0; y < srcRect.height; ++y)
	{
		int dstY = y + dstRect.y;
		int srcY = y + srcRect.y;
		uchar* dstData = &((uchar*)dst)[dstY * dstStride + dstOff];
		uchar* srcData = &((uchar*)src)[srcY * srcStride + srcOff];
		memcpy(dstData, srcData, srcRect.width);
	}
}

void Window::updateFrameCPU()
{
	updateTextureCPU(true);
}

void Window::updateTextureCPU(bool updateFrame)
{
	int newHeight = m_origHeight * LEVEL_TILE_NUM[m_level];
	int newWidth = m_origWidth * LEVEL_TILE_NUM[m_level];
	int size = newWidth * newHeight;
	int halfSize = ceil(newWidth * 0.5) * ceil(newHeight * 0.5);

	if (newWidth != m_dataWidth || newHeight != m_dataHeight)
	{
		free(m_dataY);
		free(m_dataU);
		free(m_dataV);
		m_dataY = (uchar*)malloc(size);		
		m_dataU = (uchar*)malloc(halfSize);
		m_dataV = (uchar*)malloc(halfSize);
		m_dataWidth = newWidth;
		m_dataHeight = newHeight;
	}

#pragma omp parallel for 
	for(int i = 0; i < size; ++i)
	{
		m_dataY[i] = 16;
	}

#pragma omp parallel for 
	for (int i = 0; i < halfSize; ++i)
	{
		m_dataU[i] = 128;
		m_dataV[i] = 128;
	}

	int x_tl = max(m_offX * (double)LEVEL_TILE_NUM[m_level], 0.0);
	int y_tl = max(m_offY *(double)LEVEL_TILE_NUM[m_level], 0.0);
	int tx = -min(m_offX * (double)LEVEL_TILE_NUM[m_level], 0.0);
	int ty = -min(m_offY * (double)LEVEL_TILE_NUM[m_level], 0.0);
	int tileWidth = m_mipmapManager.GetTileWidth();
	int tileHeight = m_mipmapManager.GetTileHeight();

	int h = 0, w = 0, cnt = 0, stride = m_mipmapManager.GetLineStride();
	for (int j = m_tl[1]; j <= m_br[1]; ++j)
	{
		for (int i = m_tl[0]; i <= m_br[0]; ++i)
		{
			int tl_nxt[2] = { (i + 1) * tileWidth, (j + 1) * tileHeight };

			const uchar* imgData = m_mipmapManager.GetFrameData(m_level, i, j, m_curID, cnt++);
			
			w = max(0, min(min(int(newWidth - tx), tl_nxt[0] - x_tl),  tileWidth));
			h = max(0, min(min(int(newHeight - ty), tl_nxt[1] - y_tl), tileHeight));

			if (w > 0 && h > 0)
			{
				copyTo2(m_dataY, Rect(tx, ty, w, h), newWidth, imgData, Rect(x_tl - i * tileWidth, y_tl - j * tileHeight, w, h), stride);
				copyTo2(m_dataU, Rect(tx / 2, ty / 2, w / 2, h / 2), newWidth / 2, &imgData[stride * tileHeight], Rect((x_tl - i * tileWidth) / 2, (y_tl - j * tileHeight) / 2, w / 2, h / 2), stride / 2);
				copyTo2(m_dataV, Rect(tx / 2, ty / 2, w / 2, h / 2), newWidth / 2, &imgData[stride * tileHeight + stride / 2 * tileHeight / 2], Rect((x_tl - i * tileWidth) / 2, (y_tl - j * tileHeight) / 2, w / 2, h / 2), stride / 2);
			}
			tx += w;

			x_tl = tileWidth * (i + 1);
		}
		y_tl = tileHeight * (j + 1);
		x_tl = max(m_offX * (double)LEVEL_TILE_NUM[m_level], 0.0);

		ty += h;
		tx = -min(m_offX * (double)LEVEL_TILE_NUM[m_level], 0.0);
	}

	m_glWidget->updateYUVTexture(m_dataY, m_dataU, m_dataV, m_dataWidth, m_dataHeight);

	if (updateFrame)
	{
		m_curID += 1;
		if (m_curID >= m_frameNum)
		{
			m_curID -= m_frameNum;
		}
	}
}

void Window::stopAllTimers()
{
	m_playTimer->stop();
	m_zoomTimer->stop();
	m_moveTimer->stop();
}

void Window::startAllTimers()
{
	m_playTimer->start(PLAY_INTERVAL);
	m_zoomTimer->start(UI_UPDATE_TIME);
	m_moveTimer->start(UI_UPDATE_TIME);
}

void Window::updateTextures()
{
	vector<const uchar*> dataPtrs;
	vector<Mat> testVec;
	for (int j = m_tl[1]; j <= m_br[1]; ++j)
	{
		for (int i = m_tl[0]; i <= m_br[0]; ++i)
		{
			dataPtrs.push_back(m_mipmapManager.GetFrameData(m_level, i, j, m_curID, dataPtrs.size()));
		}
	}
			
	m_glWidget->updateTexture(dataPtrs, m_br[1] - m_tl[1] + 1, m_br[0] - m_tl[0] + 1, m_imgWidth, m_imgHeight, LEVEL_TILE_NUM[m_level]);
	
	m_curID += 1;
	if (m_curID >= m_frameNum)
	{
		m_curID -= m_frameNum;
	}
}

void Window::updateTextureGPU()
{
	static bool updateFrame = true;

	//double timer_Start = (double)clock();

	//printf("Current Frame = %d, Update = %d, level = %d\n", updateFrame, m_curID, m_level);
	int tileWidth = m_mipmapManager.GetTileWidth();
	int tileHeight = m_mipmapManager.GetTileHeight();
	int totalWidth = tileWidth * (m_br[0] - m_tl[0] + 1);
	int totalHeight = tileHeight * (m_br[1] - m_tl[1] + 1);

	double coords[2] = {-1.f, -1.f};
	if (totalWidth > 0 && totalHeight > 0)
	{
		coords[0] = (m_offX * LEVEL_TILE_NUM[m_level] - m_tl[0] * tileWidth) / (double)totalWidth;
		coords[1] = (m_offY * LEVEL_TILE_NUM[m_level] - m_tl[1] * tileHeight) / (double)totalHeight;
	}

	m_renderScale = m_origWidth * LEVEL_TILE_NUM[m_level] / (double)totalWidth;

	// only update texture data on GPU when current level and covered tiles change
	if (updateFrame || m_level != m_levelLast || m_tl[0] != m_tlLast[0] || m_tl[1] != m_tlLast[1] || m_br[0] != m_brLast[0] || m_br[1] != m_brLast[1])
	{
		vector<const uchar*> dataPtrs;
		for (int j = m_tl[1]; j <= m_br[1]; ++j)
		{
			for (int i = m_tl[0]; i <= m_br[0]; ++i)
			{
				dataPtrs.push_back(m_mipmapManager.GetFrameData(m_level, i, j, m_curID, dataPtrs.size()));
			}
		}

		m_glWidget->updateTextureDataYUV(dataPtrs, m_br[1] - m_tl[1] + 1, m_br[0] - m_tl[0] + 1, tileWidth, tileHeight, m_mipmapManager.GetLineStride(), m_renderScale, coords);
		//m_glWidget->updateTextureData(dataPtrs, m_br[1] - m_tl[1] + 1, m_br[0] - m_tl[0] + 1, m_width, m_height, scale, coords);
		
		m_levelLast = m_level;
		m_tlLast[0] = m_tl[0];
		m_tlLast[1] = m_tl[1];
		m_brLast[0] = m_br[0];
		m_brLast[1] = m_br[1];
	}
	else // update parameters only
	{
		m_glWidget->updateTextureParams(m_br[1] - m_tl[1] + 1, m_br[0] - m_tl[0] + 1, tileWidth, tileHeight, m_renderScale, coords);
	}

	if (updateFrame)
	{
		m_curID += 1;
		if (m_curID >= m_frameNum)
		{
			m_curID -= m_frameNum;
		}
	}
	updateFrame = !updateFrame;

	//double timer_Finish = (double)clock();

	//printf("GPU Time: %.2fms\n", (timer_Finish - timer_Start));
}

void Window::autoMoveCheckBoxChecked(int state)
{
	if (state == Qt::Unchecked)
	{
		m_autoEnabled = false;
	}
	else if (state == Qt::Checked)
	{
		m_autoEnabled = true;
	}
}

void Window::zoomInButtonPressed()
{
	m_scaleDelta = UI_UPDATE_SCALE_UNIT;
	m_scaleEnd = -1.f;
	m_zoomTimer->start(UI_UPDATE_TIME);
}

void Window::zoomInButtonClicked()
{
	m_scaleEnd = m_glWidget->getScale() + 0.1f;
	m_zoomTimer->start(UI_UPDATE_TIME);
}

void Window::zoomInButtonReleased()
{
	m_scaleEnd = m_glWidget->getScale() + 0.1f;
}

void Window::zoomOutButtonPressed()
{
	m_scaleDelta = -UI_UPDATE_SCALE_UNIT;
	m_scaleEnd = -1.f;
	m_zoomTimer->start(UI_UPDATE_TIME);
}

void Window::zoomOutButtonClicked()
{
	m_scaleEnd = m_glWidget->getScale() - 0.1f;
	m_zoomTimer->start(UI_UPDATE_TIME);
}

void Window::zoomOutButtonReleased()
{
	m_scaleEnd = m_glWidget->getScale() - 0.1f;
}

void Window::zoomSliderValueChanged(int val)
{
	g_sliderUpdate = false;
	m_scaleEnd = val / 100.f * double(LEVEL_TILE_NUM[m_mipmapManager.GetLevelNum() - 1] - m_scaleMin) + m_scaleMin;
	m_zoomTimer->start(UI_UPDATE_TIME);
}

void Window::updateScaleEnd(double scale)
{
	m_scaleEnd = scale; 
	if (!m_zoomTimer->isActive())
	{
		m_zoomTimer->start(UI_UPDATE_TIME);
	}
}

void Window::updateMoveEnd(double offXBefore, double offYBefore, double offX, double offY)
{
	m_moveOffXEnd = offX;
	m_moveOffYEnd = offY;

	if(m_autoEnabled)
	{
		m_moveOffXDelta = offX - offXBefore;
		m_moveOffYDelta = offY - offYBefore;
		double dis = UI_UPDATE_MOVE_UNIT / sqrt(m_moveOffXDelta * m_moveOffXDelta + m_moveOffYDelta * m_moveOffYDelta);
		m_moveOffXDelta = dis * m_moveOffXDelta;
		m_moveOffYDelta = dis * m_moveOffYDelta;
	}
	
	if (abs(m_moveOffXEnd - offXBefore) > 0 || abs(m_moveOffYEnd - offYBefore) > 0)
	{
		if (!m_moveTimer->isActive())
		{
			m_moveTimer->start(UI_UPDATE_TIME);
		}
	}
}

void Window::renderWindow()
{
	int id = m_wndImgCounter % MAX_FRAME_NUM;

	QPainter ipainter(&m_wndImgs[id]);
	this->render(&ipainter);

	m_wndImgCounter++;

	// write to disk first
	if (m_wndImgCounter % MAX_FRAME_NUM == 0)
	{
		stopAllTimers();

		char fileName[260];
		for (int f = 0; f < MAX_FRAME_NUM; ++f)
		{
			sprintf(fileName, "f_%04d.png", f);
			m_wndImgs[f].save(fileName);
		}

		startAllTimers();
	}
}

void Window::renderWindowEnabled()
{
	if (m_wndImgRenderEnable)
	{
		stopAllTimers();

		char fileName[260];
		for (int f = 0; f < m_wndImgCounter; ++f)
		{
			sprintf(fileName, "f_%04d.png", f);
			m_wndImgs[f].save(fileName);
		}

		startAllTimers();

		m_wndImgRenderEnable = false;
		m_wndImgCounter = 0;
	}
	else
	{
		m_wndImgRenderEnable = true;
		m_wndImgCounter = 0;

		m_wndImgs.resize(MAX_FRAME_NUM);
#pragma omp parallel for 
		for (int f = 0; f < MAX_FRAME_NUM; ++f)
		{
			m_wndImgs[f] = QImage(this->width(), this->height(), QImage::Format_RGB888);
		}
	}
}

void Window::setAutoMoveEnabled()
{
	m_autoEnabled = !m_autoEnabled;
	if (!m_autoEnabled)
	{
		stopAutoTimer();
	}

	m_autoMoveCheckBox->blockSignals(true);
	m_autoMoveCheckBox->setChecked(m_autoEnabled);
	m_autoMoveCheckBox->blockSignals(false);
}

void Window::stopAutoTimer()
{
	if (m_zoomTimer->isActive())
	{
		m_zoomTimer->stop();
		g_autoMove = false;
	}
	if (m_moveTimer->isActive())
	{
		m_moveTimer->stop();
		g_autoMove = false;
	}
}

void Window::updateZoomScale()
{
	double scale = m_glWidget->getScale();
	
	if (!g_autoMove)
	{
		if (m_scaleEnd < 0.f)
		{
			m_glWidget->setScale(scale + m_scaleDelta);
		}
		else
		{
			scale = scale * (1.f - DYNAMIC_WEIGHT) + m_scaleEnd * DYNAMIC_WEIGHT;
			m_glWidget->setScale(scale);

			double delta = abs(scale - m_scaleEnd);
			if (m_autoEnabled && delta < UI_UPDATE_SCALE_UNIT)
			{
				if (scale < m_scaleEnd)
				{
					m_scaleDelta = UI_UPDATE_SCALE_UNIT;
				}
				else
				{
					m_scaleDelta = -UI_UPDATE_SCALE_UNIT;
				}
				g_autoMove = true;
			}
			else if (delta < 0.001f)
			{
				m_zoomTimer->stop();
				g_sliderUpdate = true;
			}
		}
	}
	else
	{
		scale *= (1.f + m_scaleDelta);
		m_glWidget->setScale(scale);
		if (scale >= m_scaleMax || scale < 0.f)
		{
			m_zoomTimer->stop();
			g_sliderUpdate = true;
			g_autoMove = false;
		}
	}
}

void Window::updateMovePos()
{
	double offX = 0.f, offY = 0.f;
	m_glWidget->getTexOffXY(offX, offY);
	
	if (!g_autoMove)
	{
		offX = offX * (1.f - DYNAMIC_WEIGHT) + m_moveOffXEnd * DYNAMIC_WEIGHT;
		offY = offY * (1.f - DYNAMIC_WEIGHT) + m_moveOffYEnd * DYNAMIC_WEIGHT;

		m_glWidget->setTexOffXY(offX, offY);
		double deltaX = abs(offX - m_moveOffXEnd);
		double deltaY = abs(offY - m_moveOffYEnd);

		if (m_autoEnabled && deltaX < UI_UPDATE_MOVE_UNIT && deltaY < UI_UPDATE_MOVE_UNIT)
		{
			g_autoMove = true;
		}
		else if (deltaX < 1.f && deltaY < 1.f)
		{
			m_moveTimer->stop();
			g_autoMove = false;
		}
	}
	else
	{
		offX += m_moveOffXDelta;
		offY += m_moveOffYDelta;
		m_glWidget->setTexOffXY(offX, offY);

		double scale = m_glWidget->getScale();
		if (abs(offX) >= abs(m_imgWidth * scale) || abs(offY) >= abs(m_imgHeight * scale))
		{
			m_moveTimer->stop();
			g_autoMove = false;
		}
	}
}