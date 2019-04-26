#include "videomanager.h"
#include "viewerconfig.h"

#include <Windows.h>
#include <time.h>
#include <qmutex.h>
#include <omp.h>
#include <fstream>

QMutex g_mutex;

uchar LEVEL_TILE_NUM[MAX_LEVEL_NUM] = { 1, 2, 4, 8, 16, 32 };

static void findFile(const char* lpPath, vector<string>& fileList)
{
	char szFile[MAX_PATH];
	WIN32_FIND_DATA FindFileData;
	strcpy(szFile, lpPath);
	strcat(szFile, "\\*");

	WCHAR wszFind[260];
	size_t convertedChars = 0;
	mbstowcs_s(&convertedChars, wszFind, 260, szFile, _TRUNCATE);
	HANDLE hFind = ::FindFirstFile(wszFind, &FindFileData);

	char fileName[260];
	memset(fileName, 0, 260);

	if (INVALID_HANDLE_VALUE == hFind)
	{
		return;
	}
	while (TRUE)
	{
		if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (FindFileData.cFileName[0] != '.')
			{
				wcstombs(fileName, FindFileData.cFileName, 260);
				strcpy(szFile, lpPath);
				strcat(szFile, "\\");
				strcat(szFile, fileName);
				findFile(szFile, fileList);
			}
		}
		else
		{
			wcstombs(fileName, FindFileData.cFileName, 260);
			strcpy(szFile, lpPath);
			strcat(szFile, "\\");
			strcat(szFile, fileName);
			fileList.push_back(szFile);
		}
		if (!FindNextFile(hFind, &FindFileData))
		{
			break;
		}
	}
	FindClose(hFind);
}

static void findDir(const char* lpPath, vector<string>& dirList)
{
	char szFile[MAX_PATH];
	WIN32_FIND_DATA FindFileData;
	strcpy(szFile, lpPath);
	strcat(szFile, "\\*");

	WCHAR wszFind[260];
	size_t convertedChars = 0;
	mbstowcs_s(&convertedChars, wszFind, 260, szFile, _TRUNCATE);
	HANDLE hFind = ::FindFirstFile(wszFind, &FindFileData);

	char fileName[260];
	memset(fileName, 0, 260);

	if (INVALID_HANDLE_VALUE == hFind)
	{
		return;
	}
	while (TRUE)
	{
		if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (FindFileData.cFileName[0] != '.')
			{
				wcstombs(fileName, FindFileData.cFileName, 260);
				strcpy(szFile, lpPath);
				strcat(szFile, "\\");
				strcat(szFile, fileName);
				dirList.push_back(szFile);
			}
		}
		if (!FindNextFile(hFind, &FindFileData))
		{
			break;
		}
	}
	FindClose(hFind);
}

VideoMipmap::VideoMipmap(const ViewerConfig& config)
	:m_config(config)
{
	m_frameNum = MAX_FRAME_NUM;
	m_mipmapDir = config.getMipmapDir();
	m_mipmapLevelMax = config.getLoadLevels();
	m_preloadThreadMax = config.getPreloadThreadNum();
	m_loadRepresentativeFrame = config.getLoadRF();

	m_loadID = new int[m_preloadThreadMax];
	for (int i = 0; i < m_preloadThreadMax; ++i)
	{
		m_loadID[i] = -1;
	}

	m_loadNum = 0;
	m_mipmapQueueUpdate = false;

	// initialize sizes of the tiles in each level
	InitMipmapLevels();

	// estimate maximum of tiles to be loaded based on the available memory
	float preloadMem = config.getPreloadMemory() * 1024.f;
	if (m_loadRepresentativeFrame)
	{
		preloadMem -= 2.f * m_tileNum;
	}
	m_preloadTileMax = preloadMem / 320.f;
	m_mipmapSequence = new Sequence[m_preloadTileMax];
	printf("Load Tile Max = %d\n", m_preloadTileMax);

	// preload representative frames and videos, and allocate memory for preloaded videso
	InitLoading();
}

VideoMipmap::~VideoMipmap()
{
	for (int i = 0; i < m_preloadTileMax; ++i)
	{
		for (int f = 0; f < m_frameNum; ++f)
		{
			free(m_mipmapSequence[i].seq[f]);
			m_mipmapSequence[i].seq[f] = NULL;
		}
		m_mipmapSequence[i].seq.clear();
	}
}

void VideoMipmap::InitLoading()
{
	// initialize all the tiles and get video handle
	InitMipmapTiles();

	// Load the first video and get some attributes
	InitFFMPEG();

	// initialize all the vector of mipmap sequence
	for (int i = 0; i < m_preloadTileMax; ++i)
	{
		if (m_mipmapSequence[i].seq.size()) continue;

		m_mipmapSequence[i].seq.resize(m_frameNum);
		for (int f = 0; f < m_frameNum; ++f)
		{
			m_mipmapSequence[i].seq[f] = (uchar*)malloc(m_byteNum * sizeof(uchar));
		}
	}

	// convert representative images' format to NV12 without alignment
	string fName = m_mipmapDir + LOADING_IMG_NAME;
	cv::Mat imgMat = cv::imread(fName.c_str());
	m_loadingImage = (uchar*)malloc(m_width * m_height * 3 / 2);
	convert_Image_to_YUV(imgMat, m_loadingImage, m_width);
	
	int totalSize = m_lineStride * m_tileHeight * 3 / 2;
	if (m_loadRepresentativeFrame)
	{
#pragma omp parallel for
		for (int id = 0; id < m_tileNum; ++id)
		{
			string imgName = m_mipmapDir + m_mipmapTiles[id].preName + IMG_EXT;
			cv::Mat imgMat = cv::imread(imgName.c_str());
			m_mipmapTiles[id].img = (uchar*)malloc(totalSize);
			convert_Image_to_YUV(imgMat, m_mipmapTiles[id].img, m_lineStride);
		}
	}
	else
	{
		m_representiveFrame = (uchar*)malloc(totalSize);
		int size = m_tileHeight * m_tileWidth;
#pragma omp parallel for 
		for (int i = 0; i < size; ++i)
		{
			m_representiveFrame[i] = 16;
		}

		int halfSize = size / 2;
#pragma omp parallel for 
		for (int i = 0; i < halfSize; ++i)
		{
			m_representiveFrame[i + size] = 128;
		}
	}
}

void VideoMipmap::InitMipmapTiles()
{
	m_mipmapTiles.resize(m_tileNum);

	for (int l = 0; l < m_mipmapLevelMax; ++l)
	{
		int tr = m_mipmapLevel[l].rows;
		int tc = m_mipmapLevel[l].cols;
		int tSum = m_mipmapLevel[l].tileSum;

#pragma omp parallel for
		for (int ty = 0; ty < tr; ++ty)
		{
			for (int tx = 0; tx < tc; ++tx)
			{
				int id = tSum + ty * tc + tx;
				m_mipmapTiles[id] = Tile(l, tx, ty, INVALID_UCHAR, tr, tc, 0.f, id);
			}
		}
	}
}

void VideoMipmap::InitMipmapLevels()
{
	string configpath = m_mipmapDir + "\\mipmap.config";

	std::fstream file;
	file.open(configpath, std::fstream::in);
	string tmpStr;
	int levelNum = 0;
	file >> tmpStr >> levelNum >> tmpStr >> m_tileWidth >> tmpStr >> m_frameNum >> 
		    tmpStr >> m_width >> tmpStr >> m_height;
	
	m_tileHeight = m_tileWidth;
	m_tileNum = 0;

	int level = 0;
	for (int l = 0; l < m_mipmapLevelMax; ++l)
	{
		file >> tmpStr >> level >> tmpStr >> m_mipmapLevel[l].rows >> tmpStr >> m_mipmapLevel[l].cols;
		assert(level == l);
		m_mipmapLevel[l].tileSum = m_tileNum;
		m_tileNum += m_mipmapLevel[l].rows * m_mipmapLevel[l].cols;
	}

	file.close();
}

void VideoMipmap::UpdateMipmapQueue(float centerX, float centerY, float centerZ, int curLevel)
{
	// update priority of all the tiles based on their Manhattan distance to target
	float factor = centerZ / (float)LEVEL_TILE_NUM[curLevel];

	for (int l = 0; l < m_mipmapLevelMax; ++l)
	{
		int tr = m_mipmapLevel[l].rows;
		int tc = m_mipmapLevel[l].cols;

#pragma omp parallel for
		for (int ty = 0; ty < tr; ++ty)
		{
			for (int tx = 0; tx < tc; ++tx)
			{
				int id = m_mipmapLevel[l].tileSum + ty * tc + tx;
				Tile& tile = m_mipmapTiles[id];
				float cx = centerX * m_mipmapLevel[l].cols;
				float cy = centerY * m_mipmapLevel[l].rows;

				float dis = sqrt((tile.cy - cy) * (tile.cy - cy)) + sqrt((tile.cx - cx) * (tile.cx - cx));
				if (l < curLevel)
				{
					dis += abs(centerZ - LEVEL_TILE_NUM[l]);
				}
				else if (l == curLevel)
				{
					dis *= factor;
				}
				else
				{
					dis = dis * LEVEL_TILE_NUM[l - 1] / (float)LEVEL_TILE_NUM[l] + abs(centerZ - LEVEL_TILE_NUM[l - 1]);
				}
				if (dis > 0.f)
				{
					tile.priority = 1.f / dis;
				}
				else
				{
					tile.priority = 1.f;
				}
			}
		}
	}

	// rebuild priority queue
	g_mutex.lock();

	m_mipmapQueue = priority_queue<Priority, vector<Priority>, PCmp>();
	for (int t = 0; t < m_tileNum; ++t)
	{
		if (m_mipmapTiles[t].priority > 0.2f)
			m_mipmapQueue.push(Priority(t, m_mipmapTiles[t].priority));
	}
	m_mipmapQueueUpdate = true;

	g_mutex.unlock();
}

bool VideoMipmap::UpdateLoadedID(int threadId, float priority)
{
	bool isOK = false;

	if (m_loadNum < m_preloadTileMax)
	{
		m_loadID[threadId] = m_loadNum;
		m_loadNum += 1;
		isOK = true;
	}
	else
	{
		float pMin = 100.f;
		int idMin = -1;

		// replace the one with minimum priority
		for (int i = 0; i < m_preloadTileMax; ++i)
		{
			int sid = m_mipmapSequence[i].id;
			if (sid >= 0 && pMin > m_mipmapTiles[sid].priority)
			{
				pMin = m_mipmapTiles[sid].priority;
				idMin = i;
			}
		}

		if (pMin < priority)
		{
			int tid = m_mipmapSequence[idMin].id;

			// remove tile from cache
			m_mipmapTiles[tid].loadid = INVALID_UCHAR;
			m_mipmapTiles[tid].fid = 0;

			if (m_mipmapTiles[tid].pFormatCtx != NULL)
			{
				avcodec_close(m_mipmapTiles[tid].pCodecCtx);
				avformat_close_input(&(m_mipmapTiles[tid].pFormatCtx));
				avformat_free_context(m_mipmapTiles[tid].pFormatCtx);
				m_mipmapTiles[tid].pFormatCtx = NULL;
				m_mipmapTiles[tid].pCodecCtx = NULL;
			}

			m_mipmapSequence[idMin].id = -1;
			m_loadID[threadId] = idMin;
			m_loadNum = m_preloadTileMax;
			isOK = true;
		}
	}

	return isOK;
}

void VideoMipmap::LoadSequence()
{
	// load at most MAX_NUM sequences
	if (m_loadNum > m_preloadTileMax || m_loadNum >= m_tileNum)
	{
		return;
	}

	m_stopFlag = false;
	omp_set_num_threads(m_preloadThreadMax); // Use 4 threads for all consecutive parallel regions
	int* tileIds  = new int[m_preloadThreadMax];
	bool* toLoads = new bool[m_preloadThreadMax];
	for(int t = 0; t < m_preloadThreadMax; ++t)
	{
		tileIds[t] = -1;
		toLoads[t] = false;
	}

	float priorityMin = 99999.f;
	g_mutex.lock();

	for (int thid = 0; thid < m_preloadThreadMax; ++thid)
	{
		if (!m_mipmapQueue.empty())
		{
			tileIds[thid] = m_mipmapQueue.top().id;
			m_mipmapQueue.pop();
			m_mipmapQueueUpdate = false;
			if (m_mipmapTiles[tileIds[thid]].priority < priorityMin)
			{
				priorityMin = m_mipmapTiles[tileIds[thid]].priority;
			}
		}
	}

	g_mutex.unlock();

	for (int thid = 0; thid < m_preloadThreadMax; ++thid)
	{
		if (tileIds[thid] >= 0 && m_mipmapTiles[tileIds[thid]].loadid == INVALID_UCHAR)
		{
			toLoads[thid] = UpdateLoadedID(thid, priorityMin);
		}
	}

#pragma omp parallel for
	for (int thid = 0; thid < m_preloadThreadMax; ++thid)
	{
		if (tileIds[thid] >= 0)
		{
			Tile* tileNode = &m_mipmapTiles[tileIds[thid]];
			if (tileNode->loadid == INVALID_UCHAR)
			{
				// update index in the loaded sequence
				if (toLoads[thid])
				{
					tileNode->loadid = m_loadID[thid];
					LoadTileVideoByFFMPEG(tileNode, thid);
				}
			}
			else if (tileNode->fid < m_frameNum) // not finished
			{
				m_loadID[thid] = tileNode->loadid;
				LoadTileVideoByFFMPEG(tileNode, thid);
			}
		}
	}

	delete[] tileIds;
	delete[] toLoads;
}

static void SaveFrame(uint8_t* data, int width, int height, int strides, int i)
{
	cv::Mat img = cv::Mat::zeros(height, width, CV_8UC3);
	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			img.at<cv::Vec3b>(y, x) = cv::Vec3b(data[y * strides + x], data[y * strides + x], data[y * strides + x]);
		}
	}
	string fname = to_string(i) + ".png";
	cv::imwrite(fname.c_str(), img);
}

void VideoMipmap::InitFFMPEG()
{
	// load the video clips of the first level
	double startTime = (double)clock();

	av_register_all();
	
	AVFrame *pFrame = av_frame_alloc();
	AVPacket packet;
	int frameFinished = 0;

	int vNum = m_mipmapLevel[0].rows * m_mipmapLevel[0].cols;
	for (int v = 0; v < vNum; ++v)
	{
		Tile& tile = m_mipmapTiles[v];
		tile.pFormatCtx = avformat_alloc_context();
		string vName = m_mipmapDir + tile.preName + VIDEO_EXT;
		avformat_open_input(&tile.pFormatCtx, vName.c_str(), NULL, NULL);

		tile.pCodecCtx = (*(tile.pFormatCtx)->streams)->codec;

		// initialize once
		if (v == 0)
		{
			m_pCodec = avcodec_find_decoder(tile.pCodecCtx->codec_id);
			if (m_pCodec == NULL)
			{
				std::printf("Error: Can't find codec.\n");
			}
			m_pOptions = NULL;
			av_dict_set(&m_pOptions, "b", "2.5M", 0);
		}
		
		avcodec_open2(tile.pCodecCtx, m_pCodec, &m_pOptions);

		m_mipmapSequence[v].id = v;

		if (v == 0)
		{
			m_lineStride = m_tileWidth % ALIGNED_BYTE ? (m_tileWidth / ALIGNED_BYTE + 1) * ALIGNED_BYTE : m_tileWidth;
			m_byteNum = m_lineStride * m_tileHeight * 3 / 2;
		}

		int cnt = 0;
		for (int t = 0; t < FRAME_OFFSET; ++t)
		{
			if (av_read_frame(tile.pFormatCtx, &packet) >= 0)
			{
				avcodec_decode_video2(tile.pCodecCtx, pFrame, &frameFinished, &packet);
				av_free_packet(&packet);
			}
		}

		if (m_mipmapSequence[v].seq.size() == 0)
		{
			m_mipmapSequence[v].seq.resize(m_frameNum);
			for (int f = 0; f < m_frameNum; ++f)
			{
				m_mipmapSequence[v].seq[f] = (uchar*)malloc(m_byteNum * sizeof(uchar));
			}
		}

		while (av_read_frame(tile.pFormatCtx, &packet) >= 0 && cnt < m_frameNum)
		{
			avcodec_decode_video2(tile.pCodecCtx, pFrame, &frameFinished, &packet);

			if (frameFinished)
			{
				memcpy(m_mipmapSequence[v].seq[cnt], pFrame->data[0], m_lineStride * m_tileHeight);
				memcpy(&(m_mipmapSequence[v].seq[cnt][m_lineStride * m_tileHeight]), pFrame->data[1], m_lineStride * m_tileHeight / 4);
				memcpy(&(m_mipmapSequence[v].seq[cnt][m_lineStride * m_tileHeight + m_lineStride * m_tileHeight / 4]), pFrame->data[2], m_lineStride * m_tileHeight / 4);
			}

			av_free_packet(&packet);

			cnt++;
		}

		assert(cnt == m_frameNum);
		
		avcodec_close(tile.pCodecCtx);
		avformat_close_input(&(tile.pFormatCtx));
		avformat_free_context(tile.pFormatCtx);

		tile.fid = m_frameNum;
		tile.pFormatCtx = NULL;
		tile.pCodecCtx = NULL;
	}
	
	av_frame_free(&pFrame);
	double endTime = (double)clock();
	printf("Decode Time: %.02lf\n", endTime - startTime);
}

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
	FILE *pFile;
	char szFilename[32];
	int  y;

	// Open file
	sprintf(szFilename, "frame%d.ppm", iFrame);
	pFile = fopen(szFilename, "wb");
	if (pFile == NULL)
		return;

	// Write header
	fprintf(pFile, "P6\n%d %d\n255\n", width, height);

	// Write pixel data
	for (y = 0; y < height; y++)
		fwrite(pFrame->data[0] + y*pFrame->linesize[0], 1, width * 3, pFile);

	// Close file
	fclose(pFile);
}

void VideoMipmap::LoadVideoByFFMPEG(const char* fileName, vector<unsigned char*>& videoImgs, int& stride)
{
	AVFormatContext   *pFormatCtx = NULL;

	// Register all formats and codecs
	//av_register_all();

	// Open video file
	if (avformat_open_input(&pFormatCtx, fileName, NULL, NULL) != 0)
		return; // Couldn't open file

	// Retrieve stream information
	if (avformat_find_stream_info(pFormatCtx, NULL)<0)
		return; // Couldn't find stream information

	// Dump information about file onto standard error
	//av_dump_format(pFormatCtx, 0, fileName, 0);

	// Find the first video stream
	int videoStream = -1;
	for (int i = 0; i < pFormatCtx->nb_streams; i++)
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			videoStream = i;
			break;
		}
	if (videoStream == -1)
		return; // Didn't find a video stream

	// Get a pointer to the codec context for the video stream
	AVCodecContext* pCodecCtxOrig = pFormatCtx->streams[videoStream]->codec;
	// Find the decoder for the video stream
	AVCodec* pCodec = avcodec_find_decoder(pCodecCtxOrig->codec_id);
	if (pCodec == NULL)
	{
		fprintf(stderr, "Unsupported codec!\n");
		return; // Codec not found
	}
	// Copy context
	AVCodecContext* pCodecCtx = avcodec_alloc_context3(pCodec);
	if (avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0)
	{
		fprintf(stderr, "Couldn't copy codec context");
		return; // Error copying codec context
	}

	// Open codec
	if (avcodec_open2(pCodecCtx, pCodec, NULL)<0)
		return; // Could not open codec

	// Allocate video frame
	AVFrame* pFrame = av_frame_alloc();

	// Allocate an AVFrame structure
	AVFrame* pFrameRGB = av_frame_alloc();
	if (pFrameRGB == NULL)
		return;

	// Determine required buffer size and allocate buffer
	int numBytes = avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);
	uint8_t* buffer = (uint8_t *)av_malloc(numBytes*sizeof(uint8_t));

	// Output strides
	stride = numBytes / pCodecCtx->height;

	// Assign appropriate parts of buffer to image planes in pFrameRGB
	// Note that pFrameRGB is an AVFrame, but AVFrame is a superset
	// of AVPicture
	avpicture_fill((AVPicture *)pFrameRGB, buffer, AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);

	// initialize SWS context for software scaling
	struct SwsContext * sws_ctx = sws_getContext(pCodecCtx->width,
		pCodecCtx->height,
		pCodecCtx->pix_fmt,
		pCodecCtx->width,
		pCodecCtx->height,
		AV_PIX_FMT_RGB24,
		SWS_BILINEAR,
		NULL,
		NULL,
		NULL
		);

	// Read frames
	int numFrames = pFormatCtx->streams[videoStream]->nb_frames;
	//printf("frameNum = %d\n", numFrames);
	int f = 0;
	int frameFinished = 0;
	AVPacket packet;

	videoImgs.resize(numFrames);
	char szFilename[32];

	while (av_read_frame(pFormatCtx, &packet) >= 0)
	{
		// Decode video frame
		avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

		// Did we get a video frame?
		if (frameFinished)
		{
			//videoImgs[f] = cv::Mat::zeros(pCodecCtx->height, pCodecCtx->width, CV_8UC3);
			videoImgs[f] = (unsigned char*)malloc(numBytes);

			// Convert the image from its native format to RGB
			uint8_t *p[1] = { (uint8_t*)videoImgs[f] };
			sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
				pFrame->linesize, 0, pCodecCtx->height,
				&p[0], pFrameRGB->linesize);

			f++;
		}

		// Free the packet that was allocated by av_read_frame
		av_free_packet(&packet);
	}

	while (f < numFrames)
	{
		packet.data = NULL;
		packet.size = 0;
		avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

		// Did we get a video frame?
		if (frameFinished)
		{
			videoImgs[f] = (unsigned char*)malloc(numBytes);

			// Convert the image from its native format to RGB
			uint8_t *p[1] = { (uint8_t*)videoImgs[f] };
			sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
				pFrame->linesize, 0, pCodecCtx->height,
				&p[0], pFrameRGB->linesize);

			f++;
		}

		av_free_packet(&packet);
	}

	av_free_packet(&packet);

	// Free the RGB image
	av_free(buffer);
	av_frame_free(&pFrameRGB);

	// Free the YUV frame
	av_frame_free(&pFrame);

	// Close the codecs
	avcodec_close(pCodecCtx);

	// Close the video file
	avformat_close_input(&pFormatCtx);
}

void VideoMipmap::LoadVideoByFFMPEG(const char* fileName, vector<cv::Mat>& videoImgs)
{
	AVFormatContext   *pFormatCtx = NULL;

	// Register all formats and codecs
	//av_register_all();

	// Open video file
	if (avformat_open_input(&pFormatCtx, fileName, NULL, NULL) != 0)
		return; // Couldn't open file

	// Retrieve stream information
	if (avformat_find_stream_info(pFormatCtx, NULL)<0)
		return; // Couldn't find stream information

	// Dump information about file onto standard error
	//av_dump_format(pFormatCtx, 0, fileName, 0);

	// Find the first video stream
	int videoStream = -1;
	for (int i = 0; i < pFormatCtx->nb_streams; i++)
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) 
		{
			videoStream = i;
			break;
		}
	if (videoStream == -1)
		return; // Didn't find a video stream

	// Get a pointer to the codec context for the video stream
	AVCodecContext* pCodecCtxOrig = pFormatCtx->streams[videoStream]->codec;
	// Find the decoder for the video stream
	AVCodec* pCodec = avcodec_find_decoder(pCodecCtxOrig->codec_id);
	if (pCodec == NULL) 
	{
		fprintf(stderr, "Unsupported codec!\n");
		return; // Codec not found
	}
	// Copy context
	AVCodecContext* pCodecCtx = avcodec_alloc_context3(pCodec);
	if (avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) 
	{
		fprintf(stderr, "Couldn't copy codec context");
		return; // Error copying codec context
	}

	// Open codec
	if (avcodec_open2(pCodecCtx, pCodec, NULL)<0)
		return; // Could not open codec

	// Allocate video frame
	AVFrame* pFrame = av_frame_alloc();

	// Allocate an AVFrame structure
	AVFrame* pFrameRGB = av_frame_alloc();
	if (pFrameRGB == NULL)
		return;

	// Determine required buffer size and allocate buffer
	int numBytes = avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width,
		pCodecCtx->height);
	uint8_t* buffer = (uint8_t *)av_malloc(numBytes*sizeof(uint8_t));

	// Assign appropriate parts of buffer to image planes in pFrameRGB
	// Note that pFrameRGB is an AVFrame, but AVFrame is a superset
	// of AVPicture
	avpicture_fill((AVPicture *)pFrameRGB, buffer, AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);

	// initialize SWS context for software scaling
	struct SwsContext * sws_ctx = sws_getContext(pCodecCtx->width,
		pCodecCtx->height,
		pCodecCtx->pix_fmt,
		pCodecCtx->width,
		pCodecCtx->height,
		AV_PIX_FMT_RGB24,
		SWS_BILINEAR,
		NULL,
		NULL,
		NULL
		);

	// Read frames
	int numFrames = pFormatCtx->streams[videoStream]->nb_frames;
	//printf("frameNum = %d\n", numFrames);
	int f = 0;
	int frameFinished = 0;
	AVPacket packet;

	videoImgs.resize(numFrames);
	char szFilename[32];
	
	while (av_read_frame(pFormatCtx, &packet) >= 0)
	{
		// Decode video frame
		avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

		// Did we get a video frame?
		if (frameFinished)
		{
			videoImgs[f] = cv::Mat::zeros(pCodecCtx->height, pCodecCtx->width, CV_8UC3);

			// Convert the image from its native format to RGB
			uint8_t *p[1] = { videoImgs[f].data };
			sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
				pFrame->linesize, 0, pCodecCtx->height,
				/*pFrameRGB->data*/&p[0], pFrameRGB->linesize);

			// Save the frame to disk
			//if (frameNum <= 200)
			//sprintf(szFilename, "frame%d.jpg", f);
			//cv::imwrite(szFilename, videoImgs[f]);
			//printf("linesize = %d, linesize = %d, mat_stride = %d\n", pFrame->linesize, pFrameRGB->linesize, videoImgs[f].step);
			//SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, f);
			f++;
		}

		// Free the packet that was allocated by av_read_frame
		av_free_packet(&packet);
	}

	while (f < numFrames)
	{
		packet.data = NULL;
		packet.size = 0;
		avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

		// Did we get a video frame?
		if (frameFinished)
		{
			videoImgs[f] = cv::Mat::zeros(pCodecCtx->height, pCodecCtx->width, CV_8UC3);

			// Convert the image from its native format to RGB
			uint8_t *p[1] = { videoImgs[f].data };
			sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
				pFrame->linesize, 0, pCodecCtx->height,
				/*pFrameRGB->data*/&p[0], pFrameRGB->linesize);

			// Save the frame to disk
			//if (frameNum <= 200)
			//SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, f);
			f++;
		}
		
		av_free_packet(&packet);
	}

	av_free_packet(&packet);

	// Free the RGB image
	av_free(buffer);
	av_frame_free(&pFrameRGB);

	// Free the YUV frame
	av_frame_free(&pFrame);

	// Close the codecs
	avcodec_close(pCodecCtx);

	// Close the video file
	avformat_close_input(&pFormatCtx);
	cv::imwrite("videoImg.jpg", videoImgs[0]);
	//printf("frameNum = %d\n", f);
}

void VideoMipmap::LoadTileVideoByFFMPEG(const char* fileName, vector<cv::Mat>& videoImgs)
{
	AVFormatContext* pFormatCtx = avformat_alloc_context();
	AVPacket packet;

	avformat_open_input(&pFormatCtx, fileName, NULL, NULL);
	printf("fileName: %s, Format Ctx: %d\n", fileName, int(pFormatCtx));
	if (pFormatCtx)
	{
		AVCodecContext* pCodecCtx = (*(pFormatCtx)->streams)->codec;
		avcodec_open2(pCodecCtx, m_pCodec, &m_pOptions);

		AVFrame *pFrame = av_frame_alloc();
		int frameFinished = 0;
		for (int t = 0; t < FRAME_OFFSET; ++t)
		{			
			if (av_read_frame(pFormatCtx, &packet) >= 0)
			{
				avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
				av_free_packet(&packet);
			}
		}

		uchar* seqData = (uchar*)malloc(m_byteNum * sizeof(uchar));

		for (int f = 0; f < m_frameNum; ++f)
		{
			if (av_read_frame(pFormatCtx, &packet) >= 0)
			{
				avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

				if (frameFinished)
				{
					memcpy(seqData, pFrame->data[0], m_lineStride * m_tileHeight);
					memcpy(&(seqData[m_lineStride * m_tileHeight]), pFrame->data[1], m_lineStride * m_tileHeight / 4);
					memcpy(&(seqData[m_lineStride * m_tileHeight + m_lineStride * m_tileHeight / 4]), pFrame->data[2], m_lineStride * m_tileHeight / 4);

					convert_YUV_to_Image(seqData, videoImgs[f], m_lineStride);
				}

				av_free_packet(&packet);
			}
		}

		free(seqData);
		av_frame_free(&pFrame);
		avcodec_close(pCodecCtx);
		avformat_close_input(&pFormatCtx);
		avformat_free_context(pFormatCtx);
		pFormatCtx = NULL;
	}
}

void VideoMipmap::LoadTileVideoByFFMPEG(Tile* tile, int threadId)
{
	m_mipmapSequence[m_loadID[threadId]].id = tile->id;

	AVFrame *pFrame = av_frame_alloc();
	int frameFinished = 0;
	AVPacket packet;

	if (tile->pFormatCtx == NULL)
	{
		tile->pFormatCtx = avformat_alloc_context();
		string vName = m_mipmapDir + tile->preName + VIDEO_EXT;
		avformat_open_input(&tile->pFormatCtx, vName.c_str(), NULL, NULL);
		if (tile->pFormatCtx == NULL)
		{
			av_frame_free(&pFrame);
			return;
		}

		tile->fid = 0;
		tile->pCodecCtx = (*(tile->pFormatCtx)->streams)->codec;
		avcodec_open2(tile->pCodecCtx, m_pCodec, &m_pOptions);

		for (int t = 0; t < FRAME_OFFSET; ++t)
		{	
			if (av_read_frame(tile->pFormatCtx, &packet) >= 0)
			{
				avcodec_decode_video2(tile->pCodecCtx, pFrame, &frameFinished, &packet);
				av_free_packet(&packet);
			}
		}
	}

	for (int f = tile->fid; f < m_frameNum; ++f)
	{
		if (!m_mipmapQueueUpdate && !m_stopFlag)
		{
			if (av_read_frame(tile->pFormatCtx, &packet) >= 0)
			{
				avcodec_decode_video2(tile->pCodecCtx, pFrame, &frameFinished, &packet);

				if (frameFinished)
				{
					memcpy(m_mipmapSequence[m_loadID[threadId]].seq[f], pFrame->data[0], m_lineStride * m_tileHeight);
					memcpy(&(m_mipmapSequence[m_loadID[threadId]].seq[f][m_lineStride * m_tileHeight]), pFrame->data[1], m_lineStride * m_tileHeight / 4);
					memcpy(&(m_mipmapSequence[m_loadID[threadId]].seq[f][m_lineStride * m_tileHeight + m_lineStride * m_tileHeight / 4]), pFrame->data[2], m_lineStride * m_tileHeight / 4);
				}

				av_free_packet(&packet);
			}
		}
		else
		{
			tile->fid = f;
			av_frame_free(&pFrame);
			m_stopFlag = true;
			return;
		}
	}

	av_frame_free(&pFrame);
	avcodec_close(tile->pCodecCtx);
	avformat_close_input(&tile->pFormatCtx);
	avformat_free_context(tile->pFormatCtx);

	tile->fid = m_frameNum;
	tile->pFormatCtx = NULL;
	tile->pCodecCtx = NULL;
}

const uchar* VideoMipmap::GetFrameData(int level, int tx, int ty, int frame, int tmpid) const 
{
	int tid = m_mipmapLevel[level].tileSum + ty * m_mipmapLevel[level].cols + tx;

	int id = -1;
	for (int i = 0; i < m_preloadTileMax; ++i)
	{
		if (m_mipmapSequence[i].id == tid)
		{
			id = i;
			break;
		}
	}

	if (id >= 0 && m_mipmapTiles[tid].fid > frame)
	{
		return m_mipmapSequence[id].seq[frame];
	}

	if (m_loadRepresentativeFrame)
	{
		return m_mipmapTiles[tid].img;
	}

	return m_representiveFrame;
}

bool VideoMipmap::GetFrameData(cv::Mat& res, int level, int tx, int ty, int frame) const
{
	int tid = m_mipmapLevel[level].tileSum + ty * m_mipmapLevel[level].cols + tx;

	int id = -1;
	for (int i = 0; i < m_preloadTileMax; ++i)
	{
		if (m_mipmapSequence[i].id == tid)
		{
			id = i;
			break;
		}
	}

	if (id >= 0 && m_mipmapTiles[tid].fid > frame)
	{
		convert_YUV_to_Image(m_mipmapSequence[id].seq[frame], res, m_lineStride);
		return true;
	}

	printf("Invalid frame = %d, id = %d, fid = %d\n", frame, id, m_mipmapTiles[tid].fid);

	return false;
}

void VideoMipmap::SetFrameData(const cv::Mat& src, int level, int tx, int ty, int frame)
{
	int tid = m_mipmapLevel[level].tileSum + ty * m_mipmapLevel[level].cols + tx;

	int id = -1;
	for (int i = 0; i < m_preloadTileMax; ++i)
	{
		if (m_mipmapSequence[i].id == tid)
		{
			id = i;
			break;
		}
	}

	if (id >= 0 && m_mipmapTiles[tid].fid > frame)
	{
		convert_Image_to_YUV(src, m_mipmapSequence[id].seq[frame], m_lineStride);
	}
	else
	{
		printf("Invalid frame = %d, id = %d, fid = %d\n", frame, id, m_mipmapTiles[tid].fid);
	}
}

