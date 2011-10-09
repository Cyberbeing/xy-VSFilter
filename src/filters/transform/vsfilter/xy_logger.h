/************************************************************************/
/* author: xy                                                           */
/* date: 20110511                                                       */
/************************************************************************/
#ifndef __XY_LOGGER_H_3716A27F_2940_4636_8BC9_C764648362AE__
#define __XY_LOGGER_H_3716A27F_2940_4636_8BC9_C764648362AE__

#include "log4cplus/logger.h"
#include "log4cplus/configurator.h"
#include "timing_logger.h"
#include "once_logger.h"
#include <stdio.h>

#ifdef __DO_LOG
extern int g_log_once_id;

#define XY_LOG_INFO(msg) LOG4CPLUS_INFO(xy_logger::g_logger, msg)
#define XY_AUTO_TIMING(msg) TimingLogger(xy_logger::g_logger, msg, __FILE__, __LINE__)
#define XY_LOG_ONCE(id, msg) OnceLogger(id, xy_logger::g_logger, msg, __FILE__, __LINE__)
#define XY_LOG_ONCE2(msg) {\
    static bool entered=false;\
    if(!entered)\
    {\
        entered=true;\
        LOG4CPLUS_INFO(xy_logger::g_logger, msg);\
    }\
}


#define XY_DO_ONCE(expr) do {\
	static bool entered=false;\
    if(!entered) { \
        entered = true;\
	    {expr;}\
    }\
} while(0)

#else //__DO_LOG

#define XY_LOG_INFO(msg)
#define XY_AUTO_TIMING(msg)
#define XY_LOG_ONCE(id, msg)
#define XY_DO_ONCE(expr)

#endif

namespace xy_logger
{

#ifdef __DO_LOG
extern log4cplus::Logger g_logger;
#endif

void doConfigure(const log4cplus::tstring& configFilename);

static void write_file(const char * filename, const void * buff, int size)
{
    FILE* out_file = fopen(filename,"ab");
	fwrite(buff, size, 1, out_file);
	fclose(out_file);
}

} //namespace xy

#endif // end of __XY_LOGGER_H_3716A27F_2940_4636_8BC9_C764648362AE__
