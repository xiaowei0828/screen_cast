
// ReceiverDlg.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "Receiver.h"
#include "ReceiverDlg.h"
#include "afxdialogex.h"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <thread>
#include "protocol.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// ����Ӧ�ó��򡰹��ڡ��˵���� CAboutDlg �Ի���

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// �Ի�������
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

// ʵ��
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


// CReceiverDlg �Ի���



CReceiverDlg::CReceiverDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CReceiverDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CReceiverDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT_SERVICE_NAME, m_editServiceName);
	DDX_Control(pDX, IDC_STATIC_VIDEO, m_staticVideo);
}

BEGIN_MESSAGE_MAP(CReceiverDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_DESTROY()
END_MESSAGE_MAP()


// CReceiverDlg ��Ϣ�������

BOOL CReceiverDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// ��������...���˵�����ӵ�ϵͳ�˵��С�

	// IDM_ABOUTBOX ������ϵͳ���Χ�ڡ�
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

	// ���ô˶Ի����ͼ�ꡣ  ��Ӧ�ó��������ڲ��ǶԻ���ʱ����ܽ��Զ�
	//  ִ�д˲���
	SetIcon(m_hIcon, TRUE);			// ���ô�ͼ��
	SetIcon(m_hIcon, FALSE);		// ����Сͼ��

	// TODO:  �ڴ���Ӷ���ĳ�ʼ������
	if (mem_dc_ == NULL)
	{
		CDC* pDC = m_staticVideo.GetDC();
		CRect rect;
		m_staticVideo.GetWindowRect(rect);

		mem_dc_ = CreateCompatibleDC(pDC->GetSafeHdc());
		HBITMAP bitmap = CreateCompatibleBitmap(pDC->GetSafeHdc(), rect.Width(), rect.Height());
		SelectObject(mem_dc_, bitmap);
		DeleteObject(bitmap);
	}

	h264_decoder_.Init();

	udp_receive_buffer_.reset(new char[udp_receive_buffer_size]);
	tcp_receive_buffer_.reset(new char[tcp_receive_buffer_size]);
	video_decode_buffer_.reset(new char[video_decode_buffer_size]);

	namespace ip = boost::asio::ip;
	ioservice_ptr_ = new boost::asio::io_service();

	udp_socket_ptr_ = new ip::udp::socket(*ioservice_ptr_, ip::udp::endpoint(ip::udp::v4(), BROADCAST_PORT));
	udp_socket_ptr_->async_receive_from(boost::asio::buffer(udp_receive_buffer_.get(), udp_receive_buffer_size), udp_peer_endpoint_,
		boost::bind(&CReceiverDlg::handle_udp_receive_from, this,
		boost::asio::placeholders::error,
		boost::asio::placeholders::bytes_transferred));

	tcp_socket_ptr_ = new ip::tcp::socket(*ioservice_ptr_);

	tcp_acceptor_ptr_ = new ip::tcp::acceptor(*ioservice_ptr_, ip::tcp::endpoint(ip::tcp::v4(), 0), false);
	tcp_acceptor_ptr_->async_accept(*tcp_socket_ptr_, tcp_peer_endpoint_,
		boost::bind(&CReceiverDlg::handle_tcp_accept, this,
		boost::asio::placeholders::error));

	ioservice_thread_.swap(std::thread(std::bind(&CReceiverDlg::ioservice_thread_proc, this)));

	return TRUE;  // ���ǽ��������õ��ؼ������򷵻� TRUE
}

void CReceiverDlg::OnSysCommand(UINT nID, LPARAM lParam)
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

// �����Ի��������С����ť������Ҫ����Ĵ���
//  �����Ƹ�ͼ�ꡣ  ����ʹ���ĵ�/��ͼģ�͵� MFC Ӧ�ó���
//  �⽫�ɿ���Զ���ɡ�

void CReceiverDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // ���ڻ��Ƶ��豸������

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// ʹͼ���ڹ����������о���
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// ����ͼ��
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();

		if (mem_dc_ != NULL)
		{
			CDC* pDC = m_staticVideo.GetDC();
			CRect rect;
			m_staticVideo.GetWindowRect(rect);
			BitBlt(pDC->GetSafeHdc(), 0, 0, rect.Width(), rect.Height(), mem_dc_, 0, 0, SRCCOPY);
		}
	}
}

//���û��϶���С������ʱϵͳ���ô˺���ȡ�ù��
//��ʾ��
HCURSOR CReceiverDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CReceiverDlg::OnDestroy()
{
	// TODO:  �ڴ˴������Ϣ����������
	if (udp_socket_ptr_)
	{
		udp_socket_ptr_->close();
	}
	if (tcp_acceptor_ptr_)
	{
		tcp_acceptor_ptr_->close();
	}
	if (tcp_socket_ptr_)
	{
		tcp_socket_ptr_->close();
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
	delete tcp_acceptor_ptr_;
	delete tcp_socket_ptr_;
	delete ioservice_ptr_;

	CDialogEx::OnDestroy();
}


void CReceiverDlg::ioservice_thread_proc()
{
	if (ioservice_ptr_)
	{
		ioservice_ptr_->run();
	}
}

void CReceiverDlg::handle_udp_receive_from(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (!error)
	{
		namespace ip = boost::asio::ip;
		screen_cast_header_t* data_ptr = (screen_cast_header_t*)udp_receive_buffer_.get();
		if (strcmp((char*)data_ptr->magic_code, magic_code_str) == 0)
		{
			if (data_ptr->type == TYPE_FIND_SERVICE)
			{
				CString service_name;
				m_editServiceName.GetWindowTextW(service_name);
				char service_name_utf8[1024];
				int count = ::WideCharToMultiByte(CP_UTF8, 0, service_name.GetBuffer(), service_name.GetLength(), service_name_utf8, 1024, NULL, NULL);
				service_name_utf8[count] = 0;

				std::string peer_addr = udp_peer_endpoint_.address().to_string();
				screen_cast_response_t response;
				response.tcp_port = tcp_acceptor_ptr_->local_endpoint().port();
				strcpy_s((char*)response.name, 256, service_name_utf8);
				udp_socket_ptr_->async_send_to(boost::asio::buffer(&response, sizeof(response)), udp_peer_endpoint_,
					boost::bind(&CReceiverDlg::handle_udp_send_to, this,
					boost::asio::placeholders::error));
			}
		}

		udp_socket_ptr_->async_receive_from(boost::asio::buffer(udp_receive_buffer_.get(), udp_receive_buffer_size), udp_peer_endpoint_,
			boost::bind(&CReceiverDlg::handle_udp_receive_from, this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
	}
	else
	{
		OutputDebugStringA(error.message().c_str());
	}
	
}


void CReceiverDlg::handle_udp_send_to(const boost::system::error_code& error)
{
	
	if (error)
	{
		OutputDebugStringA(error.message().c_str());
	}
	return;
}

void CReceiverDlg::handle_tcp_accept(const boost::system::error_code& error)
{
	if (!error)
	{
		//MessageBoxA(this->GetSafeHwnd(), error.message().c_str(), "Alert", MB_OK);
		boost::asio::async_read(*tcp_socket_ptr_, boost::asio::buffer(tcp_receive_buffer_.get(), sizeof(screen_cast_header_t)),
			boost::bind(&CReceiverDlg::handle_tcp_read_header, this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
	}
	else
	{
		OutputDebugStringA(error.message().c_str());
	}
	
}

void CReceiverDlg::handle_tcp_read_header(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (!error)
	{
		if (bytes_transferred == sizeof(screen_cast_header_t))
		{
			screen_cast_header_t* header_ptr = (screen_cast_header_t*)tcp_receive_buffer_.get();
			if (strcmp((char*)header_ptr->magic_code, magic_code_str) == 0)
			{
				boost::asio::async_read(*tcp_socket_ptr_, boost::asio::buffer(tcp_receive_buffer_.get() + bytes_transferred, header_ptr->length - bytes_transferred),
					boost::bind(&CReceiverDlg::handle_tcp_read_body, this,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred));
				OutputDebugStringA("receive header \r\n");
			}
		}
	}
	else
	{
		OutputDebugStringA(error.message().c_str());
	}
}

void CReceiverDlg::handle_tcp_read_body(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (!error)
	{
		screen_cast_header_t* header_ptr = (screen_cast_header_t*)tcp_receive_buffer_.get();
		if (header_ptr->type == TYPE_VIDEO_DATA)
		{
			uint8_t *video_decode_buffer_ptr;
			int video_decode_width;
			int video_decode_height;
			int pic_type;
			screen_cast_video_data_t* video_data_ptr = (screen_cast_video_data_t*)header_ptr;
			if (h264_decoder_.DecodeVideoData(video_data_ptr->video_buffer, header_ptr->length - sizeof(screen_cast_header_t), 
				&video_decode_buffer_ptr, video_decode_width, video_decode_height, pic_type))
			{
				if (!dib_.is_valid())
				{
					dib_.create(video_decode_width, video_decode_height);
				}
				dib_.set_dib_bits(video_decode_buffer_ptr, video_decode_width * video_decode_height * 3, false);

				CRect rect;
				m_staticVideo.GetWindowRect(rect);
				ScreenToClient(rect);

				dib_.stretch_blt(mem_dc_, 0, 0, rect.Width(), rect.Height(),
					0, 0, dib_.get_width(), dib_.get_height());
				RedrawWindow(rect);
			}
			OutputDebugStringA("receive body");
		}

		boost::asio::async_read(*tcp_socket_ptr_, boost::asio::buffer(tcp_receive_buffer_.get(), sizeof(screen_cast_header_t)),
			boost::bind(&CReceiverDlg::handle_tcp_read_header, this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
	}
	else
	{
		OutputDebugStringA(error.message().c_str());
	}
}