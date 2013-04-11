/************************************************************************/
/* author: xy                                                           */
/* date: 20110512                                                       */
/************************************************************************/
#ifndef __TIMING_LOGGER_H_D6F8D390_64A9_43E7_978F_9A55358FA0B1__
#define __TIMING_LOGGER_H_D6F8D390_64A9_43E7_978F_9A55358FA0B1__

#include "log4cplus/logger.h"
#include <WinBase.h>
#include <sstream>

class TimingLogger
{
public:
    TimingLogger(const log4cplus::Logger& l, const log4cplus::tstring& _msg,
        const char* _file=NULL, int _line=-1)
        : logger(l), msg(_msg), file(_file), line(_line)
    {
        using namespace log4cplus;
        if(logger.isEnabledFor(TRACE_LOG_LEVEL))
        {
            std::wostringstream buff;
            _start = GetTickCount();
            buff<<LOG4CPLUS_TEXT("ENTER at ")<<_start<<LOG4CPLUS_TEXT(" ms: ")<<msg;
//            logger.forcedLog(TRACE_LOG_LEVEL, LOG4CPLUS_TEXT("ENTER at ")<<_start<<LOG4CPLUS_TEXT(" ms: ")<<msg, file, line);
            logger.forcedLog(TRACE_LOG_LEVEL, buff.str(), file, line);
        }
    }

    ~TimingLogger()
    {
        using namespace log4cplus;
        if(logger.isEnabledFor(TRACE_LOG_LEVEL))
        {
            std::wostringstream buff;
            _end = GetTickCount();
//             logger.forcedLog(TRACE_LOG_LEVEL, LOG4CPLUS_TEXT("EXIT after ")<<_end-_start<<
//                 LOG4CPLUS_TEXT(" at ")<<_end<<LOG4CPLUS_TEXT(" ms:  ")<<msg,
//                 file, line);
            buff<<LOG4CPLUS_TEXT("EXIT after ")<<_end-_start<<LOG4CPLUS_TEXT(" at ")<<_end<<LOG4CPLUS_TEXT(" ms:  ")<<msg;
            logger.forcedLog(TRACE_LOG_LEVEL, buff.str(),
                file, line);
        }
    }
private:
    int _start;
    int _end;

    log4cplus::Logger logger;
    log4cplus::tstring msg;
    const char* file;
    int line;
};

#endif // end of __TIMING_LOGGER_H_D6F8D390_64A9_43E7_978F_9A55358FA0B1__

