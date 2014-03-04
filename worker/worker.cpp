#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <map>
#include <stdexcept>
#ifdef WIN32
#include <windows.h>
#else
#include <signal.h>
#endif

#include "server_link.h"
#include "XGetopt.h"
#include "global_func.h"


using namespace std;


#define WORKER_VERSION "v1.0 "__DATE__
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class Worker
{
public:
  Worker();
  ~Worker();
  int Run( int argc, char* const argv[] );
private:
  int DoRealWork();
  void StopClient();
  bool StartClient();
  void PrintUsage();
  void PrintVersion();
  std::string config_file_;
  std::string session_name_;
  std::size_t work_timeout_;
  server_link* client_;
  char message_buffer_[8192];
  std::size_t message_length_;
  char pkg_defing_name_[256];
  server_info connect_info;
};

Worker::Worker(): client_( NULL ), work_timeout_( 3000 ), message_length_( 0 )
{
}
Worker::~Worker()
{
  if(NULL != client_)
  {
    delete client_;
  }
}
int Worker::Run( int argc, char* const argv[] )
{
  const char short_opts[] = "hvc:s:";
  int option;
  while( ( option = getopt( argc, argv, short_opts ) ) != EOF )
  {
    switch( option )
    {
    case 'c':
      config_file_ = optarg;
      break;
    case 'h':
      PrintUsage();
      return 0;
    case 'v':
      PrintVersion();
      return 0;
    case 's':
      session_name_ = optarg;
      break;
    default:
      return -1;
    }
  }
  if( session_name_.empty() )
  {
    cout << "must specify session key name" << endl;
    return -1;
  }
  if( config_file_.empty() )
  {
    config_file_ = "./workercfg.ini";
  }
#if 0
  char full_path[2048] = {0};
  ::GetCurrentDirectory( sizeof( full_path ) - 1, full_path );
  std::string fn = "c:\\posp_";
  fn += session_name_;
  fn += ".txt";
  std::fstream fs( fn.c_str(), std::ios_base::app | std::ios_base::binary );
  fs << full_path;
  fs.close();
#endif
  connect_info.service_code = 1000;
  connect_info.internal_id = 1;
  strcpy(connect_info.svrip,"127.0.0.1");
  connect_info.svrport = atoi(this->session_name_.c_str());
  return DoRealWork();
}
void Worker::PrintUsage()
{
}
void Worker::PrintVersion()
{
  std::cout << "Version : " << WORKER_VERSION;
#ifdef DEBUG
  std::cout << " DEBUG" << std::endl;
#else
  std::cout << " RELEASE" << std::endl;
#endif
}
int Worker::DoRealWork()
{
  if( !StartClient() )
  {
    cout << "connect to server error!" << endl;
    return -1;
  }
  
  while(client_->test_connect())
  {
    server_message msg;
    //std::cout<<"开始接收数据"<<std::endl;
    if(!client_->get_message(msg))
    {
      // 接收数据错误，
      continue;
    }
    if(msg.message_length == 0 && msg.bin_data == 0)
    {
      continue; // 没有任务
    }
    std::cout<<"worker recv message :"<<msg.message_length<<std::endl;
    // process request
    if(!client_->answer_data(msg))
    {
      // 发送数据错误
      msg.bin_data = NULL;
      continue;
    }
    msg.bin_data = NULL;
    cout<<"process request success"<<endl;
  }
  std::cout << "工作进程退出" << endl;
  StopClient();
  cout<<"Press any key to exit"<<endl;
  string l;
  cin>>l;
  return 0;
}

void Worker::StopClient()
{
  if(client_ != NULL)
  {
    delete client_;
    client_ = NULL;
  }
  cout << "Close client" << endl;
}
bool Worker::StartClient()
{
  if(client_ != NULL)
    StopClient();
  client_ = new server_link(connect_info);
  if(client_->connect_to(3))
  {
    return true;
  }
  std::cout<<"不能连接中心服务器"<<std::endl;
  return false;
}
///////////////////////////////////////////////////////////////////////////////
int main( int argc, char* const argv[] )
{

#ifndef WIN32
	signal(SIGCHLD, SIG_IGN);
#endif
  Worker worker;
  return worker.Run( argc, argv );
}
