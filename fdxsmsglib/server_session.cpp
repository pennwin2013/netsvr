#include "sequence_id.hpp"
#include "singleton.hpp"
#include "server_session.h"
#include "client_session.h"
#include "details/server_sess_impl.h"
#include "details/client_sess_impl.h"


namespace FDXSLib
{
///////////////////////////////////////////////////////////////////////////////
typedef FDXSSingleton<sequence_id<boost::mutex, connect_info> > connection_sequence_id;

connect_info::connect_info() : connection_id_( 0 )
{
}
connect_info::connect_info( std::size_t id ): connection_id_( id )
{
}
connect_info::~connect_info()
{
}
void connect_info::new_id()
{
  connection_id_ = connection_sequence_id::get()->next_id();
}
///////////////////////////////////////////////////////////////////////////////
#define TEST_IMPLEMENT do{ if(NULL == implement_) implement_ = new details::server_impl(); }while(0)

server::server() : implement_( NULL )
{

}
server::~server()
{
  if( NULL != implement_ )
  {
    delete implement_;
    implement_ = NULL;
  }
}
int server::start( std::size_t port )
{
  TEST_IMPLEMENT;
  return implement_->start( port );
}
void server::stop()
{
  TEST_IMPLEMENT;
  implement_->stop();
}
bool server::is_running() const
{
  return implement_->is_running();
}
server_callback* server::register_callback( server_callback* callback )
{
  TEST_IMPLEMENT;
  return implement_->register_callback( callback );
}
int server::push_response( const connect_info& conn_info, protocol_message_ptr response )
{
  TEST_IMPLEMENT;
  return implement_->push_response( conn_info, response );
}
void server::close_connection( const connect_info& conn_info )
{
  TEST_IMPLEMENT;
  implement_->close_connection( conn_info );
}
///////////////////////////////////////////////////////////////////////////////
#define TEST_CLIENT_IMPLEMENT do{ if(NULL == implement_) implement_ = new details::client_impl(); }while(0)
client::client() : implement_( 0 )
{
}
client::~client()
{
  if( implement_ != NULL )
    delete implement_;
}
bool client::connect( const char* ip, std::size_t port, std::size_t timeout )
{
  TEST_CLIENT_IMPLEMENT;
  return implement_->connect( ip, port, timeout );
}
void client::disconnect()
{
  TEST_CLIENT_IMPLEMENT;
  implement_->disconnect();
}
bool client::send_message( const protocol_message* message, std::size_t timeout )
{
  TEST_CLIENT_IMPLEMENT;
  return implement_->send_message( message, timeout );
}
bool client::recv_message( protocol_message* message, std::size_t timeout )
{
  TEST_CLIENT_IMPLEMENT;
  return implement_->recv_message( message, timeout );
}
///////////////////////////////////////////////////////////////////////////////
}

