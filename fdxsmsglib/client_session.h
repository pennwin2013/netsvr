#ifndef _CLIENT_SESSION_H_
#define _CLIENT_SESSION_H_

#include <string>
#include "protocol_def.h"

namespace FDXSLib
{
///////////////////////////////////////////////////////////////////////////////
class client
{
public:
  client();
  virtual ~client();

  virtual bool connect( const char* ip, std::size_t port, std::size_t timeout );
  virtual void disconnect();
  virtual bool send_message( const protocol_message* message, std::size_t timeout );
  virtual bool recv_message( protocol_message* message, std::size_t timeout );
private:
  client( const client& );
  client& operator= ( const client& );
  client* implement_;
};
///////////////////////////////////////////////////////////////////////////////
}
#endif // _CLIENT_SESSION_H_
