#ifndef _FDXS_SINGLETON_HPP_

#define _FDXS_SINGLETON_HPP_



namespace FDXSLib {

template<typename T>

class FDXSSingleton

{

 public:

  typedef T InstanceType;

  typedef T* InstancePtrType;

  typedef T& InstanceRefType;

  FDXSSingleton()

  {

  }

  ~FDXSSingleton()

  {

  }

  static InstancePtrType get()

  {

    if(s_instance_ == NULL)

    {

      LockType lock(mutex_);

      if(s_instance_ == NULL)

      {

        s_instance_ = new InstanceType;

      }

    }

    return s_instance_;

  }

  InstancePtrType operator->()

  {

    return get();

  }

  bool operator!()

  {

    InstancePtrType ptr = get();

    return !ptr;

  }

 private:

  typedef boost::recursive_mutex MutexType;

  typedef boost::unique_lock<MutexType> LockType;

  static MutexType mutex_;

  static InstancePtrType s_instance_;

};

template<typename T>

typename FDXSSingleton<T>::MutexType FDXSSingleton<T>::mutex_;

template<typename T>

typename FDXSSingleton<T>::InstancePtrType FDXSSingleton<T>::s_instance_;



}

#endif // _FDXS_SINGLETON_HPP_

