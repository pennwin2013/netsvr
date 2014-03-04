// vim: ts=2:et
#include <string>
#include <stdexcept>
#include <sstream>
#include <iostream>
#ifdef WIN32
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#endif
#include "subprocess.hpp"
///////////////////////////////////////////////////////////////////////////////
namespace subprocess
{
///////////////////////////////////////////////////////////////////////////////
status::status( const child& c, exit_code_type code ):
  child_( c ), exit_code_( code )
{
}
status::~status()
{
}

///////////////////////////////////////////////////////////////////////////////
#ifdef WIN32
child::child( HANDLE h ):handle_(h),pid_(0)
{
  pid_ = get_invalidate_pid();
  if(handle_ != INVALID_HANDLE_VALUE)
  {
    pid_ = GetProcessId(handle_);
  }
}
#else
child::child( pid_type pid ): pid_( pid )
{
}
#endif

child::~child()
{
#ifdef WIN32
#else
#endif
}

child::child( const child& rhs )
{
  this->pid_ = rhs.pid_;
#ifdef WIN32
  this->handle_ = rhs.handle_;
#endif
}
child& child::operator=( const child& rhs )
{
  this->pid_ = rhs.pid_;
#ifdef WIN32
  this->handle_ = rhs.handle_;
#endif
  return *this;
}
status child::wait()
{
#ifdef WIN32
  if( pid_ == get_invalidate_pid() )
  {
    return status( *this, 0 );
  }
  ::WaitForSingleObject( handle_, INFINITE );
  DWORD exit_code;

  if( ::GetExitCodeProcess( handle_, &exit_code ) )
  {
    return status( *this, exit_code );
  }
  ::CloseHandle( handle_ );
  return status( *this, 0 );
#else
  int st, options;
  options = WUNTRACED;
  waitpid( pid_, &st, options );
  return status( *this, st );
#endif
}

bool child::terminate()
{
  if( pid_ == get_invalidate_pid() )
  {
    return false;
  }
#ifdef WIN32
  try
  {
    BOOL ret;
    ret = ::TerminateProcess( handle_, 0 );
    //::CloseHandle(pid_);
    return ( ret ) ? true : false;
  }
  catch( ... )
  {
    return true;
  }
#else
  if( !kill( pid_, SIGTERM ) )
  {
    int st, options;
    options = WUNTRACED;
    waitpid( pid_, &st, options );
    return true;
  }
  return false;
#endif
}

bool child::alive()
{
  if( pid_ == child::get_invalidate_pid() )
    return false;
#ifdef WIN32
  DWORD exit_code = 0;
  BOOL ret = ::GetExitCodeProcess( handle_, &exit_code );
  if( ret )
  {
    if( STILL_ACTIVE == exit_code )
      return true;
  }
  return false;
#else
  if( kill( pid_, 0 ) )
    return false;
  return true;
#endif
}
///////////////////////////////////////////////////////////////////////////////
command_line::command_line( const std::string& cmd ): command_( cmd )
{

}

command_line::~command_line()
{

}

command_line& command_line::argument( const std::string& arg )

{

  if( args_.size() >= kMaxCommandArgs )
    throw std::runtime_error( "Max argument exceed" );

  std::stringstream is(arg);
  while( !is.eof() && is.str().find(' ') >= 0)
  {
    std::string v;
    std::getline(is ,v ,' ');
    if(!v.empty())
      args_.push_back(v);
  }
  return *this;
}

command_line::ArgumentVector::const_iterator command_line::begin() const

{
  return args_.begin();
}

command_line::ArgumentVector::const_iterator command_line::end() const
{
  return args_.end();
}
///////////////////////////////////////////////////////////////////////////////
launcher::launcher()
{
}
launcher::~launcher()
{
}

child launcher::start( const command_line& cmd_line, context& ctx )
{
  throw std::runtime_error( "not implement function" );
}

child launcher::start( const command_line& cmd_line )
{
#ifdef WIN32
  std::stringstream stream;
  stream << cmd_line.command() << " ";
  for( command_line::ArgumentItertor iter = cmd_line.begin();
       iter != cmd_line.end(); ++iter )
  {
    stream << *iter << " ";
  }
  BOOL ret;
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  memset( &si, 0, sizeof si );
  memset( &pi, 0, sizeof pi );

  ret = ::CreateProcess( NULL, ( LPSTR )stream.str().c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi );
  if( !ret )
  {
    throw std::runtime_error( "cannot create subprocess!" );
  }
  child c( pi.hProcess );
  return c;
#else
  pid_t id = fork();
  if( id > 0 )
  {
    return child( id );
  }
  else if( id == 0 )
  {
    const char* argv[command_line::kMaxCommandArgs + 1];
    memset( argv, 0, sizeof argv );
    argv[0] = cmd_line.command().c_str();
    std::size_t i = 1;
    for( command_line::ArgumentItertor iter = cmd_line.begin();
         iter != cmd_line.end(); ++iter, ++i )
    {
      argv[i] = ( *iter ).c_str();
    }
    argv[i] = 0;
    //for(std::size_t x = 0; argv[x] != 0; ++x)
    //  std::cout<<"argv["<<x<<"]="<<argv[x]<<std::endl;
    execv( cmd_line.command().c_str(), ( char * const* )argv );
  }
  else
  {
    throw std::runtime_error( "cannot create subprocess" );
  }
#endif

}

}
