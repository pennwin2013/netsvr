#ifndef _C_LOGGER_H
#define _C_LOGGER_H

#include <string>
#include <sys/stat.h>
//#include <unistd.h>
#include <iostream>
#include <string.h>
#include <log4cplus/logger.h>
#include <log4cplus/consoleappender.h>
#include <log4cplus/layout.h>
#include <log4cplus/fileappender.h>
#include <log4cplus/configurator.h>

using namespace std;
using namespace log4cplus;
using namespace log4cplus::helpers;

// see ref: http://www.delorie.com/gnu/docs/gcc/gcc_78.html
//  
// __PRETTY_FUNCTION__  complete function description
// __FUNCTION__ summary function description

//#define APPEND_FUNCTION(MSG) MSG << " [" << __PRETTY_FUNCTION__ << "]"
#define APPEND_FUNCTION(MSG) "[" << __FUNCTION__ << "] ["<<__LINE__<<"]" << MSG
//#define APPEND_FUNCTION(MSG) MSG
    
#define LOG(TYPE,MSG) LOG_##TYPE( g_logger , APPEND_FUNCTION(MSG));
    
#define LOG_TRACE(a,b) LOG4CPLUS_TRACE(a,b)
#define LOG_DEBUG(a,b) LOG4CPLUS_DEBUG(a,b)
#define LOG_INFO(a,b)  LOG4CPLUS_INFO(a,b)
#define LOG_WARN(a,b)  LOG4CPLUS_WARN(a,b)
#define LOG_ERROR(a,b) LOG4CPLUS_ERROR(a,b)
#define LOG_FATAL(a,b) LOG4CPLUS_FATAL(a,b)
#define LOG_NOTICE(a,b) LOG4CPLUS_INFO(a,b)

extern log4cplus::Logger g_logger;

bool init_logger( const string &log_config_pathname );

#endif
