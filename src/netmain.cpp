#include <Ice/Ice.h>
#include <Ice/Application.h>
#include <callserver.h>
#include <iostream>
#include <fstream>

#if defined(WIN32)
#include <boost/bind.hpp>
#include <boost/function.hpp>
#else
#include <signal.h>
#endif

#include "CurrentSqlContext.h"
#include "../include/fdxslib.h"
#include "queue.h"
#include "singleton.hpp"
#include "logger_imp.h"

bool application_kill_flag = false;
void signal_handler(int type)
{
	LOG(INFO,"type="<<type);
	switch(type)
	{
	case SIGINT:
		LOG(INFO,"recv a int signal");
		break;
	case SIGTERM:
		LOG(INFO,"recv a term signal");
		::application_kill_flag = true;
		break;
	}
	return;
}
using namespace std;
using namespace transfer;

typedef FDXSLib::FDXSSingleton<netsvr::queue_manager> queue_manager_inst;

class Server : public CallServer
{
private:
  const CurrentSqlContext _currentCtx;
public:
  string errmsg;
  int errcode;

  Server( const CurrentSqlContext& );
  ~Server();
  int ProcessRequest( const char* sbuff, int slen, char* rbuff, int& rlen );
  void SendMsg_async( const AMD_CallServer_SendMsgPtr& cb, const RequestProtocol& req, const Ice::Current& current );
  void QueryMsg_async( const AMD_CallServer_QueryMsgPtr& cb , const RequestProtocol& req, const Ice::Current& current );
  transfer::ByteSeq ServerStatistic(const Ice::Current& current);
  int SaveMsg( int len, const char* msg );
};

Server::Server( const CurrentSqlContext& currentCtx ) :
  _currentCtx( currentCtx )
{
  errcode = 0;
  errmsg = "";
}
Server::~Server()
{
}

transfer::ByteSeq Server::ServerStatistic(const Ice::Current& current)
{
	try
	{
		//LOG(INFO,"收到ServerStatistic请求");
		std::string sendStr="";
		queue_manager_inst::get()->getServerStatistic(sendStr);
		//LOG(INFO,"ServerStatistic::sendStr="<<sendStr.c_str());
		::transfer::ByteSeq sendData;
		sendData.clear();
		for(int i = 0; i < sendStr.size(); i++)
			sendData.push_back(sendStr[i]);
		sendData.push_back(0);
		return sendData;
	}
	catch(...)
	{
		LOG(INFO,"捕获ServerStatistic异常");
	}
	
//
	//transfer::ResponseProtocol resp;
 //   resp.returnCode = 9999;
 //   std::stringstream msg;

 //   msg<<"系统未找到服务号 "<< req.serviceCode <<" 的服务";
 //   resp.returnMessage = msg.str();
 //   cb->ice_response( resp );
}
void Server::SendMsg_async( const AMD_CallServer_SendMsgPtr& cb, const RequestProtocol& req, const Ice::Current& current )
{
  //cout << " request buff len:" << req.data.size() << endl;
  const char* ucReqBuff = ( const char* ) & ( *req.data.begin() );
  //cout << " request buff data:" << ucReqBuff << endl;
  int ret = SaveMsg( req.data.size(), ucReqBuff );
  if( ret )
  {
    cout << "save msg error ret=" << ret << endl;
    return;
  }
  transfer::RequestProtocol protocol;
  protocol = req;
  netsvr::netsvr_message_ptr request(
    new netsvr::netsvr_message( netsvr::netsvr_message::nm_sendmsg, protocol, cb ) );
  if(queue_manager_inst::get()->push_queue( request ))
  {
    transfer::ResponseProtocol resp;
    resp.returnCode = 9999;
    std::stringstream msg;
	//msg<<"系统未找到服务号 "<< req.serviceCode <<" 的服务";
	msg<<"系统未找到功能号 "<< req.requestCode <<" 的服务";
	
    resp.returnMessage = msg.str();
    cb->ice_response( resp );
  }
  return;
}
void Server::QueryMsg_async( const AMD_CallServer_QueryMsgPtr& cb , const RequestProtocol& req, const Ice::Current& current )
{
  transfer::RequestProtocol protocol;
  protocol = req;
  netsvr::netsvr_message_ptr request(
    new netsvr::netsvr_message( netsvr::netsvr_message::nm_querymsg, protocol, cb ) );
  queue_manager_inst::get()->push_queue( request );
  return;
}

int Server::SaveMsg( int len, const char* message )
{
  return 0;
}
int Server::ProcessRequest( const char* sbuff, int slen, char* rbuff, int& rlen )
{
  return -1;
}

class YKTServer: public Ice::Application
{
private:
  CurrentSqlContext _currentCtx;
public:
  YKTServer( const CurrentSqlContext& );
  virtual int run( int, char*[] );
};
YKTServer::YKTServer( const CurrentSqlContext& currentCtx ) :Ice::Application(Ice::NoSignalHandling),
  _currentCtx( currentCtx )
{
}

int YKTServer::run( int argc, char* argv[] )
{
  Ice::PropertiesPtr properties = communicator()->getProperties();

  Ice::ObjectAdapterPtr adapter = communicator()->createObjectAdapter( "YKT" );

  Ice::Identity id = communicator()->stringToIdentity( properties->getProperty("Identity") );
  transfer::CallServerPtr callserver = new Server(_currentCtx);
  //adapter->add( new Server( _currentCtx ),  );
  adapter->add( callserver, id );

  adapter->activate();

  //Ice::PropertiesPtr prop = communicator()->getProperties();
  string port = properties->getPropertyWithDefault( "queue.port", "10010" );
  string maxcount = properties->getPropertyWithDefault( "queue.processcount", "5" );

  std::size_t iport = atoi( port.c_str() );
  std::size_t imax = atoi( maxcount.c_str() );

  queue_manager_inst::get()->update_properties( properties );
  LOG(INFO,"start signal handler");
  signal(SIGINT,signal_handler);
  signal(SIGTERM,signal_handler);//kill 命令的捕获
  if( queue_manager_inst::get()->start( iport ) )
  {
    return -1;
  }
  communicator()->waitForShutdown();
  queue_manager_inst::get()->stop();
  return EXIT_SUCCESS;
}


#if defined(WIN32)
boost::function0<void> console_ctrl_function;

BOOL WINAPI console_ctrl_handler( DWORD ctrl_type )
{
  switch( ctrl_type )
  {
  case CTRL_C_EVENT:
  case CTRL_BREAK_EVENT:
  case CTRL_CLOSE_EVENT:
  case CTRL_SHUTDOWN_EVENT:
    //Ice::Application::destroyOnInterrupt();
    Ice::Application::shutdownOnInterrupt();
    return TRUE;
  default:
    return FALSE;
  }
}
#endif


int main( int argc, char* argv[] )
{
#if 0
  struct sqlca sqlca;
  EXEC SQL CONTEXT USE DEFAULT;
  EXEC SQL ENABLE THREADS;
#endif // USE_ORACLE
  init_logger( "log4cplus.conf" );
  LOG(INFO,"in main");
  Ice::InitializationData initData;
  initData.properties = Ice::createProperties( argc, argv );
 // initData.properties->load( "config.server" );


  const string connectInfo = initData.properties->getPropertyWithDefault( "Oracle.ConnectInfo", "ykt_cur/kingstar@orcl" );
  CurrentSqlContext currentCtx( connectInfo );
#if 0
  initData.threadHook = currentCtx.getHook();
#endif

  YKTServer app( currentCtx );
#ifdef WIN32
  ::SetConsoleCtrlHandler( console_ctrl_handler, TRUE );
#else
  signal(SIGCHLD, SIG_IGN);
#endif
  return app.main( argc, argv, initData );
}
