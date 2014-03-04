#include <fstream>
#include "queue.h"
#include "logger_imp.h"
#include "queue_stack.h"
#include "server_msg.h"
#include "subprocess.hpp"

namespace netsvr
{
using namespace FDXSLib;
///////////////////////////////////////////////////////////////////////////////
link_info::link_info()
{}
link_info::~link_info()
{}
link_info::link_info( const link_info& rhs )
{
  this->connect_id = rhs.connect_id;
  this->process_id = rhs.process_id;
  this->link_id = rhs.link_id;
  this->internal_id = rhs.internal_id;
  this->start_militime = rhs.start_militime;//==20140122新添加
  this->request_msg = rhs.request_msg;
  this->register_flag = rhs.register_flag;
  this->last_msg_ptr = rhs.last_msg_ptr;
}
link_info& link_info::operator=( const link_info& rhs )
{
  this->connect_id = rhs.connect_id;
  this->process_id = rhs.process_id;
  this->link_id = rhs.link_id;
  this->internal_id = rhs.internal_id;
  this->start_militime = rhs.start_militime;//==20140122新添加
  this->request_msg = rhs.request_msg;
  this->register_flag = rhs.register_flag;
  this->last_msg_ptr = rhs.last_msg_ptr;
  return *this;
}

///////////////////////////////////////////////////////////////////////////////
queue_stack::queue_stack( queue_manager& manager ):
  running_( false ), fronzen_( false ), manager_( manager ),free_link_cnt_(0),connected_flag(false),terminate_flag(false)//==20140116新添加
{
}
queue_stack::~queue_stack()
{
}
void queue_stack::clear()
{
  lock_type lock( mutex_ );
  // TODO: free all memory resource
  input_queue_.clear();
}
std::size_t queue_stack::get_running_time()//获取ecardsvr的运行时间
{
	boost::posix_time::ptime pEnd = boost::posix_time::second_clock::local_time();
	boost::posix_time::time_duration td(pEnd-ecardsvr_start_time);
	size_t gaps = td.total_seconds();
	return gaps;
}
void queue_stack::startup()
{
  if( fronzen_ )
    return;
  fronzen_ = true;
  for( std::size_t i = 0; i < this->max_process_count_; ++i )
  {
    link_info link;
    link.process_id = 0;
    link.register_flag = false;
	//LOG(INFO,"queue_stack::startup() 里设置link["<<i<<"].request_msg=false");
    link.request_msg = false;
    processes_[i] = link;
  }
  current_max_process_ = this->process_count_;
  worklink_events_.run();
  ice_events_.run();

}
void queue_stack::ice_response_error(const netsvr_message_ptr& message,
                                     int errcode,const std::string& errmsg)
{
  netsvr_message_ptr ptr(new netsvr_message);
  ptr->cb = message->cb;
  ptr->service_code = message->service_code;
  ptr->internal_id = message->internal_id;
  ptr->return_code = errcode;
  ptr->return_message = errmsg;
  if(message->msg_type == netsvr_message::nm_sendmsg)
  {
    post_ice( iec_sendmsg_end , ptr );
  }
  else
  {
    post_ice( iec_querymsg_end , ptr );
  }
}
int queue_stack::push( const netsvr_message_ptr& pack )
{
	if(application_kill_flag==true)
	{	
		LOG(DEBUG,"已收到kill信号，驳回客户端请求");
		ice_response_error( pack, 9998, "系统即将退出，不处理新请求");
	}
	lock_type lock( mutex_ );
	if( free_link_cnt_ > 0 )
	{
		LOG(DEBUG,"立即处理 ICE 请求");
		if( pack->msg_type == netsvr_message::nm_sendmsg )
			post_ice( iec_sendmsg_begin, pack );
		else if(pack->msg_type == netsvr_message::nm_funcno)
			post_ice( iec_funcno_query, pack);
		else
			post_ice( iec_querymsg_begin, pack );
	}
	else
	{
		LOG(DEBUG,"ICE 队列满");
		//input_queue_.push_back( pack );
		//cond_.notify_one();
		ice_response_error( pack, 9998, "系统无可用单元");
	}
	return 0;
}
int queue_stack::peek( netsvr_message_ptr& pack )
{
  lock_type lock( mutex_ );
  if( input_queue_.size() == 0 )
    return 1;
  pack = input_queue_.front();
  input_queue_.pop_front();
  return 0;
}
bool queue_stack::if_file_exists( const std::string& f )
{
#ifdef _MSC_VER
  std::locale loc = std::locale::global( std::locale( "" ) );
#endif
  std::ifstream ifs( f.c_str() );
#ifdef _MSC_VER
  std::locale::global( loc );
#endif
  return ifs.is_open();
}
void queue_stack::copy_vector( ::transfer::ByteSeq& vec, std::size_t begin,
                               FDXSLib::protocol_message_ptr& msg, std::size_t maxlen )
{
  std::size_t count = 0;
  FDXSLib::protocol_message* m;
  for( m = msg.get(); m != NULL && count < maxlen ; m = m->next() )
  {
    std::size_t i = m->rd_left();
    i = std::min( i, maxlen - count );
    vec.insert( vec.end(), m->rd(), m->rd() + i );
    m->rd( i );
    count += i;
  }
  assert( count == maxlen );
}


void queue_stack::run()
{
  running_ = true;
  post_worklink( qs_mt_check_link );
  int c = 0;
  while( running_ && !boost::this_thread::interruption_requested() )
  {
    // 检查等待任务的请求
    //
    check_request_queue();
    if(c >= 5)
    {
      c = 0;
      post_worklink( qs_mt_check_link );
    }
    ++c;
  }
  worklink_events_.stop();
  ice_events_.stop();

}
void queue_stack::check_request_queue()
{
  boost::system_time curr = boost::posix_time::second_clock::local_time();
  netsvr_message_ptr message;
  {
    lock_type lock( mutex_ );
    if( input_queue_.empty() )
    {
      if( !cond_.timed_wait( lock, boost::posix_time::seconds( 3 ) ) )
        return;
    }
    for(;;)
    {
      if( input_queue_.empty() )
        return;
      message = input_queue_.front();
      input_queue_.pop_front();
      if(message->create_time < curr)
      {
        ice_response_error( message, 9998, "系统无可用单元");
      }
      else
        break;
    }
  }
  LOG( DEBUG, "处理队列中任务" );
  if( message->msg_type == netsvr_message::nm_sendmsg )
    post_ice( netsvr::iec_sendmsg_begin, message );
  else
    post_ice( netsvr::iec_querymsg_begin, message );
}
void queue_stack::stop()
{
  running_ = false;
}

void queue_stack::post_worklink( const FDXSLib::connect_info& conn_info,
                                 const server_message_head& head,
                                 FDXSLib::protocol_message_ptr& request )
{
  worklink_event_object_ptr ptr( new worklink_event_object );
  ptr->conn = conn_info;
  ptr->head = head;
  ptr->message = request;
  worklink_events_.register_event( boost::bind( &queue_stack::on_worklink_event,
                                   this, ptr ) );
}
void queue_stack::post_worklink( std::size_t evt_code , const FDXSLib::connect_info& conn )
{
  worklink_event_object_ptr ptr( new worklink_event_object );
  ptr->conn = conn;
  ptr->head.message_code = evt_code;
  worklink_events_.register_event( boost::bind( &queue_stack::on_worklink_event,
                                   this, ptr ) );
}
void queue_stack::post_worklink( std::size_t evt_code )
{
  worklink_event_object_ptr ptr( new worklink_event_object );
  ptr->head.message_code = evt_code;
  worklink_events_.register_event( boost::bind( &queue_stack::on_worklink_event,
                                   this, ptr ) );
}
void queue_stack::on_worklink_event( worklink_event_object_ptr ptr )
{
  unsigned char m = static_cast<unsigned char>( ptr->head.message_code );
  switch( m )
  {
  case mt_register: // 签到
    on_clt_register( ptr );
    break;
  case mt_heartbeat : // 心跳
    on_clt_heartbeat( ptr );
    break;
  case mt_queryreq:   // link来获取请求任务，说明该link已经处理完处于空闲状态了
    on_clt_queryreq( ptr );
    break;
  case mt_response:   // 应答请求
    on_clt_response( ptr );
    break;
  case mt_pushreq:    // 推送请求
    on_clt_pushreq( ptr );
    break;
  case mt_callreq:     // 外调请求
    on_clt_callreq( ptr );
    break;
  case mt_funcno:   //发送功能号请求
	on_clt_funcno(ptr);
	break;
  case qs_mt_on_close:// 连接关闭
    on_clt_close( ptr );
    break;
  case qs_mt_check_link: // 检查 work link
    on_clt_check_link( ptr );
    break;
  default:
    on_clt_error( ptr ); // 无效请求，关闭连接
    break;
  }
}
void queue_stack::on_clt_error( worklink_event_object_ptr& ptr )
{
  server_instance::get()->close_connection( ptr->conn );
}
void queue_stack::on_clt_register( worklink_event_object_ptr& ptr )
{
	svr_msg_register reg;
	*( ptr->message ) >> reg;
	std::size_t link_id = 0;
	bool ok = false;
	if( !process_name_.empty() )
	{
		//LOG(INFO,"queue_stack::on_clt_register process_name_ not empty="<<process_name_);
		std::size_t t = 0;
		for( process_map::iterator i = processes_.begin() ;
			i != processes_.end() && t < current_max_process_; ++i, ++t )
		{
			link_info& link = i->second;
			if( !link.register_flag )
			{
				if( link.process_id == reg.process_id )
				{
					link_id = i->first;
					link.connect_id = ptr->conn.connection_id();
					link.internal_id = ptr->head.internal_id;
					link.link_id = link_id;
					link.register_flag = true;
					ok = true;
					//LOG(INFO,"queue_stack::on_clt_register process_name="<<process_name_<<" ok=true");
				}
			}
		}
	}
	else
	{
		//LOG(INFO,"else process empty");
		std::size_t t = 0;
		for( process_map::iterator i = processes_.begin();
			i != processes_.end() && t < current_max_process_; ++i, ++t )
		{
			link_info& link = i->second;
			if( !link.register_flag )
			{
				link.process_id = reg.process_id;
				link.connect_id = ptr->conn.connection_id();
				link.internal_id = ptr->head.internal_id;
				link_id = i->first;
				link.link_id = link_id;
				link.register_flag = true;
				ok = true;
				//LOG(INFO,"process_name empty ==>ok=true");
			}
		}
	}
	if( ok )
	{
		protocol_message_ptr response( new protocol_message() );
		server_message_head resp_head;
		resp_head.service_code = ptr->head.service_code;
		resp_head.internal_id = ptr->head.internal_id;
		resp_head.message_code = ptr->head.message_code;

		*response << resp_head;

		reg.link_id = link_id;
		*response << reg;

		server_instance::get()->push_response( ptr->conn, response );
		//connections_[conn_info.connection_id()] = head.service_code;
	}
	return;
}
void queue_stack::on_clt_heartbeat( worklink_event_object_ptr& ptr )
{
  svr_msg_heartbeat heartbeat;
  *( ptr->message ) >> heartbeat;
 // LOG( INFO, "接收 worklink " << heartbeat.link_id << "心跳" );

  protocol_message_ptr response( new protocol_message() );

  server_message_head resp_head;
  resp_head.service_code = ptr->head.service_code;
  resp_head.internal_id = ptr->head.internal_id;
  resp_head.message_code = ptr->head.message_code;

  *response << resp_head;
  *response << heartbeat;

  server_instance::get()->push_response( ptr->conn, response );
}
void queue_stack::on_clt_queryreq( worklink_event_object_ptr& ptr )
{
  svr_msg_queryreq query;
  *( ptr->message ) >> query;//	取出msg的内容到query

  if( processes_.find( query.link_id ) == processes_.end() )
  {
    on_clt_error( ptr );
    return;
  }

  link_info& link = processes_[query.link_id];
  if( link.request_msg )
  {
    LOG( ERROR, "Service[" << service_code_ << "]Link[" << query.link_id << "] 请求顺序不一致" );
    return;
  }
  ++free_link_cnt_;
  link.request_msg = true;
  //LOG(INFO,"queue_stack::on_clt_queryreq 给request_msg赋值 true");

}
void queue_stack::on_clt_response( worklink_event_object_ptr& ptr )
{
 // LOG( INFO, "worklink 发送应答" );
  svr_msg_response req_head;
  *( ptr->message ) >> req_head;
  //bool ret = response_ice( req_head, ptr->message );

  protocol_message_ptr response( new protocol_message() );

  server_message_head resp_head;
  resp_head.service_code = ptr->head.service_code;
  resp_head.internal_id = ptr->head.internal_id;
  resp_head.message_code = ptr->head.message_code;

  *response << resp_head;

  svr_msg_response result;
  result.link_id = req_head.link_id;
  result.return_code = 0;//( ret ) ? 0 : 9999;
  result.has_data = 0;
  result.bin_data = 0;
  *response << result;

  //std::cout<<"push response connection :"<<ptr->conn.connection_id()<<std::endl;
  server_instance::get()->push_response( ptr->conn, response );
  

  // 发送应答任务
  if( !req_head.has_data )
    req_head.data_length = 0;
  if( !req_head.bin_data )
    req_head.bin_length = 0;
  link_info& link = processes_[req_head.link_id];
  netsvr_message_ptr sendret( new netsvr_message( ptr->message, req_head.data_length,
                              req_head.bin_length, link.last_msg_ptr->cb ) );
  sendret->msg_type = link.last_msg_ptr->msg_type;
  sendret->internal_id = link.last_msg_ptr->internal_id;

  sendret->request_code = link.last_msg_ptr->request_code;//20140114新添加，取出on_ice_sendmsg_begin的时候存的requestCode的值
  post_ice( netsvr::iec_sendmsg_end, ptr->conn , sendret );
  record_funcno_info(link);//==20140122新添加 ，统计本次的处理信息
  link.last_msg_ptr.reset();
  link.request_msg = false;//后面假如link处理完请求后会调get_message来通知netsvr该link已经空闲，会给request_msg重新赋值为true
}


typedef FDXSLib::FDXSSingleton<netsvr::queue_manager> queue_manager_inst;
void queue_stack::on_clt_funcno( worklink_event_object_ptr& ptr )
{
	//接收功能号保存在容器
	LOG( INFO, "接收功能号列表 ,sevice_code="<<ptr->head.service_code );
	std::size_t tol_len = ptr->message.get()->total_length();
	//LOG(INFO,"tol_Len="<<tol_len);
	//在queue_managers_里找到对应的queue_stack对象
	try
	{
		svr_msg_funcno* pst_funcno = reinterpret_cast<svr_msg_funcno*>(new char[tol_len]);
		*( ptr->message ) >> *pst_funcno;//读取信息到结构体里
		queue_stack& queue =  queue_manager_inst::get()->find_queue(ptr->head.service_code);
		if(queue.connected_flag == true)
		{
			//LOG(INFO,"功能号列表已存在，本次不用添加");
			queue.ecardsvr_start_time = boost::posix_time::second_clock::local_time();	//重新计时	
			return;//假如以后收取过功能号了就不再insert
		}
		for(int i= 0;i<pst_funcno->func_count;i++)
		{
			//LOG(INFO,"service_code="<<queue.service_code_<<",insert funcno="<<pst_funcno->data[i].funcno<<",funcdesc="<<pst_funcno->data[i].funcdesc);
			(queue.funcno_list).insert(pst_funcno->data[i].funcno);
			funcno_info tmpInfo;
			tmpInfo.m_desc = pst_funcno->data[i].funcdesc;
			tmpInfo.m_desc.erase(0,tmpInfo.m_desc.find_first_not_of("\r\t\n "));//相当于trim
			tmpInfo.m_desc.erase(tmpInfo.m_desc.find_last_not_of("\r\t\n ")+1);
			typedef pair<std::size_t, funcno_info> PairType;
			(queue.funcno_info_map_).insert(PairType(pst_funcno->data[i].funcno,tmpInfo));
		}		
		//LOG( INFO, "接收功能号列表完毕!!func_count="<<pst_funcno->func_count<<",service_code="<<ptr->head.service_code );
		delete [] pst_funcno;
		pst_funcno = NULL;
		//开始为ecardsvr计时
		queue.ecardsvr_start_time = boost::posix_time::second_clock::local_time();	
		queue.connected_flag = true;//后面收到这个请求以后就不用执行insert的操作
	}
	catch(...)
	{
		LOG( INFO, "funcno size 错误");
	}
	
	
	
 //   *( ptr->message ) >> st_funcno;//读取信息到结构体里
	////在queue_managers_里找到对应的queue_stack对象
	//try
	//{
	//	queue_stack& queue =  queue_manager_inst::get()->find_queue(ptr->head.service_code);
	//	for(int i= 0;i<st_funcno.func_count;i++)
	//	{
	//		LOG(INFO,"service_code="<<queue.service_code_<<",insert funcno="<<st_funcno.data[i].funcno<<",funcdesc="<<st_funcno.data[i].funcdesc);
	//		(queue.funcno_list).insert(st_funcno.data[i].funcno);
	//	}		
	//}
	//catch(...)
	//{
	//}
	//LOG( INFO, "接收worklink 发送的功能号完毕!!func_count="<<st_funcno.func_count<<",service_code="<<ptr->head.service_code );
	
}
void queue_stack::on_clt_pushreq( worklink_event_object_ptr& ptr )
{
}
void queue_stack::on_clt_callreq( worklink_event_object_ptr& ptr )
{
}
void queue_stack::on_clt_close( worklink_event_object_ptr& ptr )
{
  for( process_map::iterator i = processes_.begin();
       i != processes_.end(); ++i )
  {
    link_info& link = i->second;
    if( link.connect_id == ptr->conn.connection_id() )
    {
      LOG( NOTICE, "worklink " << link.link_id << " 退出" );
      link.register_flag = false;
      return;
    }
  }
}
void queue_stack::on_clt_check_link( worklink_event_object_ptr& ptr )
{
	if( this->process_name_.empty() )
		return;
	if( !if_file_exists( this->process_name_ ) )
		return;
	if( !running_ )
		return;
	std::size_t t = 0;
	bool has_all_links_idle =true;
	std::size_t cnt = 0;
	for( process_map::iterator i = processes_.begin();
		i != processes_.end() && t < current_max_process_; ++i, ++t )
	{
		link_info& link = i->second;
		if( link.register_flag == true )
		{
			if(link.request_msg == false)//表示link还被占用中
			{
				//LOG(INFO,"link["<<cnt++<<"]还未空闲,linkid="<<link.link_id<<",process_id="<<link.process_id);
				has_all_links_idle = false;//表示还存在未空闲的link
			}
			continue;
		}
		{
			lock_type lock( mutex_ );
			if( link.register_flag == false )
			{
				if(application_kill_flag==true)
				{
					LOG(INFO,"虽然link.register_flag == false，但是application_kill_flag==true，故不再拉起ecardsvr");
					continue;
				}
				subprocess::command_line cmdline( process_name_ );
				std::stringstream arg;
				//LOG(INFO,"准备startup Process service_code="<<this->service_code_);
				arg << " -s " << manager_.listen_port()<<" -i "<<this->service_code_;//===20131226新添加,启动进程时添加一个参数

				cmdline.argument( arg.str() );
				try
				{
					subprocess::launcher lnch;
					subprocess::child c = lnch.start( cmdline );
					link.process_id = ( std::size_t )c.get_pid();
					//std::cout << "process id ：" << link.process_id << std::endl;
				}
				catch( ... )
				{
					LOG(INFO,"startup Process "<<this->process_name_<<"失败");
					return;
				}
				//LOG(INFO,"startup Process "<<this->process_name_<<"成功");
			}
		}
	}
	if(application_kill_flag == true)//系统收到退出指令
	{
		if(has_all_links_idle == false)
		{
			LOG(INFO,"虽然收到退出指令，但是link没有全部空闲");
		}
		else
		{
			terminate_flag = true;//表示该queue_stack下的link已经全部空闲
		}
	}
	
}
///////////////////////////////////////////////////////////////////////////////

void queue_stack::post_ice( std::size_t evt, const netsvr_message_ptr& message )
{
  ice_event_object_ptr ptr( new ice_event_object );
  ptr->event_code = evt;
  ptr->message = message;
  ice_events_.register_event( boost::bind( &queue_stack::on_ice_event, this, ptr ) );
}
void queue_stack::post_ice( std::size_t evt, const FDXSLib::connect_info& conn,
                            const netsvr_message_ptr& message )
{
  ice_event_object_ptr ptr( new ice_event_object );
  ptr->event_code = evt;
  ptr->message = message;
  ptr->conn = conn;
  ice_events_.register_event( boost::bind( &queue_stack::on_ice_event, this, ptr ) );
}
void queue_stack::on_ice_event( ice_event_object_ptr ptr )
{
	try
	{
		switch( ptr->event_code )
		{
		case iec_sendmsg_begin:
		case iec_querymsg_begin:
			on_ice_sendmsg_begin( ptr );
			break;
		case iec_sendmsg_end:
		case iec_querymsg_end:
			on_ice_sendmsg_end( ptr );
			break;
		case iec_funcno_query:
			break;
		case iec_query_msg_next:
		default:
			LOG( ERROR, "queue_stack::on_ice_event unknown event ,code " << ptr->event_code );
			break;
		}
	}
	catch(...)
	{
		LOG(INFO,"捕获on_ice_event异常");
	}
}
// 处理 ice 请求事件
void queue_stack::on_ice_sendmsg_begin( ice_event_object_ptr& ptr )
{
	for( process_map::iterator i = processes_.begin(); i != processes_.end(); ++i )
	{
		link_info& link = i->second;
		if( link.request_msg )
		{
			// 应答数据
			if( !query_msg_end( link, ptr->message ) )
			{
				LOG( ERROR, "应答Link [" << i->first << "] 请求错误" );
				return;
			}
			else
			{
				--free_link_cnt_;
				link.request_msg = false;//表示该进程已经被使用，不能request信息了
				link.start_militime = boost::posix_time::microsec_clock::local_time();//==20140122新添加,开始计时
				//LOG(INFO,"queue_stack::on_ice_sendmsg_begin 成功 赋值该link的requst_msg=false");
				//link.last_msg_ptr->request_code = ptr->message->request_code;//20140114新添加,备份一下requestCode信息
				//LOG(INFO,"else 中 link.last_msg_ptr->request_code="<<link.last_msg_ptr->request_code);
				return;
			}
		}
	}
	// TODO: 没有可用单元，缓存队列
	LOG(DEBUG," ICE 请求无可用单元处理 ");
	ice_response_error(ptr->message,9998,"ICE 请求无可用单元处理");
}
//统计本次功能号处理的相关信息
void queue_stack::record_funcno_info( link_info& link)
{
	try
	{
		//统计该功能号的信息
		boost::posix_time::ptime pStart = link.start_militime;//->create_time;//create_micro_time;//create_time会在netsvr_message构造的时候被更新，且会加上3
		//pStart -= boost::posix_time::seconds(3);
		boost::posix_time::ptime pEnd = boost::posix_time::microsec_clock::local_time();
		boost::posix_time::time_duration td(pEnd-pStart);
		size_t gapms = td.total_milliseconds();
		netsvr_message_ptr& pack = link.last_msg_ptr;
		if(gapms == 0)
			gapms = 1;//接近0ms的处理时间都用1ms代替
		//LOG(INFO,"本次 "<<pack->request_code<<" 的处理时间="<<gapms<<"ms");
		if(gapms<this->funcno_info_map_[pack->request_code].m_min_time)
			this->funcno_info_map_[pack->request_code].m_min_time=gapms;
		if(gapms>this->funcno_info_map_[pack->request_code].m_max_time)
			this->funcno_info_map_[pack->request_code].m_max_time=gapms;
		this->funcno_info_map_[pack->request_code].m_cnt++;
		this->funcno_info_map_[pack->request_code].m_total_time+=gapms;
	}
	catch(...)
	{
		LOG(INFO,"queue_stack::record_funcno_info 统计时间时出现异常");
	}
	//LOG(INFO,"queue_stack::query_msg_end 统计时间结束");
}
//! 发送ice请求给 worklink
bool queue_stack::query_msg_end( link_info& link, netsvr_message_ptr& pack )
{
	//LOG(INFO,"queue_stack::query_msg_end 收到ice请求，并准备转发给link");
	protocol_message_ptr response( new protocol_message );
	server_message_head head;
	head.service_code = this->service_code_;
	head.internal_id = link.internal_id;
	head.message_code = netsvr::mt_queryreq;
	*response << head;
	svr_msg_queryreq req;
	req.msg_flag = 0;
	req.bin_flag = 0;
	req.message_length = 0;
	req.bin_length = 0;
	req.link_id = link.link_id;
	
	if( pack->data && pack->data_length > 0 )
	{
		req.msg_flag = 1;
		req.message_length = pack->data_length;

	}
	if( pack->data && pack->bin_length > 0 )
	{
		req.bin_flag = 1;
		req.bin_length = pack->bin_length;

	}
	*response << req;
	if( pack->data )
	{
		protocol_message* m = new protocol_message( msgblk_empty );
		pack->data->swap( *m );
		pack->data.reset();
		response->add_tail( m );
	}

	// push to response queue
	link.last_msg_ptr = pack;

	connect_info conn( link.connect_id );
	if( server_instance::get()->push_response( conn, response ) )
	{
		return false;
	}
	//LOG(INFO,"queue_stack::query_msg_end 转发ICE请求成功");
	return true;
}

void queue_stack::on_ice_sendmsg_end( ice_event_object_ptr& ptr )
{
	//LOG(DEBUG,"queue_stack::on_ice_sendmsg_end  收到link处理结果，并准备应答ICE客户端");
	//
	transfer::ResponseProtocol resp;
	resp.serviceCode = this->service_code_;
	resp.internalID = ptr->message->internal_id;
	resp.returnCode = ptr->message->return_code;
	resp.returnMessage = ptr->message->return_message;
	resp.status = 0;
	if( ptr->message->data_length > 0 )
	{
		resp.data.reserve( ptr->message->data_length );
		// copy to vector
		copy_vector( resp.data, 0 , ptr->message->data, ptr->message->data_length );
	}
	if( ptr->message->bin_length > 0 )
	{
		resp.bindata.reserve( ptr->message->bin_length );
		// copy to vector
		copy_vector( resp.bindata, 0, ptr->message->data, ptr->message->bin_length );
	}
	// release data
	if( ptr->message->cb.type() == typeid( transfer::AMD_CallServer_SendMsgPtr ) )
	{
		transfer::AMD_CallServer_SendMsgPtr cb_ptr =
			boost::any_cast<transfer::AMD_CallServer_SendMsgPtr>
			( ptr->message->cb );
		cb_ptr->ice_response( resp );
	}
	else if( ptr->message->cb.type() ==  typeid( transfer::AMD_CallServer_QueryMsgPtr ) )
	{
		transfer::AMD_CallServer_QueryMsgPtr cb_ptr =
			boost::any_cast<transfer::AMD_CallServer_QueryMsgPtr>
			( ptr->message->cb );
		cb_ptr->ice_response( resp );
	}
	else
	{
		LOG( ERROR, "应答数据时，队列中请求句柄类型异常" );
		return;
	}
	//LOG( TRACE, "ICE END: "<< (const char*)&(*resp.data.begin()) );
	//LOG( INFO, "queue_stack::on_ice_sendmsg_end  应答ICE客户端请求成功: len :"<<ptr->message->data_length 
	//	<<" : " << ptr->message->bin_length);
	return;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
inline void netsvr_message::update_time()
{
  create_time = boost::posix_time::second_clock::local_time();
  create_time += boost::posix_time::seconds(3);
  //create_micro_time = boost::posix_time::microsec_clock::local_time();//==20140116新添加,开始计时
}
void netsvr_message::copy_message( transfer::ByteSeq& seq, FDXSLib::protocol_message*& data )
{
  if( seq.size() == 0 )
    return;
  data = new FDXSLib::protocol_message();
  data->copy_from_buffer( ( const char* ) & ( *seq.begin() ), seq.size() );
}

///////////////////////////////////////////////////////////////////////////////
}

