#ifndef _H264_CODEC_H_
#define _H264_CODEC_H_

//#ifndef int64_t
//typedef __int64 int64_t;
//#endif

// The x264 libs
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swscale.lib")
//#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "libx264.dll.a")

#ifdef _MSC_VER	/* MSVC */
#pragma warning(disable: 4996)
#define snprintf	_snprintf
#define vsnprintf	_vsnprintf
#define strcasecmp	stricmp
#define strncasecmp strnicmp
#endif

#include <stdint.h>

extern "C"
{
#include "x264.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#include "libswscale/swscale.h"
};

#include <list>
using namespace  std;

typedef list<int>	TimeStampList;

#define FRAME_TYPE_MAX	7

//#define PIC_WIDTH	320
//#define PIC_HEIGHT	240
//
//#define PIC_MID_WIDTH 400
//#define PIC_MID_HEIGHT 300
//
//#define PIC_MAX_WIDTH 640
//#define PIC_MAX_HEIGHT 480
//
//#define PIC_MAX_EN_WIDTH 640
//#define PIC_MAX_EN_HEIGHT 480

#define PIC_WIDTH_160	160
#define PIC_HEIGHT_120	120

#define PIC_WIDTH_320	320
#define PIC_HEIGHT_240	240

#define PIC_WIDTH_480	480
#define PIC_HEIGHT_360	360

#define PIC_WIDTH_640	640
#define PIC_HEIGHT_480	480

#define PIC_WIDTH_960	960
#define PIC_HEIGHT_640	640

#define PIC_WIDTH_1280	1280
#define PIC_HEIGHT_720	720

#define PIC_WIDTH_1920	1920
#define PIC_HEIGHT_1080 1080

//#define PIC_MAX_EN_WIDTH 1920
//#define PIC_MAX_EN_HEIGHT 1080

#define DEFAULT_Q	30

enum Resolution
{
	VIDEO_120P,
	VIDEO_240P,
	VIDEO_360P,
	VIDEO_480P,
	VIDEO_640P,
	VIDEO_720P,
	VIDEO_1080P,
};

class H264Encoder
{
public:
	H264Encoder();
	~H264Encoder();

	bool Init(int nFrame, int iSrcWidth, int iSrcHeight, int iDestWidth, int iDestHeight, int iQP = DEFAULT_Q);

	bool COConnect();
	void CORelease();
	bool EncodeVideoData(uint8_t *pInData, int iLen, enum AVPixelFormat src_pix_fmt, uint8_t *pOutBuf, int *iOutLen, int *nKeyFrame);

	bool UpQP(uint32_t rate);
	bool DownQP(uint32_t rate);

	void ResetQP(int iQP);
	void ResetCodec();

	unsigned int GetWidth()  const { return m_nWidth; }
	unsigned int GetHeight() const { return m_nHeight; }
	unsigned int GetEnWidth() const {return m_h264_codec_width;};
	unsigned int GetEnHeight() const {return m_h264_codec_height;};
	uint8_t* GetBuffer() const { return m_pBuf; }

	int	 GetBandwidth() const {return m_nBandwidthPre;};

protected:
	void PolicyQ(int nEncodeSize);
	void ConfigParam(bool faster);
	void ResetVideo();

	int GetResolution() const {return m_nResolution;};

protected:
	uint8_t*	m_pBuf;

	unsigned int	m_h264_codec_width;		// Output Width
	unsigned int	m_h264_codec_height;	// Output Height

	unsigned int	m_nWidth;				// Input Width
	unsigned int	m_nHeight;				// Input Height

	SwsContext*		m_swsContext, *m_swsYuvContext;

	//X264对象
	x264_picture_t	m_pic_out;
	x264_picture_t	m_en_picture;
	x264_t *		m_en_h;
	x264_param_t	m_en_param;
	unsigned int	m_en_nQP;
	int				m_nQIndex;
	//帧统计
	unsigned int	m_arrFrameCount[FRAME_TYPE_MAX];	

	//质量调整参数
	int				m_nFrame;
	int				m_nBandPos;
	int*			m_pBandwidth;
	int				m_nBandwidthPre;
	bool			m_bFater;

	int				m_qListIndex;
	int				m_nResolution;
};

class H264Decoder
{
public:
	H264Decoder();
	~H264Decoder();

	bool Init(/*int iSrcWidth, int iSrcHeight, int iDestWidth, int iDestHeight, */int iQP = DEFAULT_Q);

	bool DeConnect();
	void DeRelease();
	bool DecodeVideoData(uint8_t *pInData, int iLen, uint8_t **ppOutBuf, int &out_width, int& out_height, int32_t &pict_type);

	unsigned int GetWidth() { return m_nCurrWidth; }
	unsigned int GetHeight() { return m_nCurrHeight; }

	/*unsigned int GetDeWidth() { return m_h264_codec_width; }
	unsigned int GetDeHeight() { return m_h264_codec_height; }*/

protected:
	//unsigned int		m_nWidth; // Input Width (src)
	//unsigned int		m_nHeight; // Input Height

	//unsigned int		m_h264_codec_width; // Output Width
	//unsigned int		m_h264_codec_height; // Output Height

	unsigned int        m_nCurrWidth;
	unsigned int        m_nCurrHeight;

	uint8_t*            m_pDecodeData;

	AVFrame*			m_de_frame;
	AVCodec*			m_de_codec;
	AVCodecContext*		m_de_context;

	AVPacket			m_avpkt;
	AVPicture			m_outpic;

	SwsContext*			m_swsContext;
};

#endif
