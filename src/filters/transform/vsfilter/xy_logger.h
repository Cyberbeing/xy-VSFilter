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
#include <ostream>

#if ENABLE_XY_LOG

#  define XY_LOG_VAR_2_STR(var) " "#var"='"<<(var)<<"' "
#  define XY_LOG_TRACE(msg)     LOG4CPLUS_TRACE(xy_logger::g_logger, __FUNCTION__<<"\t:"<<msg)
#  define XY_LOG_DEBUG(msg)     LOG4CPLUS_DEBUG(xy_logger::g_logger, __FUNCTION__<<"\t:"<<msg)
#  define XY_LOG_INFO(msg)      LOG4CPLUS_INFO(xy_logger::g_logger, __FUNCTION__<<"\t:"<<msg)
#  define XY_LOG_WARN(msg)      LOG4CPLUS_WARN(xy_logger::g_logger, __FUNCTION__<<"\t:"<<msg)
#  define XY_LOG_ERROR(msg)     LOG4CPLUS_ERROR(xy_logger::g_logger, __FUNCTION__<<"\t:"<<msg)
#  define XY_LOG_FATAL(msg)     LOG4CPLUS_FATAL(xy_logger::g_logger, __FUNCTION__<<"\t:"<<msg)

  extern int g_log_once_id;

#  define CHECK_N_LOG(hr, msg) if(FAILED(hr)) XY_LOG_ERROR(msg)

#  define XY_RAND_VAR2(v,line) v ## line
#  define XY_RAND_VAR(v,line) XY_RAND_VAR2(v,line)
#  define XY_AUTO_TIMING(msg) TimingLogger XY_RAND_VAR(t,__LINE__)(xy_logger::g_logger, \
    ((log4cplus::tostringstream*)&(log4cplus::tostringstream()<<msg))->str(), \
    __FILE__, __LINE__)

#  define XY_LOG_ONCE(id, msg) OnceLogger(id, xy_logger::g_logger, msg, __FILE__, __LINE__)
#  define XY_LOG_ONCE2(msg) {                    \
    static bool entered=false;                   \
    if(!entered) {                               \
      entered=true;                              \
      LOG4CPLUS_INFO(xy_logger::g_logger, msg);  \
    }                                            \
  }

#  define XY_DO_ONCE(expr) do { \
    static bool entered=false;\
    if(!entered) {            \
        entered = true;       \
        {expr;}               \
    }                         \
} while(0)


#  define DO_FOR(num, expr) do {  \
    static int repeat_num=(num);  \
    if (repeat_num>0)             \
    {                             \
        repeat_num--;             \
        expr;                     \
    }                             \
} while(0)

#else //ENABLE_XY_LOG

#  define XY_LOG_VAR_2_STR(var)
#  define XY_LOG_TRACE(msg)
#  define XY_LOG_DEBUG(msg)
#  define XY_LOG_INFO(msg)
#  define XY_LOG_WARN(msg)
#  define XY_LOG_ERROR(msg)
#  define XY_LOG_FATAL(msg)

#  define CHECK_N_LOG(hr, msg)

#  define XY_AUTO_TIMING(msg)
#  define XY_LOG_ONCE(id, msg)
#  define XY_DO_ONCE(expr)
#  define DO_FOR(num, expr)

#endif

std::ostream& operator<<(std::ostream& os, const POINT& obj);
std::wostream& operator<<(std::wostream& os, const POINT& obj);

std::ostream& operator<<(std::ostream& os, const SIZE& obj);
std::wostream& operator<<(std::wostream& os, const SIZE& obj);

std::ostream& operator<<(std::ostream& os, const RECT& obj);
std::wostream& operator<<(std::wostream& os, const RECT& obj);

CString XyUuidToString(const UUID& in_uuid);

namespace xy_logger
{
#if ENABLE_XY_LOG
  extern log4cplus::Logger g_logger;
#endif

bool doConfigure(log4cplus::tistream& property_stream);
bool doConfigure(const log4cplus::tstring& configFilename);

void write_file(const char * filename, const void * buff, int size);

void DumpPackBitmap2File(POINT pos, SIZE size, LPCVOID pixels, int pitch, const char *filename);

void XyDisplayType(LPTSTR label, const AM_MEDIA_TYPE *pmtIn);

void XyDumpGraph(IFilterGraph *pGraph, DWORD dwLevel);

} //namespace xy

#endif // end of __XY_LOGGER_H_3716A27F_2940_4636_8BC9_C764648362AE__
