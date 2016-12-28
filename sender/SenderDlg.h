
// SenderDlg.h : 头文件
//

#pragma once
#include "afxwin.h"
#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include "h264_codec.h"
#include "dib.h"
#include <thread>


// CSenderDlg 对话框
class CSenderDlg : public CDialogEx
{
// 构造
public:
	CSenderDlg(CWnd* pParent = NULL);	// 标准构造函数

// 对话框数据
	enum { IDD = IDD_SENDER_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnBnClickedButtonFindService();
	afx_msg void OnBnClickedButtonConnectService();
	afx_msg void OnDestroy();
	DECLARE_MESSAGE_MAP()

private:
	void init_monitor_data();
	void uninit_monitor_data();
	void get_monitor_data();

	void ioservice_thread_proc();
	void handle_udp_send_to(const boost::system::error_code& error, size_t bytes_transferred);
	void handle_udp_recv_from(const boost::system::error_code& error, size_t bytes_transferred);

	void handle_tcp_connect(const boost::system::error_code& error);
	void handle_tcp_write(const boost::system::error_code& error, size_t bytes_transferred);

	void handle_video_timer_expires(const boost::system::error_code& error);
private:
	CButton m_btFindService;
	CListBox m_listService;

	std::thread ioservice_thread_;

	boost::asio::io_service* ioservice_ptr_;
	boost::asio::ip::udp::socket* udp_socket_ptr_;
	boost::asio::ip::tcp::socket* tcp_socket_ptr_;
	boost::asio::deadline_timer* video_timer_ptr_;

	boost::asio::ip::udp::endpoint udp_peer_endpoint_;
	boost::asio::ip::tcp::endpoint tcp_peer_endpoint_;

	enum 
	{
		udp_send_buffer_size = 500,
		udp_receive_buffer_size = 500,
		tcp_send_buffer_size = 1920 * 1080
	};
	std::unique_ptr<char[]> udp_send_buffer_;
	std::unique_ptr<char[]> udp_receive_buffer_;
	std::unique_ptr<char[]> tcp_send_buffer_;

	int video_width_;
	int video_height_;
	std::unique_ptr<char[]> video_data_ptr_;

	int frame_interval_in_ms_;

	std::shared_ptr<H264Encoder> h264_encoder_ptr_;

	std::vector<std::pair<std::string, boost::asio::ip::tcp::endpoint>> vec_service_endpoint_;

	CStatic m_staticVideo;

	HDC screen_dc_;
	HDC mem_dc_;
	HBITMAP bitmap_handle_;
	BITMAPINFO bitmap_info_;
};
