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

#ifndef WINDOW_H
#define WINDOW_H

#include <QWidget>
#include <QtWidgets>
#include <qlabel.h>
#include <vector>

#include "videomanager.h"
#include "glwidget.h"
#include "loaderthread.h"

class GLWidget;

using namespace std;
using namespace cv;

class Window : public QWidget
{
	Q_OBJECT

public:
	Window(const ViewerConfig& config, VideoMipmap& mipmapManager, const char* mipmapNoDy);
	~Window();

	inline void setCurID(int fid) { m_curID = fid; }
	inline int  getCurID()        { return m_curID; }

	void updateOffset(double scale, double& offsetX, double& offsetY);
	void updateTextureCenter(double scale, double xOffset, double yOffset);
	void fetchTexture(double scale, int xOffset, int yOffset);
	void updateTextureCPU(bool updateFrame);
	void updateScaleEnd(double scale);
	void updateMoveEnd(double offXBefore, double offYBefore, double offX, double offY);
	void stopAutoTimer();
	void renderWindow();
	void renderWindowEnabled();
	void setAutoMoveEnabled();

protected:
	void resizeEvent(QResizeEvent * event);
	void closeEvent(QCloseEvent *event);
	
	private slots:
	void updateFrameCPU();
	void updateTextures();
	void updateTextureGPU();
	void autoMoveCheckBoxChecked(int state);
	void zoomInButtonPressed();
	void zoomInButtonClicked();
	void zoomInButtonReleased();
	void zoomOutButtonPressed();
	void zoomOutButtonClicked();
	void zoomOutButtonReleased();
	void zoomSliderValueChanged(int val);
	void updateZoomScale();
	void updateMovePos();


private:
	void stopAllTimers();
	void startAllTimers();

	VideoMipmap&  m_mipmapManager; // read only
	LoaderThread* m_loaderThread;

	GLWidget*    m_glWidget; // render widget
	QLabel*      m_viewerLabel;
	QPixmap      m_pixmap;
	QCheckBox*   m_autoMoveCheckBox;
	QPushButton* m_zoomInButton, *m_zoomOutButton;
	QSlider*     m_zoomSlider;
	QTimer*      m_zoomTimer, *m_moveTimer;
	QTimer*      m_playTimer;

	bool m_autoEnabled;
	int m_curID;
	int m_frameNum;
	int m_windWidth, m_windHeight;
	int m_imgWidth, m_imgHeight;
	int m_dataWidth, m_dataHeight;

	double m_scaleMin, m_scaleMax;
	double m_origWidth, m_origHeight;
	double m_offX, m_offY;
	double m_centX, m_centY;
	double m_moveOffXEnd, m_moveOffYEnd, m_moveOffXDelta, m_moveOffYDelta;
	double m_scaleDelta, m_scaleEnd, m_renderScale;

	Mat m_mat;
	uchar* m_dataY, *m_dataU, *m_dataV;
	uchar m_tl[2], m_br[2], m_tlLast[2], m_brLast[2];
	uchar m_level, m_levelLast;

	string m_mipmapNoDyDir;
	int m_stopFrame;

	bool m_wndImgRenderEnable;
	int m_wndImgCounter;
	vector<QImage> m_wndImgs;
};		

#endif
