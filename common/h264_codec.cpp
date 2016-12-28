#ifdef WIN32
#include <Windows.h>
#include <TCHAR.H>
#endif

#include <assert.h>
#include <stdio.h>
#include "h264_codec.h"

#define TOP_BANDWIDTH		(22 * 1024)
#define BOTTOM_BANDWIDTH	(12 * 1024)
#define POLIY_Q_TIME		10

//视频质量参照表 
#define QARRAY_SIZE			5
const static unsigned int QARRAY[QARRAY_SIZE] = {25, 28, 30, 33, 38};

typedef struct stVideoQInfo
{
	int min_q;
	int max_q;
	int const_q;

	int	bitrate;
	int max_bitrate;
} VideoQInfo;

//定义一个调节参数列表
#define QLISTSIZE 4
const VideoQInfo VIDEOQLIST[QLISTSIZE] = {
	{5, 36, 18, 112, 136},
	{10, 36, 22, 96, 120},
	{15, 36, 26, 72, 88},
	{20, 38, 28, 56, 72}
};

#ifdef WIN32
//获取CPU个数
typedef void (WINAPI *PGNSI)(LPSYSTEM_INFO);
DWORD GetNumberOfProcessors()
{
	SYSTEM_INFO si;
	PGNSI pfnGNSI = (PGNSI) GetProcAddress(GetModuleHandle(_T("kernel32.dll")), "GetNativeSystemInfo");
	if(pfnGNSI)
		pfnGNSI(&si);
	else
		GetSystemInfo(&si);

	return si.dwNumberOfProcessors;
}
#else
int GetNumberOfProcessors()
{
	return 2;
}
#endif

H264Encoder::H264Encoder()
{
	m_pBuf					= NULL;
	m_pBandwidth			= NULL;

	m_nWidth				= PIC_WIDTH_640;
	m_nHeight				= PIC_HEIGHT_480;

	m_h264_codec_width		= PIC_WIDTH_640;
	m_h264_codec_height		= PIC_HEIGHT_480;

	m_swsContext			= NULL;
	m_swsYuvContext			= NULL;

	m_en_h					= NULL;
	m_nQIndex				= 3;
	m_bFater				= false;
	m_nBandwidthPre			= 0;

	memset(&m_en_picture, 0, sizeof(m_en_picture));
	memset(&m_en_param, 0, sizeof(m_en_param));

	memset(m_arrFrameCount, 0x00, sizeof(m_arrFrameCount));

	m_qListIndex = 0;

	m_nResolution = VIDEO_240P;
}

H264Encoder::~H264Encoder()
{
	CORelease();

	if(m_pBandwidth != NULL){
		delete []m_pBandwidth;
		m_pBandwidth = NULL;
	}

	if(m_pBuf != NULL){
		delete []m_pBuf;
		m_pBuf = NULL;
	}
}

bool H264Encoder::Init(int nFrame, int iSrcWidth, int iSrcHeight, int iDestWidth, int iDestHeight, int iQP)
{
	m_nWidth = iSrcWidth;
	m_nHeight = iSrcHeight;

	m_h264_codec_width = iDestWidth;
	m_h264_codec_height = iDestHeight;

	if (iDestWidth == PIC_WIDTH_1920 && iDestHeight == PIC_HEIGHT_1080)
		m_nResolution = VIDEO_1080P;
	else if (iDestWidth == PIC_WIDTH_1280 && iDestHeight == PIC_HEIGHT_720)
		m_nResolution = VIDEO_720P;
	else if (iDestWidth == PIC_WIDTH_960 && iDestHeight == PIC_HEIGHT_640)
		m_nResolution = VIDEO_640P;
	else if (iDestWidth == PIC_WIDTH_640 && iDestHeight == PIC_HEIGHT_480)
		m_nResolution = VIDEO_480P;
	else if (iDestWidth == PIC_WIDTH_480 && iDestHeight == PIC_HEIGHT_360)
		m_nResolution = VIDEO_360P;
	else if (iDestWidth == PIC_WIDTH_320 && iDestHeight == PIC_HEIGHT_240)
		m_nResolution = VIDEO_240P;
	else if (iDestWidth == PIC_WIDTH_160 && iDestHeight == PIC_HEIGHT_120)
		m_nResolution = VIDEO_120P;
	else
		return false;

	m_nQIndex		= 3;

	m_nFrame		= nFrame;
	m_nBandPos		= 0;
	m_nBandwidthPre	= 0;

	m_pBuf			= new uint8_t[m_h264_codec_width * m_h264_codec_height * 3];
	m_pBandwidth	= new int[m_nFrame * POLIY_Q_TIME];

	m_qListIndex	= 0;

	return COConnect();
}


bool H264Encoder::COConnect()
{
	bool faster = true;
	int CPUNum = GetNumberOfProcessors();
	if(CPUNum >= 4){
		faster		= false;
		m_nQIndex	= 2;
	}

	m_bFater = faster;

	x264_param_default(&m_en_param);

	if (x264_param_default_preset(&m_en_param, "medium", "zerolatency") != 0)//medium,veryslow
		return false;

	//设置编码器参数
	ConfigParam(faster);

	//构造编码器
	if (x264_param_apply_profile(&m_en_param, "main") != 0)
		return false;

	if ((m_en_h = x264_encoder_open(&m_en_param)) == NULL)
		return false;

	if (x264_picture_alloc(&m_en_picture, X264_CSP_I420, m_h264_codec_width, m_h264_codec_height) != 0)
		return false;

	//构造颜色空间转换器
	m_swsContext = sws_getContext(m_nWidth, m_nHeight, 
		PIX_FMT_BGR24, m_h264_codec_width, m_h264_codec_height, 
		PIX_FMT_YUV420P, SWS_FAST_BILINEAR, NULL, NULL, NULL);

	return true;
}

void H264Encoder::CORelease()
{
	if (m_en_h != NULL){
		x264_picture_clean(&m_en_picture);
		x264_encoder_close(m_en_h);

		sws_freeContext(m_swsContext);
		m_swsContext = NULL;
		sws_freeContext(m_swsYuvContext);
		m_swsYuvContext = NULL;

		m_en_h = NULL;
	}

	if(m_pBandwidth != NULL) {
		delete []m_pBandwidth;
		m_pBandwidth = NULL;
	}

	if (m_pBuf != NULL){
		delete [] m_pBuf;
		m_pBuf = NULL;
	}

	m_nBandwidthPre = 0;

	m_nResolution = VIDEO_240P;
}

bool H264Encoder::EncodeVideoData(uint8_t *pInData, int iLen, enum AVPixelFormat src_pix_fmt, uint8_t *pOutBuf, int *iOutLen, int *nKeyFrame)
{
	if(m_en_h == NULL || m_swsContext == NULL)
		return false;

	int ret;
	static AVPicture pic = { 0 };
	uint8_t *pic_data[4];

	m_en_param.i_frame_total++;

	if (src_pix_fmt == AV_PIX_FMT_RGB24 || src_pix_fmt == AV_PIX_FMT_BGR24){
		//RGB -> YUV
		int srcStride = m_nWidth * 3;
		ret = sws_scale(m_swsContext, &pInData, &srcStride, 0, m_nHeight,
			m_en_picture.img.plane, m_en_picture.img.i_stride);
	}
	else if (src_pix_fmt == AV_PIX_FMT_YUV420P){
		ret = avpicture_fill(&pic, pInData, PIX_FMT_YUV420P, m_nWidth, m_nHeight);

		if (m_nWidth != m_h264_codec_width || m_nHeight != m_h264_codec_height){
			if (m_swsYuvContext == NULL){
				//构造颜色空间转换器
				m_swsYuvContext = sws_getContext(m_nWidth, m_nHeight,
					src_pix_fmt, m_h264_codec_width, m_h264_codec_height,
					src_pix_fmt, SWS_FAST_BILINEAR, NULL, NULL, NULL);
			}

			int srcStride[3];
			srcStride[0] = m_nWidth;
			srcStride[1] = m_nWidth / 2;
			srcStride[2] = m_nWidth / 2;

			ret = sws_scale(m_swsContext, &pInData, srcStride, 0, m_nHeight,
				m_en_picture.img.plane, m_en_picture.img.i_stride);
		}


		pic_data[0] = m_en_picture.img.plane[0];
		pic_data[1] = m_en_picture.img.plane[1];
		pic_data[2] = m_en_picture.img.plane[2];
		pic_data[3] = m_en_picture.img.plane[3];

		pic.linesize[0] = m_en_picture.img.i_stride[0];
		pic.linesize[1] = m_en_picture.img.i_stride[1];
		pic.linesize[2] = m_en_picture.img.i_stride[2];
		pic.linesize[3] = m_en_picture.img.i_stride[3];

		m_en_picture.img.plane[0] = pic.data[0];
		m_en_picture.img.plane[1] = pic.data[1];
		m_en_picture.img.plane[2] = pic.data[2];
		m_en_picture.img.plane[3] = pic.data[3];
	}
	else{
		// TODO:
	}

	m_en_picture.i_pts = (int64_t)m_en_param.i_frame_total * m_en_param.i_fps_den;

	x264_nal_t *nal = NULL;
	int i_nal		= 0;

	//X.264 编码
	ret = x264_encoder_encode(m_en_h, &nal, &i_nal, &m_en_picture, &m_pic_out);

	if (src_pix_fmt == AV_PIX_FMT_YUV420P){
		m_en_picture.img.plane[0] = pic_data[0];
		m_en_picture.img.plane[1] = pic_data[1];
		m_en_picture.img.plane[2] = pic_data[2];
		m_en_picture.img.plane[3] = pic_data[3];
	}

	if (ret > 0){
		*iOutLen = ret;
		memcpy(pOutBuf, nal[0].p_payload, ret);

		//*nKeyFrame = (m_pic_out.i_type == X264_TYPE_IDR ? true : false);
		*nKeyFrame = m_pic_out.i_type;
		//帧信息计数
		if(m_pic_out.i_type < FRAME_TYPE_MAX)
			m_arrFrameCount[m_pic_out.i_type] ++;

		PolicyQ(ret);
		return true;
	}

	return false;
}

void H264Encoder::PolicyQ(int nEncodeSize)
{
	/*if(m_nResolution != VIDEO_320P)
		return ;*/

	int index = m_nBandPos % (m_nFrame * POLIY_Q_TIME);
	m_pBandwidth[index] = nEncodeSize;
	m_nBandPos ++;
	
	int nTotalBandwidth = 0;
	if(m_nBandPos < m_nFrame * POLIY_Q_TIME){
		for(int i = 0; i < m_nBandPos; i ++)
			nTotalBandwidth += m_pBandwidth[i];

		m_nBandwidthPre = nTotalBandwidth * m_nFrame / m_nBandPos;
	}
	else{
		for(int i = 0; i < m_nFrame * POLIY_Q_TIME; i ++)
			nTotalBandwidth += m_pBandwidth[i];

		m_nBandwidthPre = nTotalBandwidth / POLIY_Q_TIME;
	}
}

bool H264Encoder::UpQP(uint32_t rate)
{
	if (m_nResolution != VIDEO_240P)
		return false;

	if(rate > 136)
		rate = 136;

	if(rate > m_en_param.rc.i_vbv_max_bitrate + 8){
		m_en_param.rc.i_vbv_max_bitrate = rate;
		m_en_param.rc.i_bitrate = rate - 24;

		ResetVideo();
	}

	return true;
}

bool H264Encoder::DownQP(uint32_t rate)
{
	if (m_nResolution != VIDEO_240P)
		return false;

	if(rate < 56)
		rate = 56;

	if(rate < m_en_param.rc.i_vbv_max_bitrate){
		m_en_param.rc.i_vbv_max_bitrate = rate;
		m_en_param.rc.i_bitrate = rate - 16;

		ResetVideo();
	}

	return true;
}

void H264Encoder::ResetCodec()
{
	if(m_en_h != NULL){
		x264_encoder_close(m_en_h);
		m_en_h = x264_encoder_open(&m_en_param);
	}
}

void H264Encoder::ResetVideo()
{
	x264_encoder_close(m_en_h);

	m_nQIndex	= 2;

	//重新设置参数,降低质量
	m_en_param.rc.i_qp_max			= VIDEOQLIST[m_qListIndex].max_q;
	m_en_param.rc.i_qp_min			= VIDEOQLIST[m_qListIndex].min_q;
	m_en_param.rc.i_bitrate			= VIDEOQLIST[m_qListIndex].bitrate;
	m_en_param.rc.i_qp_constant		= VIDEOQLIST[m_qListIndex].const_q;
	m_en_param.rc.i_vbv_max_bitrate = VIDEOQLIST[m_qListIndex].max_bitrate;

	m_en_h = x264_encoder_open(&m_en_param);
}

void H264Encoder::ResetQP(int iQP)
{
	//清除带宽标志
	for(int i = 0; i < m_nFrame * POLIY_Q_TIME; i++)
		m_pBandwidth[i] = 0;

	//复位
	m_nBandPos = 0;

	x264_encoder_close(m_en_h);
	m_en_param.rc.i_qp_max = iQP;	// TODO: 判断大小
	m_en_h = x264_encoder_open(&m_en_param);
}

void H264Encoder::ConfigParam(bool faster)
{
	m_en_param.i_threads		= X264_THREADS_AUTO;
	m_en_param.i_width			= m_h264_codec_width;
	m_en_param.i_height			= m_h264_codec_height;

	m_en_param.i_fps_num		= m_nFrame;
	m_en_param.i_fps_den		= 1;
	m_en_param.rc.i_qp_step		= 4;

	m_en_param.i_log_level		= X264_LOG_NONE;
	m_en_param.rc.i_rc_method	= X264_RC_CRF;


	switch (m_nResolution){
	case VIDEO_1080P:
		m_en_param.rc.i_qp_min = 5;
		m_en_param.rc.i_qp_max = 40;
		m_en_param.rc.i_qp_constant = 24;
		m_en_param.rc.i_bitrate = 1600;
		m_en_param.rc.i_vbv_max_bitrate = 3200;
		m_en_param.i_keyint_min = m_nFrame * 6;
		m_en_param.i_keyint_max = m_nFrame * 6;
		m_en_param.i_bframe = 0;
		break;
	case VIDEO_720P:
		m_en_param.rc.i_qp_min = 5;
		m_en_param.rc.i_qp_max = 40;
		m_en_param.rc.i_qp_constant = 24;
		m_en_param.rc.i_bitrate = 1000;
		m_en_param.rc.i_vbv_max_bitrate = 1600;
		m_en_param.i_keyint_min = m_nFrame * 6;
		m_en_param.i_keyint_max = m_nFrame * 6;
		m_en_param.i_bframe = 0;
		break;

	case VIDEO_640P:
		m_en_param.rc.i_qp_min = 5;
		m_en_param.rc.i_qp_max = 40;
		m_en_param.rc.i_qp_constant = 24;
		m_en_param.rc.i_bitrate = 600;
		m_en_param.rc.i_vbv_max_bitrate = 800;
		m_en_param.i_keyint_min = m_nFrame * 6;
		m_en_param.i_keyint_max = m_nFrame * 6;
		m_en_param.i_bframe = 0;
		break;

	case VIDEO_480P:
		m_en_param.rc.i_qp_min		= 5;
		m_en_param.rc.i_qp_max		= 40;
		m_en_param.rc.i_qp_constant = 24;
		m_en_param.rc.i_bitrate		= 320;
		m_en_param.rc.i_vbv_max_bitrate = 480;
		m_en_param.i_keyint_min		= m_nFrame * 6;
		m_en_param.i_keyint_max		= m_nFrame * 6;
		m_en_param.i_bframe			= 0;
		break;

	case VIDEO_360P:
		m_en_param.rc.i_qp_min		= 15;
		m_en_param.rc.i_qp_max		= 38;
		m_en_param.rc.i_qp_constant = 24;
		m_en_param.rc.i_bitrate		= 144;
		m_en_param.rc.i_vbv_max_bitrate = 160;
		m_en_param.i_keyint_min		= m_nFrame * 6;
		m_en_param.i_keyint_max		= m_nFrame * 6;
		m_en_param.i_bframe			= 0;
		break;

	case VIDEO_240P:
		m_en_param.rc.i_qp_min		= 5;
		m_en_param.rc.i_qp_max		= 36;
		m_en_param.rc.i_qp_constant = 24;
		m_en_param.rc.i_bitrate		= 120;
		m_en_param.rc.i_vbv_max_bitrate = 140;
		m_en_param.i_keyint_min		= m_nFrame * 6;
		m_en_param.i_keyint_max		= m_nFrame * 6;
		m_en_param.i_bframe			= 0;
		break;

	case VIDEO_120P:
		m_en_param.rc.i_qp_min = 5;
		m_en_param.rc.i_qp_max = 32;
		m_en_param.rc.i_qp_constant = 24;
		m_en_param.rc.i_bitrate = 32;
		m_en_param.rc.i_vbv_max_bitrate = 40;
		m_en_param.i_keyint_min = m_nFrame * 6;
		m_en_param.i_keyint_max = m_nFrame * 6;
		m_en_param.i_bframe = 0;
		break;
	}

	m_en_param.rc.f_rate_tolerance	= 1.0;
	m_en_param.rc.i_vbv_buffer_size = 800;
	m_en_param.rc.f_vbv_buffer_init = 0.9;

	m_en_param.rc.b_mb_tree		= 1;
	m_en_param.b_repeat_headers = 1;
	m_en_param.b_annexb			= 1;

	m_en_param.i_frame_reference= 4;
	m_en_param.vui.b_fullrange	= 1;
	m_en_param.i_sync_lookahead	= 0;
	m_en_param.vui.i_colorprim	= 2;
	m_en_param.vui.i_transfer	= 2;
	m_en_param.vui.i_colmatrix	= 2;
	//m_en_param.vui.i_chroma_loc = 2;
	m_en_param.i_cabac_init_idc	= 2;
	m_en_param.rc.i_aq_mode		= X264_AQ_VARIANCE;
	m_en_param.rc.f_aq_strength = 1.3;
	m_en_param.rc.f_qcompress	= 0.75;

	m_en_param.b_deblocking_filter			= 0x00000800;
	m_en_param.i_deblocking_filter_alphac0	= 2;
	m_en_param.i_deblocking_filter_beta		= 2;
	m_en_param.analyse.i_me_method			= X264_ME_DIA;
	m_en_param.analyse.inter				= X264_ANALYSE_I8x8 | X264_ANALYSE_I4x4;
}


H264Decoder::H264Decoder()
{
	/*m_nWidth	= PIC_WIDTH_640;
	m_nHeight	= PIC_HEIGHT_480;*/
	m_nCurrWidth = 0;
	m_nCurrHeight = 0;
	m_pDecodeData = NULL;

	m_de_frame	= NULL;
	m_de_codec	= NULL;

	m_de_context= NULL;
	m_swsContext= NULL;
}

H264Decoder::~H264Decoder()
{
	DeRelease();
}

bool H264Decoder::Init(/*int iSrcWidth, int iSrcHeight, int iDestWidth, int iDestHeight, */int iQP)
{
	/*m_nWidth = iSrcWidth;
	m_nHeight = iSrcHeight;

	m_h264_codec_width	= iDestWidth;
	m_h264_codec_height = iDestHeight;*/

	avcodec_register_all();

	return DeConnect();
}

bool H264Decoder::DeConnect()
{
	m_de_codec		= avcodec_find_decoder(CODEC_ID_H264);
	m_de_context	= avcodec_alloc_context3(m_de_codec);

	m_de_context->error_concealment = 1;
	m_de_frame		= avcodec_alloc_frame();

	return ((avcodec_open2(m_de_context, m_de_codec, NULL) != 0) ? false : true);
}

void H264Decoder::DeRelease()
{
	if (m_pDecodeData)
		free(m_pDecodeData);

	if(m_de_context != NULL){
		avcodec_close(m_de_context);
		av_free(m_de_context);

		m_de_context = NULL;
	}

	if(m_de_frame != NULL){
		av_free(m_de_frame);
		m_de_frame = NULL;
	}
	
	if(m_swsContext != NULL){
		sws_freeContext(m_swsContext);
		m_swsContext = NULL;
	}
}

bool H264Decoder::DecodeVideoData(uint8_t *pInData, int iLen, uint8_t **ppOutBuf, int &out_width, int& out_height, int32_t &pict_type)
{
	if(m_de_context == NULL)
		return false;

	av_init_packet(&m_avpkt);
	m_avpkt.data = pInData;
	m_avpkt.size = iLen;

	int got_picture = 0;
	int iOutLen = avcodec_decode_video2(m_de_context, m_de_frame, &got_picture, &m_avpkt);
	if (iOutLen <= 0)
		return false;

	if (m_nCurrWidth != m_de_frame->width || m_nCurrHeight != m_de_frame->height){
		// When first time decoding or the width or height changed, the following code will execute
		m_nCurrWidth = m_de_frame->width;
		m_nCurrHeight = m_de_frame->height;

		if (m_pDecodeData != NULL)
			free(m_pDecodeData);

		m_pDecodeData = (uint8_t*)malloc(m_nCurrHeight * m_nCurrWidth * 3);

		if (m_swsContext != NULL)
			sws_freeContext(m_swsContext);
		// No scale here, so use the same width and height
		m_swsContext = sws_getContext(m_nCurrWidth, m_nCurrHeight, PIX_FMT_YUV420P,
			m_nCurrWidth, m_nCurrHeight,
			PIX_FMT_BGR24, SWS_FAST_BILINEAR, NULL, NULL, NULL);
	}

	if (m_swsContext)
	{
		int len = avpicture_fill(&m_outpic, m_pDecodeData, PIX_FMT_RGB24, m_nCurrWidth, m_nCurrHeight);
		//YUV -> RGB, No scale
		iOutLen = sws_scale(m_swsContext, m_de_frame->data, m_de_frame->linesize, 0,
			m_de_context->height, m_outpic.data, m_outpic.linesize);
		if (iOutLen > 0){
			*ppOutBuf = m_pDecodeData;
			out_width = m_nCurrWidth;
			out_height = m_nCurrHeight;
			pict_type = m_de_frame->pict_type;
			return true;
		}
	}
	return false;
}
