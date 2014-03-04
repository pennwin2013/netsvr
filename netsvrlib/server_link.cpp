#include <algorithm>
#ifdef WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#endif
#include <boost/shared_ptr.hpp>
#include "server_link.h"
#include <fstream>

struct message_block_proxy
{
public:
  message_block_proxy( FDXSLib::protocol_message* msg ): msg_( msg )
  {}
  ~message_block_proxy()
  {
    msg_->release();
  }
  void reset( FDXSLib::protocol_message* msg )
  {
    msg_->release();
    msg_ = msg;
  }
  FDXSLib::protocol_message* get()
  {
    return msg_;
  }
  FDXSLib::protocol_message* operator->()
  {
    return msg_;
  }
  FDXSLib::protocol_message& operator*()
  {
    return *msg_;
  }
private:
  FDXSLib::protocol_message* msg_;
};
///////////////////////////////////////////////////////////////////////////////
server_link::server_link( server_info& server ): server_( server ),
  is_connected_( false ), link_id_( 0 )
{
}
server_link::~server_link()
{
  client_.disconnect();
}

void server_link::prepare_message( netsvr::server_message_head& head, int msgtype )
{
  head.service_code = server_.service_code;
  head.internal_id = server_.internal_id;
  head.message_code = msgtype;
}
bool server_link::connect_to( int timeout )
{
  bool ret = client_.connect( server_.svrip, server_.svrport, timeout * 1000 );
  if( !ret )
    return false;

  // register client
  netsvr::svr_msg_register reg;
#ifdef WIN32
  reg.process_id =  static_cast<std::size_t>( GetCurrentProcessId() );
#else
  reg.process_id = static_cast<std::size_t>( getpid() );
#endif
  //std::cout<<"client process id : "<<reg.process_id<<std::endl;
  netsvr::server_message_head head;
  prepare_message( head, netsvr::mt_register );
  message_block_proxy message( new FDXSLib::protocol_message );
  *message << head;
  *message << reg;

  std::size_t t = timeout * 1000;
  if( !client_.send_message( message.get(), t ) )
  {
    return false;
  }
  message.reset( new FDXSLib::protocol_message );
  if( !client_.recv_message( message.get(), t ) )
  {
    return false;
  }
  *message >> head;
  *message >> reg;
  link_id_ = reg.link_id;
  is_connected_ = true;
  return true;
}
bool server_link::is_connected()
{
  return is_connected_;
}
bool server_link::test_connect()
{
  //std::cout<<"发送心跳"<<std::endl;
  netsvr::server_message_head head;
  prepare_message( head, netsvr::mt_heartbeat );
  message_block_proxy message( new FDXSLib::protocol_message );
  *message << head;

  netsvr::svr_msg_heartbeat heartbeat;
  heartbeat.link_id = link_id_;
  *message << heartbeat;
  std::size_t t = 3000;
  is_connected_ = false;
  if( !client_.send_message( message.get(), t ) )
  {
    return false;
  }
  message.reset( new FDXSLib::protocol_message );
  if( !client_.recv_message( message.get(), t ) )
  {
    return false;
  }
  is_connected_ = true;
  return true;
}

bool server_link::send_funcno(netsvr::client_func_info* function_list, size_t list_size)
{
	//包头
	netsvr::server_message_head head;
	prepare_message( head, netsvr::mt_funcno );
	message_block_proxy message( new FDXSLib::protocol_message );
	*message << head;
	//包体
	netsvr::svr_msg_funcno* pst_msg_funcno = reinterpret_cast<netsvr::svr_msg_funcno*>(new char[sizeof(netsvr::svr_msg_funcno)+(list_size-1)*sizeof(netsvr::client_func_info)]);
	pst_msg_funcno->link_id = link_id_;
	pst_msg_funcno->return_code = 0;//
	pst_msg_funcno->func_count = list_size;
	memcpy(pst_msg_funcno->data,function_list,sizeof(netsvr::client_func_info)*list_size);

	*message << *pst_msg_funcno;
	std::size_t t = 3000;
	is_connected_ = false;
	
	bool r = client_.send_message( message.get(), t );

	delete [] pst_msg_funcno;
	pst_msg_funcno = NULL;
	if(!r)
	{
		return false;
	}

	//message.reset( new FDXSLib::protocol_message );
	//if( !client_.recv_message( message.get(), t ) )
	//{
	//  return false;
	//}
	is_connected_ = true;
	return true;
}
bool server_link::get_message( server_message& msg )
{
  //std::cout<<"获取任务"<<std::endl;
  netsvr::server_message_head head;
  prepare_message( head, netsvr::mt_queryreq );
  message_block_proxy message( new FDXSLib::protocol_message );
  *message << head;

  netsvr::svr_msg_queryreq req;
  req.first_flag = 1;
  req.next_flag = 0;
  req.return_code = 0;
  req.bin_flag = 0;
  req.msg_flag = 0;
  req.bin_length = 0;
  req.message_length = 0;
  req.link_id = link_id_;

  *message << req;

  std::size_t t = 3000;
  if( !client_.send_message( message.get(), t ) )
  {
    return false;
  }
  message.reset( new FDXSLib::protocol_message );
  if( !client_.recv_message( message.get(), 0 ) )
  {
    return false;
  }
  *message >> head;
  *message >> req;
  if( head.message_code != netsvr::mt_queryreq )
  {
    return false;
  }
  if( req.msg_flag == 0 )
  {
    // there is no message
    return true;
  }
  if( req.message_length == 0 )
  {
    return false;
  }
  msg.message_length = req.message_length;
  FDXSLib::protocol_message* last_block = message.get();
  if( req.msg_flag )
  {
    /*
    msg.message = new char[msg.message_length];
    std::size_t msg_copy_length = 0;
    for( ; last_block != NULL; last_block = last_block->next() )
    {
      if( last_block->left() == 0 )
        continue;
      std::size_t t = min( last_block->left(), msg.message_length - msg_copy_length );
      memcpy( msg.message, last_block->rd(), t );
      last_block->rd( t );
      msg_copy_length += t;
    }
    */
    message->copy_to_buffer( msg.message, &( msg.message_length ), req.message_length );
  }
  if( req.bin_flag )
  {

  }
  return true;
}
bool server_link::answer_data( const server_message& msg )
{
  //std::cout<<"应答任务处理结果"<<std::endl;
  netsvr::server_message_head head;
  prepare_message( head, netsvr::mt_response );
  message_block_proxy message( new FDXSLib::protocol_message );
  *message << head;

  netsvr::svr_msg_response resp_head;
  resp_head.link_id = link_id_;
  resp_head.return_code = 0;
  resp_head.has_data = 0;
  if( msg.message )
  {
    resp_head.has_data = 1;
    resp_head.data_length = msg.message_length;
  }
  resp_head.bin_data = 0;
  if( msg.bin_data )
  {
    resp_head.bin_data = 1;
    resp_head.bin_length = msg.bin_data->total_length();
  }
  *message << resp_head;

  if( msg.message )
  {
    message->copy_from_buffer( msg.message, msg.message_length );
  }
  if( msg.bin_data )
  {
    message->add_tail( msg.bin_data );
  }

  if( !client_.send_message( message.get(), 0 ) )
  {
    return false;
  }

  message.reset( new FDXSLib::protocol_message );
  if( !client_.recv_message( message.get(), 0 ) )
  {
    return false;
  }
  *message >> head;
  *message >> resp_head;

  if(resp_head.return_code != 0)
    return false;
  
  return true;
}
bool server_link::push_data( const server_message& msg )
{
  return false;
}
bool server_link::call( const server_message& req, server_message& resp )
{
  return false;
}
///////////////////////////////////////////////////////////////////////////////

