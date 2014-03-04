#ifndef _QUEUE_STACK_H_
#define _QUEUE_STACK_H_

#include <string>
#include <map>
#include <list>
#include <deque>
#include <boost/thread.hpp>
#include "server_session.h"
#include "event_queue.h"

extern bool application_kill_flag;//全局变量

namespace netsvr
{
///////////////////////////////////////////////////////////////////////////////
struct netsvr_message
{
public:
  enum { nm_sendmsg = 0, nm_querymsg ,nm_funcno};//==20131227修改
  //typedef RequestProtocol message_buffer_type;
  typedef transfer::AMD_CallServer_SendMsgPtr callback_type;
  netsvr_message(): status( 0 ), msg_type( 0 ), data_length( 0 ),
    bin_length( 0 ), return_code( 0 )
  {
  }
  template<class T>
  netsvr_message( int t, transfer::RequestProtocol& msg, const T& callback ):
    status( 0 ), msg_type( t ), data_length( 0 ), bin_length( 0 ), return_code( 0 )
  {
    this->cb = boost::any_cast<T>( callback );
    this->service_code = msg.serviceCode;
    this->internal_id = msg.internalID;
    this->request_code = msg.requestCode;
    update_time();
    FDXSLib::protocol_message* m = 0;
    copy_message( msg.data, m );
    data.reset( m );
    data_length = data->total_length();
    if( msg.bindata.size() > 0 )
    {
      m = 0;
      copy_message( msg.bindata, m );
      data->add_tail( m );
      bin_length = msg.bindata.size();
    }
  }
  netsvr_message( FDXSLib::protocol_message_ptr& msg, std::size_t dlen,
                  std::size_t blen, boost::any& callback ):
    status( 0 ), data_length( dlen ), bin_length( blen ), return_code( 0 )
  {
    this->data = msg;
    cb = callback;
    update_time();
  }

  netsvr_message( const netsvr_message& rhs )
  {
    this->cb = rhs.cb;
    this->msg_type = rhs.msg_type;
    this->service_code = rhs.service_code;
    this->internal_id = rhs.internal_id;
    this->request_code = rhs.request_code;
    this->status = rhs.status;
    this->create_time = rhs.create_time;
	//this->create_micro_time = rhs.create_micro_time;//==20140116新添加
    this->data = rhs.data;
    this->data_length = rhs.data_length;
    this->bin_length = rhs.bin_length;
    this->return_code = rhs.return_code;
    this->return_message = rhs.return_message;
  }
  netsvr_message& operator=( const netsvr_message& rhs )
  {
    this->cb = rhs.cb;
    this->msg_type = rhs.msg_type;
    this->service_code = rhs.service_code;
    this->internal_id = rhs.internal_id;
    this->request_code = rhs.request_code;
    this->status = rhs.status;
    this->create_time = rhs.create_time;
	//this->create_micro_time = rhs.create_micro_time;//==20140116新添加
    this->data = rhs.data;
    this->data_length = rhs.data_length;
    this->bin_length = rhs.bin_length;
    this->return_code = rhs.return_code;
    this->return_message = rhs.return_message;
    return *this;
  }
  inline void update_time();
private:
  void copy_message( transfer::ByteSeq& seq, FDXSLib::protocol_message*& data );
public:
  std::size_t service_code;
  std::size_t request_code;
  std::size_t internal_id;
  std::size_t status;
  std::size_t msg_type;
  std::size_t data_length;
  std::size_t bin_length;
  std::size_t return_code;
  std::string return_message;
  FDXSLib::protocol_message_ptr data;
  boost::system_time create_time;
  //boost::system_time create_micro_time;//==20140116新添加，微秒级的创建时间
  boost::any cb;
};

typedef boost::shared_ptr<netsvr_message> netsvr_message_ptr;
//====20131226新添加，统计信息的结构体
struct funcno_info
{
	funcno_info():m_cnt(0),m_total_time(0),
		m_min_time(999999),m_max_time(0){}
	size_t m_cnt;//处理次数
	size_t m_min_time;//最短处理时间,单位：毫秒
	size_t m_max_time;//最长处理时间
	size_t m_total_time;//处理总时间
	std::string m_desc;//功能号描述
};
struct link_info
{
	link_info();
  ~link_info();
  link_info( const link_info& rhs );
  link_info& operator=( const link_info& rhs );
  std::size_t process_id;
  std::size_t connect_id;
  std::size_t link_id;
  std::size_t internal_id;
  bool request_msg;//true---表示空闲; false---表示占用中，不可使用
  bool register_flag;
  netsvr_message_ptr last_msg_ptr;
  boost::system_time start_militime;//==20140122新添加，记录开始处理任务的时刻
};

typedef std::list<netsvr_message_ptr> message_queue_type;

struct worklink_event_object
{
  FDXSLib::connect_info conn;
  server_message_head head;
  FDXSLib::protocol_message_ptr message;
};

typedef boost::shared_ptr<worklink_event_object> worklink_event_object_ptr;

enum
{
  iec_sendmsg_begin = 1, iec_sendmsg_end,
  iec_querymsg_begin, iec_querymsg_end, iec_query_msg_next ,
  iec_funcno_query//==20131227新添加
};

struct ice_event_object
{
  std::size_t event_code;
  FDXSLib::connect_info conn;
  netsvr_message_ptr message;
};
typedef boost::shared_ptr<ice_event_object> ice_event_object_ptr;

enum { qs_mt_on_close = mt_customer_begin + 1 , qs_mt_check_link };
class queue_manager;
struct queue_stack
{
public:
  typedef event_queue::event_function_handler evt_function;
  typedef std::map<std::size_t,funcno_info> funcno_info_map;//==20131226新添加
  queue_stack( queue_manager& manager );
  ~queue_stack();
  std::string name_;
  std::string desc_;
  int service_code_;
  std::size_t process_count_;
  std::size_t max_process_count_;
  std::string process_name_;
  std::size_t max_queue_;
  std::size_t max_size_;
  std::size_t current_max_process_;
  boost::posix_time::ptime ecardsvr_start_time;//==20140113新添加，存放该ecardsvr开启时间
  std::set<std::size_t> funcno_list;//==20131230新添加，存放有哪些功能号
  funcno_info_map funcno_info_map_;//==20131230新添加，存放每个功能号的信息，
  std::size_t get_running_time();//==20140113新添加，获取ecardsvr的运行时间
  bool connected_flag;//==20140116新添加，标识是否ecardsvr是否已经发送过功能号
  bool terminate_flag;//20140117新添加，标志该queue_stack下的worklink是否可以退出
  void record_funcno_info(link_info& link);//==20140116新添加,统计本次功能号处理的相关信息
  void startup();
  void clear();
  int push( const netsvr_message_ptr& pack );
  int peek( netsvr_message_ptr& pack );
  void run();
  void stop();
  void post_worklink( const FDXSLib::connect_info& conn_info,
                      const server_message_head& head,
                      FDXSLib::protocol_message_ptr& request );
  void post_worklink( std::size_t evt_code , const FDXSLib::connect_info& conn );
  void post_worklink( std::size_t evt_code );
  void post_ice( std::size_t evt, const netsvr_message_ptr& message );
  void post_ice( std::size_t evt, const FDXSLib::connect_info& conn,
                 const netsvr_message_ptr& message );
private:
  typedef boost::mutex::scoped_lock lock_type;
  // 连接序号 = 连接对象
  typedef std::map<std::size_t, link_info> process_map;
  //typedef std::deque<response_pack> response_queue_type;
  void check_request_queue();
  ///////////////////////////////////////////////////////////////////////////////
  void on_worklink_event( worklink_event_object_ptr ptr );
  void on_clt_register( worklink_event_object_ptr& ptr );
  void on_clt_heartbeat( worklink_event_object_ptr& ptr );
  void on_clt_queryreq( worklink_event_object_ptr& ptr );
  void on_clt_response( worklink_event_object_ptr& ptr );
  void on_clt_funcno( worklink_event_object_ptr& ptr );//==20131224新添加==
  void on_clt_pushreq( worklink_event_object_ptr& ptr );
  void on_clt_callreq( worklink_event_object_ptr& ptr );
  void on_clt_error( worklink_event_object_ptr& ptr );
  void on_clt_close( worklink_event_object_ptr& ptr );
  void on_clt_check_link( worklink_event_object_ptr& ptr );

  void on_ice_event( ice_event_object_ptr ptr );
  void on_ice_sendmsg_begin( ice_event_object_ptr& ptr );
  void on_ice_sendmsg_end( ice_event_object_ptr& ptr );
  ///////////////////////////////////////////////////////////////////////////////
  bool query_msg_end( link_info& link, netsvr_message_ptr& pack );
  void copy_vector( ::transfer::ByteSeq& vec, std::size_t begin,
                    FDXSLib::protocol_message_ptr& msg, std::size_t maxlen );
  void ice_response_error( const netsvr_message_ptr& message, int errcode, const std::string& errmsg );
  ///////////////////////////////////////////////////////////////////////////////
  bool if_file_exists( const std::string& f );
  boost::mutex mutex_;
  //! 客户端请求的任务队列
  message_queue_type input_queue_;
  process_map processes_;
  boost::condition_variable cond_;
  event_queue worklink_events_;
  event_queue ice_events_;
  queue_manager& manager_;
  bool running_;
  bool fronzen_;
  int free_link_cnt_;

};
///////////////////////////////////////////////////////////////////////////////
}

#endif // _QUEUE_STACK_H_
