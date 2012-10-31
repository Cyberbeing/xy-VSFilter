/* 
 *	Copyright (C) 2003-2006 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "stdafx.h"
#include "DirectVobSubFilter.h"
#include "TextInputPin.h"
#include "xy_sub_filter.h"

CTextInputPin::CTextInputPin(CBaseFilter* pFilter, CCritSec* pLock, CCritSec* pSubLock, HRESULT* phr)
    : CSubtitleInputPin(pFilter, pLock, pSubLock, phr)
{
    HRESULT hr = NOERROR;
    if (!m_pFilter)
    {
        hr = E_INVALIDARG;
    }
    else
    {
        //fix me: better design
        if (!dynamic_cast<XySubFilter*>(pFilter) && !dynamic_cast<CDirectVobSubFilter*>(pFilter))
        {
            hr = E_INVALIDARG;
        }
    }
    if (FAILED(hr) && phr)
    {
        *phr = hr;
    }
}

void CTextInputPin::AddSubStream(ISubStream* pSubStream)
{
    CDirectVobSubFilter * dvs = dynamic_cast<CDirectVobSubFilter*>(m_pFilter);
    if (dvs)
    {
        dvs->AddSubStream(pSubStream);
    }
    else
    {
        XySubFilter * xy_sub_filter = dynamic_cast<XySubFilter*>(m_pFilter);
        if (xy_sub_filter)
        {
            xy_sub_filter->AddSubStream(pSubStream);
        }
        else
        {
            ASSERT(0);
        }
    }
}

void CTextInputPin::RemoveSubStream(ISubStream* pSubStream)
{
    CDirectVobSubFilter * dvs = dynamic_cast<CDirectVobSubFilter*>(m_pFilter);
    if (dvs)
    {
        dvs->RemoveSubStream(pSubStream);
    }
    else
    {
        XySubFilter * xy_sub_filter = dynamic_cast<XySubFilter*>(m_pFilter);
        if (xy_sub_filter)
        {
            xy_sub_filter->RemoveSubStream(pSubStream);
        }
        else
        {
            ASSERT(0);
        }
    }
}

void CTextInputPin::InvalidateSubtitle(REFERENCE_TIME rtStart, ISubStream* pSubStream)
{
    CDirectVobSubFilter * dvs = dynamic_cast<CDirectVobSubFilter*>(m_pFilter);
    if (dvs)
    {
        dvs->InvalidateSubtitle(rtStart, (DWORD_PTR)(ISubStream*)pSubStream);
    }
    else
    {
        XySubFilter * xy_sub_filter = dynamic_cast<XySubFilter*>(m_pFilter);
        if (xy_sub_filter)
        {
            xy_sub_filter->InvalidateSubtitle(rtStart, (DWORD_PTR)(ISubStream*)pSubStream);
        }
        else
        {
            ASSERT(0);
        }
    }
}
