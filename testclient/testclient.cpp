#include <iostream>
#include <string>
#include "client_session.h"

using namespace std;
int main( int argc, char* const argv[] )
{
  int count = 1;
  if( argc == 2 )
  {
    count = atoi( argv[1] );
  }
  cout<<"test "<<count<<" times"<<endl;
  FDXSLib::client client;
  if( !client.connect( "127.0.0.1", 10000, 1000 ) )
  {
    std::cout << "connect server error" << std::endl;
    return -1;
  }
  for( int i = 0; i < count; ++i )
  {
    FDXSLib::protocol_message* msg = new FDXSLib::protocol_message;
    strcpy( msg->wr(), "hello" );
    msg->wr( 5 );
    if( !client.send_message( msg, 5000 ) )
    {
      cout << "send to server error" << endl;
      return -1;
    }
    msg->release();

    msg = new FDXSLib::protocol_message;
    if( !client.recv_message( msg, 5000 ) )
    {
      cout << "recv message block error" << endl;
      return -1;
    }
    msg->release();
    
  }
  cout<<"test message success"<<endl;
  client.disconnect();
  return 0;
}