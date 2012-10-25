/************************************************************************/
/* author: xy                                                           */
/* date: 20110512                                                       */
/************************************************************************/
#include "xy_logger.h"
#include <fstream>

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
    log4cplus::Hierarchy& h = log4cplus::Logger::getDefaultHierarchy();
    unsigned flags = 0;
    log4cplus::PropertyConfigurator::doConfigure(configFilename, h, flags);
#endif
}

void DumpPackBitmap2File(POINT pos, SIZE size, LPCVOID pixels, int pitch, const char *filename)
{
    using namespace std;

    ofstream axxx(filename, ios_base::app);
    int h = size.cy;
    int w = size.cx;
    const BYTE* src = reinterpret_cast<const BYTE*>(pixels);

    axxx<<"pos:("<<pos.x<<","<<pos.y<<") size:("<<size.cx<<","<<size.cy<<")"<<std::endl;
    for(int i=0;i<h;i++, src += pitch)
    {
        const BYTE* s2 = src;
        const BYTE* s2end = s2 + w*4;
        for(; s2 < s2end; s2 += 4)
        {
            axxx<<(int)s2[0]<<","<<(int)s2[1]<<","<<(int)s2[2]<<","<<(int)s2[3]<<",";
        }
        axxx<<std::endl;
    }
    axxx.close();
}

} //namespace xy_logger

