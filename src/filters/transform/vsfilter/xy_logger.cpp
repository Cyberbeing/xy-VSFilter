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
    
    log4cplus::Logger g_logger = log4cplus::Logger::getInstance( XY_TEXT("global_logger_xy") );
#endif

void doConfigure(const log4cplus::tstring& configFilename)
{
#ifdef __DO_LOG
    log4cplus::Hierarchy& h = Logger::getDefaultHierarchy;
    unsigned flags = 0;
    log4cplus::PropertyConfigurator::doConfigure(configFilename, h, flags);
#endif
}

} //namespace xy_logger

