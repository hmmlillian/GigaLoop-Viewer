#ifndef LOADER_THREAD
#define LOADER_THREAD

#include "videomanager.h"

#include <qthread.h>
#include <qeventloop.h>
#include "qtimer.h"

class LoaderThread : public QThread {

	Q_OBJECT
public:
	LoaderThread();
	~LoaderThread(){}

	void initThread(VideoMipmap* mipmapManager);

	static LoaderThread& getInstance() { return *m_instance; }
	virtual void run();

	QEventLoop m_eventLoop;

private:
	static LoaderThread* m_instance;
	VideoMipmap* m_mipmapManager; // access to write
};


#endif
