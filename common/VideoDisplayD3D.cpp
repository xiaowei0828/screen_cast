// D:\文档\Visual Studio 2013\Projects\sender\common\VideoDisplayD3D.cpp : 实现文件
//

#include "VideoDisplayD3D.h"


// CVideoDisplayD3D

#define LOAD_RGB   1
#define LOADARGB   0
#define LOAD_YUV420P 0

#if LOAD_RGB
const int bpp = 24;
#elif LOAD_ARGB
const int bpp = 32;
#elif LOAD_YUV420P
const int bpp = 12;
#endif

IMPLEMENT_DYNAMIC(CVideoDisplayD3D, CStatic)

CVideoDisplayD3D::CVideoDisplayD3D()
{

}

CVideoDisplayD3D::~CVideoDisplayD3D()
{
}


BEGIN_MESSAGE_MAP(CVideoDisplayD3D, CStatic)
	ON_WM_DESTROY()
END_MESSAGE_MAP()

void CVideoDisplayD3D::on_video_data(const uint8_t* data_ptr, int width, int height)
{
	if (!init_)
	{
		InitD3D(this->GetSafeHwnd(), width, height);
	}
}

int CVideoDisplayD3D::InitD3D(HWND hwnd, uint32_t width, uint32_t height)
{
	HRESULT lRet;
	InitializeCriticalSection(&m_critical);
	Cleanup();

	m_pDirect3D9 = Direct3DCreate9(D3D_SDK_VERSION);
	if (m_pDirect3D9 == NULL)
		return -1;

	D3DPRESENT_PARAMETERS d3dpp;
	ZeroMemory(&d3dpp, sizeof(d3dpp));
	d3dpp.Windowed = TRUE;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;

	GetClientRect(&m_rtViewport);

	lRet = m_pDirect3D9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, this->GetSafeHwnd(),
		D3DCREATE_SOFTWARE_VERTEXPROCESSING,
		&d3dpp, &m_pDirect3DDevice);
	if (FAILED(lRet))
		return -1;

#if LOAD_RGB
	lRet = m_pDirect3DDevice->CreateOffscreenPlainSurface(
		width, height,
		D3DFMT_R8G8B8,
		D3DPOOL_DEFAULT,
		&m_pDirect3DSurfaceRender,
		NULL);
#elif LOADARGB
	lRet = m_pDirect3DDevice->CreateOffscreenPlainSurface(
		width, height,
		D3DFMT_A8R8G8B8,
		D3DPOOL_DEFAULT,
		&m_pDirect3DSurfaceRender,
		NULL);
#elif LOAD_YUV420P
	lRet = m_pDirect3DDevice->CreateOffscreenPlainSurface(
		width, height,
		(D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2'),
		D3DPOOL_DEFAULT,
		&m_pDirect3DSurfaceRender,
		NULL);
#endif

	if (FAILED(lRet))
		return -1;

	return 0;
}

void CVideoDisplayD3D::Cleanup()
{
	EnterCriticalSection(&m_critical);
	if (m_pDirect3DSurfaceRender)
		m_pDirect3DSurfaceRender->Release();
	if (m_pDirect3DDevice)
		m_pDirect3DDevice->Release();
	if (m_pDirect3D9)
		m_pDirect3D9->Release();
	LeaveCriticalSection(&m_critical);
}

bool CVideoDisplayD3D::Render(const uint8_t* data_ptr, int width, int height)
{
	HRESULT lRet;
	if (m_pDirect3DSurfaceRender == NULL)
		return -1;
	D3DLOCKED_RECT d3d_rect;
	lRet = m_pDirect3DSurfaceRender->LockRect(&d3d_rect, NULL, D3DLOCK_DONOTWAIT);
	if (FAILED(lRet))
		return -1;

	byte *pSrc = (byte*)data_ptr;
	byte* pDest = (BYTE*)d3d_rect.pBits;
	int stride = d3d_rect.Pitch;
	unsigned long i = 0;

	// Copy Data
#if LOAD_RGB
	int line_size = width * 3;
	for (int i = 0; i < height; i++)
	{
		memcpy(pDest, pSrc, line_size);
		pDest += stride;
		pSrc += line_size;
	}
#elif LOAD_ARGB
	int line_size = width * 4;
	for (int i = 0; i < height; i ++)
	{
		memcpy(pDest, pSrc, line_size);
		pDest += stride;
		pSrc += line_size;
	}
#elif LOAD_YUV420P
	for(i = 0; i < height; i++){  
		memcpy(pDest + i * stride, pSrc + i * width, width);
}
	for (i = 0; i < pixel_h / 2; i++){
		memcpy(pDest + stride * height + i * stride / 2, pSrc + *height + width * height / 4 + i * width / 2, width / 2);
	}
	for (i = 0; i < pixel_h / 2; i++){
		memcpy(pDest + stride * height + stride * height / 4 + i * stride / 2, pSrc + width * height + i * width / 2, width / 2);
	}
#endif

	lRet = m_pDirect3DSurfaceRender->UnlockRect();
	if (FAILED(lRet))
		return -1;

	if (m_pDirect3DDevice == NULL)
		return -1;

	m_pDirect3DDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
	m_pDirect3DDevice->BeginScene();
	IDirect3DSurface9* pBackBuffer = NULL;
	m_pDirect3DDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);
	m_pDirect3DDevice->StretchRect(m_pDirect3DSurfaceRender, NULL, pBackBuffer, &m_rtViewport, D3DTEXF_LINEAR);
	m_pDirect3DDevice->EndScene();
	m_pDirect3DDevice->Present(NULL, NULL, NULL, NULL);
	pBackBuffer->Release();
	return true;
 }

void CVideoDisplayD3D::OnDestroy()
{
	Cleanup();
}



// CVideoDisplayD3D 消息处理程序


