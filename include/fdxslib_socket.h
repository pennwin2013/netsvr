#ifndef _FDXSLIB_SOCKET_H_
#define _FDXSLIB_SOCKET_H_


namespace FDXSLib {
struct SocketFactory : public FDXSLibImplFactory
{
public:
  SocketFactory(){}
   virtual FDXSCommunication* make_factory(const std::string& name,std::size_t max_size);
};
extern SocketFactory socket;
}

#endif // _FDXSLIB_SOCKET_H_
