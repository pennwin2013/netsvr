#include <boost/asio/ip/tcp.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/lambda/lambda.hpp>
#include "../session_utility.hpp"
#include "../protocol_def.h"
#include "client_sess_impl.h"

namespace FDXSLib
{
namespace details
{
using boost::lambda::bind;
using boost::lambda::var;
using boost::lambda::_1;
///////////////////////////////////////////////////////////////////////////////
client_impl::client_impl() : socket_( io_service_ ), deadline_( io_service_ )
{
  deadline_.expires_at( boost::posix_time::pos_infin );
}
client_impl::~client_impl()
{
}
bool client_impl::connect( const char* ip, std::size_t port, std::size_t timeout )
{
  ba::ip::tcp::resolver resolver( io_service_ );
  std::stringstream service;
  service << port;
  bp::tcp::resolver::query query( ip, service.str() );

  bp::tcp::resolver::iterator iterator = resolver.resolve( query );
  bp::tcp::endpoint ep = *iterator;


  FDXSLib::utility::timeout_guard time_guard( deadline_, timeout );
  check_deadline();

  boost::system::error_code ec = boost::asio::error::would_block;

  socket_.async_connect( ep, var( ec ) = _1 );

  do
  {
    io_service_.run_one();
  }
  while( ec == ba::error::would_block );

  if( !ec && socket_.is_open() )
    return true;
  return false;
}
void client_impl::disconnect()
{
  if( socket_.is_open() )
  {
    try
    {
      socket_.shutdown( bp::tcp::socket::shutdown_both );
    }
    catch( ... )
    {
      // ignore
    }
    socket_.close();
  }
}
bool client_impl::send_message( const protocol_message* message, std::size_t timeout )
{
  FDXSLib::utility::timeout_guard time_guard( deadline_, timeout );
  ba::streambuf buf;
  std::ostream stream( &buf );
  protocol request;

  reset_request( request );
  request.data = ( protocol_message* ) message;
  request.head.message_length = request.data->total_length();

  stream << request.head;
  //std::cout << "send message header : " << buf.size() << " : "<< a << std::endl;
  stream << request.data;

  //std::cout << "send message data : " << buf.size() << std::endl;
  boost::system::error_code ec = boost::asio::error::would_block;
  ba::async_write( socket_, buf, var( ec ) = _1 );

  do
  {
    io_service_.run_one();
  }
  while( ec == ba::error::would_block );
  if( ec )
  {
    std::cout << "read error :" << ec.value() << " : " << ec.message() << std::endl;
    return false;
  }

  return true;
}
bool client_impl::recv_message( protocol_message* message, std::size_t timeout )
{
  FDXSLib::utility::timeout_guard time_guard( deadline_, timeout );
  protocol response;
  response.data = message;
  reset_request( response );

  // recv header
  bool ret = read_header( response );

  if( !ret )
  {
    return false;
  }
  // std::cout<<"recv header , message length : "<<response.head.message_length<<std::endl;
  // empty message maybe heart beat
  if( response.head.message_length == 0 )
  {
    return true;
  }
  ret = read_body( response );
  if( !ret )
  {
    return false;
  }
  return true;
}
void client_impl::start_service()
{
}
void client_impl::stop_service()
{
}
void client_impl::check_deadline()
{
  deadline_.async_wait( boost::bind( &client_impl::on_deadline, this ) );
}
void client_impl::on_deadline()
{
  if( deadline_.expires_at() <= ba::deadline_timer::traits_type::now() )
  {
    // The deadline has passed. The socket is closed so that any outstanding
    // asynchronous operations are cancelled. This allows the blocked
    // connect(), read_line() or write_line() functions to return.
    socket_.close();
    // There is no longer an active deadline. The expiry is set to positive
    // infinity so that the actor takes no action until a new deadline is set.
    deadline_.expires_at( boost::posix_time::pos_infin );
  }
  // Put the actor back to sleep.
  deadline_.async_wait( boost::bind( &client_impl::on_deadline, this ) );
}
void client_impl::reset_request( protocol& request )
{
  request.head.major_version = protocol::kMajorVersion;
  request.head.minor_version = protocol::kMinorVersion;
  request.head.first_flag = 1;
  request.head.next_flag = 0;
  request.head.crc_flag = 0;
  request.head.message_id = new_message_id();
  request.head.sequence_no = 1;
  request.head.encrypt_type = 0;
  request.head.message_length = 0;
  memset( request.head.crc, 0xFF, sizeof( request.head.crc ) );
}
bool client_impl::read_header( protocol& response )
{
  buffer_.consume( buffer_.size() );
  std::size_t read_length = PTH_LENGTH;
  boost::system::error_code ec = boost::asio::error::would_block;
  ba::async_read( socket_, buffer_, ba::transfer_at_least( read_length ), boost::lambda::var( ec ) = boost::lambda::_1 );

  do
  {
    io_service_.run_one();
  }
  while( ec == ba::error::would_block );
  if( ec )
  {
    return false;
  }
  std::istream stream( &buffer_ );
  stream >> response.head;
  return true;
}
bool client_impl::read_body( protocol& response )
{
  if( response.head.message_length == 0 )
    return true;
  std::size_t left_length = response.head.message_length - buffer_.size();

  // not need recv message
  if( left_length > 0 )
  {
    boost::system::error_code ec = boost::asio::error::would_block;
    ba::async_read( socket_, buffer_, ba::transfer_at_least( left_length ), var( ec ) = _1 );
    do
    {
      io_service_.run_one();
    }
    while( ec == ba::error::would_block );
    if( ec )
    {
      return false;
    }
  }
  std::istream stream( &buffer_ );
  copy_message_buffer( stream, response.head.message_length, response.data );
  return true;
}

///////////////////////////////////////////////////////////////////////////////
}
}
