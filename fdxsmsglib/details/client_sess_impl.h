#ifndef _CLIENT_SESS_IMPL_H_
#define _CLIENT_SESS_IMPL_H_

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include "../client_session.h"

namespace FDXSLib
{
namespace details
{

namespace ba = boost::asio;
namespace bp = boost::asio::ip;

class client_impl : public FDXSLib::client
{
public:
  client_impl();
  virtual ~client_impl();
  bool connect ( const char* ip, std::size_t port, std::size_t timeout );
  void disconnect();
  bool send_message ( const protocol_message* message, std::size_t timeout );
  bool recv_message ( protocol_message* message, std::size_t timeout );
private:
  void start_service();
  void stop_service();
  void check_deadline();
  void on_deadline();
  void reset_request ( protocol& reqeust );
  bool read_header ( protocol& response );
  bool read_body ( protocol& response );
  ba::io_service io_service_;
  bp::tcp::socket socket_;
  ba::deadline_timer deadline_;
  ba::streambuf buffer_;
};

}
}
#endif // _CLIENT_SESS_IMPL_H_
