#pragma once

#include "../../../subtitles/SubtitleInputPin.h"

class XySubFilter;

class SubtitleInputPin2 : public CSubtitleInputPin
{
public:
    SubtitleInputPin2(XySubFilter* pFilter, CCritSec* pLock, CCritSec* pSubLock, HRESULT* phr);

protected:
    virtual void AddSubStream(ISubStream* pSubStream);
    virtual void RemoveSubStream(ISubStream* pSubStream);
    virtual void InvalidateSubtitle(REFERENCE_TIME rtStart, ISubStream* pSubStream);

protected:
    XySubFilter * xy_sub_filter;
};
