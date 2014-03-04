#ifdef WIN32
#include <Windows.h>
#else
#include <arpa/inet.h>
#endif
#include "server_msg.h"

namespace netsvr
{
///////////////////////////////////////////////////////////////////////////////
static void buffer_2_u_short( const char* buffer, u_short& val )
{
  u_short temp = 0;
  char* p = ( char* )&temp;
  memcpy( p, buffer, sizeof( u_short ) );
  val = ntohs( temp );
}

static void buffer_2_size( const char* buffer, std::size_t& val )
{
  std::size_t temp = 0;
  char* p = ( char* )&temp;
  memcpy( p, buffer, sizeof( std::size_t ) );
  val = ntohl( temp );
}

static std::size_t buffer_2_message_head( const char* buffer, server_message_head& head )
{
  /*
  u_short service_code;
  u_short internal_id;
  char message_code;
  char rfu[3];
  */
  std::size_t offset = 0;
  buffer_2_u_short( buffer + offset, head.service_code );
  offset += 2;
  buffer_2_u_short( buffer + offset, head.internal_id );
  offset += 2;
  head.message_code = buffer[offset++];
  memcpy( head.rfu, buffer + offset, 3 );
  offset += 3;
  return offset;
}

static std::size_t u_short_2_buffer(u_short val, char *buffer)
{
  u_short temp = htons(val);
  const char* p = (const char*)&temp;
  memcpy(buffer,p,sizeof(u_short));
  return sizeof(u_short);
}
static std::size_t size_2_buffer(std::size_t val,char *buffer)
{
  std::size_t temp = htonl(val);
  const char* p = (const char*)&temp;
  memcpy(buffer,p,sizeof(std::size_t));
  return sizeof(std::size_t);
}

static std::size_t message_head_2_buffer( const server_message_head& head, char* buffer )
{
  /*
  u_short service_code;
  u_short internal_id;
  char message_code;
  char rfu[3];
  */
  std::size_t offset = 0;
  offset += u_short_2_buffer(head.service_code,buffer+offset);
  offset += u_short_2_buffer(head.internal_id,buffer+offset);
  buffer[offset++] = head.message_code;
  memcpy(buffer+offset,head.rfu,sizeof(head.rfu));
  offset += sizeof(head.rfu);
  return offset;
}
FDXSLib::protocol_message& operator>>(FDXSLib::protocol_message& msg,server_message_head& data)
{
  std::size_t t = buffer_2_message_head(msg.rd(),data);
  msg.rd(t);
  return msg;
}
FDXSLib::protocol_message& operator<<(FDXSLib::protocol_message& msg,const server_message_head& data)
{
  std::size_t t = message_head_2_buffer(data,msg.wr());
  msg.wr(t);
  return msg;
}
///////////////////////////////////////////////////////////////////////////////
FDXSLib::protocol_message& operator>>(FDXSLib::protocol_message& msg,svr_msg_register& data)
{
  buffer_2_size(msg.rd(),data.process_id);
  msg.rd(sizeof(std::size_t));
  buffer_2_size(msg.rd(),data.link_id);
  msg.rd(sizeof(std::size_t));
  //buffer_2_u_short(msg.rd(),data.internal_id);
  //msg.rd(sizeof(u_short));
  return msg;
}
FDXSLib::protocol_message& operator<<(FDXSLib::protocol_message& msg,svr_msg_register& data)
{
  size_2_buffer(data.process_id,msg.wr());
  msg.wr(sizeof(std::size_t));
  size_2_buffer(data.link_id,msg.wr());
  msg.wr(sizeof(std::size_t));
  //u_short_2_buffer(data.internal_id,msg.wr());
  //msg.wr(sizeof(u_short));
  return msg;
}
///////////////////////////////////////////////////////////////////////////////
FDXSLib::protocol_message& operator>>(FDXSLib::protocol_message& msg,svr_msg_heartbeat& data)
{
  buffer_2_size(msg.rd(),data.link_id);
  msg.rd(sizeof(std::size_t));
  return msg;
}
FDXSLib::protocol_message& operator<<(FDXSLib::protocol_message& msg,svr_msg_heartbeat& data)
{
  size_2_buffer(data.link_id,msg.wr());
  msg.wr(sizeof(std::size_t));
  return msg;
}
///////////////////////////////////////////////////////////////////////////////
FDXSLib::protocol_message& operator>>(FDXSLib::protocol_message& msg,svr_msg_queryreq& data)
{
  buffer_2_size(msg.rd(),data.link_id);
  msg.rd(sizeof(std::size_t));
  buffer_2_size(msg.rd(),data.return_code);
  msg.rd(sizeof(std::size_t));
  data.first_flag = msg.rd()[0];
  data.next_flag = msg.rd()[1];
  data.msg_flag = msg.rd()[2];
  data.bin_flag = msg.rd()[3];
  msg.rd(4);
  buffer_2_size(msg.rd(),data.message_length);
  msg.rd(sizeof(std::size_t));
  buffer_2_size(msg.rd(),data.bin_length);
  msg.rd(sizeof(std::size_t));
  return msg;
}
FDXSLib::protocol_message& operator<<(FDXSLib::protocol_message& msg,svr_msg_queryreq& data)
{
  size_2_buffer(data.link_id,msg.wr());
  msg.wr(sizeof(std::size_t));
  size_2_buffer(data.return_code,msg.wr());
  msg.wr(sizeof(std::size_t));
  msg.wr()[0] = data.first_flag;
  msg.wr()[1] = data.next_flag;
  msg.wr()[2] = data.msg_flag;
  msg.wr()[3] = data.bin_flag;
  msg.wr(4);
  size_2_buffer(data.message_length,msg.wr());
  msg.wr(sizeof(std::size_t));
  size_2_buffer(data.bin_length,msg.wr());
  msg.wr(sizeof(std::size_t));
  return msg;
}
///////////////////////////////////////////////////////////////////////////////
FDXSLib::protocol_message& operator>>(FDXSLib::protocol_message& msg,svr_msg_response& data)
{
  buffer_2_size(msg.rd(),data.link_id);
  msg.rd(sizeof(std::size_t));
  buffer_2_size(msg.rd(),data.return_code);
  msg.rd(sizeof(std::size_t));
  data.has_data = msg.rd()[0];
  data.bin_data = msg.rd()[1];
  msg.rd(2);

  buffer_2_size(msg.rd(),data.data_length);
  msg.rd(sizeof(std::size_t));

  buffer_2_size(msg.rd(),data.bin_length);
  msg.rd(sizeof(std::size_t));
  return msg;
}
FDXSLib::protocol_message& operator<<(FDXSLib::protocol_message& msg,svr_msg_response& data)
{
  size_2_buffer(data.link_id,msg.wr());
  msg.wr(sizeof(std::size_t));
  size_2_buffer(data.return_code,msg.wr());
  msg.wr(sizeof(std::size_t));
  msg.wr()[0] = data.has_data;
  msg.wr()[1] = data.bin_data;
  msg.wr(2);

  size_2_buffer(data.data_length,msg.wr());
  msg.wr(sizeof(std::size_t));

  size_2_buffer(data.bin_length,msg.wr());
  msg.wr(sizeof(std::size_t));
  return msg;
}
//从msg到data
FDXSLib::protocol_message& operator>>( FDXSLib::protocol_message& msg, svr_msg_funcno& data )
{
	buffer_2_size(msg.rd(),data.link_id);	//读取数据到data.linkid
	msg.rd(sizeof(std::size_t));//移动读指针
	buffer_2_size(msg.rd(),data.return_code);	
	msg.rd(sizeof(std::size_t));
	buffer_2_size(msg.rd(),data.func_count);
	msg.rd(sizeof(std::size_t));
	
	std::size_t buf_len;
	char* pbuffer = NULL;
	msg.copy_to_buffer(pbuffer,&buf_len,0);//第三个参数为0会把剩余的全部读出来
	if(buf_len!=data.func_count*sizeof(client_func_info))
	{
		if(pbuffer!=NULL)
		{
			delete [] pbuffer;
			pbuffer = NULL;
		}
		//报错
		throw std::overflow_error( "msg length error!" );
	}
	memcpy(data.data,pbuffer,buf_len);

	if(pbuffer!=NULL)
	{
		delete [] pbuffer;
		pbuffer = NULL;
	}
	return msg;
}
//从data到msg
FDXSLib::protocol_message& operator<<( FDXSLib::protocol_message& msg, svr_msg_funcno& data )
{
	size_2_buffer(data.link_id,msg.wr());
	msg.wr(sizeof(std::size_t));
	size_2_buffer(data.return_code,msg.wr());
	msg.wr(sizeof(std::size_t));
	size_2_buffer(data.func_count,msg.wr());
	msg.wr(sizeof(std::size_t));

	std::size_t buf_len = data.func_count*sizeof(client_func_info);
	msg.copy_from_buffer((char*)data.data,buf_len);
	return msg;
}

}
