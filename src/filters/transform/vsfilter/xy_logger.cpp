/************************************************************************/
/* author: xy                                                           */
/* date: 20110512                                                       */
/************************************************************************/
#include "xy_logger.h"

#ifdef UNICODE

#define XY_TEXT(x)  L##x

#else //UNICODE

#define XY_TEXT(x)  x

#endif

namespace xy_logger
{

#ifdef __DO_LOG
    int g_log_once_id=0;
#endif

log4cplus::Logger g_logger = log4cplus::Logger::getInstance( XY_TEXT("global_logger_xy") );

void doConfigure(const log4cplus::tstring& configFilename, 
    log4cplus::Hierarchy& h /* = Logger::getDefaultHierarchy */, unsigned flags /* = 0 */)
{
#ifdef __DO_LOG
    log4cplus::PropertyConfigurator::doConfigure(configFilename, h, flags);
#endif
}

} //namespace xy_logger

