#include <iostream>
#include "server_session.h"


using namespace std;
using namespace FDXSLib;

class test_callback : public FDXSLib::server_callback
{
public:
  test_callback() : need_response_( true )
  {}
  virtual ~test_callback()
  {}
  int on_connect( connect_info& conn_info )
  {
    cout << "new connection id : " << conn_info.connection_id() << endl;
    return 0;
  }
  int on_disconnect( connect_info& conn_info )
  {
    cout << "connection closed id : " << conn_info.connection_id() << endl;
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
    cout << "connection : " << conn_info.connection_id() << " | send message success" << endl;
    return 0;
  }
  int on_timeout( connect_info& conn_info )
  {
    cout << "connection : " << conn_info.connection_id() << " has timeout" << endl;
    return 0;
  }
  inline void set_need_response( bool r = true )
  {
    need_response_ = r;
  }
private:
  bool need_response_;
};

int main()
{
  test_callback callback;
  FDXSLib::server server;
  server.register_callback(&callback);
  if(server.start(10000))
  {
    cout<<"start server error"<<endl;
    return -1;
  }
  cout<<"Press any key to exist..."<<endl;
  string input;
  cin>>input;
  server.stop();
  return 0;
}