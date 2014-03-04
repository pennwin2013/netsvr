#ifndef _SERVER_SESSION_H_
#define _SERVER_SESSION_H_

#include <string>
#include <boost/shared_ptr.hpp>
#include "protocol_def.h"

namespace FDXSLib
{

struct connect_info
{
public:
  connect_info();
  explicit connect_info( std::size_t id );
  ~connect_info();
  inline std::size_t connection_id() const
  {
    return connection_id_;
  }
  connect_info( const connect_info& rhs )
  {
    this->connection_id_ = rhs.connection_id_;
  }
  connect_info& operator= ( const connect_info& rhs )
  {
    this->connection_id_ = rhs.connection_id_;
    return *this;
  }
  void new_id();
private:
  std::size_t connection_id_;
};

typedef boost::shared_ptr<protocol_message> protocol_message_ptr;

class server_callback
{
public:
  enum {r_error = -1, r_success = 0, r_response , r_wait_send, r_wait_recv };
  server_callback()
  {}
  virtual ~server_callback()
  {}
  ///////////////////////////////////////////////////////////////////////////////
  virtual int on_connect( connect_info& conn_info ) = 0;
  virtual int on_disconnect( connect_info& conn_info ) = 0;
  /*!
   \breif - ��������յ����ݺ����
   \param conn_info - ������Ϣ
   \param request - �������ݰ�
   \param response - Ӧ�����ݰ�
   \return - {r_error = -1, r_success = 0, r_response , r_wait_send, r_wait_recv }
   */
  virtual int on_recv( connect_info& conn_info, protocol_message_ptr request, protocol_message_ptr& response ) = 0;
  virtual int on_send( connect_info& conn_info, protocol_message_ptr response ) = 0;
  virtual int on_timeout( connect_info& conn_info ) = 0;
};

class server
{
public:
  server();
  virtual ~server();
  virtual int start( std::size_t port );
  virtual void stop();
  virtual bool is_running() const;
  virtual server_callback* register_callback( server_callback* callback );
  /*!
   \breif - ���ڷ����첽��Ϣ
   \return  - 0 ��ʾ�����첽������Ϣ�ɹ���-1��ʾʧ��
   */
  virtual int push_response( const connect_info& conn_info, protocol_message_ptr response );
  virtual void close_connection( const connect_info& conn_info );
private:
  server( const server& );
  server& operator= ( const server& );
  /////////////////////////////////////////////////////////////////////////////////
  server* implement_;
};


}
#endif // _SERVER_SESSION_H_
