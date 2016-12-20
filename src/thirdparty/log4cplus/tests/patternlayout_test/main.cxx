
#include <log4cplus/logger.h>
#include <log4cplus/consoleappender.h>
#include <log4cplus/layout.h>
#include <log4cplus/ndc.h>
#include <log4cplus/helpers/loglog.h>
#include <log4cplus/helpers/threads.h>
#include <log4cplus/helpers/sleep.h>
#include <iostream>
#include <string>

using namespace std;
using namespace log4cplus;
using namespace log4cplus::helpers;

int
main()
{
    cout << "Entering main()..." << endl;
    LogLog::getLogLog()->setInternalDebugging(true);
    try {
        SharedObjectPtr<Appender> append_1(new ConsoleAppender());
        append_1->setName(LOG4CPLUS_TEXT("First"));

        log4cplus::tstring pattern = LOG4CPLUS_TEXT("%d{%m/%d/%y %H:%M:%S,%Q} [%t] %-5p %c{2} %%%x%% - %m [%l]%n");
        //	std::tstring pattern = LOG4CPLUS_TEXT("%d{%c} [%t] %-5p [%.15c{3}] %%%x%% - %m [%l]%n");
        append_1->setLayout( std::auto_ptr<Layout>(new PatternLayout(pattern)) );
        Logger::getRoot().addAppender(append_1);

        Logger logger = Logger::getInstance(LOG4CPLUS_TEXT("test.a.long_logger_name.c.logger"));
        LOG4CPLUS_DEBUG(logger, "This is the FIRST log message...");

        sleep(1, 0);
        {
            NDCContextCreator ndc(LOG4CPLUS_TEXT("second"));
            LOG4CPLUS_INFO(logger, "This is the SECOND log message...");
        }

        sleep(1, 0);
        LOG4CPLUS_WARN(logger, "This is the THIRD log message...");

        sleep(1, 0);
        LOG4CPLUS_ERROR(logger, "This is the FOURTH log message...");

        sleep(1, 0);
        LOG4CPLUS_FATAL(logger, "This is the FOURTH log message...");
    }
    catch(...) {
        cout << "Exception..." << endl;
        Logger::getRoot().log(FATAL_LOG_LEVEL, LOG4CPLUS_TEXT("Exception occured..."));
    }

    cout << "Exiting main()..." << endl;
    return 0;
}

