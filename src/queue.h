#ifndef _KS_QUEUE_H_
#define _KS_QUEUE_H_

#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/locks.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/any.hpp>
#include <Ice/Ice.h>
#include "callserver.h"
//#include "fdxslib.h"
#include "server_session.h"
#include "singleton.hpp"
#include "subprocess.hpp"
#include "server_msg.h"
#include "queue_stack.h"

namespace netsvr
{
const int CORE_SERVICE_CODE = 1;//=====20131226新添加，主ecardsvr的service_code
using namespace FDXSLib;
typedef FDXSLib::FDXSSingleton<FDXSLib::server> server_instance;

inline void FDXS_Sleep( int secs, int millsecs = 0 )
{
  boost::xtime xt;
  boost::xtime_get( &xt, boost::TIME_UTC );
  xt.sec += secs;
  boost::thread::sleep( xt );
}

class worker_link;
struct netsvr_message;

class operator_timeout_exception : std::exception
{
};
class no_request_exception : std::exception
{
};


class queue_callback;
class queue_manager
{
public:
  enum {r_success = 0, r_exceed_max, r_error};
  queue_manager(): is_done_( true ),service_code_counter(2)//==20140123新添加，用来给serviceCode编号
  {}
  queue_manager( std::size_t max_queue, Ice::PropertiesPtr& props );
  ~queue_manager();
  int update_properties( Ice::PropertiesPtr& props );
  int start( std::size_t listen_port );
  int stop();
  int push_queue( const netsvr_message_ptr& request );
  bool is_done() const;
  inline std::size_t listen_port() const
  {
    return listen_port_;
  }
  inline const Ice::PropertiesPtr& get_properties() const
  {
    return properties_;
  }
  inline queue_stack& find_queue( std::size_t service_code );
  void getServerStatistic(std::string& str);
  //bool process_register( const svr_msg_register& reg );
private:
  struct worker_process_info
  {
    worker_process_info( queue_stack& q ): queue( q )
    {}
    queue_stack& queue;
    subprocess::child process;
  };
  typedef boost::timed_mutex::scoped_timed_lock lock_type;
  typedef boost::shared_ptr<queue_stack> queue_stack_ptr;
  typedef std::map<std::size_t, queue_stack_ptr> message_queue_map_type; 
  typedef std::vector<worker_process_info> worker_process_info_vector;

  ///////////////////////////////////////////////////////////////////////////////
  queue_manager( const queue_manager& rhs );
  queue_manager& operator=( const queue_manager& rhs );

  int setup_queues( const std::string& name );
  void run();
  void check_terminate_flag();
  bool start_worker_process( worker_process_info& info );
  void stop_worker_process( worker_process_info& info );
  ///////////////////////////////////////////////////////////////////////////////
  std::size_t listen_port_;
  boost::timed_mutex mutex_;
  boost::condition_variable_any cond_;
  message_queue_map_type queue_managers_;
  worker_process_info_vector worker_process_;
  boost::thread_group thr_group_;
  Ice::PropertiesPtr properties_;
  queue_callback* queue_manager_;
  bool is_done_;
  std::size_t service_code_counter;//==20140123新添加，分配serviceCode使用的计数器
};




class queue_callback : public FDXSLib::server_callback
{
public:
  queue_callback( queue_manager& queue );
  virtual ~queue_callback();

  int on_connect( connect_info& conn_info );
  int on_disconnect( connect_info& conn_info );
  int on_recv( connect_info& conn_info, protocol_message_ptr request, protocol_message_ptr& response );
  int on_send( connect_info& conn_info, protocol_message_ptr response );
  int on_timeout( connect_info& conn_info );
private:
  typedef boost::mutex::scoped_lock lock_type;
  typedef std::map<std::size_t, std::size_t> connection_map_type;
  queue_manager& queue_manager_;
  connection_map_type connections_;
  boost::mutex mutex_;
};


} // netsvr
#endif // _KS_QUEUE_H_
