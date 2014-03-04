#ifndef _FDXSLIB_H_
#define _FDXSLIB_H_

#include <string>
#include <stdlib.h>
#include <time.h>


namespace FDXSLib
{

class FDXSMessage
{
 public:
  explicit FDXSMessage(char* block = NULL,std::size_t len = 0):message_block_(block),length_(len),max_length_(len)
  {
    new_message_id();
  }
  ~FDXSMessage()
  {
  }
  char* message_block()
  {
    return message_block_;
  }
  std::size_t length() const
  {
    return length_;
  }
  void set_max_length(std::size_t len) 
  {
    max_length_ = len;
  }
  std::size_t max_length() const
  {
    return max_length_;
  }
  std::size_t message_id() const
  {
    return message_id_;
  }
  void new_message_id(std::size_t newid = 0)
  {
    if(newid > 0)
    {
      message_id_ = newid;
      return;
    }
    srand((unsigned int)time(NULL));
    message_id_ = rand();
  }
  FDXSMessage(const FDXSMessage& rhs)
  {
    this->message_block_ = rhs.message_block_;
    this->length_ = rhs.length_;
    this->message_id_ = rhs.message_id_;
  }
  FDXSMessage& operator=(const FDXSMessage& rhs)
  {
    this->message_block_ = rhs.message_block_;
    this->length_ = rhs.length_;
    this->message_id_ = rhs.message_id_;
    return *this;
  }
  bool operator==(const FDXSMessage& rhs) const
  {
    return this->message_id_ == rhs.message_id_;
  }
  bool operator!=(const FDXSMessage& rhs) const
  {
    return this->message_id_ != rhs.message_id_;
  }
 private:
  char *message_block_;
  std::size_t length_;
  std::size_t max_length_;
  std::size_t message_id_;
};

class FDXSCommunication;

class FDXSWorkerBase
{
 public:
  FDXSWorkerBase();
  virtual ~FDXSWorkerBase();
  virtual int SendData(FDXSMessage& message,int timeout);
  virtual int RecvData(FDXSMessage& message,int timeout);
  virtual bool IsDisconnected();
  virtual std::string last_error() const;
 protected:
  FDXSCommunication* communicator_;
};

class FDXSWorkerServer : public FDXSWorkerBase
{
 public:
  FDXSWorkerServer(const std::string server_name,std::size_t buffer_max_size = 8196);
  ~FDXSWorkerServer();
  bool StartListener();
  bool StopListener();
 private:
  FDXSWorkerServer(const FDXSWorkerServer&);
  FDXSWorkerServer& operator=(const FDXSWorkerServer&);
  std::string server_name_;
  
};
class FDXSWorkerClient : public FDXSWorkerBase
{
 public:
  FDXSWorkerClient(const std::string server_name);
  ~FDXSWorkerClient();
  bool ConnectServer();
  bool DisconnectServer();
 private:
  FDXSWorkerClient(const FDXSWorkerClient&);
  FDXSWorkerClient& operator=(const FDXSWorkerClient&);
  std::string server_name_;
};
struct FDXSLibImplFactory
{
public:
  FDXSLibImplFactory()
  {
  }
  virtual FDXSCommunication* make_factory(const std::string& name,std::size_t max_size) = 0;
};
///! 设置使用的实现方式,
/// 目前支持两种实现方式, sharedmem, tcpip 
bool FDXSLib_LoadImpl(const std::string& impl_name);
struct FDXSLibImplFactory;
bool FDXSLib_LoadImpl(FDXSLibImplFactory &factory);

}
#endif // _FDXSLIB_H_
