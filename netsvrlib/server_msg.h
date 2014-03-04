#ifndef _SERVER_MSG_H_
#define _SERVER_MSG_H_

#include "server_session.h"

namespace netsvr
{
///////////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#pragma pack(1)
#define PACK
#elif defined(_GCC)
typedef unsigned short u_short;
#define PACK __attribute__ ((packed))
#else
#define PACK
#endif

enum {mt_register = 0x01, // 启动签到请求
      mt_heartbeat , // 心跳
      mt_queryreq,   // 获取请求任务
      mt_response,   // 应答请求
      mt_pushreq,    // 推送请求
      mt_callreq,     // 外调请求
	  mt_funcno,      //========20131224新添加，提交功能号请求=========
      mt_customer_begin = 0xA0 , // 自定义消息起始,
      mt_customer_end = 0xFE, // 自定义消息结束
     };

#define SVR_MSG_HEAD_LEN 12
struct server_message_head
{
  u_short service_code;
  u_short internal_id;
  char client_name[64];//
  char message_code;
  char rfu[3];
} PACK;

///////////////////////////////////////////////////////////////////////////////
struct svr_msg_register
{
  std::size_t process_id;
  std::size_t link_id;
} PACK;

FDXSLib::protocol_message& operator>>( FDXSLib::protocol_message& msg, svr_msg_register& data );
FDXSLib::protocol_message& operator<<( FDXSLib::protocol_message& msg, svr_msg_register& data );

struct svr_msg_heartbeat
{
  std::size_t link_id;
} PACK;

FDXSLib::protocol_message& operator>>( FDXSLib::protocol_message& msg, svr_msg_heartbeat& data );
FDXSLib::protocol_message& operator<<( FDXSLib::protocol_message& msg, svr_msg_heartbeat& data );

struct svr_msg_queryreq
{
  std::size_t link_id;
  std::size_t return_code;
  char first_flag;
  char next_flag;
  char msg_flag;
  char bin_flag;
  std::size_t message_length;
  std::size_t bin_length;
} PACK;
FDXSLib::protocol_message& operator>>( FDXSLib::protocol_message& msg, svr_msg_queryreq& data );
FDXSLib::protocol_message& operator<<( FDXSLib::protocol_message& msg, svr_msg_queryreq& data );

struct svr_msg_response
{
  std::size_t link_id;
  std::size_t return_code;
  char has_data;
  char bin_data;
  std::size_t data_length;
  std::size_t bin_length;
} PACK;
FDXSLib::protocol_message& operator>>( FDXSLib::protocol_message& msg, svr_msg_response& data );
FDXSLib::protocol_message& operator<<( FDXSLib::protocol_message& msg, svr_msg_response& data );


//新添加一种类型====20131223====
const size_t MaxSendLen = 20;
//功能号信息单元
struct client_func_info
{
	std::size_t funcno;
	char funcdesc[64];
}PACK;
struct svr_msg_funcno
{
  std::size_t link_id;
  std::size_t return_code;
  std::size_t func_count;
  client_func_info data[1];
} PACK;
FDXSLib::protocol_message& operator>>( FDXSLib::protocol_message& msg, svr_msg_funcno& data );
FDXSLib::protocol_message& operator<<( FDXSLib::protocol_message& msg, svr_msg_funcno& data );


#ifdef _MSC_VER
#pragma pack()
#elif defined(_GCC)
#undef PACK
#else
#undef PACK
#endif

FDXSLib::protocol_message& operator>>( FDXSLib::protocol_message& msg, server_message_head& data );
FDXSLib::protocol_message& operator<<( FDXSLib::protocol_message& msg, const server_message_head& data );

/*
void buffer_2_message_head( const char* buffer, server_message_head& head );
std::size_t message_head_2_buffer( const server_message_head& head, char* buffer );
*/
///////////////////////////////////////////////////////////////////////////////
}

#endif // _SERVER_MSG_H_
