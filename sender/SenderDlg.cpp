
// SenderDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "Sender.h"
#include "SenderDlg.h"
#include "afxdialogex.h"


#include <boost\asio.hpp>
#include <boost\bind.hpp>
#include "protocol.h"
#include <thread>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CSenderDlg 对话框



CSenderDlg::CSenderDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CSenderDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CSenderDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_BUTTON_FIND_SERVICE, m_btFindService);
	DDX_Control(pDX, IDC_LIST_SERVICE, m_listService);
	DDX_Control(pDX, IDC_STATIC_VIDEO, m_staticVideo);
}

BEGIN_MESSAGE_MAP(CSenderDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_FIND_SERVICE, &CSenderDlg::OnBnClickedButtonFindService)
	ON_BN_CLICKED(IDC_BUTTON_CONNECT_SERVICE, &CSenderDlg::OnBnClickedButtonConnectService)
	ON_WM_DESTROY()
END_MESSAGE_MAP()


// CSenderDlg 消息处理程序

BOOL CSenderDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO:  在此添加额外的初始化代码

	CDC* pDC = m_staticVideo.GetDC();
	CRect rect;
	m_staticVideo.GetWindowRect(rect);

	if (mem_dc_ == NULL)
	{
		mem_dc_ = CreateCompatibleDC(pDC->GetSafeHdc());
		HBITMAP bitmap = CreateCompatibleBitmap(pDC->GetSafeHdc(), rect.Width(), rect.Height());
		SelectObject(mem_dc_, bitmap);
		DeleteObject(bitmap);
	}

	udp_send_buffer_.reset(new char[udp_send_buffer_size]);
	udp_receive_buffer_.reset(new char[udp_receive_buffer_size]);
	tcp_send_buffer_.reset(new char[tcp_send_buffer_size]);

	video_width_ = 0;
	video_height_ = 0;
	video_data_ptr_ = NULL;
	frame_interval_in_ms_ = 1000 / VIDEO_FRAME;

	namespace ip = boost::asio::ip;

	ioservice_ptr_ = new boost::asio::io_service();

	udp_socket_ptr_ = new ip::udp::socket(*ioservice_ptr_, ip::udp::endpoint(ip::udp::v4(), 0));
	udp_socket_ptr_->set_option(boost::asio::socket_base::broadcast(true));
	udp_socket_ptr_->async_receive_from(boost::asio::buffer(udp_receive_buffer_.get(), 500), udp_peer_endpoint_,
		boost::bind(&CSenderDlg::handle_udp_recv_from, this,
		boost::asio::placeholders::error,
		boost::asio::placeholders::bytes_transferred));

	tcp_socket_ptr_ = new ip::tcp::socket(*ioservice_ptr_, ip::tcp::endpoint(ip::tcp::v4(), 0));

	video_timer_ptr_ = new boost::asio::deadline_timer(*ioservice_ptr_);

	// run the io service
	ioservice_thread_.swap(std::thread(std::bind(&CSenderDlg::ioservice_thread_proc, this)));

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CSenderDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CSenderDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
		// 刷新video显示区域
		CDC* pDC = m_staticVideo.GetDC();
		CRect rect;
		m_staticVideo.GetWindowRect(rect);

		if (mem_dc_ != NULL)
		{
			BitBlt(pDC->GetSafeHdc(), 0, 0, rect.Width(), rect.Height(), mem_dc_, 0, 0, SRCCOPY);
		}
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CSenderDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CSenderDlg::OnDestroy()
{
	

	// TODO:  在此处添加消息处理程序代码

	if (udp_socket_ptr_)
	{
		udp_socket_ptr_->close();
	}

	if (tcp_socket_ptr_)
	{
		tcp_socket_ptr_->close();
		
	}

	if (video_timer_ptr_)
	{
		video_timer_ptr_->cancel();
	}

	if (ioservice_ptr_)
	{
		ioservice_ptr_->stop();
		if (ioservice_thread_.joinable())
		{
			ioservice_thread_.join();
		}
	}
	delete udp_socket_ptr_;
	delete tcp_socket_ptr_;
	delete video_timer_ptr_;
	delete ioservice_ptr_;

	uninit_monitor_data();

	CDialogEx::OnDestroy();
}

void CSenderDlg::OnBnClickedButtonFindService()
{
	// TODO:  在此添加控件通知处理程序代码
	namespace ip = boost::asio::ip;

	vec_service_endpoint_.clear();
	for (int i = 0; i < m_listService.GetCount(); ++i)
	{
		m_listService.DeleteString(0);
	}
	
	screen_cast_find_service_t find_service;
	memcpy(udp_send_buffer_.get(), &find_service, sizeof(screen_cast_find_service_t));

	ip::udp::endpoint broadcast_endpoint(ip::address_v4::broadcast(), 8888);

	udp_socket_ptr_->async_send_to(boost::asio::buffer(udp_send_buffer_.get(), sizeof(screen_cast_find_service_t)), broadcast_endpoint, 
		boost::bind(&CSenderDlg::handle_udp_send_to, this, 
		boost::asio::placeholders::error,
		boost::asio::placeholders::bytes_transferred));
}


void CSenderDlg::OnBnClickedButtonConnectService()
{
	// TODO:  在此添加控件通知处理程序代码
	namespace ip = boost::asio::ip;
	ip::tcp::endpoint target_endpoint;

	CString service_name_u16;
	m_listService.GetText(m_listService.GetCurSel(), service_name_u16);
	char service_name_utf8[1024];
	int count = WideCharToMultiByte(CP_UTF8, 0, service_name_u16.GetBuffer(), service_name_u16.GetLength(), service_name_utf8, 1024, NULL, NULL);
	service_name_utf8[count] = 0;

	bool find = false;
	for (auto& item : vec_service_endpoint_)
	{
		if (item.first == service_name_utf8)
		{
			target_endpoint = item.second;
			find = true;
			break;
		}
	}
	if (!find)
	{
		MessageBoxA(this->GetSafeHwnd(), "Service not found", "Alert", MB_OK);
		return;
	}

	tcp_socket_ptr_->async_connect(target_endpoint,
		boost::bind(&CSenderDlg::handle_tcp_connect, this,
		boost::asio::placeholders::error));
}

void CSenderDlg::ioservice_thread_proc()
{
	if (ioservice_ptr_)
	{
		ioservice_ptr_->run();
	}
}

void CSenderDlg::handle_udp_send_to(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (error)
	{
		OutputDebugStringA(error.message().c_str());
	}
	return;
}

void CSenderDlg::handle_udp_recv_from(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (!error)
	{
		namespace ip = boost::asio::ip;
		screen_cast_header_t* header_ptr = (screen_cast_header_t*)udp_receive_buffer_.get();
		if (header_ptr->type == TYPE_RESPONSE_SERVICE)
		{
			screen_cast_response_t* response_ptr = (screen_cast_response_t*)header_ptr;
			wchar_t service_name_u16[1024];
			MultiByteToWideChar(CP_UTF8, 0, (char*)response_ptr->name, SERVICE_NAME_LENGTH, service_name_u16, 1024);
			m_listService.AddString(service_name_u16);
			std::string peer_address = udp_peer_endpoint_.address().to_string();
			ip::tcp::endpoint tcp_endpoint(udp_peer_endpoint_.address(), response_ptr->tcp_port);
			vec_service_endpoint_.push_back(std::make_pair((char*)response_ptr->name, tcp_endpoint));
		}

		udp_socket_ptr_->async_receive_from(boost::asio::buffer(udp_receive_buffer_.get(), 500), udp_peer_endpoint_,
			boost::bind(&CSenderDlg::handle_udp_recv_from, this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
	}
	else
	{
		OutputDebugStringA(error.message().c_str());
	}
}

void CSenderDlg::handle_tcp_connect(const boost::system::error_code& error)
{
	if (!error)
	{
		init_monitor_data();
		video_timer_ptr_->expires_from_now(boost::posix_time::microsec(1));
		video_timer_ptr_->async_wait(boost::bind(&CSenderDlg::handle_video_timer_expires, this,
			boost::asio::placeholders::error));
	}
	else
	{
		//MessageBoxA(this->GetSafeHwnd(), error.message().c_str(), "Alert", MB_OK);
		OutputDebugStringA(error.message().c_str());
	}

}

void CSenderDlg::handle_tcp_write(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (error)
	{
		video_timer_ptr_->cancel();
		OutputDebugStringA(error.message().c_str());
	}
	return;
}


void CSenderDlg::init_monitor_data()
{
	screen_dc_ = CreateDC(L"DISPLAY", NULL, NULL, NULL);
	mem_dc_ = CreateCompatibleDC(screen_dc_);

	int nScreenWidth = GetDeviceCaps(screen_dc_, HORZRES);
	int nScreenHeight = GetDeviceCaps(screen_dc_, VERTRES);

	if (video_width_ != nScreenWidth || video_height_ != nScreenHeight)
	{
		video_width_ = nScreenWidth;
		video_height_ = nScreenHeight;
		video_data_ptr_.reset(new char[video_width_ * video_height_ * 3]);
		h264_encoder_ptr_.reset(new H264Encoder());
		h264_encoder_ptr_->Init(VIDEO_FRAME, video_width_, video_height_, video_width_, video_height_);
	}

	bitmap_handle_= CreateCompatibleBitmap(screen_dc_, nScreenWidth, nScreenHeight);
	SelectObject(mem_dc_, bitmap_handle_);

	ZeroMemory(&bitmap_info_, sizeof(BITMAPINFO));
	bitmap_info_.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmap_info_.bmiHeader.biWidth = nScreenWidth;
	bitmap_info_.bmiHeader.biHeight = nScreenHeight;
	bitmap_info_.bmiHeader.biPlanes = 1;
	bitmap_info_.bmiHeader.biBitCount = 24;
	bitmap_info_.bmiHeader.biCompression = BI_RGB;
	bitmap_info_.bmiHeader.biSizeImage = nScreenWidth * nScreenHeight * 3;
}

void CSenderDlg::uninit_monitor_data()
{
	if (screen_dc_)
		DeleteDC(screen_dc_);
	if (mem_dc_)
		DeleteDC(mem_dc_);
	if (bitmap_handle_)
		DeleteObject(bitmap_handle_);
}


void CSenderDlg::get_monitor_data()
{
	BitBlt(mem_dc_, 0, 0, video_width_, video_height_, screen_dc_, 0, 0, SRCCOPY);
	GetDIBits(mem_dc_, bitmap_handle_, 0, video_height_, video_data_ptr_.get(), &bitmap_info_, DIB_RGB_COLORS);
	return;
}

void CSenderDlg::handle_video_timer_expires(const boost::system::error_code& error)
{
	if (!error)
	{
		get_monitor_data();
		int out_length;
		int key_frame;
		if (h264_encoder_ptr_->EncodeVideoData((uint8_t*)video_data_ptr_.get(), video_width_ * video_height_ * 3,
			AV_PIX_FMT_RGB24, (uint8_t*)(tcp_send_buffer_.get() + sizeof(screen_cast_header_t)), &out_length, &key_frame))
		{
			screen_cast_header_t header;
			header.type = TYPE_VIDEO_DATA;
			header.length = sizeof(screen_cast_header_t) + out_length;
			memcpy(tcp_send_buffer_.get(), &header, sizeof(screen_cast_header_t));
			tcp_socket_ptr_->async_send(boost::asio::buffer(tcp_send_buffer_.get(), header.length),
				boost::bind(&CSenderDlg::handle_tcp_write, this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred));
			//OutputDebugStringA("send buffer\r\n");
		}

		video_timer_ptr_->expires_from_now(boost::posix_time::millisec(frame_interval_in_ms_));
		video_timer_ptr_->async_wait(boost::bind(&CSenderDlg::handle_video_timer_expires, this,
			boost::asio::placeholders::error));
	}
}







