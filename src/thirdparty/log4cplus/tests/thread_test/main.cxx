
#include <log4cplus/consoleappender.h>
#include <log4cplus/layout.h>
#include <log4cplus/logger.h>
#include <log4cplus/ndc.h>
#include <log4cplus/helpers/loglog.h>
#include <log4cplus/helpers/threads.h>
#include <log4cplus/helpers/sleep.h>
#include <log4cplus/streams.h>
#include <exception>
#include <iostream>
#include <string>


using namespace std;
using namespace log4cplus;
using namespace log4cplus::helpers;
using namespace log4cplus::thread;


#define MILLIS_TO_NANOS 1000
#define NUM_THREADS 4
#define NUM_LOOPS 10

class SlowObject {
public:
    SlowObject() 
      : mutex( LOG4CPLUS_MUTEX_CREATE ), 
        logger(Logger::getInstance(LOG4CPLUS_TEXT("SlowObject"))) 
    { logger.setLogLevel(TRACE_LOG_LEVEL); }

    void doSomething() {
        LOG4CPLUS_TRACE_METHOD(logger, LOG4CPLUS_TEXT("SlowObject::doSomething()"));
        LOG4CPLUS_BEGIN_SYNCHRONIZE_ON_MUTEX( mutex )
            LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("Actually doing something..."));
            sleep(0, 75 * MILLIS_TO_NANOS);
            LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("Actually doing something...DONE"));
        LOG4CPLUS_END_SYNCHRONIZE_ON_MUTEX;
        log4cplus::thread::yield();
    }

private:
    LOG4CPLUS_MUTEX_PTR_DECLARE mutex;
    Logger logger;
};
SlowObject *global;


class TestThread : public AbstractThread {
public:
    TestThread(tstring n) 
        : name(n), logger(Logger::getInstance(LOG4CPLUS_TEXT("test.TestThread"))) 
     {
     }

    virtual void run();

private:
    tstring name;
    Logger logger;
};


int
main() 
{
    auto_ptr<SlowObject> globalContainer(new SlowObject());
    global = globalContainer.get();

    try {
        log4cplus::helpers::LogLog::getLogLog()->setInternalDebugging(true);
        Logger logger = Logger::getInstance(LOG4CPLUS_TEXT("main"));
        Logger::getRoot().setLogLevel(INFO_LOG_LEVEL);
        LogLevel ll = logger.getLogLevel();
        tcout << "main Priority: " << getLogLevelManager().toString(ll) << endl;

        helpers::SharedObjectPtr<Appender> append_1(new ConsoleAppender());
        append_1->setLayout( std::auto_ptr<Layout>(new log4cplus::TTCCLayout()) );
        Logger::getRoot().addAppender(append_1);
        append_1->setName(LOG4CPLUS_TEXT("cout"));

	    append_1 = 0;

        log4cplus::helpers::SharedObjectPtr<TestThread> threads[NUM_THREADS];
        int i = 0;
        for(i=0; i<NUM_THREADS; ++i) {
            tostringstream s;
            s << "Thread-" << i;
            threads[i] = new TestThread(s.str());
        }

        for(i=0; i<NUM_THREADS; ++i) {
            threads[i]->start();
        }
        LOG4CPLUS_DEBUG(logger, "All Threads started...");

        for(i=0; i<NUM_THREADS; ++i) {
            while(threads[i]->isRunning()) {
                sleep(0, 200 * MILLIS_TO_NANOS);
            }
        }
        LOG4CPLUS_INFO(logger, "Exiting main()...");
    }
    catch(std::exception &e) {
        LOG4CPLUS_FATAL(Logger::getRoot(), "main()- Exception occured: " << e.what());
    }
    catch(...) {
        LOG4CPLUS_FATAL(Logger::getRoot(), "main()- Exception occured");
    }

    log4cplus::Logger::shutdown();
    return 0;
}


void
TestThread::run()
{
    try {
        LOG4CPLUS_WARN(logger, name + LOG4CPLUS_TEXT(" TestThread.run()- Starting..."));
        NDC& ndc = getNDC();
        NDCContextCreator _first_ndc(name);
        LOG4CPLUS_DEBUG(logger, "Entering Run()...");
        for(int i=0; i<NUM_LOOPS; ++i) {
            NDCContextCreator _ndc(LOG4CPLUS_TEXT("loop"));
            global->doSomething();
        }
        LOG4CPLUS_DEBUG(logger, "Exiting run()...");

        ndc.remove();
    }
    catch(exception& e) {
        LOG4CPLUS_FATAL(logger, "TestThread.run()- Exception occurred: " << e.what());
    }
    catch(...) {
        LOG4CPLUS_FATAL(logger, "TestThread.run()- Exception occurred!!");
    }
    LOG4CPLUS_WARN(logger, name << " TestThread.run()- Finished");
} // end "run"

