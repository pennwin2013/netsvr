#ifndef _EVENT_QUEUE_H_

#define _EVENT_QUEUE_H_



#include <list>

#include <boost/thread.hpp>

#include <boost/bind.hpp>

#include <boost/utility.hpp>



namespace netsvr

{

///////////////////////////////////////////////////////////////////////////////

class event_queue : boost::noncopyable

{

public:

    typedef boost::function0<void> event_function_handler;

    static const std::size_t kMaxWaitEvents = 100;

    event_queue(std::size_t timeout_seconds = 10);

    ~event_queue();

    void run();

    void stop();

    void join();

    bool register_event(const event_function_handler& func);

private:

    typedef std::pair<boost::system_time, event_function_handler> event_element_type;

    typedef std::list<event_element_type> events_list_type;

    typedef boost::mutex::scoped_lock lock_type;

    void do_run();

    void do_event();

    boost::shared_ptr<boost::thread> thr_;

    boost::condition_variable cond_;

    boost::mutex mutex_;

    events_list_type events_;

    bool running_;

    std::size_t timeout_seconds_;

};

///////////////////////////////////////////////////////////////////////////////

}

#endif // _EVENT_QUEUE_H_

