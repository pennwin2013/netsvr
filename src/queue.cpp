#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <sstream>
//#include "fdxslib.h"
//#include "fdxslib_socket.h"
#include "subprocess.hpp"
#include "queue.h"
#include "logger_imp.h"
#include "server_msg.h"
#include <cstdio>
#include <signal.h>

namespace netsvr
{
static bool _do_load_fdxslib()
{
  /*
  if(!FDXSLib::FDXSLib_LoadImpl(FDXSLib::socket))
  {
    LOG(ERROR,"cannot load FDXSLib module");
    throw std::runtime_error("cannot load FDXSLib module");
  }
  */
  return true;
}

static bool _b_load_fdxslib = _do_load_fdxslib();

///////////////////////////////////////////////////////////////////////////////

queue_manager::queue_manager( std::size_t max_queue, Ice::PropertiesPtr& props ):
  is_done_( true ), properties_( props ), queue_manager_( NULL ),service_code_counter(2)
{

}
queue_manager::~queue_manager()
{
  stop();
}
int queue_manager::update_properties( Ice::PropertiesPtr& props )
{
  lock_type lock( mutex_ );
  properties_ = props;
  queue_managers_.clear();
  service_code_counter = 2;//1--��ecardsvr   >=2-----��ecardsvr
  std::set<std::string> service_name;
  Ice::StringSeq worker_list = properties_->getPropertyAsList( "queue.list" );//��ȡecardsvr���б�
  for( std::size_t i = 0; i < worker_list.size(); ++i )//�����ѭ�����ȡ���е�ecardsvr��Ϣ������setup_queues�ﹹ��queue_stack������ecardsvr���̲�����ecardsvr������
  {
	  std::string worker_name = worker_list[i];
	  if( service_name.find( worker_name ) != service_name.end() )
	  {
		  LOG( ERROR, "������ " << worker_name << " �����ظ������������ļ�" );
		  return -1;
	  }
	  if( setup_queues( worker_name ) )
		  return -1;
	  service_name.insert( worker_name );
  }
  if( service_name.empty() )
  {
    LOG( NOTICE, "δ�����κη�����������" );
    return -1;
  }
  return 0;
}

int queue_manager::setup_queues( const std::string& name )
{
  try
  {
    queue_stack_ptr ptr( new queue_stack(*this) );//queue_stack������queue_manager���������죬��һ����Ա���������queue_manager���������manager_
    ptr->name_ = name;
    ptr->desc_ = properties_->getPropertyWithDefault( name + ".queue.desc", name + " ����" );
    //ptr->service_code_ = properties_->getPropertyAsInt( name + ".queue.servciecode" );
	if(name == "core")
	{
		ptr->service_code_ = CORE_SERVICE_CODE;
	}
	else
	{
		ptr->service_code_ = service_code_counter++;
	}
    ptr->max_queue_ = properties_->getPropertyAsIntWithDefault( name + ".queue.maxtask", 100 );
    ptr->max_size_ = properties_->getPropertyAsIntWithDefault( name + ".queue.maxmem", 5 ) * 1024 * 1024;
    ptr->process_count_ = properties_->getPropertyAsInt( name + ".queue.processcount" );
    ptr->process_name_ = properties_->getPropertyWithDefault( name + ".queue.process", "" );
    ptr->max_process_count_ = properties_->getPropertyAsIntWithDefault(
                                name + ".queue.maxprocesscount" , ptr->process_count_ );
    ptr->clear();
    if( ptr->process_count_ < 1 )
    {
      LOG( ERROR, "���� " << name << " ���ý������������������ļ�" );
      return -1;
    }
    if( ptr->max_process_count_ > 100 )
    {
      LOG( NOTICE , "���� " << name << " {" << ptr->desc_ << "} service code :" << ptr->service_code_
           << " ,���Ԫ����̫�� " << ptr->max_process_count_ << "����ȷ��" );
    }
    queue_managers_[ptr->service_code_] = ptr;
    ptr->startup();
    LOG( INFO, "�������� name=" << name << " {" << ptr->desc_ << "} service code :" << ptr->service_code_ );
  }
  catch( ... )
  {
    LOG( ERROR, "���ط��� " << name << " ʧ�ܣ�����������" );
    return -1;
  }
  return 0;
}

int queue_manager::start( std::size_t listen_port )
{
	
  {
    lock_type lock( mutex_ );
    if( !is_done_ )
    {
      LOG( ERROR, "�����Ѿ��������˿� " << listen_port_ << " ������ֹͣ" );
      return -1;
    }

  }
  if( queue_managers_.size() == 0 )
  {
    LOG( ERROR, "δע���κη���ϵͳ�˳�" );
    return -1;
  }
  bool success = true;
  is_done_ = false;

  queue_manager_ = new queue_callback( *this );
  server_instance::get()->register_callback( queue_manager_ );
  if( server_instance::get()->start( listen_port ) )
  {
    LOG( ERROR, "������������ʧ�ܣ����ܶ˿� " << listen_port << " �ѱ�ռ��" );
    is_done_ = true;
    return -1;
  }
  listen_port_ = listen_port;

  // ����queue info �����߳�
  for( message_queue_map_type::iterator i = queue_managers_.begin();
       i != queue_managers_.end(); ++i )
  {
    thr_group_.create_thread( boost::bind( &queue_stack::run, i->second ) );
  }
  thr_group_.create_thread( boost::bind( &queue_manager::check_terminate_flag, this ) );//�������queue_stack��terminate_flag���߳�
  thr_group_.create_thread( boost::bind( &queue_manager::run, this ) );//��ʱû�õ�,��Ϊworker_process_������û�Ӷ���
  return 0;
}
int queue_manager::stop()
{
  {
    lock_type lock( mutex_ );
    if( is_done_ )
      return 0;
    is_done_ = true;
  }
  thr_group_.interrupt_all();
  thr_group_.join_all();
  if( NULL != queue_manager_ )
  {
    delete queue_manager_;
    queue_manager_ = NULL;
  }
  return 0;
}
void queue_manager::getServerStatistic(std::string& str)
{
	lock_type lock( mutex_ );
	//����queue_manager_���queue_stackÿ�����ܺ�
	std::string tmpStr;
	char buff[250];
	queue_stack_ptr queue;
	std::set<std::size_t>::const_iterator set_finder;
	std::size_t tmpFuncno;
	/*
	[
		{"serviceCode":1,"serviceName":"ecardsvr","runningtime":0,"colDesc":["funcno","cnt","max","min","total","desc"],
			"funcInfo":[[870001,0,0,0,0,"870001"],[870002,0,0,0,0,"870002"]]},
		{"serviceCode":1,"serviceName":"ecardsvr","runningtime":0,"colDesc":["funcno","cnt","max","min","total","desc"],
			"funcInfo":[[870001,0,0,0,0],[870002,0,0,0,0]]}
	]
	*/
	str+="["; 
	size_t arr1Cnt = 0;
	for( message_queue_map_type::iterator i = queue_managers_.begin();i != queue_managers_.end(); ++i )
	{
		if(arr1Cnt == 0)
		{
			str+="{";
			arr1Cnt++;
		}
		else
		{
			str+=",{";
		}
		memset(buff,0,250);
		sprintf(buff,"%zd",i->first);
		tmpStr = buff;//itoa(i->first,buff,10);
		str = str+"\"serviceCode\":"+tmpStr;
		//�õ�queue_stack����
		queue = i->second;
		tmpStr = queue->process_name_;
		tmpStr.erase(0,tmpStr.find_first_not_of("\r\t\n "));//�൱��ltrim
		tmpStr.erase(0,2);//ȥ�� "./"
		str = str+",\"serviceName\":"+"\""+tmpStr+"\"";
		memset(buff,0,250);
		sprintf(buff,"%zd",queue->get_running_time());
		tmpStr = buff;//itoa(i->first,buff,10);
		//tmpStr = itoa(queue->get_running_time(),buff,10);
		str = str+",\"runningtime\":"+tmpStr;	

		str = str+",\"colDesc\":[\"funcno\",\"cnt\",\"max\",\"min\",\"total\",\"desc\"]";
		//����queue_stack���Ź��ܺ���Ϣ������
		str+=",\"funcInfo\":";
		str+="[";
		size_t arr2Cnt = 0;
		for(std::set<std::size_t>::reverse_iterator it_funclist =  queue->funcno_list.rbegin();it_funclist!=queue->funcno_list.rend();it_funclist++)
		{
			if(arr2Cnt == 0)
			{
				str+="[";
				arr2Cnt++;
			}
			else
			{
				str+=",[";
			}
			//���ܺ�
			tmpFuncno = *it_funclist;
			memset(buff,0,250);
			sprintf(buff,"%zd",tmpFuncno);
			tmpStr = buff;
			str+=tmpStr;
			//���ô���
			memset(buff,0,250);
			sprintf(buff,"%zd",queue->funcno_info_map_[tmpFuncno].m_cnt);
			tmpStr = buff;
			str+=","+tmpStr;
			//�����ʱ��
			memset(buff,0,250);
			sprintf(buff,"%zd",queue->funcno_info_map_[tmpFuncno].m_max_time);
			tmpStr = buff;
			str+=","+tmpStr;
			//��̴���ʱ��
			memset(buff,0,250);
			sprintf(buff,"%zd",queue->funcno_info_map_[tmpFuncno].m_min_time);
			tmpStr = buff;
			str+=","+tmpStr;
			//������ʱ��
			memset(buff,0,250);
			sprintf(buff,"%zd",queue->funcno_info_map_[tmpFuncno].m_total_time);
			tmpStr = buff;
			str+=","+tmpStr;
			//���ܺ�����
			str+=",\""+queue->funcno_info_map_[tmpFuncno].m_desc+"\"";
			str+="]";
		}
		str+="]}";
	}
	str+="]";



}
int queue_manager::push_queue( const netsvr_message_ptr& request )
{
  //LOG(INFO,"�յ� ICE �ͻ�������");
  size_t request_funco = request->request_code;
  queue_stack_ptr queue;
  {
	  //LOG(INFO,"before lock");
	  lock_type lock( mutex_ );
	  //LOG(INFO,"after lock");
	  bool hasFind = false;
	  std::set<std::size_t>::const_iterator set_finder;	 	  
	  for( message_queue_map_type::iterator i = queue_managers_.begin();
		  i != queue_managers_.end(); ++i )
	  {
		  //LOG(INFO,"����queue_managers_ i->first = "<<(i->first));
		  if((i->first)!=CORE_SERVICE_CODE)//����ecardsvr����
		  {
			  queue = i->second;
			  /*			
			  //�������set
			  for(std::set<std::size_t>::reverse_iterator it_funclist =  queue->funcno_list.rbegin();it_funclist!=queue->funcno_list.rend();it_funclist++)
			  {
			  LOG(INFO,"���� service_code="<<(i->first)<<",funcno="<<*(it_funclist));
			  }
			  */
			  set_finder = queue->funcno_list.find(request_funco);
			  if(set_finder!=queue->funcno_list.end())
			  {
				  hasFind = true;
				  break;
			  }
		  }	  
	  }
	  if(hasFind==false)
	  {
		  queue = queue_managers_[CORE_SERVICE_CODE];

		  set_finder = queue->funcno_list.find(request_funco);
		  if(set_finder!=queue->funcno_list.end())
		  {
			  hasFind = true;
		  }
	  }
	  //LOG(INFO,"ICE ���� request->request_code="<<request->request_code);
	  if(hasFind == false)
	  {
		  LOG(INFO,"δ�ҵ� request->request_code="<<request_funco<<"��Ӧ��queue");
		  return -1;
	  }
	  /*
	  if( queue_managers_.find( request->service_code ) == queue_managers_.end() )
	  {
	  // �޶�Ӧ�ķ���
	  return -1;
	  }
	  queue = queue_managers_[request->service_code];
	  */
  }
  //queue->post_ice( netsvr::iec_sendmsg_begin, request );
  //LOG(INFO,"���ҵ� request->request_code="<<request_funco<<"��Ӧ��queue!queue->service_code_="<<queue->service_code_);
  queue->push( request );
  return 0;
}
bool queue_manager::is_done() const
{
  return is_done_;
}
queue_stack& queue_manager::find_queue( std::size_t service_code )
{
  if( queue_managers_.find( service_code ) == queue_managers_.end() )
  {
    std::stringstream str;
    str << "���� " << service_code << " δע��";
    LOG( ERROR, str.str() );
    throw std::runtime_error( str.str() );
  }
  return *( queue_managers_[service_code].get() );
}
void queue_manager::check_terminate_flag()
{
	// ���ÿ��queue_stack��terminate_flag�Ƿ��Ѿ�Ϊtrue
	queue_stack_ptr queue;
	{
		bool terminate_all_flag = true;
		size_t cnt = 0;
		while(1)
		{
			{
				lock_type lock( mutex_ );	 
				terminate_all_flag = true;
				cnt = 0;
				for( message_queue_map_type::iterator i = queue_managers_.begin();
					i != queue_managers_.end(); ++i )
				{
					queue = i->second;
					//LOG(INFO,"queue_manager::check_terminate_flag() queue_managers_["<<cnt++<<"].terminate_flag="<<queue->terminate_flag);
					if(queue->terminate_flag == false)
					{
						terminate_all_flag = false;
						break;
					}
				}
			}
			if(terminate_all_flag == true)
			{
				LOG(INFO,"queue_manager::check_terminate_flag() terminate_all_flag==true��ִ���˳�")//�ر�netsvr��icegridnode
				system("pkill -9 icegridnode");
				exit(0);
			}
			//LOG(INFO,"queue_manager::check_terminate_flag() application_kill_flag="<<::application_kill_flag);
			//LOG(INFO,"===================");
			sleep(5);
		}
	}
}
void queue_manager::run()
{
	//LOG(INFO,"queue_manager::run()==>worker_process_.size()="<<worker_process_.size()<<"����queue_manager::run()��ʱû�õ�");
  // �����顢������������
  while( !is_done() && !boost::this_thread::interruption_requested() )
  {
    for( std::size_t i = 0; i < worker_process_.size(); ++i )
    {
      queue_stack& info = worker_process_[i].queue;
      subprocess::child& child = worker_process_[i].process;
      if( info.process_name_.empty() )
        continue;
		
      if( !child.alive() )
      {
        start_worker_process( worker_process_[i] );
      }
    }
    FDXS_Sleep( 1 );
  }
  std::for_each( worker_process_.begin(), worker_process_.end(),
                 boost::bind( &queue_manager::stop_worker_process, this, _1 ) );
}

bool queue_manager::start_worker_process( worker_process_info& info )
{
//Ŀǰworker_processһֱΪ�գ�������ʱ��������������
  if( info.queue.process_name_.empty() )
  {
	  LOG(INFO,"queue_manager::start_worker_process==>process_name_.empty()"<<info.queue.process_name_<<",service_code="<<info.queue.service_code_);
	  return false;
  }
  if( info.process.alive() )
    info.process.terminate();
  subprocess::command_line cmdline( info.queue.process_name_ );
  std::stringstream arg;
  LOG(INFO,"queue_manager::start_worker_process==>start cmdline"<<info.queue.process_name_<<",service_code="<<info.queue.service_code_);
  arg << " -s " << listen_port() << " -i " << info.queue.service_code_;//==20131226����ӣ����������̵�ʱ�����һ��service_code�Ĳ���
  cmdline.argument( arg.str() );
  try
  {
    subprocess::launcher lnch;
    info.process = lnch.start( cmdline );
    if( info.process.alive() )
      return true;
  }
  catch( ... )
  {
    return false;
  }
  return false;
}
void queue_manager::stop_worker_process( worker_process_info& info )
{
  info.process.terminate();
}


///////////////////////////////////////////////////////////////////////////////
queue_callback::queue_callback( queue_manager& queue ): queue_manager_( queue )
{
}
queue_callback::~queue_callback()
{
}
int queue_callback::on_connect( connect_info& conn_info )
{
  lock_type lock(mutex_);
  if( connections_.find( conn_info.connection_id() ) != connections_.end() )
  {
    connections_.erase( conn_info.connection_id() );
  }
  return 0;
}
int queue_callback::on_disconnect( connect_info& conn_info )
{
  lock_type lock(mutex_);
  std::size_t service_code = 0;
  if( connections_.find( conn_info.connection_id() ) != connections_.end() )
  {
    service_code = connections_[conn_info.connection_id()];
    connections_.erase( conn_info.connection_id() );
  }
  if( service_code == 0 )
    return 0;
  try
  {
    queue_stack& queue = queue_manager_.find_queue( service_code );
    queue.post_worklink( qs_mt_on_close, conn_info );
  }
  catch( ... )
  {
  }
  return 0;
}
int queue_callback::on_recv( connect_info& conn_info, protocol_message_ptr request,
                             protocol_message_ptr& response )
{
  try
  {
    server_message_head head;
    //buffer_2_message_head( request->rd(), head );
    *request >> head;
   // LOG( INFO, "Worklink ���� ��" << ( int )head.message_code );
    queue_stack& queue = queue_manager_.find_queue( head.service_code );
    if( head.message_code == mt_register )
    {
      lock_type lock(mutex_);
      connections_[conn_info.connection_id()] = head.service_code;
    }
    queue.post_worklink( conn_info , head, request );
    return queue_callback::r_wait_send;
  }
  catch( std::exception& ex )
  {
    LOG( ERROR, "on_recv faild, " << ex.what() );
    return -1;
  }
}
int queue_callback::on_send( connect_info& conn_info, protocol_message_ptr response )
{
  return 0;
}
int queue_callback::on_timeout( connect_info& conn_info )
{
  return 0;
}


///////////////////////////////////////////////////////////////////////////////
}
