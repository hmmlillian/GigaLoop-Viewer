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

#include <QApplication>
#include <QSurfaceFormat>

#include "window.h"
#include "loaderthread.h"
#include "cmdLine.h"
#include "viewerconfig.h"

void GetInput(CmdLine& cmdLine, ViewerConfig& config)
{
	cmdLine.Param("d",   config.m_para_mipmapDir,  "Path of input mipmap directory.");
	cmdLine.Param("r",   config.m_para_loadRF,     "Load representative frames or not (0 or 1, default: 0). Note that representative frames will use much memory.");
	cmdLine.Param("l",   config.m_para_loadLevels, "Maximum number of mipmap levels to be loaded ([1, 6], default: 6).");
	cmdLine.Param("m",   config.m_para_preloadMemory, "Maximum avaliable memory reserved for viewer (unit: GB, default: 4GB, at least large enough to load all the representative static images to memory).");
	cmdLine.Param("t",   config.m_para_preloadThreadNum, "Maximum munber of threads for preloading (default: 4)");
	cmdLine.Param("w",   config.m_para_windWidth,  "Viewer window width (default: 1280)");
	cmdLine.Param("h",   config.m_para_windHeight, "Viewer window height (default: 720)");

	config.m_para_scaleMax = pow(2.0, config.m_para_loadLevels - 1);
}

int main(int argc, char *argv[])
{
    //Q_INIT_RESOURCE(textures);

	CmdLine cmdLine;
	ViewerConfig config;
	GetInput(cmdLine, config);
	if (!cmdLine.Parse(argc, argv)) 
	{
		return -1;
	}

    QApplication app(argc, argv);

    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);

	VideoMipmap mipmapManager(config);
	const char* mipmapNoDy = NULL;
	Window window(config, mipmapManager, mipmapNoDy);
	window.show();

	LoaderThread& loaderThread = LoaderThread::getInstance();
	loaderThread.initThread(&mipmapManager);
	loaderThread.start();
	loaderThread.setPriority(QThread::LowPriority);
	loaderThread.m_eventLoop.exec();

    return app.exec();
}