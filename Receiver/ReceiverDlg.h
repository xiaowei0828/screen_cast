
// ReceiverDlg.h : 头文件
//

#pragma once
#include "afxwin.h"
#include <boost\asio.hpp>
#include <memory>
#include <thread>
#include "h264_codec.h"
#include "dib.h"

using namespace boost::asio;

// CReceiverDlg 对话框
class CReceiverDlg : public CDialogEx
{
// 构造
public:
	CReceiverDlg(CWnd* pParent = NULL);	// 标准构造函数

// 对话框数据
	enum { IDD = IDD_RECEIVER_DIALOG };

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
	afx_msg void OnDestroy();
	DECLARE_MESSAGE_MAP()
private:
	void ioservice_thread_proc();
	void handle_udp_receive_from(const boost::system::error_code& error, size_t bytes_transferred);
	void handle_udp_send_to(const boost::system::error_code& error);

	void handle_tcp_accept(const boost::system::error_code& error);
	void handle_tcp_read_header(const boost::system::error_code& error, size_t bytes_transferred);
	void handle_tcp_read_body(const boost::system::error_code& error, size_t bytes_transferred);
private:
	CEdit m_editServiceName;
	CStatic m_staticVideo;

	std::thread ioservice_thread_;
	io_service* ioservice_ptr_;
	ip::udp::socket* udp_socket_ptr_; 
	ip::tcp::acceptor* tcp_acceptor_ptr_;
	ip::tcp::socket* tcp_socket_ptr_;
	ip::udp::endpoint udp_peer_endpoint_;
	ip::tcp::endpoint tcp_peer_endpoint_;
	

	enum
	{	
		udp_receive_buffer_size = 500,
		tcp_receive_buffer_size = 1920 * 1080,
		video_decode_buffer_size = 2000 * 2000,
	};
	std::unique_ptr<char[]> udp_receive_buffer_;
	std::unique_ptr<char[]> tcp_receive_buffer_;
	std::unique_ptr<char[]> video_decode_buffer_;

	H264Decoder h264_decoder_;
	CDib dib_;
	HDC mem_dc_;
};
