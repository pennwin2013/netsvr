#ifndef _SERVER_SESSION_IMPL_H_
#define _SERVER_SESSION_IMPL_H_

#include <iostream>
#include <map>
#include <deque>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/enable_shared_from_this.hpp>
#include "../protocol_def.h"
#include "../server_session.h"


namespace FDXSLib
{
namespace details
{
namespace ba = boost::asio;
namespace bp = boost::asio::ip;

struct svr_protocol
{
  protocol_head head;
  protocol_message_ptr data;
};

class tcp_session;


class server_impl : public FDXSLib::server
{
private:
  typedef boost::shared_ptr<tcp_session> tcp_session_ptr;
public:
  server_impl();
  virtual ~server_impl();
  int start( std::size_t port );
  void stop();
  bool is_running() const;
  server_callback* register_callback( server_callback* callback );
  int push_response( const connect_info& conn_info, protocol_message_ptr response );
  void close_session( tcp_session_ptr ptr );
  void close_connection( const connect_info& conn_info );
private:
  typedef boost::mutex::scoped_lock lock_type;
  typedef std::map<std::size_t, tcp_session_ptr> connection_set;
  ///////////////////////////////////////////////////////////////////////////////
  void handle_accept( tcp_session_ptr session, const boost::system::error_code& ec );
  void new_connection();
  void close_session_ptr( connection_set::value_type ptr );
  ///////////////////////////////////////////////////////////////////////////////
  ba::io_service io_service_;
  ba::ip::tcp::acceptor acceptor_;
  ba::streambuf input_buffer_;
  boost::mutex mutex_;
  tcp_session_ptr session_;
  server_callback* callback_;
  boost::shared_ptr<boost::thread> service_thr_;
  std::size_t listen_port_;
  connection_set connections_;
};

///////////////////////////////////////////////////////////////////////////////
class tcp_session : public boost::enable_shared_from_this<tcp_session>
{
public:
  tcp_session( server_impl* server, ba::io_service& iosvr, server_callback* callback );
  ~tcp_session();
  void start();
  inline bp::tcp::socket& socket()
  {
    return  socket_;
  }
  void close();
  inline connect_info& connection_info()
  {
    return conn_info_;
  }
  void async_send_queue( protocol_message_ptr response );
private:
  static const std::size_t kDefaultPackLength = 8196; // 8k
  void handle_read( const boost::system::error_code& ec, std::size_t byte_transferred );
  void handle_read_body( const boost::system::error_code& ec );
  void handle_write( const boost::system::error_code& ec, std::size_t byte_transferred );
  void handle_timeout( const boost::system::error_code& ec );
  void check_deadline();
  void error_response( std::size_t error_code );
  void reset_request();
  void reset_response();
  void free();
  void free_memory();
  ba::io_service& io_service_;
  ba::ip::tcp::socket socket_;
  ba::streambuf input_buffer_;
  ba::streambuf output_buffer_;
  ba::deadline_timer deadline_;
  server_callback* callback_;
  std::size_t connect_timeout_;
  connect_info conn_info_;
  svr_protocol request_;
  svr_protocol response_;
  server_impl* server_;
};
///////////////////////////////////////////////////////////////////////////////
}
}

#endif //_SERVER_SESSION_IMPL_H_
