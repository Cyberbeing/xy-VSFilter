/************************************************************************/
/* author: xy                                                           */
/* date: 20110512                                                       */
/************************************************************************/
#ifndef __ONCE_LOGGER_H_1E73E3E0_8213_4960_8692_6895F0674ACA__
#define __ONCE_LOGGER_H_1E73E3E0_8213_4960_8692_6895F0674ACA__

#include "log4cplus/logger.h"
#include <WinBase.h>
#include <sstream>
#include <map>

class OnceLogger
{
public:
    OnceLogger(int id, const log4cplus::Logger& l, const log4cplus::tstring& _msg,
        const char* _file=NULL, int _line=-1)
        : logger(l), msg(_msg), file(_file), line(_line)
    {
        using namespace log4cplus;

		if( s_entered[id]>0 )
			return;

		s_entered[id]++;
        if(logger.isEnabledFor(TRACE_LOG_LEVEL))
        {
            std::wostringstream buff;
            buff<<LOG4CPLUS_TEXT("ENTER :")<<msg;
//            logger.forcedLog(TRACE_LOG_LEVEL, LOG4CPLUS_TEXT("ENTER at ")<<_start<<LOG4CPLUS_TEXT(" ms: ")<<msg, file, line);
            logger.forcedLog(TRACE_LOG_LEVEL, buff.str(), file, line);
        }
    }

    OnceLogger()
    {
        using namespace log4cplus;
		if( s_entered[id]>1 )
			return;

		s_entered[id]++;
        if(logger.isEnabledFor(TRACE_LOG_LEVEL))
        {
            std::wostringstream buff;
            buff<<LOG4CPLUS_TEXT("EXIT :")<<msg;
            logger.forcedLog(TRACE_LOG_LEVEL, buff.str(),
                file, line);
        }
    }
private:
	static int s_entered[256];
    int id;

    log4cplus::Logger logger;
    log4cplus::tstring msg;
    const char* file;
    int line;
};

#endif // end of __TIMING_LOGGER_H_1E73E3E0_8213_4960_8692_6895F0674ACA__
