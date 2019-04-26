#ifndef VIDEOCONFIG_H
#define VIDEOCONFIG_H

#include "opencv\cv.h"
#include "opencv2\core\core.hpp"
#include "opencv2\core\mat.hpp"
#include "opencv2\highgui\highgui.hpp"

#define IS_DEBUG_MODE 0

#define MAX_LEVEL_NUM 6
#define FRAME_RATE 25
#define VIDEO_EXT ".mp4"
#define IMG_EXT ".jpg"
#define INVALID_UCHAR 255
#define TILE_SIZE 1024
#define LOADING_IMG_NAME "\\loading.jpg"

//#define VIDEO_EXT_TYPE vt::NV12
#define IMAGE_TYPE vt::CNV12VideoImg

#define ALIGNED_BYTE 64
#define ALIGNED_OFFSET 3
#define PLAY_INTERVAL  34

#define FRAME_OFFSET 10 // the first FRAME_OFFSET frames decompressed cannot be used

#define DYNAMIC_WEIGHT 0.2f
#define UI_UPDATE_TIME 17
#define UI_UPDATE_SCALE_UNIT 0.005f
#define UI_UPDATE_MOVE_UNIT 2.f

#define MAX_FRAME_NUM 200

struct ViewerConfig {
public:

	// Configuration for the run. These are initially setup by the calling app, but may be
	// revised at the beginning of optimization to make them consistent with the video being loaded
	// and each other. 
	ViewerConfig(){
		m_para_mipmapDir = "";  m_para_loadRF = true;  m_para_loadLevels = 6; m_para_preloadMemory = 4.f;
		m_para_preloadThreadNum = 4; m_para_windWidth = 1280; m_para_windHeight = 720; m_para_scaleMin = 0.75; m_para_scaleMax = 32.00;
	}

	// Global parameters
	std::string m_para_mipmapDir;
	int    m_para_loadRF;
	int    m_para_loadLevels;
	double m_para_scaleMin;
	double m_para_scaleMax;
	float  m_para_preloadMemory;
	int    m_para_preloadThreadNum;
	int    m_para_windWidth;
	int    m_para_windHeight;

	const std::string& getMipmapDir() const { return m_para_mipmapDir;        }
	bool getLoadRF() const                  { return m_para_loadRF;           }
	int  getLoadLevels() const              { return m_para_loadLevels;       }
	int  getPreloadMemory() const           { return m_para_preloadMemory;    }
	int  getPreloadThreadNum() const        { return m_para_preloadThreadNum; }
	int  getWindWidth() const               { return m_para_windWidth;        }
	int  getWindHeight() const              { return m_para_windHeight;       }
	double getScaleMin() const              { return m_para_scaleMin;         }
	double getScaleMax() const              { return m_para_scaleMax;         }
};

#endif