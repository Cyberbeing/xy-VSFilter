#include "stdafx.h"
#include "xy_sub_filter.h"
#include "SubtitleInputPin2.h"

//
// SubtitleInputPin2
//
SubtitleInputPin2::SubtitleInputPin2( XySubFilter* pFilter, CCritSec* pLock
                                     , CCritSec* pSubLock, HRESULT* phr )
                                     : CSubtitleInputPin(pFilter,pLock,pSubLock,phr)
                                     , xy_sub_filter(pFilter)
{
    ASSERT(pFilter);
}

void SubtitleInputPin2::AddSubStream( ISubStream* pSubStream )
{
    xy_sub_filter->AddSubStream(pSubStream);
}

void SubtitleInputPin2::RemoveSubStream( ISubStream* pSubStream )
{
    xy_sub_filter->RemoveSubStream(pSubStream);
}

void SubtitleInputPin2::InvalidateSubtitle( REFERENCE_TIME rtStart, ISubStream* pSubStream )
{
    xy_sub_filter->InvalidateSubtitle(rtStart, (DWORD_PTR)(ISubStream*)pSubStream);
}
