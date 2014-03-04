#include <stdexcept>
#include "logger_imp.h"
#include "event_queue.h"

namespace netsvr
{
///////////////////////////////////////////////////////////////////////////////
event_queue::event_queue(std::size_t timeout_seconds):
    running_(false), timeout_seconds_(timeout_seconds)
{
}
event_queue::~event_queue()
{
    stop();
}
void event_queue::run()
{
    if (running_)
        throw std::runtime_error("event queue already running");
    running_ = true;
    thr_.reset(new boost::thread(boost::bind(&event_queue::do_run, this)));
}
void event_queue::stop()
{
    if (!running_)
        return;
    running_ = false;
    thr_->interrupt();
    thr_->join();
    thr_.reset();
    events_.clear();
}
void event_queue::join()
{
    if (thr_)
        thr_->join();
}
void event_queue::do_run()
{
    while (running_ && !boost::this_thread::interruption_requested())
    {
        do_event();
    }
}
void event_queue::do_event()
{
    event_element_type element;
    {
        lock_type lock(mutex_);
        if (events_.empty())
        {
            if (!cond_.timed_wait(lock, boost::posix_time::seconds(1)))
                return;
        }
        if (events_.empty())
            return;
        element = events_.front();
        events_.pop_front();
    }
    boost::system_time now_time = boost::get_system_time();
    if (now_time > element.first)
    {
        LOG(ERROR, "event_queue::do_event event has timeout");
        return;
    }
    element.second();
}
bool event_queue::register_event(const event_function_handler& func)
{
    lock_type lock(mutex_);
    /*
    if( !lock.lock() )
    {
      LOG( ERROR, "event_queue::register_event lock error" );
      return false;
    }
    */
    boost::system_time abs_time = boost::get_system_time();
    abs_time += boost::posix_time::seconds(timeout_seconds_);
    events_.push_back(std::make_pair(abs_time, func));
    if (events_.size() > kMaxWaitEvents)
    {
        LOG(NOTICE, "event_queue waiting events exceed , " << events_.size());
    }
    cond_.notify_one();
    return true;
}
///////////////////////////////////////////////////////////////////////////////
}
