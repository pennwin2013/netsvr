#ifndef _SESSION_UTILITY_HPP_

#define _SESSION_UTILITY_HPP_



#include <boost/asio.hpp>

#ifdef ENABLE_LOG

#include "logger_imp.h"

#define TCLOG(x) LOG(DEBUG,x)

#else

#define TCLOG(x) 

#endif



namespace FDXSLib

{

namespace utility

{

///////////////////////////////////////////////////////////////////////////////

class timeout_guard

{

public:

  timeout_guard ( boost::asio::deadline_timer& timer, std::size_t timeout ) : timer_ ( timer )

  {

    if ( timeout > 0 )

    {

      boost::asio::deadline_timer::time_type abs_time = boost::asio::deadline_timer::traits_type::now();

      abs_time += boost::posix_time::milliseconds ( timeout );

      timer_.expires_at ( abs_time );

    }

    else

    {

      timer_.expires_at ( boost::posix_time::pos_infin );

    }

  }

  ~timeout_guard()

  {

    timer_.expires_at ( boost::posix_time::pos_infin );

  }

private:

  boost::asio::deadline_timer& timer_;

};

}

}

#endif //_SESSION_UTILITY_HPP_



