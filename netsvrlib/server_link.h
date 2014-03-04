#ifndef _SERVER_LINK_H_
#define _SERVER_LINK_H_

#include "client_session.h"
#include "server_msg.h"

struct server_message
{
  char* message;
  std::size_t message_length;
  FDXSLib::protocol_message* bin_data;
  server_message():message(0),message_length(0),bin_data(0)
  {
  }
  ~server_message()
  {
    if(message != 0)
    {
      delete message;
      message_length = 0;
    }
    if( bin_data != 0)
    {
      bin_data->release();
    }
  }
};

struct server_info
{
  char svrip[64];
  std::size_t svrport;
  std::size_t service_code;
  std::size_t internal_id;
};


class server_link
{
public:
  server_link(server_info& server);
  ~server_link();
  bool connect_to(int timeout);
  bool is_connected();
  bool test_connect();
  bool send_funcno(netsvr::client_func_info* function_list, size_t list_size);
  bool get_message( server_message& msg );
  bool answer_data( const server_message& msg );
  bool push_data( const server_message& msg );
  bool call( const server_message& req, server_message& resp );
private:
  void prepare_message(netsvr::server_message_head &head,int msgtype = 0);
  FDXSLib::client client_;
  server_info& server_;
  bool is_connected_;
  std::size_t link_id_;
  char client_name[64];
};


#endif // _SERVER_LINK_H_
