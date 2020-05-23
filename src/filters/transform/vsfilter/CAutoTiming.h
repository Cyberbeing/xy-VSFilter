/************************************************************************/
/* author: xy                                                           */
/* date: 20110508                                                       */
/************************************************************************/
#ifndef __CAUTOTIMING_H_AF782E62_DC39_4D7F_9FD6_4119DB3BBE2F__
#define __CAUTOTIMING_H_AF782E62_DC39_4D7F_9FD6_4119DB3BBE2F__

#include <wxdebug.h>
#include <Windows.h>

class CAutoTiming
{
private:
    const TCHAR* _szBlkName;
    const int _level;
    int _start;
    int _end;
public:
    CAutoTiming(const TCHAR* szBlkName, const int level = 15)
        : _szBlkName(szBlkName), _level(level)
    {
        _start = GetTickCount();
        DbgLog((LOG_TIMING, _level, TEXT("%s:start at %d ms"), _szBlkName, _start));
    }

    ~CAutoTiming()
    {
        _end = GetTickCount();
        DbgLog((LOG_TIMING, _level, TEXT("%s:(%d,%d)total %d ms"), _szBlkName, _start, _end, _end-_start));
    }
};

#endif // end of __CAUTOTIMING_H_AF782E62_DC39_4D7F_9FD6_4119DB3BBE2F__
