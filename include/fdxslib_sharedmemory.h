#ifndef _FDXSLIB_SHAREDMEMORY_H_
#define _FDXSLIB_SHAREDMEMORY_H_

namespace FDXSLib {
struct SharedMemFactory : public FDXSLibImplFactory
{
public:
  SharedMemFactory(){}
   virtual FDXSCommunication* make_factory(const std::string& name,std::size_t max_size);
};
extern SharedMemFactory sharedmemory;
}
#endif // _FDXSLIB_SHAREDMEMORY_H_