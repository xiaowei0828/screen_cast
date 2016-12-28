#pragma once

#include <afxwin.h>
#include <stdint.h>
#include <d3d9.h>

// CVideoDisplayD3D

class CVideoDisplayD3D : public CStatic
{
	DECLARE_DYNAMIC(CVideoDisplayD3D)

public:
	CVideoDisplayD3D();
	virtual ~CVideoDisplayD3D();

	void on_video_data(const uint8_t* data_ptr, int width, int height);

protected:
	DECLARE_MESSAGE_MAP()

	afx_msg void OnDestroy();

private:
	int InitD3D(HWND hwnd, uint32_t width, uint32_t height);
	bool Render(const uint8_t* data_ptr, int width, int height);
	void Cleanup();

	CRITICAL_SECTION m_critical;

	IDirect3D9* m_pDirect3D9 = NULL;
	IDirect3DDevice9* m_pDirect3DDevice = NULL;
	IDirect3DSurface9* m_pDirect3DSurfaceRender = NULL;
	RECT m_rtViewport;

	bool init_ = false;
};


