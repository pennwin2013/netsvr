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
  this->start_militime = rhs.start_militime;//==20140122�����
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
  this->start_militime = rhs.start_militime;//==20140122�����
  this->request_msg = rhs.request_msg;
  this->register_flag = rhs.register_flag;
  this->last_msg_ptr = rhs.last_msg_ptr;
  return *this;
}

///////////////////////////////////////////////////////////////////////////////
queue_stack::queue_stack( queue_manager& manager ):
  running_( false ), fronzen_( false ), manager_( manager ),free_link_cnt_(0),connected_flag(false),terminate_flag(false)//==20140116�����
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
std::size_t queue_stack::get_running_time()//��ȡecardsvr������ʱ��
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
	//LOG(INFO,"queue_stack::startup() ������link["<<i<<"].request_msg=false");
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
		LOG(DEBUG,"���յ�kill�źţ����ؿͻ�������");
		ice_response_error( pack, 9998, "ϵͳ�����˳���������������");
	}
	lock_type lock( mutex_ );
	if( free_link_cnt_ > 0 )
	{
		LOG(DEBUG,"�������� ICE ����");
		if( pack->msg_type == netsvr_message::nm_sendmsg )
			post_ice( iec_sendmsg_begin, pack );
		else if(pack->msg_type == netsvr_message::nm_funcno)
			post_ice( iec_funcno_query, pack);
		else
			post_ice( iec_querymsg_begin, pack );
	}
	else
	{
		LOG(DEBUG,"ICE ������");
		//input_queue_.push_back( pack );
		//cond_.notify_one();
		ice_response_error( pack, 9998, "ϵͳ�޿��õ�Ԫ");
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
    // ���ȴ����������
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
        ice_response_error( message, 9998, "ϵͳ�޿��õ�Ԫ");
      }
      else
        break;
    }
  }
  LOG( DEBUG, "�������������" );
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
  case mt_register: // ǩ��
    on_clt_register( ptr );
    break;
  case mt_heartbeat : // ����
    on_clt_heartbeat( ptr );
    break;
  case mt_queryreq:   // link����ȡ��������˵����link�Ѿ������괦�ڿ���״̬��
    on_clt_queryreq( ptr );
    break;
  case mt_response:   // Ӧ������
    on_clt_response( ptr );
    break;
  case mt_pushreq:    // ��������
    on_clt_pushreq( ptr );
    break;
  case mt_callreq:     // �������
    on_clt_callreq( ptr );
    break;
  case mt_funcno:   //���͹��ܺ�����
	on_clt_funcno(ptr);
	break;
  case qs_mt_on_close:// ���ӹر�
    on_clt_close( ptr );
    break;
  case qs_mt_check_link: // ��� work link
    on_clt_check_link( ptr );
    break;
  default:
    on_clt_error( ptr ); // ��Ч���󣬹ر�����
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
 // LOG( INFO, "���� worklink " << heartbeat.link_id << "����" );

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
  *( ptr->message ) >> query;//	ȡ��msg�����ݵ�query

  if( processes_.find( query.link_id ) == processes_.end() )
  {
    on_clt_error( ptr );
    return;
  }

  link_info& link = processes_[query.link_id];
  if( link.request_msg )
  {
    LOG( ERROR, "Service[" << service_code_ << "]Link[" << query.link_id << "] ����˳��һ��" );
    return;
  }
  ++free_link_cnt_;
  link.request_msg = true;
  //LOG(INFO,"queue_stack::on_clt_queryreq ��request_msg��ֵ true");

}
void queue_stack::on_clt_response( worklink_event_object_ptr& ptr )
{
 // LOG( INFO, "worklink ����Ӧ��" );
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
  

  // ����Ӧ������
  if( !req_head.has_data )
    req_head.data_length = 0;
  if( !req_head.bin_data )
    req_head.bin_length = 0;
  link_info& link = processes_[req_head.link_id];
  netsvr_message_ptr sendret( new netsvr_message( ptr->message, req_head.data_length,
                              req_head.bin_length, link.last_msg_ptr->cb ) );
  sendret->msg_type = link.last_msg_ptr->msg_type;
  sendret->internal_id = link.last_msg_ptr->internal_id;

  sendret->request_code = link.last_msg_ptr->request_code;//20140114����ӣ�ȡ��on_ice_sendmsg_begin��ʱ����requestCode��ֵ
  post_ice( netsvr::iec_sendmsg_end, ptr->conn , sendret );
  record_funcno_info(link);//==20140122����� ��ͳ�Ʊ��εĴ�����Ϣ
  link.last_msg_ptr.reset();
  link.request_msg = false;//�������link�������������get_message��֪ͨnetsvr��link�Ѿ����У����request_msg���¸�ֵΪtrue
}


typedef FDXSLib::FDXSSingleton<netsvr::queue_manager> queue_manager_inst;
void queue_stack::on_clt_funcno( worklink_event_object_ptr& ptr )
{
	//���չ��ܺű���������
	LOG( INFO, "���չ��ܺ��б� ,sevice_code="<<ptr->head.service_code );
	std::size_t tol_len = ptr->message.get()->total_length();
	//LOG(INFO,"tol_Len="<<tol_len);
	//��queue_managers_���ҵ���Ӧ��queue_stack����
	try
	{
		svr_msg_funcno* pst_funcno = reinterpret_cast<svr_msg_funcno*>(new char[tol_len]);
		*( ptr->message ) >> *pst_funcno;//��ȡ��Ϣ���ṹ����
		queue_stack& queue =  queue_manager_inst::get()->find_queue(ptr->head.service_code);
		if(queue.connected_flag == true)
		{
			//LOG(INFO,"���ܺ��б��Ѵ��ڣ����β������");
			queue.ecardsvr_start_time = boost::posix_time::second_clock::local_time();	//���¼�ʱ	
			return;//�����Ժ���ȡ�����ܺ��˾Ͳ���insert
		}
		for(int i= 0;i<pst_funcno->func_count;i++)
		{
			//LOG(INFO,"service_code="<<queue.service_code_<<",insert funcno="<<pst_funcno->data[i].funcno<<",funcdesc="<<pst_funcno->data[i].funcdesc);
			(queue.funcno_list).insert(pst_funcno->data[i].funcno);
			funcno_info tmpInfo;
			tmpInfo.m_desc = pst_funcno->data[i].funcdesc;
			tmpInfo.m_desc.erase(0,tmpInfo.m_desc.find_first_not_of("\r\t\n "));//�൱��trim
			tmpInfo.m_desc.erase(tmpInfo.m_desc.find_last_not_of("\r\t\n ")+1);
			typedef pair<std::size_t, funcno_info> PairType;
			(queue.funcno_info_map_).insert(PairType(pst_funcno->data[i].funcno,tmpInfo));
		}		
		//LOG( INFO, "���չ��ܺ��б����!!func_count="<<pst_funcno->func_count<<",service_code="<<ptr->head.service_code );
		delete [] pst_funcno;
		pst_funcno = NULL;
		//��ʼΪecardsvr��ʱ
		queue.ecardsvr_start_time = boost::posix_time::second_clock::local_time();	
		queue.connected_flag = true;//�����յ���������Ժ�Ͳ���ִ��insert�Ĳ���
	}
	catch(...)
	{
		LOG( INFO, "funcno size ����");
	}
	
	
	
 //   *( ptr->message ) >> st_funcno;//��ȡ��Ϣ���ṹ����
	////��queue_managers_���ҵ���Ӧ��queue_stack����
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
	//LOG( INFO, "����worklink ���͵Ĺ��ܺ����!!func_count="<<st_funcno.func_count<<",service_code="<<ptr->head.service_code );
	
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
      LOG( NOTICE, "worklink " << link.link_id << " �˳�" );
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
			if(link.request_msg == false)//��ʾlink����ռ����
			{
				//LOG(INFO,"link["<<cnt++<<"]��δ����,linkid="<<link.link_id<<",process_id="<<link.process_id);
				has_all_links_idle = false;//��ʾ������δ���е�link
			}
			continue;
		}
		{
			lock_type lock( mutex_ );
			if( link.register_flag == false )
			{
				if(application_kill_flag==true)
				{
					LOG(INFO,"��Ȼlink.register_flag == false������application_kill_flag==true���ʲ�������ecardsvr");
					continue;
				}
				subprocess::command_line cmdline( process_name_ );
				std::stringstream arg;
				//LOG(INFO,"׼��startup Process service_code="<<this->service_code_);
				arg << " -s " << manager_.listen_port()<<" -i "<<this->service_code_;//===20131226�����,��������ʱ���һ������

				cmdline.argument( arg.str() );
				try
				{
					subprocess::launcher lnch;
					subprocess::child c = lnch.start( cmdline );
					link.process_id = ( std::size_t )c.get_pid();
					//std::cout << "process id ��" << link.process_id << std::endl;
				}
				catch( ... )
				{
					LOG(INFO,"startup Process "<<this->process_name_<<"ʧ��");
					return;
				}
				//LOG(INFO,"startup Process "<<this->process_name_<<"�ɹ�");
			}
		}
	}
	if(application_kill_flag == true)//ϵͳ�յ��˳�ָ��
	{
		if(has_all_links_idle == false)
		{
			LOG(INFO,"��Ȼ�յ��˳�ָ�����linkû��ȫ������");
		}
		else
		{
			terminate_flag = true;//��ʾ��queue_stack�µ�link�Ѿ�ȫ������
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
		LOG(INFO,"����on_ice_event�쳣");
	}
}
// ���� ice �����¼�
void queue_stack::on_ice_sendmsg_begin( ice_event_object_ptr& ptr )
{
	for( process_map::iterator i = processes_.begin(); i != processes_.end(); ++i )
	{
		link_info& link = i->second;
		if( link.request_msg )
		{
			// Ӧ������
			if( !query_msg_end( link, ptr->message ) )
			{
				LOG( ERROR, "Ӧ��Link [" << i->first << "] �������" );
				return;
			}
			else
			{
				--free_link_cnt_;
				link.request_msg = false;//��ʾ�ý����Ѿ���ʹ�ã�����request��Ϣ��
				link.start_militime = boost::posix_time::microsec_clock::local_time();//==20140122�����,��ʼ��ʱ
				//LOG(INFO,"queue_stack::on_ice_sendmsg_begin �ɹ� ��ֵ��link��requst_msg=false");
				//link.last_msg_ptr->request_code = ptr->message->request_code;//20140114�����,����һ��requestCode��Ϣ
				//LOG(INFO,"else �� link.last_msg_ptr->request_code="<<link.last_msg_ptr->request_code);
				return;
			}
		}
	}
	// TODO: û�п��õ�Ԫ���������
	LOG(DEBUG," ICE �����޿��õ�Ԫ���� ");
	ice_response_error(ptr->message,9998,"ICE �����޿��õ�Ԫ����");
}
//ͳ�Ʊ��ι��ܺŴ���������Ϣ
void queue_stack::record_funcno_info( link_info& link)
{
	try
	{
		//ͳ�Ƹù��ܺŵ���Ϣ
		boost::posix_time::ptime pStart = link.start_militime;//->create_time;//create_micro_time;//create_time����netsvr_message�����ʱ�򱻸��£��һ����3
		//pStart -= boost::posix_time::seconds(3);
		boost::posix_time::ptime pEnd = boost::posix_time::microsec_clock::local_time();
		boost::posix_time::time_duration td(pEnd-pStart);
		size_t gapms = td.total_milliseconds();
		netsvr_message_ptr& pack = link.last_msg_ptr;
		if(gapms == 0)
			gapms = 1;//�ӽ�0ms�Ĵ���ʱ�䶼��1ms����
		//LOG(INFO,"���� "<<pack->request_code<<" �Ĵ���ʱ��="<<gapms<<"ms");
		if(gapms<this->funcno_info_map_[pack->request_code].m_min_time)
			this->funcno_info_map_[pack->request_code].m_min_time=gapms;
		if(gapms>this->funcno_info_map_[pack->request_code].m_max_time)
			this->funcno_info_map_[pack->request_code].m_max_time=gapms;
		this->funcno_info_map_[pack->request_code].m_cnt++;
		this->funcno_info_map_[pack->request_code].m_total_time+=gapms;
	}
	catch(...)
	{
		LOG(INFO,"queue_stack::record_funcno_info ͳ��ʱ��ʱ�����쳣");
	}
	//LOG(INFO,"queue_stack::query_msg_end ͳ��ʱ�����");
}
//! ����ice����� worklink
bool queue_stack::query_msg_end( link_info& link, netsvr_message_ptr& pack )
{
	//LOG(INFO,"queue_stack::query_msg_end �յ�ice���󣬲�׼��ת����link");
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
	//LOG(INFO,"queue_stack::query_msg_end ת��ICE����ɹ�");
	return true;
}

void queue_stack::on_ice_sendmsg_end( ice_event_object_ptr& ptr )
{
	//LOG(DEBUG,"queue_stack::on_ice_sendmsg_end  �յ�link����������׼��Ӧ��ICE�ͻ���");
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
		LOG( ERROR, "Ӧ������ʱ�������������������쳣" );
		return;
	}
	//LOG( TRACE, "ICE END: "<< (const char*)&(*resp.data.begin()) );
	//LOG( INFO, "queue_stack::on_ice_sendmsg_end  Ӧ��ICE�ͻ�������ɹ�: len :"<<ptr->message->data_length 
	//	<<" : " << ptr->message->bin_length);
	return;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
inline void netsvr_message::update_time()
{
  create_time = boost::posix_time::second_clock::local_time();
  create_time += boost::posix_time::seconds(3);
  //create_micro_time = boost::posix_time::microsec_clock::local_time();//==20140116�����,��ʼ��ʱ
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

