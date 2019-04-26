#ifndef VIDEOMANAGER_H
#define VIDEOMANAGER_H

#include "viewerconfig.h"

extern "C"
{
#include <libavformat\avformat.h>
#include <libavcodec\avcodec.h>
#include <libavutil/avconfig.h>
#include <libavutil/avutil.h>
#include <libswscale\swscale.h>
}

#include <vector>
#include <queue>

using namespace std;
//using namespace cv;

extern uchar LEVEL_TILE_NUM[6];

static void convert_Image_to_YUV(const cv::Mat& frame, uchar* nv12v, int nv12vStrides)
{
	uchar* __restrict bufY = nv12v;

	uchar* __restrict bufU = (uchar*)&nv12v[frame.rows * nv12vStrides];

	uchar* __restrict bufV = (uchar*)&nv12v[frame.rows * nv12vStrides + frame.rows * nv12vStrides / 4];

	//assert(uintptr_t(bufU) % 4 == 0 && uintptr_t(bufY) % 4 == 0 && uintptr_t(bufV) % 4 == 0);

	int width = frame.cols;
	int height = frame.rows;
	int hwidth = width >> 1;
	int hheight = height >> 1;

	for (int y = 0; y < hheight; ++y)
	{
		for (int x = 0; x < hwidth; ++x)
		{
			int x0 = x << 1;
			int x1 = x0 + 1;
			int y0 = y << 1;
			int y1 = y0 + 1;

			cv::Vec3b c00 = frame.at<cv::Vec3b>(y0, x0);
			cv::Vec3b c01 = frame.at<cv::Vec3b>(y0, x1);
			cv::Vec3b c10 = frame.at<cv::Vec3b>(y1, x0);
			cv::Vec3b c11 = frame.at<cv::Vec3b>(y1, x1);

			bufY[y0 * nv12vStrides + x0] = max(0, min(255, (66 * (int)c00[2] + 129 * (int)c00[1] + 25 * (int)c00[0] + 128 + 16 * 256) >> 8));
			bufY[y0 * nv12vStrides + x1] = max(0, min(255, (66 * (int)c01[2] + 129 * (int)c01[1] + 25 * (int)c01[0] + 128 + 16 * 256) >> 8));
			bufY[y1 * nv12vStrides + x0] = max(0, min(255, (66 * (int)c10[2] + 129 * (int)c10[1] + 25 * (int)c10[0] + 128 + 16 * 256) >> 8));
			bufY[y1 * nv12vStrides + x1] = max(0, min(255, (66 * (int)c11[2] + 129 * (int)c11[1] + 25 * (int)c11[0] + 128 + 16 * 256) >> 8)); // OPT:to_YUV

			int r = c00[2] + c01[2] + c10[2] + c11[2];
			int g = c00[1] + c01[1] + c10[1] + c11[1];
			int b = c00[0] + c01[0] + c10[0] + c11[0];

			bufU[y * (nv12vStrides >> 1) + x] = max(0, min(255, (-38 * r - 74 * g + 112 * b + 128 * 4 + 2 * (-38 - 74 + 112) + 128 * 1024) >> 10)); // U
			
			bufV[y * (nv12vStrides >> 1) + x] = max(0, min(255, (112 * r - 94 * g - 18 * b + 128 * 4 + 2 * (112 - 94 - 18) + 128 * 1024) >> 10)); // V
		}
	}
}

static void convert_YUV_to_Image(const uchar* nv12v, cv::Mat& frame, int nv12vStrides)
{
	const uchar* __restrict bufY = nv12v;

	const uchar* __restrict bufU = (const uchar*)&nv12v[frame.rows * nv12vStrides];

	const uchar* __restrict bufV = (const uchar*)&nv12v[frame.rows * nv12vStrides + frame.rows * nv12vStrides / 4];

	int width = frame.cols;
	int height = frame.rows;
	int hwidth = width >> 1;
	int hheight = height >> 1;

	for (int y = 0; y < hheight; ++y)
	{
		for (int x = 0; x < hwidth; ++x)
		{
			int x0 = x << 1;
			int x1 = x0 + 1;
			int y0 = y << 1;
			int y1 = y0 + 1;

			int u = bufU[y * (nv12vStrides >> 1) + x];
			int v = bufV[y * (nv12vStrides >> 1) + x];

			cv::Vec3i vi0 = (cv::Vec3i(-16 * 298 + 128 - 409 * 128,

				-16 * 298 + 128 + 100 * 128 + 208 * 128,

				-16 * 298 + 128 - 516 * 128) +

				cv::Vec3i(0, -100, 516) * u +

				cv::Vec3i(409, -208, 0) * v);

			const cv::Vec3i yscale(298, 298, 298);
			int y00 = bufY[y0 * nv12vStrides + x0];
			int y01 = bufY[y0 * nv12vStrides + x1];
			int y10 = bufY[y1 * nv12vStrides + x0];
			int y11 = bufY[y1 * nv12vStrides + x1];

			frame.at<cv::Vec3b>(y0, x0) = cv::Vec3b(max(0, min(255, (vi0[2] + yscale[2] * y00) >> 8)), max(0, min(255, (vi0[1] + yscale[1] * y00) >> 8)), max(0, min(255, (vi0[0] + yscale[0] * y00) >> 8)));
			frame.at<cv::Vec3b>(y0, x1) = cv::Vec3b(max(0, min(255, (vi0[2] + yscale[2] * y01) >> 8)), max(0, min(255, (vi0[1] + yscale[1] * y01) >> 8)), max(0, min(255, (vi0[0] + yscale[0] * y01) >> 8)));
			frame.at<cv::Vec3b>(y1, x0) = cv::Vec3b(max(0, min(255, (vi0[2] + yscale[2] * y10) >> 8)), max(0, min(255, (vi0[1] + yscale[1] * y10) >> 8)), max(0, min(255, (vi0[0] + yscale[0] * y10) >> 8)));
			frame.at<cv::Vec3b>(y1, x1) = cv::Vec3b(max(0, min(255, (vi0[2] + yscale[2] * y11) >> 8)), max(0, min(255, (vi0[1] + yscale[1] * y11) >> 8)), max(0, min(255, (vi0[0] + yscale[0] * y11) >> 8)));
		}
	}
}

static void convert_YUV_to_RectImage(const uchar* nv12v, cv::Mat& frame, int nv12vStrides, int* rect)
{
	const uchar* __restrict bufY = nv12v;

	const uchar* __restrict bufU = (const uchar*)&nv12v[frame.rows * nv12vStrides];

	const uchar* __restrict bufV = (const uchar*)&nv12v[frame.rows * nv12vStrides + frame.rows * nv12vStrides / 4];

	int width = rect[2] - rect[0] + 1;
	int height = rect[3] - rect[1] + 1;

	for (int y = rect[1]; y <= rect[3]; ++y)
	{
		int dy = y >> 1;
		for (int x = rect[0]; x <= rect[2]; ++x)
		{
			int dx = x >> 1;
			int u = bufU[dy * (nv12vStrides >> 1) + dx];
			int v = bufV[dy * (nv12vStrides >> 1) + dx];

			cv::Vec3i vi0 = (cv::Vec3i(-16 * 298 + 128 - 409 * 128,
				-16 * 298 + 128 + 100 * 128 + 208 * 128,
				-16 * 298 + 128 - 516 * 128) +
				cv::Vec3i(0, -100, 516) * u +
				cv::Vec3i(409, -208, 0) * v);

			const cv::Vec3i yscale(298, 298, 298);
			int y00 = bufY[y * nv12vStrides + x];
			frame.at<cv::Vec3b>(y, x) = cv::Vec3b(max(0, min(255, (vi0[2] + yscale[2] * y00) >> 8)), max(0, min(255, (vi0[1] + yscale[1] * y00) >> 8)), max(0, min(255, (vi0[0] + yscale[0] * y00) >> 8)));
		}
	}
}

static void convert_YUV_to_RGB_color(const uchar* nv12v, cv::Vec3b& col, int nv12vStrides, int x, int y, int height)
{
	const uchar* __restrict bufY = nv12v;
	const uchar* __restrict bufU = (const uchar*)&nv12v[height * nv12vStrides];
	const uchar* __restrict bufV = (const uchar*)&nv12v[height * nv12vStrides + height * nv12vStrides / 4];
	int dy = y >> 1;
	int dx = x >> 1;
	int u = bufU[dy * (nv12vStrides >> 1) + dx];
	int v = bufV[dy * (nv12vStrides >> 1) + dx];

	cv::Vec3i vi0 = (cv::Vec3i(-16 * 298 + 128 - 409 * 128,
		-16 * 298 + 128 + 100 * 128 + 208 * 128,
		-16 * 298 + 128 - 516 * 128) +
		cv::Vec3i(0, -100, 516) * u +
		cv::Vec3i(409, -208, 0) * v);

	const cv::Vec3i yscale(298, 298, 298);
	int y00 = bufY[y * nv12vStrides + x];
	col = cv::Vec3b(max(0, min(255, (vi0[2] + yscale[2] * y00) >> 8)), max(0, min(255, (vi0[1] + yscale[1] * y00) >> 8)), max(0, min(255, (vi0[0] + yscale[0] * y00) >> 8)));
}

static void convert_RGB_to_Image(const uchar* rgb, cv::Mat& frame, int rgbStrides)
{
	int width = frame.cols;
	int height = frame.rows;

	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			int id = (y * rgbStrides + x) * 3;
			frame.at<cv::Vec3b>(y, x) = cv::Vec3b(rgb[id], rgb[id + 1], rgb[id + 2]);
		}
	}
}

static void convert_RGB_to_YUV(const uchar* rgb, uchar* nv12v, int nv12vStrides, int width, int height)
{
	uchar* __restrict bufY = nv12v;
	uchar* __restrict bufU = (uchar*)&nv12v[height * nv12vStrides];
	uchar* __restrict bufV = (uchar*)&nv12v[height * nv12vStrides + height * nv12vStrides / 4];

	int hwidth = width >> 1;
	int hheight = height >> 1;

	for (int y = 0; y < hheight; ++y)
	{
		for (int x = 0; x < hwidth; ++x)
		{
			int x0 = x << 1;
			int x1 = x0 + 1;
			int y0 = y << 1;
			int y1 = y0 + 1;

			int id00 = (y0 * width + x0) * 3;
			int id01 = (y0 * width + x1) * 3;
			int id10 = (y1 * width + x0) * 3;
			int id11 = (y1 * width + x1) * 3;

			bufY[y0 * nv12vStrides + x0] = max(0, min(255, (66 * (int)rgb[id00] + 129 * (int)rgb[id00 + 1] + 25 * (int)rgb[id00 + 2] + 128 + 16 * 256) >> 8));
			bufY[y0 * nv12vStrides + x1] = max(0, min(255, (66 * (int)rgb[id01] + 129 * (int)rgb[id01 + 1] + 25 * (int)rgb[id01 + 2] + 128 + 16 * 256) >> 8));
			bufY[y1 * nv12vStrides + x0] = max(0, min(255, (66 * (int)rgb[id10] + 129 * (int)rgb[id10 + 1] + 25 * (int)rgb[id10 + 2] + 128 + 16 * 256) >> 8));
			bufY[y1 * nv12vStrides + x1] = max(0, min(255, (66 * (int)rgb[id11] + 129 * (int)rgb[id11 + 1] + 25 * (int)rgb[id11 + 2] + 128 + 16 * 256) >> 8)); // OPT:to_YUV

			int b = rgb[id00 + 2] + rgb[id01 + 2] + rgb[id10 + 2] + rgb[id11 + 2];
			int g = rgb[id00 + 1] + rgb[id01 + 1] + rgb[id10 + 1] + rgb[id11 + 1];
			int r = rgb[id00] + rgb[id01] + rgb[id10] + rgb[id11];

			bufU[y * (nv12vStrides >> 1) + x] = max(0, min(255, (-38 * r - 74 * g + 112 * b + 128 * 4 + 2 * (-38 - 74 + 112) + 128 * 1024) >> 10)); // U

			bufV[y * (nv12vStrides >> 1) + x] = max(0, min(255, (112 * r - 94 * g - 18 * b + 128 * 4 + 2 * (112 - 94 - 18) + 128 * 1024) >> 10)); // V
		}
	}
}

// heapnode with priority
struct Tile
{
	uchar level;
	uchar x;
	uchar y;
	uchar loadid;
	float cx;
	float cy;
	float priority;
	AVFormatContext* pFormatCtx;
	string preName;
	wchar_t vname[260];
	AVCodecContext* pCodecCtx;
	uchar* img;
	int id;
	int fid; // start index of sequence to be loaded

	Tile()
	{
		level = INVALID_UCHAR;
		x = INVALID_UCHAR;
		y = INVALID_UCHAR;
		loadid = INVALID_UCHAR;
		cx = 0.f;
		cy = 0.f;
		priority = 0.f;
		id = -1;
		fid = 0;
		pFormatCtx = NULL;
	}

	Tile(uchar tl, uchar tx, uchar ty, uchar tloaded, uchar trow, uchar tcol, float tp, int tid)
	{
		level = tl;
		x = tx;
		y = ty;
		cx = (tx - (tcol - 1) * 0.5f);
		cy = (ty - (trow - 1) * 0.5f);
		loadid = tloaded;
		priority = tp;
		pFormatCtx = NULL;
		id = tid;
		fid = 0;
		pFormatCtx = NULL;
		preName = "\\mipmap_" + to_string(tl) + "_" + to_string(ty) + "_" + to_string(tx);
	}
};

struct Sequence
{
	int id;
	vector<uchar*> seq;
	Sequence()
	{
		id = -1;
	}
};

struct Priority
{
	int id;
	float priority;

	Priority(int i, float p)
	{
		id = i;
		priority = p;
	}
};

struct Cmp
{
	bool operator() (const Tile* a, const Tile* b)
	{
		float d = a->priority - b->priority;
		if (d < 0.f) return true;
		if (d > 0.f) return false;
		return a->level > b->level;
	}
};

struct PCmp
{
	bool operator() (const Priority& a, const Priority& b)
	{
		float d = a.priority - b.priority;
		if (d < 0.f) return true;
		if (d > 0.f) return false;
		return a.id > b.id;
	}
};

struct MipmapLevel
{
	int rows;
	int cols;
	int tileSum;
};

struct PSV{
	int id;
	uchar p, s;
	PSV()
	{
		id = -1;
		p = 1;
		s = 0;
	}
	PSV(int psvId, int psvP, int psvS)
	{
		id = psvId;
		p = psvP;
		s = psvS;
	}
	int pack() const { return (p << 24) | (s << 16) | id;}
	void unpack(int val) { p = val >> 24;  s = ((val << 8) >> 24); id = ((val << 16) >> 16); }
};

struct CmpByValue 
{
	bool operator()(const pair<int, int> & lhs, const pair<int, int> & rhs)
	{
		return lhs.second > rhs.second;
	}
};

class VideoMipmap
{
public:

	VideoMipmap(const ViewerConfig& config);
	~VideoMipmap();

	void startLoaderThread();
	void stopLoaderThread();

	void InitLoading();
	void InitMipmapTiles();
	void InitMipmapLevels();
	void InitFFMPEG();
	void UpdateMipmapQueue(float centerX, float centerY, float centerZ, int curLevel);
	void LoadSequence();

	void LoadVideoByFFMPEG(const char* fileName, vector<unsigned char*>& videoImgs, int& stride);
	void LoadVideoByFFMPEG(const char* fileName, vector<cv::Mat>& videoImgs);
	void LoadTileVideoByFFMPEG(Tile* tile, int threadId);
	void LoadTileVideoByFFMPEG(const char* fileName, vector<cv::Mat>& videoImgs);
	
	int GetWidth()    const { return m_width; }
	int GetHeight()   const { return m_height; }
	int GetFrameNum() const { return m_frameNum; }
	int GetLevelNum() const { return m_mipmapLevelMax; }
	int GetLineStride() const { return m_lineStride;  }
	int GetTileWidth()  const { return m_tileWidth; }
	int GetTileHeight() const { return m_tileHeight; }
	int GetLevelRows(int l) const { return m_mipmapLevel[l].rows;   }
	int GetLevelCols(int l) const { return m_mipmapLevel[l].cols;   }

	const uchar*       GetLoadingImage() const { return m_loadingImage; } 
	const uchar*       GetFrameData(int level, int tx, int ty, int frame, int tmpid) const;
	bool               GetFrameData(cv::Mat& res, int level, int tx, int ty, int frame) const;
	void               SetFrameData(const cv::Mat& src, int level, int tx, int ty, int frame);
	const string&      GetMipmapStr() const{ return m_mipmapDir;  }  

private:
	bool UpdateLoadedID(int threadId, float priority);

	const ViewerConfig& m_config;
	string m_mipmapDir;
	bool   m_loadRepresentativeFrame;
	int    m_preloadTileMax;
	int    m_preloadThreadMax;
	int    m_mipmapLevelMax;
	int    m_tileNum;
	int    m_frameNum;
	int    m_loadNum;
	int    m_lineStride;
	int    m_byteNum;
	uchar* m_loadingImage;
	uchar* m_representiveFrame;
	int    m_width, m_height;
	int    m_tileWidth, m_tileHeight;
	int    m_panoramaRows, m_panoramaCols;
	bool   m_mipmapQueueUpdate, m_stopFlag;

	int*                                             m_loadID;
	Sequence*                                        m_mipmapSequence;
	priority_queue<Priority, vector<Priority>, PCmp> m_mipmapQueue;
	vector<Tile>                                     m_mipmapTiles;
	MipmapLevel                                      m_mipmapLevel[MAX_LEVEL_NUM];
	AVCodec                                         *m_pCodec;
	AVDictionary                                    *m_pOptions;
	vector<PSV>                                      m_psvList;
	vector<vector<short>>                            m_startCoords;
	vector<vector<short>>                            m_rectCoords;
	vector<string>                                   m_videoNames;
	vector<unsigned char*>                           m_videoRes, m_videoSrc;
};

#endif