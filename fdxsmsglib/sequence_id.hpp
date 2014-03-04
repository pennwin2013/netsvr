#ifndef _SEQUENCE_ID_HPP_

#define _SEQUENCE_ID_HPP_



#include <boost/thread.hpp>

#include <boost/utility.hpp>



namespace FDXSLib

{



template<class T, class U, std::size_t N = 0>

class sequence_id : boost::noncopyable

{

public:

  typedef T mutex_type;

  typedef U clazz_type; // dummy

  typedef typename mutex_type::scoped_lock lock_type;

  static const std::size_t kMaxSeqId = 0xFFFFFFFF;



  sequence_id() : id_ ( N )

  {

  }

  ~sequence_id()

  {

  }

  std::size_t next_id()

  {

    lock_type lock ( m_ );

    if(id_ == kMaxSeqId)

      id_ = N;

    return ( ++id_ );

  }

  std::size_t current_id()

  {

    lock_type lock ( m_ );

    return id_;

  }

private:

  mutex_type m_;

  std::size_t id_;

};



template<class T, class U, std::size_t N>

const std::size_t sequence_id<T,U,N>::kMaxSeqId;



}



#endif // _SEQUENCE_ID_HPP_

