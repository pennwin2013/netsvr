#define BOOST_TEST_MODULE test_svr_sess
#include <string>
#include <fstream>
#include <boost/test/unit_test.hpp>
#include "server_session.h"
#include "client_session.h"

using namespace FDXSLib;
using namespace std;

class test_callback : public FDXSLib::server_callback
{
public:
  test_callback() : need_response_( true )
  {}
  virtual ~test_callback()
  {}
  int on_connect( connect_info& conn_info )
  {
    BOOST_TEST_MESSAGE( "new connection id : " << conn_info.connection_id() );
    return 0;
  }
  int on_disconnect( connect_info& conn_info )
  {
    BOOST_TEST_MESSAGE( "connection closed id : " << conn_info.connection_id() );
    return 0;
  }
  int on_recv( connect_info& conn_info, protocol_message_ptr request, protocol_message_ptr& response )
  {
    if( need_response_ )
    {
      response.reset(new protocol_message);
      if( request->length() > 0 )
      {
        memcpy( response->wr(), request->rd(), request->length() );
        response->wr( request->length() );
      }
      response->next( request->next() );
      request->next( NULL );
      return 1;
    }
    return 0;
  }
  int on_send( connect_info& conn_info, protocol_message_ptr response )
  {
    BOOST_TEST_MESSAGE( "connection : " << conn_info.connection_id() << " | send message success" );
    return 0;
  }
  int on_timeout( connect_info& conn_info )
  {
    BOOST_TEST_MESSAGE( "connection : " << conn_info.connection_id() << " has timeout" );
    return 0;
  }
  inline void set_need_response( bool r = true )
  {
    need_response_ = r;
  }
private:
  bool need_response_;
};
///////////////////////////////////////////////////////////////////////////////
class test_server
{
public:
  static const std::size_t kPort = 10000;
  test_server() : log_file_( "test_svr_sess.txt" )
  {
    boost::unit_test::unit_test_log.set_stream( log_file_ );
    BOOST_TEST_MESSAGE( "setup server fixture" );
    start();
  }
  ~test_server()
  {
    BOOST_TEST_MESSAGE( "shutdown server fixture" );
    stop();
    boost::unit_test::unit_test_log.set_stream( std::cout );
  }
  server* server()
  {
    return &server_;
  }
  test_callback* callback()
  {
    return &callback_;
  }
  client* client()
  {
    return &client_;
  }
private:
  void start()
  {
    server_.register_callback( &callback_ );
    server_.start( kPort );
  }
  void stop()
  {
    client_.disconnect();
    server_.stop();
  }
  FDXSLib::server server_;
  FDXSLib::client client_;
  test_callback callback_;
  std::ofstream log_file_;
};

BOOST_FIXTURE_TEST_SUITE( svr, test_server )

BOOST_AUTO_TEST_CASE( test_init )
{
  BOOST_CHECK( server()->is_running() );
  /*
  BOOST_CHECK( client()->connect( "127.0.0.1", kPort, 1000 ) );

  protocol_message* msg = new protocol_message;
  strcpy( msg->wr(), "hello" );
  msg->wr( 5 );
  BOOST_CHECK( client()->send_message( msg, 5000 ) );
  msg->release();

  msg = new protocol_message;
  BOOST_CHECK( client()->recv_message( msg, 5000 ) );
  msg->release();
  */
  string line;
  cout<<"press enter to exit"<<endl;
  cin>>line;
}


BOOST_AUTO_TEST_SUITE_END()
