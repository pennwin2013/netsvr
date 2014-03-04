#include <cassert>
#include <sstream>
#include "server_sess_impl.h"
#include "../session_utility.hpp"

namespace FDXSLib
{
namespace details
{

///////////////////////////////////////////////////////////////////////////////
server_impl::server_impl() : acceptor_( io_service_ ), callback_( NULL ),
  listen_port_( 0 )
{
}
server_impl::~server_impl()
{
  if( service_thr_ )
    service_thr_->join();
}
// 启动服务
int server_impl::start( std::size_t port )
{
  if( is_running() )
    return -1;

  listen_port_ = port;

  std::stringstream service;
  service << listen_port_;

  bp::tcp::resolver resolver( io_service_ );
  bp::tcp::resolver::query query( "0.0.0.0", service.str() );
  bp::tcp::resolver::iterator iterator = resolver.resolve( query );
  bp::tcp::endpoint ep = *iterator;
  acceptor_.open( ep.protocol() );

  acceptor_.set_option( bp::tcp::acceptor::reuse_address( true ) );
  try
  {
    acceptor_.bind( ep );
    acceptor_.listen();
    new_connection();
    service_thr_.reset( new boost::thread(
                          boost::bind( &ba::io_service::run, &io_service_ ) ) );
    return 0;
  }
  catch( std::exception& ex )
  {
    std::cout << "bind error " << ex.what();
    return -1;
  }
}
// 创建新连接
void server_impl::new_connection()
{
  tcp_session* s = new tcp_session( this, io_service_, callback_ );
  tcp_session_ptr session( s );
  {
    lock_type lock( mutex_ );
    connections_[session->connection_info().connection_id()] = session;
    
  }
  acceptor_.async_accept( session->socket(),
                          boost::bind( &server_impl::handle_accept, this, session, ba::placeholders::error ) );
}
// 接收到新请求
void server_impl::handle_accept( tcp_session_ptr session,
                                 const boost::system::error_code& ec )
{
  session->start();
  //std::cout<<"new connection id :"<<session->connection_info().connection_id()<<std::endl;
  new_connection();
}
// 停止服务
void server_impl::stop()
{
  if( acceptor_.is_open() )
    acceptor_.close();

  {
    lock_type lock( mutex_ );
    std::for_each( connections_.begin(), connections_.end(),
                   boost::bind( &server_impl::close_session_ptr, this , _1 ) );
    connections_.clear();
  }
  io_service_.stop();
  if( service_thr_ )
  {
    service_thr_->join();
    service_thr_.reset();
  }
}
void server_impl::close_session_ptr( connection_set::value_type ptr )
{
  ptr.second->close();
}
// 服务运行状态
bool server_impl::is_running() const
{
  return acceptor_.is_open();
}
// 注册业务接口
server_callback* server_impl::register_callback( server_callback* callback )
{
  lock_type lock( mutex_ );
  server_callback* old = callback_;
  callback_ = callback;
  return old;
}
// 加入发送队列
int server_impl::push_response( const connect_info& conn_info, protocol_message_ptr response )
{
  std::size_t connid = conn_info.connection_id();
  lock_type lock( mutex_ );
  if( connections_.find( connid ) == connections_.end() )
  {
    //std::cout<<"server_impl::push_response connection not found : "<<connid<<std::endl;
    return -1;
  }
  //std::cout<<"server_impl::push_response connection : "<<connid<<std::endl;
  tcp_session_ptr ptr = connections_[connid];
  io_service_.post( boost::bind( &tcp_session::async_send_queue, ptr, response ) );
  return 0;
}
void server_impl::close_connection( const connect_info& conn_info )
{
  std::size_t connid = conn_info.connection_id();
  tcp_session_ptr ptr;
  {
    lock_type lock( mutex_ );
    if( connections_.find( connid ) == connections_.end() )
    {
      return;
    }
    ptr = connections_[connid];
  }
  close_session( ptr );
}
// 关闭session
void server_impl::close_session( tcp_session_ptr ptr )
{
  lock_type lock( mutex_ );
  ptr->close();
  //std::cout<<"server_impl::close_session : "<<ptr->connection_info().connection_id()<<std::endl;
  connections_.erase( ptr->connection_info().connection_id() );
}
///////////////////////////////////////////////////////////////////////////////

#ifndef _MSC_VER
const std::size_t tcp_session::kDefaultPackLength;
#endif
tcp_session::tcp_session( server_impl* server, ba::io_service& iosvr,
                          server_callback* callback ) :
  server_( server ), io_service_( iosvr ), socket_( iosvr ),
  callback_( callback ), deadline_( iosvr ), connect_timeout_( 10 )
{
  assert( callback_ != NULL );
  conn_info_.new_id();
  //std::cout << "create new session :" << conn_info_.connection_id() << std::endl;
  callback_->on_connect( conn_info_ );
}
tcp_session::~tcp_session()
{
  //std::cout << "free session :" << conn_info_.connection_id() << std::endl;
  deadline_.expires_at( boost::posix_time::pos_infin );
  free_memory();
}
void tcp_session::start()
{
  //std::cout << "start to recv data...," << input_buffer_.size() << std::endl;
  check_deadline();
  reset_request();
  reset_response();

  std::size_t readlen = 0;
  if( PTH_LENGTH > input_buffer_.size() )
    readlen = PTH_LENGTH - input_buffer_.size();
  ba::async_read( socket_, input_buffer_, ba::transfer_at_least( readlen ),
                  boost::bind( &tcp_session::handle_read, shared_from_this(),
                               ba::placeholders::error, ba::placeholders::bytes_transferred ) );
}
void tcp_session::async_send_queue( const protocol_message_ptr response )
{
  output_buffer_.consume( output_buffer_.size() );
  std::ostream stream( &output_buffer_ );
  response_.data = response;
  response_.head.message_length = response->total_length();
  stream << response_.head;

  std::vector<ba::const_buffer> send_vec;
  send_vec.push_back( ba::buffer( output_buffer_.data() ) );
  std::size_t total_send = 0;
  for( protocol_message* m = response.get();
       m != NULL; m = m->next() )
  {
    if( m->rd_left() == 0 )
      continue;
    send_vec.push_back( ba::buffer( m->rd(), m->rd_left() ) );
    total_send += m->rd_left();
  }
  if( response_.head.message_length != total_send )
  {
    std::cout << "message length error" << std::endl;
  }
  //std::cout << "send response : " << output_buffer_.size() << std::endl;
  ba::async_write( socket_, send_vec,
                   boost::bind( &tcp_session::handle_write, shared_from_this(),
                                ba::placeholders::error, ba::placeholders::bytes_transferred ) );
  //std::cout << "send response to client ," << response_.head.message_length 
  //  <<" : "<<send_vec.size()<< std::endl;

}
void tcp_session::close()
{
  deadline_.expires_at( boost::posix_time::pos_infin );
  if( socket_.is_open() )
    socket_.close();
  callback_->on_disconnect( this->conn_info_ );
}
void tcp_session::free_memory()
{
  output_buffer_.consume( output_buffer_.size() );
  input_buffer_.consume( input_buffer_.size() );
}
void tcp_session::handle_read( const boost::system::error_code& ec, std::size_t byte_transferred )
{
  if( ec )
  {
    //TCLOG( "接收数据错误，" << ec.value() << " : " << ec.message() );
    std::cout << "server , read error: " << ec.value() << " : " << ec.message() << std::endl;
    free();
    return;
  }
  // read header
  //std::cout<<"tcp_session::handle_read : "<<input_buffer_.size()<<std::endl;
  std::istream stream( &input_buffer_ );
  stream >> request_.head;
  if( request_.head.message_length == 0 )
  {
    //std::cout << "tcp_session::handle_read no message" << std::endl;
    start(); // 继续接收
    return;
  }

  if( request_.head.major_version != protocol::kMajorVersion )
  {
    // version number not matched!
    free();
    return;
  }
  std::size_t remain_size = 0;
  if( request_.head.message_length > input_buffer_.size() )
    remain_size = request_.head.message_length - input_buffer_.size();

  remain_size = std::min( remain_size, kDefaultPackLength );
  //std::cout << "tcp_session::handle_read ," << request_.head.message_length
  //          << " success :" << input_buffer_.size()
  //          << " , left " << remain_size << std::endl;
  ba::async_read( socket_, input_buffer_, ba::transfer_at_least( remain_size ),
                  boost::bind( &tcp_session::handle_read_body, shared_from_this(),
                               ba::placeholders::error ) );
}
void tcp_session::handle_read_body( const boost::system::error_code& ec )
{
  if( ec )
  {
    std::cout << "server , read error: " << ec.value() << " : " << ec.message() << std::endl;
    server_->close_session( shared_from_this() );
    return;
  }

  std::istream stream( &input_buffer_ );
  protocol_message* msg = request_.data.get();
  std::size_t read_len = request_.head.message_length;
  if( msg )
    read_len -= msg->total_length();

  //std::cout << "tcp_session::handle_read_body continue total["<<request_.head.message_length<<"][" << read_len
  //          << "][" << input_buffer_.size() << "]" << std::endl;

  read_len = std::min( read_len , input_buffer_.size() );
  copy_message_buffer( stream, read_len, msg );

  if( request_.data->total_length() < request_.head.message_length )
  {
    std::size_t remain_size = request_.head.message_length - request_.data->total_length();
    remain_size = std::min( remain_size , kDefaultPackLength );
    //std::cout<<"tcp_session::handle_read_body continue : "<<remain_size<<std::endl;
    ba::async_read( socket_, input_buffer_, ba::transfer_at_least( remain_size ),
                    boost::bind( &tcp_session::handle_read_body, shared_from_this(),
                                 ba::placeholders::error ) );
    return;
  }
  //std::cout<<"recv full body success"<<std::endl;
  std::size_t error_code = 0;
  memcpy( &response_.head, &request_.head, sizeof( response_.head ) );
  //std::cout << "tcp_session::handle_read_body success " << std::endl;
  int deal_mode = callback_->on_recv( conn_info_, request_.data, response_.data );
  request_.data.reset();
  //std::cout << "callback::on_recv , ret[" << deal_mode << " , " <<
  //          ( response_.data.get() != NULL ) << std::endl;
  if( deal_mode == server_callback::r_response && response_.data.get() != NULL )
  {
    //std::cout<<"continue response"<<std::endl;
    async_send_queue( response_.data );
  }
  else if( deal_mode == server_callback::r_error )
  {
    //std::cout<<"continue error"<<std::endl;
    error_response( 9999 );
    free();
  }
  else if( deal_mode == server_callback::r_success )
  {
    //std::cout<<"continue recv"<<std::endl;
    start(); // 继续接收
  }
  else if( deal_mode == server_callback::r_wait_send )
  {
    // wait for send to client
    //std::cout<<"continue recv"<<std::endl;
    start(); // 继续接收
  }
  else
  {
    //std::cout<<"continue exception"<<std::endl;
    free();
  }

}
void tcp_session::error_response( std::size_t error_code )
{
}
void tcp_session::handle_write( const boost::system::error_code& ec, std::size_t byte_transferred )
{
  // write data
  if( ec )
  {
    std::cout << "tcp_session::handle_write error, " << ec.value() << " : " << ec.message() << std::endl;
    error_response( 9997 );
    free();
    return;
  }
  //LOG ( DEBUG, "应答成功，:" << output_buffer_.size() );
  // success
  if( callback_->on_send( conn_info_, response_.data ) )
  {
    //LOG ( ERROR, "callback_->on_send error" );
    free();
    return;
  }
  response_.data.reset();
  //start(); // 继续接收

}
void tcp_session::check_deadline()
{
#if 0
  deadline_.expires_from_now( boost::posix_time::seconds( connect_timeout_ ) );
  deadline_.async_wait( boost::bind( &tcp_session::handle_timeout, shared_from_this(),
                                     ba::placeholders::error ) );
#else
  deadline_.expires_at( boost::posix_time::pos_infin );
#endif
}
void tcp_session::handle_timeout( const boost::system::error_code& ec )
{
  if( deadline_.expires_at() <= ba::deadline_timer::traits_type::now() )
  {
    callback_->on_timeout( conn_info_ );
    server_->close_session( shared_from_this() );
    return;
  }
  deadline_.async_wait( boost::bind( &tcp_session::handle_timeout, shared_from_this(),
                                     ba::placeholders::error ) );
}
void tcp_session::reset_request()
{
  //std::cout << "reset request..." << std::endl;
  request_.head.major_version = protocol::kMajorVersion;
  request_.head.minor_version = protocol::kMinorVersion;
  request_.head.first_flag = 1;
  request_.head.next_flag = 0;
  request_.head.message_id = new_message_id();
  request_.head.sequence_no = 1;
  request_.head.encrypt_type = 0;
  memset( request_.head.crc, 0xFF, sizeof( request_.head.crc ) );
  request_.data.reset( new protocol_message() );
}
void tcp_session::reset_response()
{
  //std::cout << "reset response..." << std::endl;
  response_.head.major_version = protocol::kMajorVersion;
  response_.head.minor_version = protocol::kMinorVersion;
  response_.head.first_flag = 1;
  response_.head.next_flag = 0;
  response_.head.message_id = 0;
  response_.head.sequence_no = 1;
  response_.head.encrypt_type = 0;
  memset( response_.head.crc, 0xFF, sizeof( response_.head.crc ) );
  response_.data.reset();
}
void tcp_session::free()
{
  server_->close_session( shared_from_this() );
}
///////////////////////////////////////////////////////////////////////////////
}
}


