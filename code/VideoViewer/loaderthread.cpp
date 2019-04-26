#include "loaderthread.h"

LoaderThread* LoaderThread::m_instance = new LoaderThread();

LoaderThread::LoaderThread()
	: QThread()
{

}

void LoaderThread::initThread(VideoMipmap* mipmapManager)
{
	m_mipmapManager = mipmapManager;
	connect(this, SIGNAL(finished()), &m_eventLoop, SLOT(quit()));
}

void LoaderThread::run()
{
	while (true)
	{
		m_mipmapManager->LoadSequence();
	}
}

